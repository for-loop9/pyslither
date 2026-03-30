import pyray as rl
import pyslither

from pathlib import Path
import random
import math

WW = 768
WH = 432
MY_SNAKE = 0
HERE = Path(__file__).parent

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


def restart(sim: pyslither.Simulation):
    sim.reset()

    for i in range(0, sim.config.max_snakes):
        rand_ang = random.random() * math.pi * 2
        rand_d = sim.spawn_radius * math.sqrt(random.random())
        x = sim.config.radius + rand_d * math.cos(rand_ang)
        y = sim.config.radius + rand_d * math.sin(rand_ang)

        sim.new_snake(x, y)

    for i in range(0, 2048):
        rand_ang = random.random() * math.pi * 2
        rand_d = sim.spawn_radius * math.sqrt(random.random())
        x = sim.config.radius + rand_d * math.cos(rand_ang)
        y = sim.config.radius + rand_d * math.sin(rand_ang)
        v = random.uniform(3, 8)
        sim.new_food(x, y, v)


sim = pyslither.Simulation(radius=5000, max_snakes=32)

restart(sim)

rl.set_trace_log_level(rl.TraceLogLevel.LOG_ERROR)
rl.init_window(WW, WH, "pyslither")
rl.set_target_fps(125)

font_file = HERE.parent / "res" / "jetbrains.ttf"
font = rl.load_font_ex(str(font_file), 20, None, 0)
FONT_SIZE = font.baseSize
cam = rl.Camera2D((WW * 0.5, WH * 0.5), (sim.config.radius, sim.config.radius), 0, 0.5)

while not rl.window_should_close():
    rl.begin_drawing()
    rl.clear_background(BG)

    wheel = rl.get_mouse_wheel_move()
    if wheel != 0:
        zoom_factor = 1.1
        cam.zoom *= math.pow(zoom_factor, wheel)

    mw = rl.get_screen_to_world_2d(rl.get_mouse_position(), cam)
    ta = math.atan2(mw.y - cam.target.y, mw.x - cam.target.x)
    if ta < 0:
        ta += math.pi * 2

    sim.set_snake_target_angle(ta, MY_SNAKE)

    if rl.is_mouse_button_pressed(rl.MouseButton.MOUSE_BUTTON_LEFT):
        sim.set_snake_boost(1, MY_SNAKE)
    elif rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
        sim.set_snake_boost(0, MY_SNAKE)

    for i, (target_angle, dead) in enumerate(
        zip(sim.get_snake_target_angle_array(), sim.get_snake_dead_array())
    ):
        if i == 0:
            continue

        if not dead:
            rand_drift = (random.random() - 0.5) * 0.5
            sim.set_snake_target_angle((target_angle + rand_drift) % (2 * math.pi), i)

            dx = sim.config.radius - sim.get_snake_head_x(i)
            dy = sim.config.radius - sim.get_snake_head_y(i)

            d2 = dx * dx + dy * dy
            sr = sim.safe_radius * 0.9

            if d2 > sr * sr:
                sim.set_snake_target_angle(math.atan2(dy, dx), i)

    dtms = (rl.get_frame_time() * 1000) / pyslither.MS_PER_TICK
    sim.tick(dtms)

    if sim.get_snake_dead_array()[MY_SNAKE]:
        restart(sim)

    cam.target.x = sim.get_snake_head_x(MY_SNAKE)
    cam.target.y = sim.get_snake_head_y(MY_SNAKE)

    rl.begin_mode_2d(cam)

    rl.draw_circle_v(
        (sim.config.radius, sim.config.radius), sim.config.radius, WORLD_BG
    )
    rl.draw_circle_v((sim.config.radius, sim.config.radius), sim.safe_radius, ENV_BG)

    for fx, fy, fv in zip(
        sim.get_food_x_array(), sim.get_food_y_array(), sim.get_food_value_array()
    ):
        rl.draw_circle_v((fx, fy), fv, FOOD_COL)

    for i, (dead, radius, num_parts) in enumerate(
        zip(
            sim.get_snake_dead_array(),
            sim.get_snake_radius_array(),
            sim.get_snake_num_parts_array(),
        )
    ):
        if not dead:
            lx = hx = sim.get_snake_head_x(i)
            ly = hy = sim.get_snake_head_y(i)
            srx2 = radius + radius

            for j in range(num_parts):
                px = sim.get_snake_part_x(i, j)
                py = sim.get_snake_part_y(i, j)
                rl.draw_line_ex((lx, ly), (px, py), srx2, SNAKE_BODY)
                lx, ly = px, py
                rl.draw_circle_v((px, py), radius, SNAKE_BODY)

            rl.draw_circle_v((hx, hy), radius, SNAKE_HEAD)

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
