from setuptools import Extension, setup

ext = Extension(
    name='soundbox',
    sources=['soundbox.cpp'],
)

setup(
    name='soundbox',
    version='0.3.0',
    ext_modules=[ext],
    packages=['soundbox_core'],
    package_data={
        'soundbox_core': ['soft_oal.dll'],
        'soundbox-stubs': ['__init__.pyi'],
    },
    include_package_data=True,
)
