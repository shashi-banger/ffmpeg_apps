cmake_minimum_required(VERSION 3.2)

project(ts_muxer)


include_directories(BEFORE
                   ${PROJECT_SOURCE_DIR}/../../libavformat/
                   ${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/include
                   ${PROJECT_SOURCE_DIR}/ts_muxer/src
                   )

link_directories(${PROJECT_SOURCE_DIR}/../../ffmpeg_bin/lib /usr/local/lib)

add_executable(ts_muxer ${PROJECT_SOURCE_DIR}/src/ts_muxer.c
                        ${PROJECT_SOURCE_DIR}/src/ts_muxer_fifo.c
                        ${PROJECT_SOURCE_DIR}/src/circular_buf.c
                        ${PROJECT_SOURCE_DIR}/src/ts_muxer_app.c)

target_link_libraries(ts_muxer avformat avcodec x264 avdevice avfilter swresample swscale avutil fdk-aac bz2 z lzma m va va-drm va-x11 X11 dl pthread)

INSTALL(TARGETS ts_muxer DESTINATION /usr/local/bin/)

