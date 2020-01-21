
#ifndef __TS_MUXER_H__
#define __TS_MUXER_H__

#include <pthread.h>
#include "ts_muxer_fifo.h"

#define MAX_AUDIO_STREAMS 32

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
    int  vid_pid;
    AvCodecEnum   vid_codec;
    AvCodecEnum   aud_codec[MAX_AUDIO_STREAMS];
    
}ts_muxer_params_t;

typedef struct ts_muxer_stream {
    AVStream  *st;
    ts_muxer_fifo  *inp_fifo;
    int            pid_num;
    int64_t        last_pts;
}ts_muxer_stream;

typedef struct ts_muxer {
    ts_muxer_stream   st_video;
    int               num_aud_streams;
    ts_muxer_stream   st_audio[MAX_AUDIO_STREAMS];
    ts_muxer_fifo   *out_fifo;
    int64_t                last_video_pts;
    AVFormatContext        *av_fmt_ctxt;
    AVRational time_base; //(AVRational){1, 90000}
    pthread_t     thread;
}ts_muxer;

ts_muxer* create_ts_muxer(ts_muxer_params_t *params);
int write_video_frame(ts_muxer *mux, unsigned char *buf, 
                      int size, int64_t pts);
int  write_audio_frame(ts_muxer *mux, unsigned char *buf, int size,
                       int64_t pts, int aud_index);

int read_muxed_data(ts_muxer *mux, unsigned char *buf, int size);
int get_video_write_avail_size(ts_muxer *mux);
int get_audio_write_avail_size(ts_muxer *mux, int aud_index);



#endif //__TS_MUXER_H__