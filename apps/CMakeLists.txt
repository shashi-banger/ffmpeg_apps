cmake_minimum_required(VERSION 3.2)

project(ffmpeg_apps)

add_subdirectory(av_to_es)
add_subdirectory(transcoder)
add_subdirectory(ts_muxer)

SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Shashidhar Banger") #required

INCLUDE(CPack)
