from setuptools import Extension, setup

ext = Extension(
    name='soundbox',
    sources=['soundbox.cpp'],
    define_macros=[],
    include_dirs=[],
    library_dirs=[],
    libraries=[],
)

setup(
    name='soundbox',
    version='0.2.0',
    ext_modules=[ext],
)
