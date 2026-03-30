from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension
import sys

compile_args = ["-O3"]
if sys.platform != "win32":
    compile_args += ["-march=native", "-ffast-math"]

setup(
    ext_modules=[
        Pybind11Extension(
            "pyslither._core",
            [
                "native/util/tdarray.c",
                "native/env.c",
                "native/bindings.cpp",
            ],
            include_dirs=["native"],
            extra_compile_args=compile_args,
            language="c++",
        )
    ]
)