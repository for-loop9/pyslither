from setuptools import setup
from setuptools.command.build_ext import build_ext
from pybind11.setup_helpers import Pybind11Extension


class BuildExt(build_ext):
    def build_extensions(self):
        ct = self.compiler.compiler_type

        for ext in self.extensions:
            ext.extra_compile_args = ["/O2"] if ct == "msvc" else ["-O3"]

        super().build_extensions()


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
            language="c++",
        )
    ],
    cmdclass={"build_ext": BuildExt},
)