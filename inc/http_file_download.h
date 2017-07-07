#ifndef _HTTP_FILE_DOWNLOAD_H_
#define _HTTP_FILE_DOWNLOAD_H_

#include "http_file_download_thread.h"

#define HTTP_FILE_DOWNLOAD_DEBUG_ENABLE     (1)

#define HTTP_THREAD_STACK_SIZE_SSL          (0x3000)
#define HTTP_THREAD_STACK_SIZE_NOSSL        (0x1000)

#define HTTP_DOWNLOAD_FILE_MAX_THREAD_NUM   (2)

typedef FILE_DOWNLOAD_CONTEXT_S*            FILE_DOWNLOAD_CONTEXT;

//user api
OSStatus http_file_download_start( FILE_DOWNLOAD_CONTEXT *file_download_context_user, const char *file_url, FILE_DOWNLOAD_STATE_CB download_state_cb, FILE_DOWNLOAD_DATA_CB download_data_cb, uint32_t user_args);
HTTP_FILE_DOWNLOAD_CONTROL_STATE_E http_file_download_get_state( FILE_DOWNLOAD_CONTEXT *file_download_context );
OSStatus http_file_download_pause( FILE_DOWNLOAD_CONTEXT *file_download_context );
OSStatus http_file_download_continue( FILE_DOWNLOAD_CONTEXT *file_download_context );
OSStatus http_file_download_stop( FILE_DOWNLOAD_CONTEXT *file_download_context, bool is_block);

#endif
