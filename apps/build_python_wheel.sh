#!/bin/sh

touch ffmpeg_lib.i
pip uninstall -y --exists-action w ffmpeg_lib
python setup.py bdist_wheel --universal
pip install dist/ffmpeg_lib-1.0.0-cp36-cp36m-linux_x86_64.whl
