#ifndef _HTTP_FILE_DOWNLOAD_THREAD_H_
#define _HTTP_FILE_DOWNLOAD_THREAD_H_

#include "HTTPUtils.h"
#include "mico.h"

#define HTTP_FILE_DOWNLOAD_THREAD_DEBUG_ENABLE      (1)

#define HTTP_HOST_BUFF_SIZE                         (64)
#define HTTP_IP_BUFF_SIZE                           (32)

#define HTTP_PORT_DEFAULT_NOSSL                     (80)
#define HTTP_PORT_DEFAULT_SSL                       (443)

#define HTTP_DOWNLOAD_HEAD_MAX_LEN                  (1024)

#define FILE_DOWNLOAD_SSL_CONNECT_TIMEOUT           (6000)

#define HTTP_SEND_TIME_OUT                          (3000)
#define HTTP_RECV_TIME_OUT                          (3000)
#define HTTP_KEEP_IDLE_TIME                         (6)
#define HTTP_KEEP_INTVL_TIME                        (3)
#define HTTP_KEEP_COUNT                             (3)

#define HTTP_SELECT_YIELD_TMIE                      (3000)

#define DOWNLOAD_FILE_MAX_RETRY                     (3)

#define HTTP_RECV_CALLBACK_MAX_PACKET_LEN           (2000)

#define HTTP_REDIRECT_URL_MAX_LEN                   (4096)
#define HTTP_REDIRECT_HEAD                          ("Location:")

#define HTTP_FILE_DOWNLOAD_REQUEST_STR \
("GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
Connection: Keepalive\r\n\
Range: bytes=%ld-\r\n\
Accept-Encoding: identity\r\n\r\n")

#define HTTP_FILE_DOWNLOAD_REQUEST_STR_NOURI \
("GET / HTTP/1.1\r\n\
Host: %s\r\n\
Connection: Keepalive\r\n\
Range: bytes=%ld-\r\n\
Accept-Encoding: identity\r\n\r\n")


typedef enum {
  HTTP_SECURITY_HTTP,
  HTTP_SECURITY_HTTPS,
  HTTP_SECURITY_MAX,
  HTTP_SECURITY_NONE
}HTTP_SECURITY_TYPE_E;

typedef enum {
    HTTP_FILE_DOWNLOAD_STATE_START,
    HTTP_FILE_DOWNLOAD_STATE_SUCCESS,
    HTTP_FILE_DOWNLOAD_STATE_LOADING,
    HTTP_FILE_DOWNLOAD_STATE_FAILED,
    HTTP_FILE_DOWNLOAD_STATE_FAILED_AND_RETRY,
    HTTP_FILE_DOWNLOAD_STATE_REDIRET,
    HTTP_FILE_DOWNLOAD_STATE_MAX,
    HTTP_FILE_DOWNLOAD_STATE_NONE
}HTTP_FILE_DOWNLOAD_STATE_E; //download state

typedef enum {
    HTTP_FILE_DOWNLOAD_CONTROL_STATE_PAUSE,
    HTTP_FILE_DOWNLOAD_CONTROL_STATE_CONTINUE,
    HTTP_FILE_DOWNLOAD_CONTROL_STATE_STOP,
    HTTP_FILE_DOWNLOAD_CONTROL_STATE_MAX,
    HTTP_FILE_DOWNLOAD_CONTROL_STATE_NONE
}HTTP_FILE_DOWNLOAD_CONTROL_STATE_E; //control state

typedef struct _FILE_INFO_S{
    uint32_t    file_total_len; //total length
    uint32_t    download_len; //already download length
} FILE_INFO_S;

typedef struct _DOWNLOAD_IP_INFO_S{
    HTTP_SECURITY_TYPE_E   serurity_type;
    char host[HTTP_HOST_BUFF_SIZE];
    char ip[HTTP_IP_BUFF_SIZE];
    int  port;
    int  http_fd;
    mico_ssl_t http_ssl;
} DOWNLOAD_IP_INFO_S;

typedef void (*FILE_DOWNLOAD_STATE_CB) (void * context, HTTP_FILE_DOWNLOAD_STATE_E state, uint32_t sys_args, uint32_t user_args);
typedef bool (*FILE_DOWNLOAD_DATA_CB) (void * context, const char *data, uint32_t data_len, uint32_t user_args);

typedef struct _FILE_DOWNLOAD_CONTEXT_S{
  bool                      is_redirect;
  bool                      is_success;
  char *                    url;        //malloc space!!! remember free it!
  char *                    http_req;   //malloc space!!! remember free it!
  char *                    http_req_uri_p;
  uint32_t                  http_req_len;
  HTTPHeader_t *            httpHeader; //http header
  DOWNLOAD_IP_INFO_S        ip_info;
  FILE_INFO_S               file_info;
  HTTP_FILE_DOWNLOAD_CONTROL_STATE_E control_state;
  FILE_DOWNLOAD_STATE_CB    download_state_cb;
  FILE_DOWNLOAD_DATA_CB     download_data_cb;
  uint32_t                  user_args; //user args
} FILE_DOWNLOAD_CONTEXT_S;

void http_file_download_thread(mico_thread_arg_t arg);
uint32_t http_file_download_get_thread_num(void);

#endif
