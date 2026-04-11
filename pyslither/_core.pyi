"""
Python bindings for the Slither.io-style simulation environment.
"""
from __future__ import annotations
import enum
import numpy
import numpy.typing
import typing
__all__: list[str] = ['Config', 'Event', 'MS_PER_TICK', 'Simulation']
class Config:
    """
    Simulation configuration. Set via Simulation constructor.
    """
    @property
    def base_speed(self) -> float:
        ...
    @property
    def max_parts(self) -> int:
        ...
    @property
    def max_snakes(self) -> int:
        ...
    @property
    def max_speed(self) -> float:
        ...
    @property
    def mouth_radius(self) -> int:
        ...
    @property
    def mouth_speed(self) -> float:
        ...
    @property
    def radius(self) -> float:
        ...
    @property
    def segment_length(self) -> int:
        ...
    @property
    def speed_thickness(self) -> float:
        ...
    @property
    def tail_stiffness(self) -> float:
        ...
    @property
    def turn_speed(self) -> float:
        ...
class Event(enum.IntEnum):
    """
    Events that trigger user-defined callbacks in the simulation.
    
    Attributes
    ----------
    SnakeKilled
        Fired when a snake is killed:
        ``(sim, victim, killer)``, where ``victim``
        is the victim snake's index, and ``killer`` is either the killer snake's index or ``-1``    if victim is killed by border.
    FoodEaten
        Fired when a snake eats a food:
        ``(sim, snake, food)``, where ``eater`` is
        the eating snake's index and ``food`` is the consumed food's index.
    SnakeGrowth
        Fired when a snake's fractional growth changes:
        ``(sim, snake)``, where ``snake`` is the snake's index.
    SnakeShift
        Fired when a snake's body parts shift (when snake moves ``config.segment_length`` units):
        ``(sim, snake, new_part)``, where ``snake`` is the snake's index, and ``new_part`` is whether the snake gained a new body part or not.
    LosePart
        Fired when a snake loses a body part:
        ``(sim, snake)``, where ``snake`` is the snake's index.
    BoostFoodSpawn
        Fired when a food is spawned due to boost:
        ``(sim, food, snake)``, where ``food`` is the food's index and ``snake`` is the snake's index.
    DeathFoodSpawn
        Fired when food spawns due to a snake's death:
        ``(sim, food, num_food, snake)``, where ``food`` is the food's starting index, ``num_food`` is the total number of foods spawned due to death, and ``snake`` is the snake's index.
    
    """
    BoostFoodSpawn: typing.ClassVar[Event]  # value = <Event.BoostFoodSpawn: 5>
    DeathFoodSpawn: typing.ClassVar[Event]  # value = <Event.DeathFoodSpawn: 6>
    FoodEaten: typing.ClassVar[Event]  # value = <Event.FoodEaten: 1>
    LosePart: typing.ClassVar[Event]  # value = <Event.LosePart: 4>
    SnakeGrowth: typing.ClassVar[Event]  # value = <Event.SnakeGrowth: 2>
    SnakeKilled: typing.ClassVar[Event]  # value = <Event.SnakeKilled: 0>
    SnakeShift: typing.ClassVar[Event]  # value = <Event.SnakeShift: 3>
    @classmethod
    def __new__(cls, value):
        ...
    def __format__(self, format_spec):
        """
        Convert to a string according to format_spec.
        """
