"""
Python bindings for the Slither.io-style simulation environment.
"""
from __future__ import annotations
from pyslither._core import Config
from pyslither._core import Simulation
from . import _core
__all__: list[str] = ['Config', 'MS_PER_TICK', 'Simulation']
MS_PER_TICK: int = 8
