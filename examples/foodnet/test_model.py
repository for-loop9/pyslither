import pyray as rl
from agent import *
from pathlib import Path
from stable_baselines3 import PPO
import pyslither

HERE = Path(__file__).parent
WW = 768
WH = 432

def draw_obs(sim: Simulation):
    hx = sim.get_snake_head_x(SNAKE_IDX)
    hy = sim.get_snake_head_y(SNAKE_IDX)
    angles = sim.get_snake_angle_array()
    heading = angles[SNAKE_IDX]

    rl.draw_circle_lines(int(hx), int(hy), VIEW_RADIUS, (100, 125, 111, 70))
    rl.draw_poly(
        (hx + math.cos(heading) * (sr * 2), hy + math.sin(heading) * (sr * 2)),
        3,
        sr * 0.75,
        math.degrees(heading),
        SNAKE_HEAD,
    )

    for ray_i in range(NUM_FOOD_SECTORS):
        oi = 3 + ray_i * 4
        nd, val, cos_rel, sin_rel = obs[oi], obs[oi + 1], obs[oi + 2], obs[oi + 3]

        ray_ang = heading + ray_i * FOOD_RAY_ANGLE
        ex = hx + math.cos(ray_ang) * VIEW_RADIUS
        ey = hy + math.sin(ray_ang) * VIEW_RADIUS

        rl.draw_line_v((hx, hy), (ex, ey), (100, 125, 111, 70))

        if nd > 0:
            rel_ang = math.atan2(sin_rel, cos_rel)
            world_ang = heading + rel_ang
            d = (1 - nd) * VIEW_RADIUS
            fx = hx + math.cos(world_ang) * d
            fy = hy + math.sin(world_ang) * d
            food_r = val * MAX_FOOD_VALUE
            rl.draw_line_v((hx, hy), (fx, fy), (160, 110, 70, 180))
            rl.draw_circle_v((fx, fy), food_r, (180, 130, 90, 200))

font_file = HERE.parent / "res" / "jetbrains.ttf"
model_file = HERE / "model" / "foodnet"

model = PPO.load(str(model_file))
print(f"Loaded model.")

sim = Simulation()

state_reset(sim)

rl.set_trace_log_level(rl.TraceLogLevel.LOG_ERROR)
rl.init_window(WW, WH, "pyslither")
rl.set_target_fps(125)

FONT_SIZE = 20
font = rl.load_font_ex(str(font_file), FONT_SIZE, None, 0)

cam = rl.Camera2D((WW * 0.5, WH * 0.5), (sim.config.radius, sim.config.radius), 0, 0.5)

obs = get_observation(sim)

BG = (235, 232, 225, 255)
WORLD_BG = (215, 210, 200, 255)
ENV_BG = (228, 224, 216, 255)
FOOD_COL = (210, 200, 200, 255)
SNAKE_BODY = (140, 145, 150, 255)
SNAKE_HEAD = (110, 115, 120, 255)
TEXT_COL = (90, 85, 78, 255)
DIM_COL = (160, 155, 148, 255)

# comment for light theme
BG = (18, 18, 22, 255)
WORLD_BG = (28, 28, 35, 255)
ENV_BG = (22, 22, 30, 255)
FOOD_COL = (80, 60, 60, 150)
SNAKE_BODY = (70, 80, 95, 255)
SNAKE_HEAD = (100, 115, 135, 255)
TEXT_COL = (200, 198, 190, 255)
DIM_COL = (90, 88, 82, 255)

time_alive = 0.0
last_inference = 0
inference_interval = 50
enable_obs = True

while not rl.window_should_close():
    dt = rl.get_frame_time()
    fps = rl.get_fps()

    rl.begin_drawing()
    rl.clear_background(BG)

    # Zoom
    wheel = rl.get_mouse_wheel_move()
    if wheel != 0:
        cam.zoom *= math.pow(1.1, wheel)

    if rl.is_key_pressed(rl.KeyboardKey.KEY_O):
        enable_obs = not enable_obs

    ctm = rl.get_time() * 1000

    if ctm - last_inference >= 50:
        last_inference = ctm
        prediction, _ = model.predict(obs)
        set_action(sim, prediction)

    time_alive += dt

    dtms = (dt * 1000) / pyslither.MS_PER_TICK
    sim.tick(dtms)

    deads = sim.get_snake_dead_array()
    if deads[SNAKE_IDX]:
        state_reset(sim)
        time_alive = 0.0

    obs = get_observation(sim)
    snake_length = sim.get_snake_length(SNAKE_IDX)

    cam.target.x = sim.get_snake_head_x(SNAKE_IDX)
    cam.target.y = sim.get_snake_head_y(SNAKE_IDX)

    rl.begin_mode_2d(cam)

    rl.draw_circle_v(
        (sim.config.radius, sim.config.radius), sim.config.radius, WORLD_BG
    )
    rl.draw_circle_v((sim.config.radius, sim.config.radius), sim.safe_radius, ENV_BG)

    for x, y, value in zip(
        sim.get_food_x_array(), sim.get_food_y_array(), sim.get_food_value_array()
    ):
        rl.draw_circle_v((x, y), value, FOOD_COL)

    for i, (sr, dead, num_parts) in enumerate(
        zip(
            sim.get_snake_radius_array(),
            sim.get_snake_dead_array(),
            sim.get_snake_num_parts_array(),
        )
    ):
        if not dead:
            lx = hx = sim.get_snake_head_x(i)
            ly = hy = sim.get_snake_head_y(i)
            srx2 = sr + sr

            for j in range(num_parts):
                px = sim.get_snake_part_x(i, j)
                py = sim.get_snake_part_y(i, j)
                rl.draw_line_ex((lx, ly), (px, py), srx2, SNAKE_BODY)
                lx, ly = px, py
                rl.draw_circle_v((px, py), sr, SNAKE_BODY)

            rl.draw_circle_v((hx, hy), sr, SNAKE_HEAD)

    if enable_obs:
        draw_obs(sim)

    rl.end_mode_2d()

    rl.draw_text_ex(
        font,
        f"fps: {rl.get_fps()}\nlength: {sim.get_snake_length(SNAKE_IDX):.2f}",
        (10, 10),
        FONT_SIZE,
        0,
        TEXT_COL,
    )

    rl.end_drawing()

rl.unload_font(font)
