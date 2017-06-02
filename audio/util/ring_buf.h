#ifndef __KFIFO_H
#define __KFIFO_H

#include "mico.h"

struct ring_buffer
{
  void         *buffer;
  uint32_t     size;
  uint32_t     in;
  uint32_t     out;
  mico_mutex_t lock;
};

struct ring_buffer* ring_buf_init(void *buffer, uint32_t size, mico_mutex_t lock);
void ring_buf_free(struct ring_buffer *ring_buf);
void ring_buf_clean(struct ring_buffer *ring_buf);
uint32_t ring_buf_len(struct ring_buffer *ring_buf);
uint32_t ring_buf_get(struct ring_buffer *ring_buf, void *buffer, uint32_t size);
uint32_t ring_buf_put(struct ring_buffer *ring_buf, void *buffer, uint32_t size);

#endif
