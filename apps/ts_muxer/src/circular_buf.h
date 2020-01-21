
#ifndef __CIRCULAR_BUF_H__
#define __CIRCULAR_BUF_H__

typedef struct circular_buf_t {
    unsigned char *data;
    int            data_max_size;  //Should be multiple of 4
    int            data_wr;
    int            data_rd;
}circular_buf_t;

circular_buf_t* cb_create(int size);
int cb_write(circular_buf_t *c_buf, unsigned char * data, int size);
int cb_read(circular_buf_t  *c_buf, unsigned char *buf, int size);
int cb_writable_size(circular_buf_t *c_buf);
int cb_peek(circular_buf_t *c_buf, unsigned char *data, int size);

#endif //__CIRCULAR_BUF_H__