/**
 ******************************************************************************
 * @file    http_client.c
 * @author  William Xu
 * @version V1.0.0
 * @date    21-May-2015
 * @brief   MiCO http client demo to read data from www.baidu.com
 ******************************************************************************
 *  The MIT License
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */

#include "mico.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "StringUtils.h"
#include "eland_http_client.h"

#define client_log(M, ...) custom_log("eland_http_clent", M, ##__VA_ARGS__)

static char *eland_http_requeset = NULL;
static bool is_https_connect = false; //HTTPS 是否连接

static OSStatus onReceivedData(struct _HTTPHeader_t *httpHeader,
                               uint32_t pos,
                               uint8_t *data,
                               size_t len,
                               void *userContext);
static void onClearData(struct _HTTPHeader_t *inHeader, void *inUserContext);

static mico_semaphore_t wait_sem = NULL;

typedef struct _http_context_t
{
    char *content;
    uint64_t content_length;
} http_context_t;

void simple_http_get(char *host, char *query);
void simple_https_get(char *host, char *query);

#define SIMPLE_GET_REQUEST    \
    "GET / HTTP/1.1\r\n"      \
    "Host: www.baidu.com\r\n" \
    "Connection: close\r\n"   \
    "Content-length:17\r\n"   \
    "\r\n"                    \
    "<html.....</html>"

//设置https连接状态
void set_https_connect_status(bool status)
{
    is_https_connect = status;
}

//获取https连接状态
bool get_https_connect_status(void)
{
    return is_https_connect;
}
//域名DNS解析
static OSStatus usergethostbyname(const char *domain, uint8_t *addr, uint8_t addrLen)
{
    struct hostent *host = NULL;
    struct in_addr in_addr;
    char **pptr = NULL;
    char *ip_addr = NULL;

    if (addr == NULL || addrLen < 16)
    {
        return kGeneralErr;
    }

    host = gethostbyname(domain);
    if ((host == NULL) || (host->h_addr_list) == NULL)
    {
        return kGeneralErr;
    }

    pptr = host->h_addr_list;
    //    for (; *pptr != NULL; pptr++ )
    {
        in_addr.s_addr = *(uint32_t *)(*pptr);
        ip_addr = inet_ntoa(in_addr);
        memset(addr, 0, addrLen);
        memcpy(addr, ip_addr, strlen(ip_addr));
    }

    return kNoErr;
}

//根据eland_http_req来生成一个http请求,组合出一个字符串
static OSStatus generate_eland_http_request(ELAND_HTTP_REQUEST_SETTING_S *eland_http_req)
{
    OSStatus err = kGeneralErr;
    uint32_t http_req_all_len = strlen(eland_http_req->http_body) + 1024; //为head部分预留1024字节

    eland_http_requeset = malloc(http_req_all_len);
    if (eland_http_requeset == NULL)
    {
        app_log("[ERROR]malloc error!!! malloc len is %ld", http_req_all_len);
        return kGeneralErr;
    }

    memset(eland_http_requeset, 0, http_req_all_len);

    if (eland_http_req->method != HTTP_POST && eland_http_req->method != HTTP_GET)
    {
        app_log("http method error!");
        err = kGeneralErr;
        goto exit;
    }

    if (eland_http_req->method == HTTP_POST)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_POST, eland_http_req->request_uri);
    }
    else if (eland_http_req->method == HTTP_GET)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_GET, eland_http_req->request_uri);
    }

    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Host: %s\r\n", eland_http_req->host_name); //增加hostname

    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Type: application/json\r\nConnection: Keepalive\r\n"); //增加Content-Type和Connection设置
    //sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Type: application/json\r\n"); //增加Content-Type和Connection设置
    //sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Type: application/json\r\nConnection: Close\r\n"); //增加Content-Type和Connection设置

    if (eland_http_req->is_jwt == true)
    {
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Authorization: JWT %s\r\n", eland_http_req->jwt);
    }

    //增加http body部分
    if (eland_http_req->http_body != NULL)
    {
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: %d\r\n\r\n", strlen((const char *)eland_http_req->http_body)); //增加Content-Length

        sprintf(eland_http_requeset + strlen(eland_http_requeset), "%s", eland_http_req->http_body);

        if (eland_http_req->http_body != NULL)
        {
            free(eland_http_req->http_body);
            eland_http_req->http_body = NULL;
        }
    }
    else
    {
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: 0\r\n\r\n"); //增加Content-Length
    }

    err = kNoErr;

#if (HTTP_REQ_LOG == 1)
    app_log("--------------------------------------");
    app_log("\r\n%s", eland_http_requeset);
    app_log("--------------------------------------");
#endif

exit:
    if (err != kNoErr)
    {
        if (eland_http_requeset != NULL)
        {
            free(eland_http_requeset);
            eland_http_requeset = NULL;
        }
    }

    return err;
}

void start_client_serrvice(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "client_serrvice", eland_client_serrvice, 0x2800, 0);
}
static void _serrvice(mico_thread_arg_t arg)
{
    OSStatus err = kGeneralErr;
    int http_fd = -1;
    int ssl_errno = 0;
    int ret = 0;
    mico_ssl_t client_ssl = NULL;
    fd_set readfds;
    char ipstr[20] = {0};

HTTP_SSL_START:

SSL_SEND:
    goto SSL_SEND;

exit:
    goto HTTP_SSL_START;
    if (eland_http_requeset != NULL)
    {
        free(eland_http_requeset);
        eland_http_requeset = NULL;
    }

    /* Read http data from server */
    simple_http_get("www.baidu.com", SIMPLE_GET_REQUEST);
    simple_https_get("www.baidu.com", SIMPLE_GET_REQUEST);

    return err;
}

