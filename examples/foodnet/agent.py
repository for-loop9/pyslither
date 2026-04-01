from pyslither import Simulation
import numpy as np

NUM_FOOD_SECTORS = 8
VIEW_RADIUS = 300
VIEW_RADIUS2 = VIEW_RADIUS * VIEW_RADIUS
NUM_OBS = 3 + NUM_FOOD_SECTORS * 4
TAU = np.pi * 2
SNAKE_IDX = 0
MAX_FOOD_VALUE = 14
FOOD_RAY_ANGLE = TAU / NUM_FOOD_SECTORS


def state_reset(sim: Simulation):
    sim.reset()

    rand_ang = np.random.random() * TAU
    rand_d = sim.safe_radius * 0.8 * np.sqrt(np.random.random())
    x = sim.config.radius + rand_d * np.cos(rand_ang)
    y = sim.config.radius + rand_d * np.sin(rand_ang)

    sim.new_snake(x, y, np.random.random() * np.pi * 2)

    for i in range(0, 1024):
        rand_ang = np.random.random() * TAU
        rand_d = sim.safe_radius * 0.9 * np.sqrt(np.random.random())
        x = sim.config.radius + rand_d * np.cos(rand_ang)
        y = sim.config.radius + rand_d * np.sin(rand_ang)
        v = np.random.uniform(3, 8)
        sim.new_food(x, y, v)


def get_observation(sim: Simulation):
    obs = np.zeros(NUM_OBS, dtype=np.float32)

    if sim.snake_dead_flags[SNAKE_IDX]:
        return obs

    hx = sim.get_snake_head_x(SNAKE_IDX)
    hy = sim.get_snake_head_y(SNAKE_IDX)
    heading = sim.snake_angles[SNAKE_IDX]

    dx = sim.config.radius - hx
    dy = sim.config.radius - hy

    d2 = dx * dx + dy * dy
    d = np.sqrt(d2)

    obs[0] = np.cos(heading)
    obs[1] = np.sin(heading)
    obs[2] = 1 - (d / sim.safe_radius)

    fcr = int(np.ceil(VIEW_RADIUS / sim.food_cell_size))
    fcx = max(fcr, min(int(hx * sim.inv_food_cell_size), sim.food_grid_size - fcr - 1))
    fcy = max(fcr, min(int(hy * sim.inv_food_cell_size), sim.food_grid_size - fcr - 1))

    fr = [(-1, -1, -1, -1)] * NUM_FOOD_SECTORS

    fxs = sim.food_xs
    fys = sim.food_ys
    fvs = sim.food_values

    for iy in range(-fcr, fcr + 1):
        for ix in range(-fcr, fcr + 1):
            cell_idx = (fcy + iy) * sim.food_grid_size + (fcx + ix)

            cell = sim.get_food_cell(cell_idx)
            for food in cell:
                dx = fxs[food] - hx
                dy = fys[food] - hy
                d2 = dx * dx + dy * dy
                if d2 <= VIEW_RADIUS2:
                    rel_ang = (np.atan2(dy, dx) - heading + TAU) % TAU
                    ray = min(int(rel_ang / FOOD_RAY_ANGLE), NUM_FOOD_SECTORS - 1)
                    d = np.sqrt(d2)
                    nd = 1 - d / VIEW_RADIUS
                    val = fvs[food] / MAX_FOOD_VALUE

                    if nd > fr[ray][0]:
                        fr[ray] = (nd, val, np.cos(rel_ang), np.sin(rel_ang))

    for i, f in enumerate(fr):
        obs_idx = 3 + i * 4
        obs[obs_idx : obs_idx + 4] = f

    return obs


def get_reward(sim: Simulation, prev_length):
    snake_length = sim.get_snake_length(SNAKE_IDX)
    died = sim.snake_dead_flags[SNAKE_IDX]

    reward = (snake_length - prev_length) * 0.5
    reward += 0.0001

    if died:
        reward -= 1.0

    return reward, died


def set_action(sim: Simulation, prediction):
    cos_a, sin_a = prediction[0], prediction[1]
    target_angle = np.atan2(sin_a, cos_a)
    sim.set_snake_target_angle(
        (sim.snake_angles[SNAKE_IDX] + target_angle) % TAU, SNAKE_IDX
    )
    # ignore boost since we only care about food
