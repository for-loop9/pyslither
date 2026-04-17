from setuptools import setup
from pybind11.setup_helpers import Pybind11Extension, build_ext as build_ext_orig

class build_ext(build_ext_orig):
    def build_extensions(self):
        original_compile = self.compiler._compile

        def _compile(obj, src, ext, cc_args, extra_postargs, pp_opts):
            if src.endswith(".c"):
                extra_postargs = [
                    arg for arg in (extra_postargs or [])
                    if not arg.startswith("-std=c++")
                ]
            return original_compile(obj, src, ext, cc_args, extra_postargs, pp_opts)

        self.compiler._compile = _compile
        super().build_extensions()

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