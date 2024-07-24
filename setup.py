from setuptools import Extension, setup

ext = Extension(
    name='soundbox.soundbox',
    sources=['soundbox.cpp'],
)

setup(
    name='soundbox',
    version='0.2.0',
    ext_modules=[ext],
    packages=['soundbox'],
    package_data={'soundbox': ['soft_oal.dll']},
    include_package_data=True,
)
