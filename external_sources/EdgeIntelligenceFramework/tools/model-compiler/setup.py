from setuptools import setup, find_packages

setup(
    name="eif_compiler",
    version="0.1.0",
    packages=find_packages(),
    install_requires=[
        "numpy",
        "onnx",
        "onnxruntime"
    ],
    entry_points={
        "console_scripts": [
            "eif-convert=eif_compiler.converter:main",
        ],
    },
)
