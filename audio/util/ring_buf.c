#include "ring_buf.h"

#define ring_buf_log(M, ...) custom_log("RING_BUF", M, ##__VA_ARGS__)
//判断x是否是2的次方
#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))
//取a和b中最小值
#define min(a, b) (((a) < (b)) ? (a) : (b))

//初始化缓冲区
struct ring_buffer* ring_buf_init(void *buffer, uint32_t size, mico_mutex_t lock)
{
  struct ring_buffer *ring_buf = NULL;
//  if (!is_power_of_2(size))
//  {
//    ring_buf_log("size must be power of 2");
//    return ring_buf;
//  }
  buffer = malloc(size);
  if( buffer == NULL ){
    ring_buf_log("Failed to malloc memory");
    return ring_buf;
  }
  
  ring_buf = (struct ring_buffer *)malloc(sizeof(struct ring_buffer));
  if (!ring_buf){
    ring_buf_log("Failed to malloc memory");
    return ring_buf;
  }
  memset(ring_buf, 0, sizeof(struct ring_buffer));
  ring_buf->buffer = buffer;
  ring_buf->size = size;
  ring_buf->in = 0;
  ring_buf->out = 0;
  ring_buf->lock = lock;
  mico_rtos_init_mutex(&ring_buf->lock);
  return ring_buf;
}

//释放缓冲区
void ring_buf_free(struct ring_buffer *ring_buf)
{
  if (ring_buf)
  {
    if (ring_buf->buffer)
    {
      free(ring_buf->buffer);
      ring_buf->buffer = NULL;
    }
    free(ring_buf);
    ring_buf = NULL;
  }
}

//缓冲区的长度
static uint32_t __ring_buf_len(const struct ring_buffer *ring_buf)
{
  return (ring_buf->in - ring_buf->out);
}

static void __ring_buf_clean(struct ring_buffer *ring_buf)
{
  ring_buf->in = 0;
  ring_buf->out = 0;
}

//从缓冲区中取数据
static uint32_t __ring_buf_get(struct ring_buffer *ring_buf, void * buffer, uint32_t size)
{
  uint32_t len = 0;
    
  size  = min(size, ring_buf->in - ring_buf->out);        
  /* first get the data from fifo->out until the end of the buffer */
  len = min(size, ring_buf->size - (ring_buf->out & (ring_buf->size - 1)));
  memcpy(buffer,  (uint8_t *)ring_buf->buffer + (ring_buf->out & (ring_buf->size - 1)), len);
  /* then get the rest (if any) from the beginning of the buffer */
  memcpy((uint8_t *)buffer + len, ring_buf->buffer, size - len);
  ring_buf->out += size;
  return size;
}

//向缓冲区中存放数据
static uint32_t __ring_buf_put(struct ring_buffer *ring_buf, void *buffer, uint32_t size)
{
  uint32_t len = 0;
  size = min(size, ring_buf->size - ring_buf->in + ring_buf->out);
  /* first put the data starting from fifo->in to buffer end */
  len  = min(size, ring_buf->size - (ring_buf->in & (ring_buf->size - 1)));
  memcpy((uint8_t *)ring_buf->buffer + (ring_buf->in & (ring_buf->size - 1)), buffer, len);
  /* then put the rest (if any) at the beginning of the buffer */
  memcpy(ring_buf->buffer, (uint8_t *)buffer + len, size - len);
  ring_buf->in += size;
  return size;
}

void ring_buf_clean(struct ring_buffer *ring_buf)
{
  mico_rtos_lock_mutex(&ring_buf->lock);
  __ring_buf_clean(ring_buf);
  mico_rtos_unlock_mutex(&ring_buf->lock);
}

uint32_t ring_buf_len(struct ring_buffer *ring_buf)
{
  uint32_t len = 0;
  mico_rtos_lock_mutex(&ring_buf->lock);
  len = __ring_buf_len(ring_buf);
  mico_rtos_unlock_mutex(&ring_buf->lock);
  return len;
}

uint32_t ring_buf_get(struct ring_buffer *ring_buf, void *buffer, uint32_t size)
{
  uint32_t ret;
  mico_rtos_lock_mutex(&ring_buf->lock);
  ret = __ring_buf_get(ring_buf, buffer, size);
  //buffer中没有数据
  if (ring_buf->in == ring_buf->out)
    ring_buf->in = ring_buf->out = 0;
  mico_rtos_unlock_mutex(&ring_buf->lock);
  return ret;
}

uint32_t ring_buf_put(struct ring_buffer *ring_buf, void *buffer, uint32_t size)
{
  uint32_t ret;
  mico_rtos_lock_mutex(&ring_buf->lock);
  ret = __ring_buf_put(ring_buf, buffer, size);
  mico_rtos_unlock_mutex(&ring_buf->lock);
  return ret;
}