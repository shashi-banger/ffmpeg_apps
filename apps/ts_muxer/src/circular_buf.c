#include <stdlib.h>
#include <string.h>
#include "circular_buf.h"

circular_buf_t* cb_create(int size)
{
    circular_buf_t  *c_buf = (circular_buf_t*)malloc(sizeof(circular_buf_t));

    c_buf->data = (unsigned char *)malloc(size);
    c_buf->data_max_size = size;
    c_buf->data_rd = c_buf->data_wr = 0;

    return (circular_buf_t *)c_buf;
}

int cb_writable_size(circular_buf_t *c_buf)
{    
    int  wr_size;

    if(c_buf->data_rd <= c_buf->data_wr)
    {
        wr_size = c_buf->data_max_size - c_buf->data_wr + 
                             c_buf->data_rd;
    }
    else
    {
        wr_size = c_buf->data_rd - c_buf->data_wr;
    }

    return wr_size;
}

int cb_write(circular_buf_t *c_buf, unsigned char *data, int size)
{
    int wr_avail = 0;

    wr_avail = cb_writable_size(c_buf);
    if(size > wr_avail)
    {
        return -1;
    }

    if(size > (c_buf->data_max_size - c_buf->data_wr))
    {
        int  size_wr = (c_buf->data_max_size - c_buf->data_wr);
        memcpy(c_buf->data + c_buf->data_wr, data, 
               size_wr);
        
        data = data + size_wr;
        memcpy(c_buf->data, data, (size - size_wr));
        c_buf->data_wr = (size - size_wr);
    }
    else
    {
        memcpy(c_buf->data + c_buf->data_wr, data, size);
        c_buf->data_wr += size;
        if(c_buf->data_wr == c_buf->data_max_size) {
            c_buf->data_wr = 0;
        }
    }
    return 0;
}

int cb_readable_size(circular_buf_t  *c_buf)
{
    int  rd_size;

    if(c_buf->data_rd > c_buf->data_wr)
    {
        rd_size = c_buf->data_max_size - c_buf->data_rd + 
                             c_buf->data_wr;
    }
    else
    {
        rd_size = c_buf->data_wr - c_buf->data_rd;
    }
    return rd_size;
}

int cb_peek(circular_buf_t *c_buf, unsigned char *data, int size)
{
    int rd_size;
    int rd_avail;

    rd_avail = cb_readable_size(c_buf);
    if(size > rd_avail)
    {
        return -1;
    }

    if(size > (c_buf->data_max_size - c_buf->data_rd))
    {
        rd_size = c_buf->data_max_size - c_buf->data_rd;
        memcpy(data, c_buf->data + c_buf->data_rd, rd_size);
        data = data + rd_size;
        memcpy(data, c_buf->data, (size - rd_size));
    }
    else
    {
        memcpy(data, c_buf->data + c_buf->data_rd, size);
    }
    return 0;
}

int cb_read(circular_buf_t  *c_buf, unsigned char *data, int size)
{
    int rd_size;
    int rd_avail;

    rd_avail = cb_readable_size(c_buf);
    if(size > rd_avail)
    {
        return -1;
    }

    if(size > (c_buf->data_max_size - c_buf->data_rd))
    {
        rd_size = c_buf->data_max_size - c_buf->data_rd;
        memcpy(data, c_buf->data + c_buf->data_rd, rd_size);
        data = data + rd_size;
        memcpy(data, c_buf->data, (size - rd_size));
        c_buf->data_rd = (size - rd_size);
    }
    else
    {
        memcpy(data, c_buf->data + c_buf->data_rd, size);
        c_buf->data_rd += size;
        if(c_buf->data_rd == c_buf->data_max_size) {
            c_buf->data_rd = 0;
        }
    }
    
    return 0;

}
