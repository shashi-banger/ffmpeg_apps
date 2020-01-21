
#include <stdlib.h>
#include <pthread.h>
#include "ts_muxer_fifo.h"
//#include "ts_muxer_common.h"
#include "circular_buf.h"

static const int gsi_header_size = 12;

ts_muxer_fifo *create_fifo(int size)
{
    ts_muxer_fifo *fifo = (ts_muxer_fifo *)malloc(sizeof(ts_muxer_fifo));
    fifo->buf = cb_create(size);
    //fifo->codec_type = codec_type;
    pthread_mutex_init(&fifo->lock, NULL);
    return fifo;
}

static void lock_fifo(ts_muxer_fifo *fifo)
{
    pthread_mutex_lock(&fifo->lock);
}

static void unlock_fifo(ts_muxer_fifo *fifo)
{
    pthread_mutex_unlock(&fifo->lock);
}

static int read_next_header(ts_muxer_fifo* fifo, es_frame_header  *hdr)
{
    cb_read(fifo->buf, (unsigned char *)&hdr->size, sizeof(int));
    cb_read(fifo->buf, (unsigned char *)&hdr->pts, sizeof(int64_t));
    return 0;
}

static int write_next_header(ts_muxer_fifo* fifo, es_frame_header  *hdr)
{
    cb_write(fifo->buf, (unsigned char *)&hdr->size, sizeof(int));
    cb_write(fifo->buf, (unsigned char *)&hdr->pts, sizeof(int64_t));
    return 0;
}

static int peek_next_header(ts_muxer_fifo* fifo, es_frame_header  *hdr)
{
    int  ret_val = 0;
    unsigned char  buf[12];
    ret_val = cb_peek(fifo->buf, (unsigned char *)&buf, 12);
    if(ret_val == 0)
    {
        memcpy(&hdr->size, buf, sizeof(int));
        memcpy(&hdr->pts, (buf+sizeof(int)), sizeof(int64_t));

    }
    return ret_val;
}

int write_fifo_with_hdr(ts_muxer_fifo *fifo,
                            unsigned char *buf,
                            int   size,
                            es_frame_header  *hdr)
{
    int ret_val = 0;
    lock_fifo(fifo);
    if(cb_writable_size(fifo->buf) >= (size + gsi_header_size))
    {
        write_next_header(fifo, hdr);
        //cb_write(fifo->buf, (unsigned char*)hdr, sizeof(*hdr));
        cb_write(fifo->buf, buf, size);
    }
    else
    {
        ret_val = -1;
    }
    unlock_fifo(fifo);
    return ret_val;
}


int write_fifo(ts_muxer_fifo *fifo, unsigned char *buf, 
                   int size)
{
    int ret_val = 0;

    lock_fifo(fifo);
    if(cb_writable_size(fifo->buf) >= size)
    {
        cb_write(fifo->buf, buf, size);
    }
    else
    {
        ret_val = -1;
    }
    unlock_fifo(fifo);
    return ret_val;
    
}

int write_avail_fifo(ts_muxer_fifo *fifo)
{
    return cb_writable_size(fifo->buf);
}

int read_fifo_with_hdr(ts_muxer_fifo *fifo,
                  unsigned char *buf, 
                  int size, es_frame_header *frm_hdr)
{
    int ret_val = 0;
    lock_fifo(fifo);
    if(cb_readable_size(fifo->buf) >= (size + gsi_header_size))
    {
        read_next_header(fifo, frm_hdr);
        //cb_read(fifo->buf, (unsigned char*)frm_hdr, sizeof(es_frame_header));
        cb_read(fifo->buf, buf, size);
    }
    else
    {
        ret_val = -1;
    }
    unlock_fifo(fifo);
    return ret_val;

}

int read_fifo(ts_muxer_fifo *fifo,
                  unsigned char *buf, 
                  int size)
{
    int ret_val = 0;

    lock_fifo(fifo);
    if(cb_readable_size(fifo->buf) >= size)
    {
        cb_read(fifo->buf, buf, size);
    }
    else
    {
        ret_val = -1;
    }
    unlock_fifo(fifo);
    return ret_val;

}

int peek_next_hdr(ts_muxer_fifo *fifo , es_frame_header *hdr)
{
    int ret_val;
    lock_fifo(fifo);
    ret_val = peek_next_header(fifo, hdr);
    //ret_val = cb_peek(fifo->buf, (unsigned char*)hdr, sizeof(es_frame_header));
    unlock_fifo(fifo);
    return ret_val;
}

int read_avail_fifo(ts_muxer_fifo *fifo)
{
    return cb_readable_size(fifo->buf);

}
