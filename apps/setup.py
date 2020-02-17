
import os
import pathlib
from setuptools import setup, find_packages
from distutils.core import Extension
import shutil

ffmpeg_module = Extension('_ffmpeg_lib',
                          sources = ['ts_muxer/src/circular_buf.c',
                                     'ts_muxer/src/ts_muxer_fifo.c',
                                     'ts_muxer/src/ts_muxer.c', 'ffmpeg_lib.i'],
                          include_dirs = ['/usr/local/amagi/include',
                                          'ts_muxer/src'],
                          library_dirs = ["/usr/local/amagi/lib/ffmpeg/",
                                          '/usr/local/lib'],
                          runtime_library_dirs=['/usr/local/amagi/lib/ffmpeg/'],
                          libraries = ['avformat', 'avcodec', 'x264',
                                       'avdevice', 'avfilter', 'swresample', 
                                       'swscale', 'avutil', 'fdk-aac', 'bz2',
                                       'z', 'lzma', 'm', 'va', 'va-drm',
                                       'va-x11', 'X11', 'dl', 'pthread'],
                          #extra_objexts = ["../ffmpeg_bin/lib/libavformat.so"],


                          extra_compile_args=["-fPIC"],
                          swig_opts = ["-modern", "-I./ts_muxer/src/"])



def main():
    setup(name="ffmpeg_lib",
          version="1.0.0",
          #packages=["ffmpeg_lib"],
          #package_dir={'ffmpeg_lib':'.'},
          description="Python interface for ffmpeg based ts muxer",
          author="Shashidhar Banger",
          author_email="bangu97@gmail.com",
          #py_modules=['ffmpeg_lib'],
          #package_data={'ffmpeg_lib': ['../ffmpeg_bin/lib/*.so']},
          ext_modules=[ffmpeg_module])

if __name__ == "__main__":
    main()

