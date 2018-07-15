#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>


#define MAX_STREAMS 16
//#define AV_FIFO_SINK

static AVFormatContext *fmt_ctx = NULL;
static AVPacket pkt;

static struct {
    enum AVMediaType   type;
    int           fd;
} av_streams[MAX_STREAMS];

static void
errExit(const char *format, ...)
{
    char buf[1024];
    va_list argList;
    va_start(argList, format);
    vsprintf(buf, format, argList);
    printf("%s, Error:%s\n", buf, strerror(errno));
    va_end(argList);

    exit(1);
}


int init_av_stream(char *base_path, int i, enum AVMediaType t)
{
    char   full_path[128];
    av_streams[i].type = t;
    sprintf(full_path, "%s_%d_%02d.es", base_path, t, i);
#ifndef AV_FIFO_SINK
    av_streams[i].fd = open(full_path, O_CREAT|O_WRONLY|O_TRUNC| S_IWUSR | S_IWGRP);
#else
    if(mkfifo(full_path, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST)
    {
        errExit("ERROR: mkfifo %s", full_path);
    }
    av_streams[i].fd = open(full_path, O_WRONLY);
#endif //AV_FIFO_SINK
    if(av_streams[i].fd == -1)
    {
        errExit("Could not open %s", full_path);
    }

    return 0;
}

int main(int argc, char **argv)
{
    int ret = 0;
    char *src_filename;
    int i;
    int stream_index;

    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <inp_av> <out_path>\n", argv[0]);
        exit(1);
    }
    src_filename = argv[1];
    char *base_out_path = argv[2];

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    for(i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if(fmt_ctx->streams[i]->codec != NULL)
        {
            printf("type:%d, %s\n", fmt_ctx->streams[i]->codec->codec_type,
                   av_get_media_type_string(fmt_ctx->streams[i]->codec->codec_type));
            init_av_stream(base_out_path, i, fmt_ctx->streams[i]->codec->codec_type);
        }
        else
        {
            printf("Codec not initialized\n");
        }
    }

    while(av_read_frame(fmt_ctx, &pkt) >= 0) {
        stream_index = pkt.stream_index;
        if(stream_index == 0)
        {
            static int c = 0;
            printf("Vid frame %d, %d, %ld\n", c++, pkt.size, pkt.pts);
        }
        write(av_streams[stream_index].fd, pkt.data, pkt.size);
    }

}
