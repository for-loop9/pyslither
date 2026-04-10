import pyray as rl
import pyslither
import numpy as np
from pyslither import Event

from pathlib import Path

WW = 768
WH = 432
MY_SNAKE = 0
HERE = Path(__file__).parent

BG = (13, 13, 20, 255)
WORLD_BG = (26, 26, 46, 255)
ENV_BG = (18, 18, 42, 255)
FOOD_COL = (231, 76, 60, 200)
SNAKE_BODY = (26, 92, 58, 255)
SNAKE_HEAD = (39, 174, 96, 255)
TEXT_COL = (200, 198, 190, 255)
DIM_COL = (90, 88, 82, 255)

BOT_BODY = (46, 74, 110, 255)
BOT_HEAD = (61, 100, 148, 255)

def restart(sim: pyslither.Simulation):
    sim.reset()

    for i in range(0, sim.config.max_snakes):
        rand_ang = np.random.random() * np.pi * 2
        rand_d = sim.spawn_radius * np.sqrt(np.random.random())
        x = sim.config.radius + rand_d * np.cos(rand_ang)
        y = sim.config.radius + rand_d * np.sin(rand_ang)
        sim.new_snake(x, y, np.random.random() * np.pi * 2)

    for i in range(0, 200):
        rand_ang = np.random.random() * np.pi * 2
        rand_d = sim.safe_radius * np.sqrt(np.random.random())
        x = sim.config.radius + rand_d * np.cos(rand_ang)
        y = sim.config.radius + rand_d * np.sin(rand_ang)
        v = np.random.uniform(3, 8)
        sim.new_food(x, y, v)

    sim.user_data["killed"] = False

def snake_killed(sim: pyslither.Simulation, killed: int, killer: int):
    if killed == MY_SNAKE:
        sim.user_data["killed"] = True


sim = pyslither.Simulation(
    radius=1000,
    max_snakes=5,
    user_data={"dead": False},
    # optional callbacks
    callbacks={
        Event.SnakeKilled: snake_killed,
    },
)

restart(sim)

rl.set_trace_log_level(rl.TraceLogLevel.LOG_ERROR)
rl.init_window(WW, WH, "pyslither")
rl.set_target_fps(125)

font_file = HERE.parent / "res" / "jetbrains.ttf"
font = rl.load_font_ex(str(font_file), 20, None, 0)
FONT_SIZE = font.baseSize
cam = rl.Camera2D((WW * 0.5, WH * 0.5), (sim.config.radius, sim.config.radius), 0, 0.5)

while not rl.window_should_close():
    dt = rl.get_frame_time()

    rl.begin_drawing()
    rl.clear_background(BG)

    wheel = rl.get_mouse_wheel_move()
    if wheel != 0:
        cam.zoom *= np.pow(1.1, wheel)

    mw = rl.get_screen_to_world_2d(rl.get_mouse_position(), cam)
    ta = np.atan2(mw.y - cam.target.y, mw.x - cam.target.x)
    if ta < 0:
        ta += np.pi * 2

    sim.set_snake_target_angle(ta, MY_SNAKE)

    if rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT):
        sim.set_snake_boost(True, MY_SNAKE)
    elif rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
        sim.set_snake_boost(False, MY_SNAKE)
    
    for i, target_angle in enumerate(sim.snake_target_angles):
        if i != 0:
            rand_drift = (np.random.random() - 0.5) * 0.5
            sim.set_snake_target_angle((target_angle + rand_drift) % (2 * np.pi), i)

            dx = sim.config.radius - sim.get_snake_head_x(i)
            dy = sim.config.radius - sim.get_snake_head_y(i)
            d2 = dx * dx + dy * dy
            sr = sim.safe_radius * 0.9

            if d2 > sr * sr:
                sim.set_snake_target_angle(np.atan2(dy, dx), i)

    dtms = (dt * 1000) / pyslither.MS_PER_TICK
    sim.tick(dtms)

    if sim.user_data["killed"]:
        restart(sim)

    cam.target.x = sim.get_snake_head_x(MY_SNAKE)
    cam.target.y = sim.get_snake_head_y(MY_SNAKE)

    rl.begin_mode_2d(cam)

    rl.draw_circle_v(
        (sim.config.radius, sim.config.radius), sim.config.radius, WORLD_BG
    )
    rl.draw_circle_v((sim.config.radius, sim.config.radius), sim.safe_radius, ENV_BG)

    for fx, fy, fv in zip(sim.food_xs, sim.food_ys, sim.food_values):
        rl.draw_circle_v((fx, fy), fv, FOOD_COL)

    for i, (radius, num_parts) in enumerate(
        zip(sim.snake_radii, sim.snake_part_counts)
    ):
        is_player = i == MY_SNAKE
        body_col = SNAKE_BODY if is_player else BOT_BODY
        head_col = SNAKE_HEAD if is_player else BOT_HEAD

        lx = hx = sim.get_snake_head_x(i)
        ly = hy = sim.get_snake_head_y(i)
        srx2 = radius + radius

        for j in range(num_parts):
            px = sim.get_snake_part_x(i, j)
            py = sim.get_snake_part_y(i, j)
            rl.draw_line_ex((lx, ly), (px, py), srx2, body_col)
            lx, ly = px, py
            rl.draw_circle_v((px, py), radius, body_col)

        rl.draw_circle_v((hx, hy), radius, head_col)

    rl.end_mode_2d()

    rl.draw_text_ex(
        font,
        f"fps: {rl.get_fps()}\nlength: {sim.get_snake_length(MY_SNAKE):.2f}",
        (10, 10),
        FONT_SIZE,
        0,
        TEXT_COL,
    )

    rl.end_drawing()

rl.unload_font(font)
