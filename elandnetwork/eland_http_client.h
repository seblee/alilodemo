#ifndef __ELAND_HTTP_CLIENT_H_
#define __ELAND_HTTP_CLIENT_H_

#define ELAND_HTTP_SEND_TIME_OUT (3000)
#define ELAND_HTTP_RECV_TIME_OUT (3000)
#define ELAND_HTTP_KEEP_IDLE_TIME (6)
#define ELAND_HTTP_KEEP_INTVL_TIME (3)
#define ELAND_HTTP_KEEP_COUNT (3)

#define HTTP_YIELD_TMIE (2000) //http超时时间

#define ELAND_HTTP_DOMAIN_NAME ("160.16.237.210") //HTTP服务器地址
#define ELAND_HTTP_PORT_SSL (443)                 //fog v2 http SSL端口
#define HTTP_REQ_LOG (0)                          //log 打印信息開關 1:enable 0:disable

#define HTTP_REQUEST_BODY_MAX_LEN (2048)
#define HTTP_REQUEST_HOST_NAME_MAX_LEN (64)
#define HTTP_REQUEST_REQ_URI_MAX_LEN (64)

#define HTTP_RESPONSE_BODY_MAX_LEN (2048) //HTTP返回接收的最大长度为2K

#define HTTP_HEAD_METHOD_POST ("POST")
#define HTTP_HEAD_METHOD_GET ("GET")

#define WAIT_HTTP_RES_MAX_TIME (7 * 1000) //http等待返回最大超时时间为7s
#define HTTP_MAX_RETRY_TIMES (4)          //最大重试次数

typedef enum {
    HTTP_POST = 1,
    HTTP_GET = 2
} ELAND_HTTP_METHOD;

typedef enum {
    HTTP_REQUEST_ERROR = 1,    //请求错误
    HTTP_CONNECT_ERROR = 2,    //连接失败
    HTTP_RESPONSE_SUCCESS = 3, //成功
    HTTP_RESPONSE_FAILURE = 4  //失败
} ELAND_HTTP_RESPONSE_E;

typedef enum {
    ELAND_REQUEST_NONE,           //eland NONE
    ELAND_COMMUNICATION_INFO_GET, //eland 情報取得
    ELAND_ALARM_GET,              //eland 鬧鐘聲音取得
} ELAND_HTTP_REQUEST_TYPE;

typedef struct _ELAND_HTTP_RESPONSE_SETTING
{
    ELAND_HTTP_RESPONSE_E send_status; //发送状态
    uint32_t http_res_id;
    int32_t status_code; //http错误码
    char *eland_response_body;
} ELAND_HTTP_RESPONSE_SETTING_S;

typedef struct _ELAND_HTTP_REQUEST_SETTING
{
    uint32_t http_req_id;
    ELAND_HTTP_METHOD method;
    ELAND_HTTP_REQUEST_TYPE eland_request_type;
    char request_uri[HTTP_REQUEST_REQ_URI_MAX_LEN];
    char host_name[HTTP_REQUEST_HOST_NAME_MAX_LEN];
    char *http_body;
} ELAND_HTTP_REQUEST_SETTING_S;

typedef struct _http_context_t
{
    char *content;
    uint64_t content_length;
} http_context_t;
typedef enum __SOUND_DOWNLOAD_STATUS {
    SOUND_DOWNLOAD_IDLE,
    SOUND_DOWNLOAD_START,
    SOUND_DOWNLOAD_PAUSE,
    SOUND_DOWNLOAD_CONTINUE,
    SOUND_DOWNLOAD_STOP,
} SOUND_DOWNLOAD_STATUS;
/********************************************/

extern mico_queue_t eland_http_request_queue;  //eland HTTP的发送请求队列
extern mico_queue_t eland_http_response_queue; //eland HTTP的接收响应队列

extern char *certificate;
extern char *private_key;
extern char *capem;

OSStatus start_client_serrvice(void);
bool get_https_connect_status(void);
//域名域名DNS解析
OSStatus usergethostbyname(const char *domain, uint8_t *addr, uint8_t addrLen);
int user_set_tcp_keepalive(int socket, int send_timeout, int recv_timeout, int idle, int interval, int count);

#endif