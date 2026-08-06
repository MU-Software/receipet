# Everything static lives in pyproject.toml; this only wires up the CFFI
# builder, which setuptools cannot yet declare declaratively.
from setuptools import setup

setup(cffi_modules=["python/build_ffi.py:ffibuilder"])
