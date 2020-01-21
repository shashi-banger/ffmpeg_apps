/*
 * Ts muxer
 */

#include <stdlib.h>
#include <stdio.h>
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
#include "ts_muxer.h"


#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

static const int video_fifo_size = 4000000; //1 sec buf @32Mbps
static const int audio_fifo_size = 384000/8; //1 sec buf at 384kbps
static const int out_ts_mux_size = 1000000;
#define MAX_INT64 (1L<<63 - 1)

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

static int write_frame(ts_muxer_stream *mux_st, AVFormatContext *fmt_ctx, 
                       const AVRational *time_base, 
                       AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    /*if(pkt->pts > mux_st->last_pts)
    {
        pkt->dts = pkt->pts;
        mux_st->last_pts = pkt->pts;
    }*/

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(ts_muxer *mux, ts_muxer_stream *mux_st,
                       enum AVCodecID codec_id, int pid_num)
{
    int i;
    AVCodec *codec;
    AVFormatContext *oc = mux->av_fmt_ctxt;
    int        fifo_size;

    printf("add_stream %x %d\n", codec_id, pid_num);
    /* find the encoder */
    codec = avcodec_find_encoder(codec_id);
    if (!(codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }

    mux_st->st = avformat_new_stream(mux->av_fmt_ctxt, NULL);
    if (!mux_st->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    mux_st->pid_num = pid_num;
    mux_st->st->id = pid_num;
    mux_st->last_pts = 0;

    switch ((codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        fifo_size = audio_fifo_size;
        break;
    case AVMEDIA_TYPE_VIDEO:
        fifo_size = video_fifo_size;
        break;
    }
    mux_st->inp_fifo = create_fifo(fifo_size);

    mux_st->st->time_base = (AVRational){ 1, 90000 };
}

/**
 * @brief This function will be called from avio_write function
 *        in aviobuf.c
 * 
 * @param opaque 
 * @param buf 
 * @param buf_size 
 * @return int 
 */
static int write_packet_fn(void *opaque, uint8_t *buf, int buf_size)
{
    int bytes_written = 0;
    ts_muxer *mux = (ts_muxer*)opaque;
    
    printf("^^^^^^^^^^^^^^^^^^^^^^^^write_packet_fn enter %d %d\n", buf_size, write_avail_fifo(mux->out_fifo));
    /* Ensure that buf_size worth data is written out or wait
       till it can be written out */
    while(1)
    {
        if(write_avail_fifo(mux->out_fifo) > buf_size)
        {
            bytes_written= write_fifo(mux->out_fifo, buf, buf_size);
            break;
        }
        else
        {
            /* Wait for some time before write can be done */
            struct timeval  tv;

            tv.tv_sec = 0;
            tv.tv_usec = 30000; //30 milliseconds
            select(0, NULL, NULL, NULL, &tv);
        }
    }
    printf("^^^^^^^^^^^^^^^^^^^^^^^^write_packet_fn exit\n");

    return bytes_written;    
}

static int init_avio_context(AVIOContext **pb, int buffer_size, 
                            ts_muxer *mux)
{
    unsigned char *buf = av_malloc(buffer_size);
    *pb = avio_alloc_context(
                  buf,    buffer_size,
                  1,      mux,         NULL,
                  write_packet_fn,     NULL);
} 

static int init_next_av_packet(AVPacket *pkt, ts_muxer_fifo *fifo, int64_t max_pts)
{
    int ret_val = 0;

    es_frame_header   hdr = {0};
    if(peek_next_hdr(fifo, &hdr) == 0)
    {
        printf("################ %d %ld, %ld\n", read_avail_fifo(fifo), hdr.pts, max_pts);
        if (hdr.pts <= max_pts)
        {
            /* Next frame exists in fifo */
            unsigned char *data = av_malloc(hdr.size);
            printf("init_next_av_packet %d %d\n", hdr.size, fifo->buf->data_rd);
            read_fifo_with_hdr(fifo, data, hdr.size, &hdr);
            pkt->pts = hdr.pts;
            av_packet_from_data(pkt, data, hdr.size);
            
            printf("Pts = %ld %ld\n", pkt->pts/90, max_pts/90);
        } 
        else
        {
            ret_val = -1;
        }
         
    }
    else
    {
        ret_val = -1;
    }
    return ret_val;
}

int write_video_frame(ts_muxer *mux, unsigned char *buf, 
                      int size, int64_t pts)
{
    es_frame_header  hdr;

    hdr.size = size;
    hdr.pts  = pts;
    write_fifo_with_hdr(mux->st_video.inp_fifo, buf, size, &hdr);
    return 0;
}

int  write_audio_frame(ts_muxer *mux, unsigned char *buf, int size,
                       int64_t pts, int aud_index)
{
    es_frame_header  hdr;

    hdr.size = size;
    hdr.pts  = pts;
    write_fifo_with_hdr(mux->st_audio[aud_index].inp_fifo, 
                        buf, size, &hdr);
    return 0;
}

int read_muxed_data(ts_muxer *mux, unsigned char *buf, int size)
{
    int ret_val;
    ret_val = read_fifo(mux->out_fifo, buf, size);
    return ret_val;
}

void* ts_muxer_entry(void *arg)
{
    ts_muxer *mux = (ts_muxer *)arg;
    es_frame_header    hdr;
    int        aud_index;
    AVPacket   pkt = {0};
    int        ret_val;
    int64_t    last_vid_pts;
    

    while(1) {
        printf("Before video init_next_av_packet\n");
        ret_val = init_next_av_packet(&pkt, mux->st_video.inp_fifo, MAX_INT64);
        printf("After video init_next_av_packet\n");
        if(ret_val == 0)
        {
            printf("=========New video packet read\n");
            last_vid_pts = pkt.pts;
            write_frame(&mux->st_video, mux->av_fmt_ctxt, &mux->time_base,
                    mux->st_video.st, &pkt);
        }
        else
        {
            printf("No video packet to read\n");
            usleep(40000);
            continue;
        }
        
        for(aud_index = 0; aud_index < mux->num_aud_streams; 
                                   aud_index++)
        {
            while(1)
            {
                printf("Before audio init_next_av_packet\n");
                ret_val = init_next_av_packet(&pkt, 
                            mux->st_audio[aud_index].inp_fifo, last_vid_pts);
                printf("After audio init_next_av_packet %d\n", ret_val);
                if(ret_val == 0)
                {
                    printf("=========New audio packet read %d\n", pkt.size);
                    write_frame(&mux->st_audio[aud_index], mux->av_fmt_ctxt, &mux->time_base,
                            mux->st_audio[aud_index].st, &pkt);
                    printf("*******************************\n");
                }
                else
                {
                    printf("+++++++ %ld\n", last_vid_pts);
                    break;
                }
                
            }
            
        }        
    }
    print("Exiting Thread");
    return arg;
}

static int get_av_codec_id(AvCodecEnum codec_enum)
{
    int  codec_id;
    switch(codec_enum) {
    case AV_CODEC_VID_H264:
        codec_id = AV_CODEC_ID_H264;
        break;
    case AV_CODEC_VID_MP2:
        codec_id = AV_CODEC_ID_MPEG2VIDEO;
        break;
    case AV_CODEC_AUD_AAC:
        codec_id = AV_CODEC_ID_AAC;
        break;
    case AV_CODEC_AUD_MP2:
        codec_id = AV_CODEC_ID_MP2;
        break;
    default:
        codec_id = AV_CODEC_ID_H264;
        break;
    }
    return codec_id;

}

ts_muxer* create_ts_muxer(ts_muxer_params_t *params)
{
    ts_muxer *mux = malloc(sizeof(ts_muxer));
    int i;
    int vid_codec_id;
    int aud_codec_id;
    int ret;

    /* allocate the output media context */
    avformat_alloc_output_context2(&mux->av_fmt_ctxt, 
                                   NULL, "mpegts", 
                                   NULL);

    vid_codec_id = get_av_codec_id(params->vid_codec);
    add_stream(mux, &mux->st_video, vid_codec_id, params->vid_pid);

    for(i = 0; i < params->num_aud_tracks; i++)
    {
        aud_codec_id = get_av_codec_id(params->aud_codec[i]);
        add_stream(mux, &mux->st_audio[i], aud_codec_id, params->aud_pid[i]);
    }

    init_avio_context(&mux->av_fmt_ctxt->pb, out_ts_mux_size, 
                            mux);

    ret = avformat_write_header(mux->av_fmt_ctxt, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return NULL;
    }

    mux->num_aud_streams = params->num_aud_tracks;
    mux->time_base = (AVRational){ 1, 90000 };

    mux->out_fifo = create_fifo(out_ts_mux_size);

    pthread_create(&mux->thread, NULL, ts_muxer_entry, (void*)mux);
    
    return mux;
}

int get_video_write_avail_size(ts_muxer *mux)
{
    int  avail_size = 0;
    avail_size = write_avail_fifo(mux->st_video.inp_fifo);
    return avail_size;
}

int get_audio_write_avail_size(ts_muxer *mux, int aud_index)
{
    int  avail_size = 0;
    avail_size = write_avail_fifo(mux->st_audio[aud_index].inp_fifo);
    return avail_size;
}


#if 0
/**************************************************************/
/* media file output */

int main(int argc, char **argv)
{
    OutputStream video_st = { 0 }, audio_st = { 0 };
    const char *filename;
    AVOutputFormat *fmt;
    AVFormatContext *oc;
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;

    if (argc < 2) {
        printf("usage: %s output_file\n"
               "API example program to output a media file with libavformat.\n"
               "This program generates a synthetic audio and video stream, encodes and\n"
               "muxes them into a file named output_file.\n"
               "The output format is automatically guessed according to the file extension.\n"
               "Raw images can also be output by using '%%d' in the filename.\n"
               "\n", argv[0]);
        return 1;
    }

    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, "mpegts", NULL);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&video_st, oc, &video_codec, AV_CODEC_ID_H264);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_MP2);
        have_audio = 1;
        encode_audio = 1;
    }


    av_dump_format(oc, 0, filename, 1);

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }

    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                                            audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &video_st);
        } else {
            encode_audio = !write_audio_frame(oc, &audio_st);
        }
    }

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);

    /* Close each codec. */
    if (have_video)
        close_stream(oc, &video_st);
    if (have_audio)
        close_stream(oc, &audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}
#endif