class Simulation:
    def __init__(self, radius: typing.SupportsFloat | typing.SupportsIndex = 5000.0, base_speed: typing.SupportsFloat | typing.SupportsIndex = 5.389999866485596, speed_thickness: typing.SupportsFloat | typing.SupportsIndex = 0.4000000059604645, max_speed: typing.SupportsFloat | typing.SupportsIndex = 14.0, turn_speed: typing.SupportsFloat | typing.SupportsIndex = 0.032999999821186066, tail_stiffness: typing.SupportsFloat | typing.SupportsIndex = 0.4300000071525574, mouth_speed: typing.SupportsFloat | typing.SupportsIndex = 0.20800000429153442, mouth_radius: typing.SupportsFloat | typing.SupportsIndex = 40.0, segment_length: typing.SupportsInt | typing.SupportsIndex = 42, max_parts: typing.SupportsInt | typing.SupportsIndex = 450, max_snakes: typing.SupportsInt | typing.SupportsIndex = 32, user_data: typing.Any = None, callbacks: dict = {}) -> None:
        ...
    @typing.overload
    def get_food_cell(self, x: typing.SupportsFloat | typing.SupportsIndex, y: typing.SupportsFloat | typing.SupportsIndex) -> memoryview:
        """
        Food indices in the food collision grid cell that contains world position (`x`, `y`).
        """
    @typing.overload
    def get_food_cell(self, i: typing.SupportsInt | typing.SupportsIndex) -> memoryview:
        """
        Food indices in food collision grid cell ``i``.
        """
    @typing.overload
    def get_segment_cell(self, x: typing.SupportsFloat | typing.SupportsIndex, y: typing.SupportsFloat | typing.SupportsIndex) -> memoryview:
        """
        Segment indices in the collision grid cell that contains world position (`x`, `y`).
        """
    @typing.overload
    def get_segment_cell(self, i: typing.SupportsInt | typing.SupportsIndex) -> memoryview:
        """
        Segment indices in collision grid cell ``i``.
        """
    def get_segment_snake_idx(self, i: typing.SupportsInt | typing.SupportsIndex) -> int:
        """
        Index of the snake that owns segment ``i``.
        """
    def get_segment_x0(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        Start X of collision segment ``i``.
        """
    def get_segment_x1(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        End X of collision segment ``i``.
        """
    def get_segment_y0(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        Start Y of collision segment ``i``.
        """
    def get_segment_y1(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        End Y of collision segment ``i``.
        """
    def get_snake_head_x(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        X coordinate of snake ``i``'s head.
        """
    def get_snake_head_y(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        Y coordinate of snake ``i``'s head.
        """
    def get_snake_length(self, i: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        Effective length of snake ``i`` (number of parts + fractional growth).
        """
    def get_snake_part_x(self, i: typing.SupportsInt | typing.SupportsIndex, j: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        X coordinate of body part ``j`` of snake ``i``.
        """
    def get_snake_part_y(self, i: typing.SupportsInt | typing.SupportsIndex, j: typing.SupportsInt | typing.SupportsIndex) -> float:
        """
        Y coordinate of body part ``j`` of snake ``i``.
        """
    def new_food(self, x: typing.SupportsFloat | typing.SupportsIndex, y: typing.SupportsFloat | typing.SupportsIndex, value: typing.SupportsFloat | typing.SupportsIndex) -> bool:
        """
        Place food at (`x`, `y`) with the given value. Returns `False` if outside the safe radius.
        """
    def new_snake(self, x: typing.SupportsFloat | typing.SupportsIndex, y: typing.SupportsFloat | typing.SupportsIndex, angle: typing.SupportsFloat | typing.SupportsIndex) -> bool:
        """
        Spawn a snake at (`x`, `y`) with `angle` in radians (``0`` to ``2pi``). Returns `False` if the position is outside the spawn radius, occupied, or the max snake count is reached.
        """
    def reset(self) -> None:
        """
        Reset the environment.
        """
    def tick(self, dtms: typing.SupportsFloat | typing.SupportsIndex) -> None:
        """
        Advance the simulation one step. `dtms` is elapsed time **normalized to 8 ms**.
        """
    @property
    def config(self) -> Config:
        ...
    @property
    def food_cell_size(self) -> int:
        ...
    @property
    def food_grid_size(self) -> int:
        ...
    @property
    def food_ids(self) -> memoryview:
        """
        Unique IDs of all food.
        """
    @property
    def food_values(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Values of all foods.
        """
    @property
    def food_xs(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        X coordinates of all foods.
        """
    @property
    def food_ys(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Y coordinates of all foods.
        """
    @property
    def inv_food_cell_size(self) -> float:
        ...
    @property
    def inv_segment_cell_size(self) -> float:
        ...
    @property
    def num_food(self) -> int:
        """
        Number of foods currently in the world.
        """
    @property
    def num_snakes(self) -> int:
        """
        Total number of snakes alive.
        """
    @property
    def safe_radius(self) -> float:
        ...
    @property
    def segment_cell_size(self) -> int:
        ...
    @property
    def segment_grid_size(self) -> int:
        ...
    @property
    def snake_angles(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Current angles (radians) of all snakes.
        """
    @property
    def snake_boosts(self) -> numpy.typing.NDArray[numpy.bool]:
        """
        Boost flags of all snakes **(Writeable)**.
        """
    @property
    def snake_growths(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Fractional growth values of all snakes.
        """
    @property
    def snake_ids(self) -> memoryview:
        """
        Unique IDs of all snakes.
        """
    @property
    def snake_kill_counts(self) -> memoryview:
        """
        Kill counts of all snakes.
        """
    @property
    def snake_part_counts(self) -> memoryview:
        """
        Part counts of all snakes.
        """
    @property
    def snake_radii(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Collision radii of all snakes.
        """
    @property
    def snake_speeds(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Speeds of all snakes.
        """
    @property
    def snake_target_angles(self) -> numpy.typing.NDArray[numpy.float32]:
        """
        Target angles of all snakes in radians (``0`` to ``2pi``) **(Writeable)**.
        """
    @property
    def spawn_radius(self) -> float:
        ...
    @property
    def user_data(self) -> typing.Any:
        """
        User data.
        """
    @user_data.setter
    def user_data(self, arg1: typing.Any) -> None:
        ...
MS_PER_TICK: int = 8
