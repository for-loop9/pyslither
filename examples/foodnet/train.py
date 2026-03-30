import time
import random
import warnings
from pathlib import Path

import numpy as np
import gymnasium as gym
from gymnasium import spaces
from stable_baselines3 import PPO
from stable_baselines3.common.env_checker import check_env
from stable_baselines3.common.vec_env import SubprocVecEnv, VecMonitor
from stable_baselines3.common.callbacks import (
    BaseCallback,
    CallbackList,
)
from rich.progress import (
    Progress,
    BarColumn,
    TextColumn,
    TimeRemainingColumn,
    TaskProgressColumn,
)

import pyslither
from pyslither import Simulation
from agent import (
    NUM_OBS,
    SNAKE_IDX,
    state_reset,
    get_observation,
    get_reward,
    set_action,
)


HERE = Path(__file__).parent
MODEL_DIR = HERE / "model"
MODEL_NAME = "foodnet"


def fmt_time(seconds: float) -> str:
    s = int(seconds)
    h, rem = divmod(s, 3600)
    m, sec = divmod(rem, 60)
    return f"{h:02d}:{m:02d}:{sec:02d}"


class SlitherEnv(gym.Env):
    metadata = {"render_modes": []}

    def __init__(self, max_steps: int = 4000):
        super().__init__()
        self.max_steps = max_steps

        self.action_space = spaces.Box(low=-1.0, high=1.0, shape=(2,), dtype=np.float32)
        self.observation_space = spaces.Box(
            low=-1.0, high=1.0, shape=(NUM_OBS,), dtype=np.float32
        )

        self.sim = Simulation()
        self._step = 0
        self._prev_length = 0.0

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        if seed is not None:
            random.seed(seed)
            np.random.seed(seed)

        state_reset(self.sim)
        self._step = 0
        self._prev_length = self.sim.get_snake_length(SNAKE_IDX)

        return get_observation(self.sim), {}

    def step(self, action):
        set_action(self.sim, action)
        self.sim.tick(1.0)
        self._step += 1

        obs = get_observation(self.sim)
        reward, died = get_reward(self.sim, self._prev_length)
        self._prev_length = self.sim.get_snake_length(SNAKE_IDX)

        terminated = bool(died)
        truncated = self._step >= self.max_steps

        info = {}
        if terminated or truncated:
            info["snake_length"] = self._prev_length
            info["time_alive_s"] = self._step * pyslither.MS_PER_TICK / 1000.0

        return obs, reward, terminated, truncated, info

    def close(self):
        pass


_PROGRESS_BAR_WIDTH = 64


class StatsCallback(BaseCallback):
    def __init__(self, print_freq: int, n_envs: int, total_timesteps: int):
        super().__init__(verbose=0)
        self.print_freq = print_freq
        self.n_envs = n_envs
        self.total_timesteps = total_timesteps
        self.train_start = None
        self._steps_at_run_start = 0
        self._ep_rewards = []
        self._ep_lengths = []
        self._ep_time_alive = []
        self._progress: Progress | None = None
        self._task = None

    def _on_training_start(self):
        self.train_start = time.time()
        self._steps_at_run_start = self.num_timesteps
        self._progress = Progress(
            TextColumn("[progress.percentage]{task.percentage:>5.1f}%"),
            BarColumn(bar_width=_PROGRESS_BAR_WIDTH),
            TaskProgressColumn(text_format="{task.completed:>,}/{task.total:>,}"),
            TimeRemainingColumn(),
        )
        self._task = self._progress.add_task("", total=self.total_timesteps)
        self._progress.start()

    def _on_training_end(self):
        if self._progress is not None:
            self._progress.update(self._task, completed=self.total_timesteps)
            self._progress.stop()

    def _on_step(self) -> bool:
        infos = self.locals.get("infos", [])
        dones = self.locals.get("dones", [])

        for done, info in zip(dones, infos):
            if done:
                ep = info.get("episode", {})
                if ep:
                    self._ep_rewards.append(ep["r"])
                if "snake_length" in info:
                    self._ep_lengths.append(info["snake_length"])
                if "time_alive_s" in info:
                    self._ep_time_alive.append(info["time_alive_s"])

        steps_this_run = self.num_timesteps - self._steps_at_run_start
        if self._progress is not None:
            self._progress.update(self._task, completed=steps_this_run)

        if steps_this_run % self.print_freq < self.n_envs:
            self._print()

        return True

    def _print(self):
        elapsed = time.time() - self.train_start
        sim_time_s = self.num_timesteps * pyslither.MS_PER_TICK / 1000.0
        steps_this_run = self.num_timesteps - self._steps_at_run_start
        pct = 100 * steps_this_run / self.total_timesteps

        avg_reward = (
            np.mean(self._ep_rewards[-100:]) if self._ep_rewards else float("nan")
        )
        avg_length = (
            np.mean(self._ep_lengths[-100:]) if self._ep_lengths else float("nan")
        )
        avg_alive = (
            np.mean(self._ep_time_alive[-100:]) if self._ep_time_alive else float("nan")
        )

        line = (
            f"[{pct:5.1f}%]  "
            f"wall={fmt_time(elapsed)}  "
            f"sim={fmt_time(sim_time_s)}  "
            f"avg_reward={avg_reward:7.3f}  "
            f"avg_snake_length={avg_length:6.1f}  "
            f"avg_alive={fmt_time(avg_alive)}"
        )
        if self._progress is not None:
            self._progress.console.print(line)
        else:
            print(line)


def make_env(rank: int = 0, seed: int = 0):
    def _init():
        env = SlitherEnv()
        env.reset(seed=seed + rank)
        return env

    return _init


def train(
    n_envs: int = 4,
    total_timesteps: int = 1_000_000,
    resume: bool = True,
    seed: int = 42,
):
    MODEL_DIR.mkdir(parents=True, exist_ok=True)

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        check_env(SlitherEnv(), warn=False)

    vec_env = SubprocVecEnv([make_env(i, seed) for i in range(n_envs)])
    vec_env = VecMonitor(vec_env)

    model_path = MODEL_DIR / MODEL_NAME

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")

        if resume and model_path.with_suffix(".zip").exists():
            print(f"Resuming from {model_path}")
            model = PPO.load(str(model_path), env=vec_env, device="auto")
        else:
            print("Starting fresh.")
            model = PPO(
                policy="MlpPolicy",
                env=vec_env,
                learning_rate=3e-4,
                n_steps=2048,
                batch_size=256,
                n_epochs=10,
                gamma=0.99,
                gae_lambda=0.95,
                clip_range=0.2,
                ent_coef=0.01,
                vf_coef=0.5,
                max_grad_norm=0.5,
                verbose=0,
                seed=seed,
                device="auto",
                policy_kwargs=dict(net_arch=[64, 64]),
            )

    stats_cb = StatsCallback(
        print_freq=max(50_000 // n_envs, 1) * n_envs,
        n_envs=n_envs,
        total_timesteps=total_timesteps,
    )

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        model.learn(
            total_timesteps=total_timesteps,
            callback=CallbackList([stats_cb]),
            reset_num_timesteps=not resume,
            progress_bar=False,
        )

    model.save(str(model_path))
    print(f"Saved to {model_path}.zip")
    vec_env.close()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--envs", type=int, default=4)
    parser.add_argument("--steps", type=int, default=1_000_000)
    parser.add_argument("--no-resume", action="store_true")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    train(
        n_envs=args.envs,
        total_timesteps=args.steps,
        resume=not args.no_resume,
        seed=args.seed,
    )
