#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include "ts_muxer.h"

#define USAGE "./ts_muxer_app out_file.ts h264 stream_vid.es mp2 stream_aud.es aac stream_aud_1.es\n"

#define MAX_VIDEO_FRAME_SIZE  1000000 //1MB
#define MAX_AUDIO_FRAME_SIZE  64000 //1MB
#define MAX_SCTE_FRAME_SIZE  1024 //1MB


typedef struct frame_header {
    int         size;
    int64_t     pts;
    int64_t     dts;
    int64_t     utc;
    int         flags;
}frame_header_t;

static const int  frame_header_size_packed = 32;

int main(int argc, char **argv)
{
    char  *vid_codec_type;
    char  *vid_es_file;
    char  *aud_codec_types[MAX_AUDIO_STREAMS];
    char  *aud_es_file[MAX_AUDIO_STREAMS];
    char  *scte_es_file[MAX_SCTE_STREAMS];
    char  *out_ts_file;
    int i, aud_codec_index, scte_codec_index;
    FILE *video_es_fd;
    FILE *audio_es_fd[MAX_AUDIO_STREAMS];
    FILE *scte_es_fd[MAX_SCTE_STREAMS];
    FILE *mux_out_fd;
    ts_muxer_params_t   params;
    ts_muxer*  mux;
    int vid_codec_pid;
    int aud_codec_pid_start;
    int scte_codec_pid_start;

    if(argc < 3)
    {
        printf(USAGE);
        exit(1);
    }

    out_ts_file = argv[1];
    vid_codec_type = argv[2];
    vid_es_file = argv[3];

    aud_codec_index = 0;
    scte_codec_index = 0;
    for(i = 4; i < argc; i+=2)
    {
        if(strcmp(argv[i], "mp2") == 0 ||
           strcmp(argv[i], "aac") == 0 ||
           strcmp(argv[i], "ac3") == 0)
        {
            aud_codec_types[aud_codec_index] = argv[i];
            if ((i+1) < argc)
            {
                aud_es_file[aud_codec_index] = argv[i+1];
            }
            else
            {
                printf("Audio Es name not present for audio %d\n", i);
                exit(1);
            }
            aud_codec_index += 1;
        }
        else if(strcmp(argv[i], "scte") == 0)
        {
            if ((i+1) < argc)
            {
                scte_es_file[scte_codec_index] = argv[i+1];
            }
            else
            {
                printf("Scte Es name not present for scte %d\n", i);
                exit(1);
            }
            scte_codec_index += 1;
        }
        
    }

    params.num_aud_tracks = aud_codec_index;
    params.num_scte_tracks = scte_codec_index;
    vid_codec_pid = 2064;
    aud_codec_pid_start = 2068;
    scte_codec_pid_start = 500;
    params.frame_rate = 29.97;

    for(i = 0; i < params.num_aud_tracks; i++)
    {
        params.aud_pid[i] = aud_codec_pid_start + i;
        if(strcmp(aud_codec_types[i], "mp2") == 0)
        {
            params.aud_codec[i] = AV_CODEC_AUD_MP2;
        }
        else if (strcmp(aud_codec_types[i], "aac") == 0)
        {
            params.aud_codec[i] = AV_CODEC_AUD_AAC;
        }
        else
        {
            printf("Unsupported audio codec type %s\n",                      aud_codec_types[i]);
            exit(1);
        }
    }

    for(i = 0; i < params.num_scte_tracks; i++)
    {
        params.scte_pid[i] = scte_codec_pid_start + i;
    }

    if(strcmp(vid_codec_type, "h264") == 0)
    {
        params.vid_codec = AV_CODEC_VID_H264;
    } 
    else
    {
        printf("Unsupported video codec type %s\n",                          vid_codec_type);
        exit(1);
    }

    params.vid_pid = vid_codec_pid;
    params.mux_rate = 30000000;

    mux = create_ts_muxer(&params);

    video_es_fd = fopen(vid_es_file, "rb");
    for(i = 0; i < params.num_aud_tracks; i++)
    {
        audio_es_fd[i] = fopen(aud_es_file[i], "rb");
    }
    for(i = 0; i < params.num_scte_tracks; i++)
    {
        scte_es_fd[i] = fopen(scte_es_file[i], "rb");
    }
    mux_out_fd = fopen(out_ts_file, "wb");

    muxer_process(mux, video_es_fd, params.num_aud_tracks, audio_es_fd,
                  params.num_scte_tracks,  scte_es_fd,
                  mux_out_fd);
    

}


static int read_frame_hdr(FILE *es_fd, frame_header_t *hdr)
{
    int ret = 0;
    int num_bytes_read = 0;

    ret = fread(&hdr->size, sizeof(int), 1,  es_fd);
    num_bytes_read += ret * sizeof(int);
    ret = fread(&hdr->pts, sizeof(int64_t), 1, es_fd);
    num_bytes_read += ret * sizeof(int64_t);
    ret = fread(&hdr->dts, sizeof(int64_t), 1, es_fd);
    num_bytes_read += ret * sizeof(int64_t);
    ret = fread(&hdr->utc, sizeof(int64_t), 1, es_fd);
    num_bytes_read += ret * sizeof(int64_t);
    ret = fread(&hdr->flags, sizeof(int), 1,   es_fd);
    num_bytes_read += ret * sizeof(int);
    
    if (num_bytes_read == (2*sizeof(int) + 3*sizeof(int64_t)))
    {
        num_bytes_read = sizeof(frame_header_t);
    }
    printf("read_frame_hdr %d\n", num_bytes_read);
    
    return num_bytes_read;
}


