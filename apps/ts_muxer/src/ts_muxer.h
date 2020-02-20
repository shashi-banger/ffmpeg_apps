
#ifndef __TS_MUXER_H__
#define __TS_MUXER_H__

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include "ts_muxer_fifo.h"

#define MAX_AUDIO_STREAMS 32
#define MAX_SCTE_STREAMS 16

typedef enum {
    AV_CODEC_UNDEFINED,
    AV_CODEC_VID_H264,
    AV_CODEC_VID_MP2,
    AV_CODEC_AUD_UNDEX = 0x1000,
    AV_CODEC_AUD_AAC,
    AV_CODEC_AUD_MP2,
    AV_CODEC_AUD_AC3
}AvCodecEnum;

typedef struct ts_muxer_params_t {
    int  num_aud_tracks;
    int  aud_pid[MAX_AUDIO_STREAMS];
    int  num_scte_tracks;
    int  scte_pid[MAX_SCTE_STREAMS];
    int  vid_pid;
    float         frame_rate;
    AvCodecEnum   vid_codec;
    int   aud_codec[MAX_AUDIO_STREAMS];
    
}ts_muxer_params_t;

typedef struct ts_muxer_stream {
    AVStream  *st;
    ts_muxer_fifo  *inp_fifo;
    int            pid_num;
    int64_t        last_pts;
    int64_t        last_dts;
    int            min_dts_diff;
    int64_t        last_monotonic_pts;
    int64_t        last_applied_dts;
    enum AVMediaType    type;
}ts_muxer_stream;

typedef struct ts_muxer {
    ts_muxer_stream   st_video;
    int               num_aud_streams;
    ts_muxer_stream   st_audio[MAX_AUDIO_STREAMS];
    int               num_scte_streams;
    ts_muxer_stream   st_scte[MAX_SCTE_STREAMS];
    ts_muxer_fifo   *out_fifo;
    int64_t                last_video_pts;
    AVFormatContext        *av_fmt_ctxt;
    AVRational time_base; //(AVRational){1, 90000}
    pthread_t     thread;
    int64_t       base_ts;
}ts_muxer;

ts_muxer* create_ts_muxer(ts_muxer_params_t *params);
int  write_video_frame(ts_muxer *mux, unsigned char *buf, 
                      int size, int64_t pts, int64_t dts);
int  write_audio_frame(ts_muxer *mux, unsigned char *buf, int size,
                       int64_t pts, int64_t dts, int aud_index);
int  write_scte35_frame(ts_muxer *mux, unsigned char *buf, int size,
                       int64_t  pts, int64_t dts, int scte_index);

int read_muxed_data(ts_muxer *mux, unsigned char *buf, int size);
int get_muxed_output_avail_size(ts_muxer *mux);
int get_video_write_avail_size(ts_muxer *mux);
int get_audio_write_avail_size(ts_muxer *mux, int aud_index);
int get_scte_write_avail_size(ts_muxer *mux, int scte_index);



#endif //__TS_MUXER_H__