static void eland_client_serrvice(mico_thread_arg_t arg)
{
    OSStatus err;
    int http_fd = -1;
    int ssl_errno = 0;
    mico_ssl_t client_ssl = NULL;
    fd_set readfds;
    char ipstr[16];
    struct sockaddr_in addr;
    HTTPHeader_t *httpHeader = NULL;
    http_context_t context = {NULL, 0};
    char *fog_host = ELAND_HTTP_DOMAIN_NAME;
    struct hostent *hostent_content = NULL;
    char **pptr = NULL;
    struct in_addr in_addr;

HTTP_SSL_START:
    set_https_connect_status(false);
    client_log("start dns annlysis, domain:%s", fog_host);
    err = usergethostbyname(fog_host, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        client_log("dns error!!! doamin:%s", fog_host);
        mico_thread_msleep(200);
        goto HTTP_SSL_START;
    }
    client_log("HTTP server address: host:%s, ip: %s", fog_host, ipstr);

    /*HTTPHeaderCreateWithCallback set some callback functions */
    httpHeader = HTTPHeaderCreateWithCallback(1024, onReceivedData, onClearData, &context);
    if (httpHeader == NULL)
    {
        mico_thread_msleep(200);
        client_log("HTTPHeaderCreateWithCallback() error");
        goto HTTP_SSL_START;
    }
    SocketClose(&http_fd);
    http_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(ELAND_HTTP_PORT_SSL); //HTTP SSL端口 443

    /**
    *   add  设置tcp keep_alive 参数
    */
    app_log("start connect");
    err = connect(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");

    //ssl_version_set(TLS_V1_2_MODE);    //设置SSL版本
    ssl_set_client_version(TLS_V1_2_MODE);

    app_log("start ssl_connect");

    client_ssl = ssl_connect(http_fd, 0, NULL, &ssl_errno);
    //client_ssl = ssl_connect( http_fd, strlen(http_server_ssl_cert_str), http_server_ssl_cert_str, &ssl_errno );
    require_action(client_ssl != NULL, exit, {err = kGeneralErr; app_log("https ssl_connnect error, errno = %d", ssl_errno); });

    app_log("#####https connect#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);

SSL_SEND:
    set_https_connect_status(true);
    //1.从消息队列中取一条信息
    err = mico_rtos_pop_from_queue(&eland_http_request_queue, &eland_http_req, MICO_WAIT_FOREVER);
    require_noerr(err, SSL_SEND);

    req_id = eland_http_req->http_req_id;

    err = generate_eland_http_request(eland_http_req); //生成http请求
    if (err != kNoErr)
    {
        send_response_to_queue(HTTP_REQUEST_ERROR, req_id, 0, NULL);
        goto SSL_SEND;
    }

    /* Send HTTP Request */
    ssl_send(client_ssl, query, strlen(query));

    FD_ZERO(&readfds);
    FD_SET(http_fd, &readfds);

    select(http_fd + 1, &readfds, NULL, NULL, NULL);
    if (FD_ISSET(http_fd, &readfds))
    {
        /*parse header*/
        err = SocketReadHTTPSHeader(client_ssl, httpHeader);
        switch (err)
        {
        case kNoErr:
            PrintHTTPHeader(httpHeader);
            err = SocketReadHTTPSBody(client_ssl, httpHeader); /*get body data*/
            require_noerr(err, exit);
            /*get data and print*/
            client_log("Content Data: %s", context.content);
            break;
        case EWOULDBLOCK:
        case kNoSpaceErr:
        case kConnectionErr:
        default:
            client_log("ERROR: HTTP Header parse error: %d", err);
            break;
        }
    }

exit:
    client_log("Exit: Client exit with err = %d, fd: %d", err, http_fd);
    if (client_ssl)
        ssl_close(client_ssl);
    SocketClose(&http_fd);
    HTTPHeaderDestory(&httpHeader);
}

/*one request may receive multi reply*/
static OSStatus onReceivedData(struct _HTTPHeader_t *inHeader, uint32_t inPos, uint8_t *inData,
                               size_t inLen, void *inUserContext)
{
    OSStatus err = kNoErr;
    http_context_t *context = inUserContext;
    if (inHeader->chunkedData == false)
    { //Extra data with a content length value
        if (inPos == 0 && context->content == NULL)
        {
            context->content = calloc(inHeader->contentLength + 1, sizeof(uint8_t));
            require_action(context->content, exit, err = kNoMemoryErr);
            context->content_length = inHeader->contentLength;
        }
        memcpy(context->content + inPos, inData, inLen);
    }
    else
    { //extra data use a chunked data protocol
        client_log("This is a chunked data, %d", inLen);
        if (inPos == 0)
        {
            context->content = calloc(inHeader->contentLength + 1, sizeof(uint8_t));
            require_action(context->content, exit, err = kNoMemoryErr);
            context->content_length = inHeader->contentLength;
        }
        else
        {
            context->content_length += inLen;
            context->content = realloc(context->content, context->content_length + 1);
            require_action(context->content, exit, err = kNoMemoryErr);
        }
        memcpy(context->content + inPos, inData, inLen);
    }

exit:
    return err;
}

/* Called when HTTPHeaderClear is called */
static void onClearData(struct _HTTPHeader_t *inHeader, void *inUserContext)
{
    UNUSED_PARAMETER(inHeader);
    http_context_t *context = inUserContext;
    if (context->content)
    {
        free(context->content);
        context->content = NULL;
    }
}