int muxer_process(ts_muxer *mux, FILE *video_es_fd, int num_aud, 
                  FILE *audio_es_fd[], 
                  int num_scte,
                  FILE *scte_es_fd[], FILE *mux_out_fd)
{
    frame_header_t   video_frame_hdr;
    frame_header_t   audio_frame_hdr;
    frame_header_t   scte_frame_hdr;
    unsigned char  *video_buffer;
    unsigned char  *audio_buffer;
    unsigned char  *scte_buffer;
    int ret, i;
    unsigned char mux_out_buf[188*7];

    video_buffer = (unsigned char*)malloc(MAX_VIDEO_FRAME_SIZE);
    audio_buffer = (unsigned char*)malloc(MAX_AUDIO_FRAME_SIZE);
    scte_buffer = (unsigned char*)malloc(MAX_SCTE_FRAME_SIZE);

    while(1)
    {

        while(1)
        {
            ret = read_muxed_data(mux, mux_out_buf, 188*7);
            if(ret ==0)
            {
                fwrite(mux_out_buf, 1, 188*7, mux_out_fd);
            }
            else
            {
                break;
            }
        }

        ret = read_frame_hdr(video_es_fd, &video_frame_hdr);
        printf("VIDE_HDR: read_size = %d %lu %d  %ld  %ld  %ld   %d\n", ret, sizeof(frame_header_t), 
                video_frame_hdr.size, 
                video_frame_hdr.pts, video_frame_hdr.dts, video_frame_hdr.utc, video_frame_hdr.flags); 
        if(ret != sizeof(frame_header_t))
        {
            printf("Exiting Main app loop\n");
            break;
        }
        else
        {
            if(get_video_write_avail_size(mux) > video_frame_hdr.size)
            {
                fread(video_buffer, 1, video_frame_hdr.size, 
                        video_es_fd);
                write_video_frame(mux, video_buffer, 
                        video_frame_hdr.size, video_frame_hdr.pts, 
                        video_frame_hdr.dts);
            }
            else
            {
                fseek(video_es_fd, 
                            -frame_header_size_packed,  SEEK_CUR);
                printf("##############Avail size = %d\n", get_video_write_avail_size(mux));
                usleep(40000); //40ms
                continue;
            }
            
        }
        for(i = 0; i < num_aud; i++)
        {
            while (1)
            {
                ret = read_frame_hdr(audio_es_fd[i], &audio_frame_hdr);
                printf("AUDIO_HDR: read_size = %d %lu %d  %ld  %ld  %ld   %d\n", ret, sizeof(frame_header_t), 
                audio_frame_hdr.size, 
                audio_frame_hdr.pts, audio_frame_hdr.dts, audio_frame_hdr.utc, audio_frame_hdr.flags);
                if(ret != sizeof(frame_header_t))
                {
                    break;
                }
                else
                {
                    if((get_audio_write_avail_size(mux, i) > audio_frame_hdr.size) &&
                       audio_frame_hdr.pts <= video_frame_hdr.pts)
                    {
                        static int64_t   __aud_frame_pts = 0;
                        fread(audio_buffer, 1, audio_frame_hdr.size, 
                            audio_es_fd[i]);
                        printf("Ts_muxer_app audio %f\n", (audio_frame_hdr.pts - __aud_frame_pts)/90.);
                        __aud_frame_pts = audio_frame_hdr.pts;
                        write_audio_frame(mux, audio_buffer, 
                            audio_frame_hdr.size, audio_frame_hdr.pts,
                            audio_frame_hdr.dts, i);
                    }
                    else
                    {
                        fseek(audio_es_fd[i], 
                            -frame_header_size_packed,  SEEK_CUR);
                        break;
                    }
                    
                }
            }            
        }

        for(i = 0; i < num_scte; i++)
        {
            while (1)
            {
                ret = read_frame_hdr(scte_es_fd[i], &scte_frame_hdr);
                /*printf("SCTE_HDR: read_size = %d %ld %ld %ld\n", scte_frame_hdr.size,
                            scte_frame_hdr.pts, scte_frame_hdr.dts, 
                            video_frame_hdr.pts);*/
                if(ret != sizeof(frame_header_t))
                {
                    break;
                }
                else
                {
                    if((get_scte_write_avail_size(mux, i) > scte_frame_hdr.size) &&
                       scte_frame_hdr.pts <= video_frame_hdr.pts)
                    {
                        fread(scte_buffer, 1, scte_frame_hdr.size, 
                            scte_es_fd[i]);
                        write_scte35_frame(mux, scte_buffer, 
                            scte_frame_hdr.size, scte_frame_hdr.pts,
                            scte_frame_hdr.dts, i);
                        printf("Writing scte %ld %ld\n", scte_frame_hdr.pts, scte_frame_hdr.dts);
                    }
                    else
                    {
                        fseek(scte_es_fd[i], 
                            -frame_header_size_packed,  SEEK_CUR);
                        break;
                    }
                    
                }
            }            
        }


    }

    for(i = 0; i < 12; i++)
    {
        while(1)
        {
            ret = read_muxed_data(mux, mux_out_buf, 188*7);
            if(ret ==0)
            {
                fwrite(mux_out_buf, 1, 188*7, mux_out_fd);
            }
            else
            {
                usleep(500000);
                break;
            }
        }
    }
    

    //usleep(5000000);

    return 0;
}