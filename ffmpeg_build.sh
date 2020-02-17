#!/bin/bash

./ffmpeg_configure.sh
make all -j 4
make install prefix=/ DESTDIR=/usr/local/amagi

