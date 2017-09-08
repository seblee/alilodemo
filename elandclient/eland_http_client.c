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

static bool http_get_wifi_status(void);
static OSStatus http_queue_init(void);
static OSStatus http_queue_deinit(void);
static void eland_client_serrvice(mico_thread_arg_t arg);
static void onClearData(struct _HTTPHeader_t *inHeader, void *inUserContext);
static OSStatus onReceivedData(struct _HTTPHeader_t *httpHeader, uint32_t pos, uint8_t *data, size_t len, void *userContext);

//啟動eland_client 線程，
OSStatus start_client_serrvice(void)
{
    OSStatus err = kGeneralErr;

    err = http_queue_init();
    require_noerr(err, exit);

    if (http_get_wifi_status() == false)
    {
        err = kGeneralErr;
        client_log("[ERROR]wifi is not connect!");
        goto exit;
    }

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "client_serrvice", eland_client_serrvice, 0x2800, 0);
    require_noerr(err, exit);

exit:
    if (err != kNoErr)
    {
        http_queue_deinit();
    }
    return err;
}
static OSStatus http_queue_init(void)
{
    OSStatus err = kGeneralErr;

    //初始化eland http 發送 request 消息
    err = mico_rtos_init_queue(&eland_http_request_queue, "eland_http_request_queue", sizeof(ELAND_HTTP_REQUEST_SETTING_S *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    //初始化eland http 接收到 response 消息
    err = mico_rtos_init_queue(&eland_http_response_queue, "eland_http_response_queue", sizeof(ELAND_HTTP_RESPONSE_SETTING_S *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

exit:
    return err;
}

static OSStatus http_queue_deinit(void)
{
    if (eland_http_request_queue != NULL)
    {
        mico_rtos_deinit_queue(&eland_http_request_queue);
    }

    if (eland_http_response_queue != NULL)
    {
        mico_rtos_deinit_queue(&eland_http_response_queue);
    }
    return kNoErr;
}

//获取网络连接状态
static bool http_get_wifi_status(void)
{
    LinkStatusTypeDef link_status;

    memset(&link_status, 0, sizeof(link_status));

    micoWlanGetLinkStatus(&link_status);

    return (bool)(link_status.is_connected);
}

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

//生成一个http回话id
static uint32_t generate_http_session_id(void)
{
    static uint32_t id = 1;

    return id++;
}

//域名域名DNS解析
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
        client_log("[ERROR]malloc error!!! malloc len is %ld", http_req_all_len);
        return kGeneralErr;
    }

    memset(eland_http_requeset, 0, http_req_all_len);

    if (eland_http_req->method != HTTP_POST && eland_http_req->method != HTTP_GET)
    {
        client_log("http method error!");
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
    client_log("--------------------------------------");
    client_log("\r\n%s", eland_http_requeset);
    client_log("--------------------------------------");
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

//设置tcp keep_alive 参数
static int user_set_tcp_keepalive(int socket, int send_timeout, int recv_timeout, int idle, int interval, int count)
{
    int retVal = 0, opt = 0;

    retVal = setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&send_timeout, sizeof(int));
    require_string(retVal >= 0, exit, "SO_SNDTIMEO setsockopt error!");

    client_log("setsockopt SO_SNDTIMEO=%d ms ok.", send_timeout);

    // retVal = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&recv_timeout, sizeof(int));
    // require_string(retVal >= 0, exit, "SO_RCVTIMEO setsockopt error!");

    // client_log("setsockopt SO_RCVTIMEO=%d ms ok.", recv_timeout);

    // set keepalive
    opt = 1;
    retVal = setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&opt, sizeof(opt)); // 开启socket的Keepalive功能
    require_string(retVal >= 0, exit, "SO_KEEPALIVE setsockopt error!");

    opt = idle;
    retVal = setsockopt(socket, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&opt, sizeof(opt)); // TCP IDLE idle秒以后开始发送第一个Keepalive包
    require_string(retVal >= 0, exit, "TCP_KEEPIDLE setsockopt error!");

    opt = interval;
    retVal = setsockopt(socket, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&opt, sizeof(opt)); // TCP后面的Keepalive包的间隔时间是interval秒
    require_string(retVal >= 0, exit, "TCP_KEEPINTVL setsockopt error!");

    opt = count;
    retVal = setsockopt(socket, IPPROTO_TCP, TCP_KEEPCNT, (void *)&opt, sizeof(opt)); // Keepalive 数量为count次
    require_string(retVal >= 0, exit, "TCP_KEEPCNT setsockopt error!");

    client_log("set tcp keepalive: idle=%d, interval=%d, cnt=%d.", idle, interval, count);

exit:
    return retVal;
}

static void eland_client_serrvice(mico_thread_arg_t arg)
{
    OSStatus err = kGeneralErr;
    int http_fd = -1;
    int ssl_errno = 0;
    int ret = 0;
    mico_ssl_t client_ssl = NULL;
    fd_set readfds;
    char ipstr[20] = {0};
    struct sockaddr_in addr;
    HTTPHeader_t *httpHeader = NULL;
    http_context_t context = {NULL, 0};
    char *eland_host = ELAND_HTTP_DOMAIN_NAME;
    struct timeval t = {0, HTTP_YIELD_TMIE * 1000};
    uint32_t req_id = 0;
    ELAND_HTTP_REQUEST_SETTING_S *eland_http_req = NULL; //http 请求

HTTP_SSL_START:
    set_https_connect_status(false);
    client_log("start dns annlysis, domain:%s", eland_host);
    err = usergethostbyname(eland_host, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        client_log("dns error!!! doamin:%s", eland_host);
        mico_thread_msleep(200);
        goto HTTP_SSL_START;
    }
    client_log("HTTP server address: host:%s, ip: %s", eland_host, ipstr);

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

    //设置tcp keep_alive 参数
    ret = user_set_tcp_keepalive(http_fd, HTTP_SEND_TIME_OUT, HTTP_RECV_TIME_OUT, HTTP_KEEP_IDLE_TIME, HTTP_KEEP_INTVL_TIME, HTTP_KEEP_COUNT);
    if (ret < 0)
    {
        client_log("user_set_tcp_keepalive() error");
        goto exit;
    }

    client_log("start connect");
    err = connect(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");

    //ssl_version_set(TLS_V1_2_MODE);    //设置SSL版本
    ssl_set_client_version(TLS_V1_2_MODE);

    client_log("start ssl_connect");

    client_ssl = ssl_connect(http_fd, 0, NULL, &ssl_errno);
    //client_ssl = ssl_connect( http_fd, strlen(http_server_ssl_cert_str), http_server_ssl_cert_str, &ssl_errno );
    require_action(client_ssl != NULL, exit, {err = kGeneralErr; client_log("https ssl_connnect error, errno = %d", ssl_errno); });

    client_log("#####https connect#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);

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
    /* Send HTTP Request */
    /* Send HTTP Request */
    ret = ssl_send(client_ssl, eland_http_requeset, strlen((const char *)eland_http_requeset)); /* Send HTTP Request */
    if (eland_http_requeset != NULL)                                                            //释放发送缓冲区
    {
        free(eland_http_requeset);
        eland_http_requeset = NULL;
    }
    if (ret > 0)
    {
        //client_log("ssl_send success [%d] [%d]", strlen((const char *)eland_http_requeset) ,ret);
        //client_log("%s", eland_http_requeset);
    }
    else
    {
        client_log("-----------------ssl_send error, ret = %d", ret);
        err = kGeneralErr;
        goto exit;
    }
    FD_ZERO(&readfds);
    FD_SET(http_fd, &readfds);

    ret = select(http_fd + 1, &readfds, NULL, NULL, &t);
    if (ret == -1 || ret == 0)
    {
        client_log("-----------------select error, ret = %d", ret);
        err = kGeneralErr;
        goto exit;
    }

    if (FD_ISSET(http_fd, &readfds))
    {
        /*parse header*/
        err = SocketReadHTTPSHeader(client_ssl, httpHeader);
        switch (err)
        {
        case kNoErr:
        {
            if ((httpHeader->statusCode == -1) || (httpHeader->statusCode >= 500))
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                goto exit; //断开重新连接
            }
            if (httpHeader->statusCode == 403) //認證錯誤
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                goto exit; //断开重新连接
            }

            if (httpHeader->statusCode == 404) //沒找到文件
            {
                client_log("[FAILURE]eland http response fail, code:%d", httpHeader->statusCode);
                send_response_to_queue(HTTP_RESPONSE_FAILURE, req_id, httpHeader->statusCode, NULL);
                break;
            }

            //只有code正确才解析返回数据,错误情况下解析容易造成内存溢出
            if (httpHeader->statusCode == 200) //正常應答
            {
                //PrintHTTPHeader( httpHeader );
                err = SocketReadHTTPSBody(client_ssl, httpHeader); /*get body data*/
                require_noerr(err, exit);
#if (HTTP_REQ_LOG == 1)
                client_log("Content Data:[%ld]%s", context.content_length, context.content); /*get data and print*/
#endif
                send_response_to_queue(HTTP_RESPONSE_SUCCESS, req_id, httpHeader->statusCode, context.content);
            }
            break;
        }
        case EWOULDBLOCK:
            break;

        case kNoSpaceErr:
            client_log("SocketReadHTTPSHeader kNoSpaceErr");
            goto exit; //断开重新连接
            break;
        case kConnectionErr:
            client_log("SocketReadHTTPSHeader kConnectionErr");
            goto exit; //断开重新连接
            break;
        default:
            client_log("ERROR: HTTP Header parse error: %d", err);
            goto exit; //断开重新连接
            break;
        }
    }
    HTTPHeaderClear(httpHeader);
    client_log("#####https send#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
    goto SSL_SEND;
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
