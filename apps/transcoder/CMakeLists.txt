cmake_minimum_required(VERSION 3.2)

project(transcoder)


include_directories(BEFORE
                   ${PROJECT_SOURCE_DIR}/../../libavformat/
                   ${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/include
                   )

link_directories(${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/lib /usr/local/lib)

add_executable(transcoder transcoding.c)

target_link_libraries(transcoder -Wl,--start-group avfilter freetype avcodec avformat x264 avdevice swresample swscale avutil postproc fdk-aac bz2 z lzma m va va-drm va-x11 X11 dl -Wl,--end-group pthread)

INSTALL(TARGETS transcoder DESTINATION /usr/local/bin/)
