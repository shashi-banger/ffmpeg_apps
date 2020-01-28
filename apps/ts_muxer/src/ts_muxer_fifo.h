#ifndef __TS_MUXER_FIFO_H__
#define __TS_MUXER_FIFO_H__

#include <stdint.h>
#include "circular_buf.h"

typedef struct es_frame_header
{
    int size;
    int64_t pts;
    int64_t dts;
} es_frame_header;

typedef struct ts_muxer_fifo
{
    /* data */
    circular_buf_t *buf;
    //ts_muxer_codec_type codec_type;
    pthread_mutex_t  lock;

} ts_muxer_fifo;

ts_muxer_fifo *create_fifo(int size);
int write_fifo_with_hdr(ts_muxer_fifo *fifo,
                            unsigned char *buf,
                            int   size,
                            es_frame_header  *hdr);
int write_fifo(ts_muxer_fifo *fifo, unsigned char *buf, 
                   int size);
int write_avail_fifo(ts_muxer_fifo *fifo);
int read_fifo_with_hdr(ts_muxer_fifo *fifo,
                  unsigned char *buf, 
                  int size, es_frame_header *frm_hdr);
int read_fifo(ts_muxer_fifo *fifo,
                  unsigned char *buf, 
                  int size);
int read_avail_fifo(ts_muxer_fifo *fifo);
int peek_next_hdr(ts_muxer_fifo *fifo , es_frame_header *hdr);


#endif //__TS_MUXER_FIFO_H__