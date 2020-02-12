#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>


#define MAX_STREAMS 16
#define AV_FIFO_SINK

static bool av_fifo_sink = true;

static AVFormatContext *fmt_ctx = NULL;
static AVPacket pkt;
static int exit_flag = 0;

void signal_handler(int sig);
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
    sprintf(full_path, "%s/stream_%s_%02d.es", base_path, av_get_media_type_string(t), i);
    if(!av_fifo_sink) {
        av_streams[i].fd = open(full_path, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU | S_IRWXG);
    }
    else
    {
        if(mkfifo(full_path, S_IRWXU | S_IRWXG ) == -1 && errno != EEXIST)
        {
            errExit("ERROR: mkfifo %s", full_path);
        }
        av_streams[i].fd = open(full_path, O_WRONLY);
    }
    
    if(av_streams[i].fd == -1)
    {
        errExit("Could not open %s", full_path);
    }

    return 0;
}

void close_fds (void)
{
    int i;
    for (i = 0; i < MAX_STREAMS; i++) {
       if (av_streams[i].fd != 0) {
           printf("closing fd %d for stream index %d\n", av_streams[i].fd, i);
	   close(av_streams[i].fd);
	   av_streams[i].fd = 0;
       }
    }
}

void signal_handler (int sig)
{
    exit_flag = 1;
}

int64_t GetTimeNowNs()
{
    uint64_t now_ns = 0;
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    return (now_ns = ((now.tv_sec * 1000000000) + now.tv_nsec));
}


int main(int argc, char **argv)
{
    int ret = 0;
    char *src_filename;
    int i;
    int stream_index;
    int size;
    char buf[64];
    int64_t  dts_value;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if(argc < 2)
    {
        fprintf(stderr, "Usage: %s <inp_av> <out_path> [out_file]\n",
                     argv[0]);
        fprintf(stderr, "Optional out_file to be used to dump output into files");
        exit(1);
    }
    src_filename = argv[1];
    char *base_out_path = argv[2];

    if(strcmp("out_file", argv[3]) == 0)
    {
        av_fifo_sink = false;
    }
    else
    {
        fprintf(stderr, "Unknown option %s", argv[3]);
        exit(1);
    }
    
    memset(&av_streams, 0, MAX_STREAMS * sizeof(av_streams));

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

    while(!exit_flag && av_read_frame(fmt_ctx, &pkt) >= 0) {
        stream_index = pkt.stream_index;
        printf("++++++Strm_index=%d", stream_index);
        /*if(stream_index == 0)
        {
            static int c = 0;
            printf("Vid frame %d, %d, %ld\n", c++, pkt.size, pkt.pts);
        }*/

        pkt.pts &= 0x1ffffffff;
        pkt.dts &= 0x1ffffffff;
        /*Handle Invalid PTS */
        /*if ((pkt.pts < 0) || (pkt.pts > pow(2,33))) {
            av_packet_unref(&pkt);
            continue;
        }*/

        printf("av_to_es: size=%d, pts=%ld, dts=%ld, index=%d, flags=%d\n", pkt.size, pkt.pts, pkt.dts, pkt.stream_index, pkt.flags);
        size = pkt.size + sizeof(pkt.pts) + sizeof(pkt.flags);
        write(av_streams[stream_index].fd, &pkt.size, sizeof(pkt.size));
        write(av_streams[stream_index].fd, &pkt.pts, sizeof(pkt.pts));
        if(pkt.dts == AV_NOPTS_VALUE)
        {
            dts_value = -1;
        }
        else
        {
            dts_value = pkt.pts;
        }
        
        write(av_streams[stream_index].fd, &dts_value, sizeof(dts_value));
        int64_t  time_now = GetTimeNowNs();
        write(av_streams[stream_index].fd, &time_now, sizeof(int64_t));
        int flags = pkt.flags & AV_PKT_FLAG_KEY;
        write(av_streams[stream_index].fd, &flags, sizeof(pkt.flags));
        write(av_streams[stream_index].fd, pkt.data, pkt.size);
        av_packet_unref(&pkt);
    }
    printf("While loop exited, cleaning up\n");
    close_fds();
    avformat_close_input(&fmt_ctx);
}
