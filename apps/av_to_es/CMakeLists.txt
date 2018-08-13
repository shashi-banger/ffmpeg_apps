cmake_minimum_required(VERSION 3.2)

project(av_to_es)


include_directories(BEFORE
                   ${PROJECT_SOURCE_DIR}/../../libavformat/
                   ${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/include
                   )

link_directories(${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/lib /usr/local/lib)

add_executable(av_to_es av_to_es.c)

target_link_libraries(av_to_es avformat avcodec x264 avdevice avfilter swresample swscale avutil fdk-aac bz2 z lzma m va va-drm va-x11 X11 dl pthread)

INSTALL(TARGETS av_to_es DESTINATION /usr/local/bin/)
