from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext

setup(
    ext_modules=[
        Pybind11Extension(
            "pyslither._core",
            [
                "native/bindings.cpp",
                "native/util/tdarray.c",
                "native/env.c",
            ],
            include_dirs=["native"],
            cxx_std=17,
        ),
    ],
    cmdclass={"build_ext": build_ext},
)