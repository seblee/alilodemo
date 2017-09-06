#ifndef __ELAND_HTTP_CLIENT_H_
#define __ELAND_HTTP_CLIENT_H_

#define ELAND_HTTP_DOMAIN_NAME ("") //HTTP服务器地址
#define ELAND_HTTP_PORT_SSL (443)   //fog v2 http SSL端口
#define HTTP_REQ_LOG (0)            //log 打印信息開關 1:enable 0:disable

#define HTTP_REQUEST_BODY_MAX_LEN (2048)
#define HTTP_REQUEST_HOST_NAME_MAX_LEN (64)
#define HTTP_REQUEST_REQ_URI_MAX_LEN (64)

#define HTTP_RESPONSE_BODY_MAX_LEN (2048) //HTTP返回接收的最大长度为2K

#define HTTP_HEAD_METHOD_POST ("POST")
#define HTTP_HEAD_METHOD_GET ("GET")

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
    char request_uri[HTTP_REQUEST_REQ_URI_MAX_LEN];
    char host_name[HTTP_REQUEST_HOST_NAME_MAX_LEN];
    char *http_body;
} ELAND_HTTP_REQUEST_SETTING_S;

#endif
