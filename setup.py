from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension
from setuptools.command.build_ext import build_ext as build_ext_orig
import subprocess
import sys

compile_args = ["-O3"]

if sys.platform != "win32":
    compile_args += ["-march=native", "-ffast-math"]


class BuildExt(build_ext_orig):
    def run(self):
        super().run()
        subprocess.check_call(
            [sys.executable, "-m", "pybind11_stubgen", "pyslither", "-o", "."]
        )


ext_modules = [
    Pybind11Extension(
        "pyslither",
        [
            "native/util/tdarray.c",
            "native/env.c",
            "native/bindings.cpp",
        ],
        include_dirs=["native"],
        extra_compile_args=compile_args,
        language="c++",
    ),
]

setup(
    name="pyslither",
    version="0.1.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": BuildExt},
    install_requires=["numpy"],
    extras_require={
        "single_player": ["raylib"],
        "foodnet": ["raylib" "stable-baselines3", "gymnasium"],
        "examples": ["raylib", "stable-baselines3", "gymnasium"],
    },
)
