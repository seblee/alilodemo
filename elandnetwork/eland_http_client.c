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
#include "flash_kh25.h"
#include "eland_sound.h"
#include "netclock.h"
#include "netclock_uart.h"
#include "audio_service.h"
#include "netclock_ota.h"

#define CONFIG_CLIENT_DEBUG
#ifdef CONFIG_CLIENT_DEBUG
#define client_log(M, ...) custom_log("Client", M, ##__VA_ARGS__)
#else
#define client_log(...)
#endif /* ! CONFIG_CLIENT_DEBUG */

static char *eland_http_requeset = NULL;

uint32_t eland_sound_download_pos = 0;
uint32_t eland_sound_download_len = 0;
SOUND_DOWNLOAD_STATUS sound_download_status = SOUND_DOWNLOAD_IDLE;
/**********************************************************/

/**********************************************/

static void onClearData(struct _HTTPHeader_t *inHeader, void *inUserContext);

/*one request may receive multi reply*/
static OSStatus onReceivedData(struct _HTTPHeader_t *inHeader,
                               uint32_t inPos,
                               uint8_t *inData,
                               size_t inLen,
                               void *inUserContext);
/**download oid sound**/
static OSStatus onReceivedData_oid(struct _HTTPHeader_t *inHeader,
                                   uint32_t inPos,
                                   uint8_t *inData,
                                   size_t inLen,
                                   void *inUserContext);
/**download ota file**/
static OSStatus onReceivedData_ota(struct _HTTPHeader_t *inHeader,
                                   uint32_t inPos,
                                   uint8_t *inData,
                                   size_t inLen,
                                   void *inUserContext);

//给response的消息队列发送消息
void send_response_to_queue(ELAND_HTTP_RESPONSE_E status,
                            uint32_t http_id,
                            int32_t status_code,
                            const char *response_body);

/*************certificate********************************/

char *certificate =
    "-----BEGIN CERTIFICATE-----\n\
MIIESzCCAzOgAwIBAgIJAMuHDGTAVpi1MA0GCSqGSIb3DQEBCwUAMIGTMQswCQYD\n\
VQQGEwJKUDEOMAwGA1UECAwFVG9reW8xGjAYBgNVBAoMEUtJTkcgSklNIENPLixM\n\
VEQuMRcwFQYDVQQLDA5SJkQgRGVwYXJ0bWVudDEZMBcGA1UEAwwQRUxBTkQgUHJp\n\
dmF0ZSBDQTEkMCIGCSqGSIb3DQEJARYVd2VibWFzdGVyQGV4YW1wbGUuY29tMCAX\n\
DTE3MDkwNjExNDIxNloYDzIxMTcwODEzMTE0MjE2WjCBoTELMAkGA1UEBhMCSlAx\n\
DjAMBgNVBAgMBVRva3lvMRMwEQYDVQQHDApDaGl5b2RhLWt1MRowGAYDVQQKDBFL\n\
SU5HIEpJTSBDTy4sTFRELjEXMBUGA1UECwwOUiZEIERlcGFydG1lbnQxEjAQBgNV\n\
BAMMCWVsYW5kLmNvbTEkMCIGCSqGSIb3DQEJARYVd2VibWFzdGVyQGV4YW1wbGUu\n\
Y29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnqwIBQSjPrp3HRvi\n\
O0h9+qWcKEkBNrhxhSiKzRN33cad/EdSwSv4ZQWBy0TggbMPDt5DPfw24BzvmQwW\n\
sl/FCUcQ70Xr+65pmdnp06Jobe90zH8YsSPyeSXta23zmvHJ2W1+7au9fRJQazRl\n\
4jBar6KCmoqFgg/Bw7qnEhI3fwRRly8LddVMj/emEmFEoiIJq5J09ZUo7fOmfCKr\n\
R0BVx8iKz6n1Cgt1/kpYJfnIkAYhTe0Ed4T2dJIVQkKjcSdW3dPFx86aJhFPcPa9\n\
3Mqv9aLSDh8A0YD0k0ohYCwvCe0g8FOMny66+Jdxbp0ybyDPZ4wvMP2sHOf7sTrg\n\
jv43iwIDAQABo4GPMIGMMAkGA1UdEwQCMAAwEQYJYIZIAYb4QgEBBAQDAgSwMCwG\n\
CWCGSAGG+EIBDQQfFh1PcGVuU1NMIEdlbmVyYXRlZCBDZXJ0aWZpY2F0ZTAdBgNV\n\
HQ4EFgQUHCxe533Sc0pFzudFwDZjpPfe6FkwHwYDVR0jBBgwFoAUWcY/DuhU0R7S\n\
cdwSiBFkabVvvKowDQYJKoZIhvcNAQELBQADggEBAGQKScFpkqqshnzMNEUVieP/\n\
CVjOIMxE2uxmhDeeQsWFR7odKnetFvHW9p2Lp2mXEGO6E0du5SKCryzD2ZTXiMsW\n\
SLdSau0G9591ROQok4F/6jn1zYkiL9GdsZBsxBdOGVl7JgO3eqYHsDkYA7vUHVoE\n\
WPt/Id+U2bmjuB+GHINNAL68+OZf+0BPt3w5UaGlaMhJRlO/f+PMJ97ZvrXnmRDj\n\
sa8GJLhffe4SpyHR168UGrrce26SJjlIzVsWB9uqK5bWG2WpqmMcz8Susn8EQFvH\n\
yQyH15iLII+5FwBQCDBjrKm3VzI7/2BD1rbbkqxv20fPTkYOAoh6vYd78aUYIBE=\n\
-----END CERTIFICATE-----";
char *private_key =
    "-----BEGIN PRIVATE KEY-----\n\
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCerAgFBKM+uncd\n\
G+I7SH36pZwoSQE2uHGFKIrNE3fdxp38R1LBK/hlBYHLROCBsw8O3kM9/DbgHO+Z\n\
DBayX8UJRxDvRev7rmmZ2enTomht73TMfxixI/J5Je1rbfOa8cnZbX7tq719ElBr\n\
NGXiMFqvooKaioWCD8HDuqcSEjd/BFGXLwt11UyP96YSYUSiIgmrknT1lSjt86Z8\n\
IqtHQFXHyIrPqfUKC3X+Slgl+ciQBiFN7QR3hPZ0khVCQqNxJ1bd08XHzpomEU9w\n\
9r3cyq/1otIOHwDRgPSTSiFgLC8J7SDwU4yfLrr4l3FunTJvIM9njC8w/awc5/ux\n\
OuCO/jeLAgMBAAECggEABi76GQf3PKiTn8TIajsG/c+aaE+ABpvlgKT108wgbboh\n\
ygUVioWmJnmydzN19FgADDpJMI81rEI0bCh2cfkdeqEUXd7BtYs0flRpsl+v5ijg\n\
yl9hnPWjq2j4+ajNR4qIrTqBKc35kng2PhdKqSftQM76e/9N+KWYjYImpKOlGgQH\n\
WSkPEBmTo7sIzNPwuye88bYY6oGh62nCIfvuwyfoTnlCQ3Gz8gDMFlqJgm/0J6Vb\n\
JYM8r1UCJyd2hKHuB0quuBEoRbNKm0agzx9Xe0KZ455GextTkm2JXvZlb957UrO/\n\
mzHs/G2MjrmmVJ8b/iusXSa9CKxQGdKXPmzwAq314QKBgQDR9wILEtJFNYmmtlv5\n\
cI9GQdPOtpMwpcPC13IdQ8KIuuTqblvy10BJLqMKXlDkhr8ZDqJR32Nhtl4pn5Kl\n\
61ZoEytjUpvb85Uv8Wvtyg/+/Lb9y/eUcSN5NI5kT+iJPTJOksDCd/6M/U16ZdMm\n\
cD0R4p32d+n3CjEZ+F8bKVRc2QKBgQDBdglLpbRFKtnMDZAQJUwwH3drcqyG6WWp\n\
Ica7aUNRUn7qrxckG3b3+ONCDhQVMQT/ijqj5yWkXQNwiZV2SJoAwkAbNH18yYPu\n\
zx0YYegVL26527uz+AJ9H88k4VzJkK6P6lmjOfqOLfytopsTSOV9r+vMIJW8E3Jq\n\
I+QlqTmJAwKBgQCPeit7RbFKeftGYPcYzUIa0IDckQakB6JuUqs4NEWLCavERwWu\n\
PElBuQzQ2QKOJ0YO6WEicXSIIQbXiqO7ncW9+Nt9U8YN17XqvR7zr1Ce/jJN3EOi\n\
vG1xNejXw4MzxQ3Lg50VRso7rhxzt4FCkxAoWKN4+Rh4KA7FoGPdO7DagQKBgB7M\n\
7hnvHc5NTjOgjSkk5wZaXCbtMO6hxh+xUvSPg7o0yiQPED4daUl9hKEFoMjm7wbI\n\
OSHTMTkD3gJSxUr5sBsi0hYCu1/crXad3uH85HhK/vP0OeQjPjIxmEck4iLtN/2N\n\
sAu+tVdhlvMGCm59kpv6IC51maFB71tar34XfSOFAoGAWoMPyUkwvlstoj8jYW1i\n\
5GDSj3MnwcYDkMsVZKVMQRMIgZCFHHIHDvtOS5pDcap41OdYMoLZjGzC7+Cp4kF1\n\
xzBtqSA0mrUr91R/KUi+Fg2pfssocYurFzgee/206/UrnLJ2a1uOTezt9p5vFBr+\n\
j1RnT/plaAAX6tMYz7zLsWY=\n\
-----END PRIVATE KEY-----";
char *capem =
    "-----BEGIN CERTIFICATE-----\n\
MIIEEDCCAvigAwIBAgIJAMuHDGTAVpizMA0GCSqGSIb3DQEBCwUAMIGTMQswCQYD\n\
VQQGEwJKUDEOMAwGA1UECAwFVG9reW8xGjAYBgNVBAoMEUtJTkcgSklNIENPLixM\n\
VEQuMRcwFQYDVQQLDA5SJkQgRGVwYXJ0bWVudDEZMBcGA1UEAwwQRUxBTkQgUHJp\n\
dmF0ZSBDQTEkMCIGCSqGSIb3DQEJARYVd2VibWFzdGVyQGV4YW1wbGUuY29tMCAX\n\
DTE3MDkwNjEwNDIyNVoYDzIxMTcwODEzMTA0MjI1WjCBkzELMAkGA1UEBhMCSlAx\n\
DjAMBgNVBAgMBVRva3lvMRowGAYDVQQKDBFLSU5HIEpJTSBDTy4sTFRELjEXMBUG\n\
A1UECwwOUiZEIERlcGFydG1lbnQxGTAXBgNVBAMMEEVMQU5EIFByaXZhdGUgQ0Ex\n\
JDAiBgkqhkiG9w0BCQEWFXdlYm1hc3RlckBleGFtcGxlLmNvbTCCASIwDQYJKoZI\n\
hvcNAQEBBQADggEPADCCAQoCggEBAKlA3QRVel3OEF7ZuNPQjkKfByOR1kTNp3bh\n\
2nmq7vvBUPC4OeBZIAr9Jb9I801fi5IuHpcbjcCBrhBVo+m5QKAKEtLaRPzD1vHS\n\
s3znxwQ/1q0Zkb9fFfuN7pYCi/DHkjMPuh5NGFoB4k1jzJNizA1pKw+eBMvrHhYD\n\
a9b2bh9XjdZsoT5p8muGdm7XMKvb81YxOkp5Z8q16Ql1YWLUTEKoVyLMjvx7bI/j\n\
JVB3Qb1zm+xBwxOTfCAmf4vHUtalM4Gk5NWU67REsmqxlLcpuIVWHh6IO3c9rH5E\n\
UVHGUdlFAbNJ71KvikdzvXRs9g0OgRKw3C/y4Hkga66ybZ0Lb58CAwEAAaNjMGEw\n\
HQYDVR0OBBYEFFnGPw7oVNEe0nHcEogRZGm1b7yqMB8GA1UdIwQYMBaAFFnGPw7o\n\
VNEe0nHcEogRZGm1b7yqMAwGA1UdEwQFMAMBAf8wEQYJYIZIAYb4QgEBBAQDAgEG\n\
MA0GCSqGSIb3DQEBCwUAA4IBAQAwkfmYqKC0Ky6Y3di8E3Fvqhm6MsvvXTSSlbIz\n\
4NGGS864IFODqZCAPZmVGDskViRpx37J9YKq7Gnvji8hRUWSb6S7hqgr7kOEz1zE\n\
82/xXw7RKDi0Vb96je9cPOMgThLbZ1diVyI3RTfY57xu+lEtgCrki9wkauCROacF\n\
STUCuV0muX6umL05hwov5+1mjzAfMoonRSW+qltTFAzZPXuQfg4YkskTmSBsjqUo\n\
SdE0WebPo0V0j6bBvtiPEt2q7SLlNk6oCvra2b8tRvZQZC6hIfkk1PtDBgavL87k\n\
9r7mdVFee0dLXDjGfsxQ9tm59xTacuOKYE0Zrw5OUkNn7OrS\n\
-----END CERTIFICATE-----";

#define SIMPLE_GET_REQUEST                                     \
    "GET /api/download.php?vid=taichi_16_024kbps HTTP/1.1\r\n" \
    "Host: 160.16.237.210\r\n"                                 \
    "Content-Type: audio/x-mp3\r\n"                            \
    "\r\n"                                                     \
    ""
/*******************************************************/

//域名域名DNS解析
OSStatus usergethostbyname(const char *domain, uint8_t *addr, uint8_t addrLen)
{
    struct hostent *host = NULL;
    struct in_addr in_addr;
    char **pptr = NULL;
    char *ip_addr = NULL;
    LinkStatusTypeDef link_status;

    if (addr == NULL || addrLen < 16)
    {
        return kGeneralErr;
    }

    memset(&link_status, 0, sizeof(link_status));
    micoWlanGetLinkStatus(&link_status);

    if (link_status.is_connected == false)
    {
        return kGeneralErr;
    }

    host = gethostbyname(domain);
    if ((host == NULL) || (host->h_addr_list) == NULL)
    {
        return kGeneralErr;
    }

    pptr = host->h_addr_list;

    in_addr.s_addr = *(uint32_t *)(*pptr);
    ip_addr = inet_ntoa(in_addr);
    memset(addr, 0, addrLen);
    memcpy(addr, ip_addr, strlen(ip_addr));

    return kNoErr;
}

//设置tcp keep_alive 参数
int user_set_tcp_keepalive(int socket, int send_timeout, int recv_timeout, int idle, int interval, int count)
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

//发送http请求 single mode
OSStatus eland_http_request(ELAND_HTTP_METHOD method,                          //POST 或者 GET
                            ELAND_HTTP_REQUEST_TYPE request_type,              //eland request type
                            char *request_uri,                                 //uri
                            char *host_name,                                   //host
                            char *http_body,                                   //BODY
                            ELAND_HTTP_RESPONSE_SETTING_S *user_http_response) //response 指針
{
    OSStatus err = kGeneralErr;
    uint32_t http_req_all_len;
    char ipstr[20] = {0};
    char *eland_host = ELAND_HTTP_DOMAIN_NAME;
    HTTPHeader_t *httpHeader = NULL;
    http_context_t context = {NULL, 0};
    struct sockaddr_in addr;
    int http_fd = -1;
    int ret = 0;
    int ssl_errno = 0;
    mico_ssl_t client_ssl = NULL;
    fd_set readfds;
    struct timeval t = {0, HTTP_YIELD_TMIE * 1000};

    if (http_body)
        http_req_all_len = strlen(http_body) + 1024; //为head部分预留1024字节 need free
    else
        http_req_all_len = 1024; //为head部分预留1024字节 need free
    eland_http_requeset = malloc(http_req_all_len);
    if (eland_http_requeset == NULL)
    {
        client_log("[ERROR]malloc error!!! malloc len is %ld", http_req_all_len);
        return kGeneralErr;
    }
    memset(eland_http_requeset, 0, http_req_all_len);
    if (method != HTTP_POST && method != HTTP_GET)
    {
        client_log("http method error!");
        err = kGeneralErr;
        goto exit;
    }
    mico_rtos_lock_mutex(&http_send_setting_mutex); //这个锁 锁住的资源比较多
    /******set request mode******/
    if (method == HTTP_POST)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_POST, request_uri);
    }
    else if (method == HTTP_GET)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_GET, request_uri);
    }
    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Host: %s\r\n", host_name); //增加hostname
    //sprintf(eland_http_requeset + strlen(eland_http_requeset), "Keep-Alive：300\r\n");                     //增加Connection设置
    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Connection: Keep-Alive\r\n"); //增加Connection设置

    //增加http body部分
    if (http_body != NULL)
    {
        /*增加Content-Length*/
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: %d\r\n\r\n", strlen((const char *)http_body));
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "%s", http_body);
        if (http_body != NULL)
        {
            free(http_body);
            http_body = NULL;
        }
    }
    else
    {
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: 0\r\n\r\n"); //增加Content-Length
    }

    if (client_ssl)
    {
        ssl_close(client_ssl);
        client_ssl = NULL;
    }
    if (http_fd > 0)
    {
        SocketClose(&http_fd);
        http_fd = -1;
    }
    client_log("start dns analysis, domain:%s", eland_host);
    err = usergethostbyname(eland_host, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        client_log("dns error!!! doamin:%s", eland_host);
        err = kGeneralErr;
        goto exit;
    }
    /*HTTPHeaderCreateWithCallback set some callback functions */
    httpHeader = HTTPHeaderCreateWithCallback(1024, onReceivedData, onClearData, &context);
    if (httpHeader == NULL)
    {
        client_log("HTTPHeaderCreateWithCallback() error");
        err = kGeneralErr;
        goto exit;
    }
    SocketClose(&http_fd);
    http_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(ELAND_HTTP_PORT_SSL); //HTTP SSL端口 443

    //设置tcp keep_alive 参数
    ret = user_set_tcp_keepalive(http_fd,
                                 ELAND_HTTP_SEND_TIME_OUT,   //http tcp 發送超時
                                 ELAND_HTTP_RECV_TIME_OUT,   //http tcp 接收超時
                                 ELAND_HTTP_KEEP_IDLE_TIME,  //http tcp 如该连接在xx秒内没有任何数据往来,则进行此TCP层的探测
                                 ELAND_HTTP_KEEP_INTVL_TIME, // 探测发包间隔为x秒
                                 ELAND_HTTP_KEEP_COUNT);     // 尝试探测的次数.如果第1次探测包就收到响应了,则后2次的不再发
    if (ret < 0)
    {
        err = kGeneralErr;
        client_log("user_set_tcp_keepalive() error ret:%d", ret);
        goto exit;
    }
    client_log("start connect");
    err = connect(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");
    // 設置 證書
    ssl_set_client_cert(certificate, private_key);
    ssl_set_client_version(TLS_V1_2_MODE);
    client_log("start ssl_connect");
    client_ssl = ssl_connect(http_fd, strlen(capem), capem, &ssl_errno);
    // client_ssl = ssl_connect(http_fd, 0, NULL, &ssl_errno);
    require_action(client_ssl != NULL, exit, {err = kGeneralErr; client_log("https ssl_connnect error, errno = %d", ssl_errno); });

    /* Send HTTP Request */
    ret = ssl_send(client_ssl, eland_http_requeset, strlen((const char *)eland_http_requeset));
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
        if (sound_download_status == SOUND_DOWNLOAD_START)
        {
            eland_sound_download_len = httpHeader->contentLength;
            sound_download_status = SOUND_DOWNLOAD_CONTINUE;
        }
        switch (err)
        {
        case kNoErr:
        {
            if (httpHeader->statusCode >= 500)
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                user_http_response->send_status = HTTP_REQUEST_ERROR;
                user_http_response->status_code = httpHeader->statusCode;
                user_http_response->eland_response_body = NULL;
                break;
            }
            if (httpHeader->statusCode == 400) //認證錯誤
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                user_http_response->send_status = HTTP_CONNECT_ERROR;
                user_http_response->status_code = httpHeader->statusCode;
                user_http_response->eland_response_body = NULL;
                break;
            }

            if (httpHeader->statusCode == 404) //沒找到文件
            {
                client_log("[FAILURE]eland http response fail, code:%d", httpHeader->statusCode);
                user_http_response->send_status = HTTP_RESPONSE_FAILURE;
                user_http_response->status_code = httpHeader->statusCode;
                user_http_response->eland_response_body = NULL;
                break;
            }

            //只有code正确才解析返回数据,错误情况下解析容易造成内存溢出
            if (httpHeader->statusCode == 200) //正常應答
            {
                //PrintHTTPHeader(httpHeader);
                err = SocketReadHTTPSBody(client_ssl, httpHeader); /*get body data*/
                client_log("data lenth = %ld", (uint32_t)context.content_length);
                require_noerr(err, exit);
#if (HTTP_REQ_LOG == 1)
                client_log("Content %s0x%08x:[%ld]",
                           context.content, context.content, context.content_length); /*get data and print*/
#endif
                user_http_response->send_status = HTTP_RESPONSE_SUCCESS;
                user_http_response->status_code = httpHeader->statusCode;
                if (context.content_length < 1501)
                {
                    user_http_response->eland_response_body = calloc(context.content_length, sizeof(uint8_t));
                    memcpy(user_http_response->eland_response_body, context.content, context.content_length);
                }
            }
            break;
        }
        case EWOULDBLOCK:
            break;

        case kNoSpaceErr:
            client_log("SocketReadHTTPSHeader kNoSpaceErr");
            user_http_response->send_status = HTTP_REQUEST_ERROR;
            break;
        case kConnectionErr:
            client_log("SocketReadHTTPSHeader kConnectionErr");
            user_http_response->send_status = HTTP_REQUEST_ERROR;
            break;
        default:
            client_log("ERROR: HTTP Header parse error: %d", err);
            user_http_response->send_status = HTTP_REQUEST_ERROR;
            break;
        }
    }
exit:
    if (client_ssl)
    {
        ssl_close(client_ssl);
        client_ssl = NULL;
    }

    SocketClose(&http_fd);

    /*free request buf*/
    if (eland_http_requeset != NULL)
    {
        free(eland_http_requeset);
        eland_http_requeset = NULL;
    }
    HTTPHeaderDestory(&httpHeader);
    mico_thread_msleep(200);
    mico_rtos_unlock_mutex(&http_send_setting_mutex); //锁必须要等到response队列返回之后才能释放
    return err;
}

/*one request may receive multi reply */
static OSStatus onReceivedData(struct _HTTPHeader_t *inHeader, uint32_t inPos, uint8_t *inData,
                               size_t inLen, void *inUserContext)
{
    OSStatus err = kNoErr;
    http_context_t *context = inUserContext;
    static uint32_t sound_flash_pos = 0;
    static bool is_sound_data = false;

    if (inHeader->chunkedData == false)
    {
        //Extra data with a content length value
        if (inPos == 0 && context->content == NULL)
        {
            client_log("This is not a chunked data");
            if (strstr(inHeader->buf, "audio/x-mp3"))
                is_sound_data = true;
            else
                is_sound_data = false;

            if (is_sound_data)
            {
                context->content = calloc(1501, sizeof(uint8_t));
                sound_flash_pos = 0;
            }
            else
                context->content = calloc(inHeader->contentLength + 1, sizeof(uint8_t));

            require_action(context->content, exit, err = kNoMemoryErr);
            context->content_length = inHeader->contentLength;
        }
        if (is_sound_data)
        {

            client_log("sound data");
            if (HTTP_W_R_struct.alarm_w_r_queue == NULL)
                goto exit;
            HTTP_W_R_struct.alarm_w_r_queue->total_len = inHeader->contentLength;
            memcpy(context->content, inData, inLen);
            HTTP_W_R_struct.alarm_w_r_queue->len = inLen;
            HTTP_W_R_struct.alarm_w_r_queue->pos = sound_flash_pos;
            HTTP_W_R_struct.alarm_w_r_queue->sound_data = (uint8_t *)context->content;
            HTTP_W_R_struct.alarm_w_r_queue->is_read = false;
            client_log("send_queue:inlen = %ld,sound_flash_pos = %ld", HTTP_W_R_struct.alarm_w_r_queue->len, HTTP_W_R_struct.alarm_w_r_queue->pos);
            err = sound_file_read_write(&sound_file_list, HTTP_W_R_struct.alarm_w_r_queue);
            require_noerr(err, exit);
            sound_flash_pos += inLen;
        }
        else
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
    client_log("clear header");
    if (context->content)
    {
        free(context->content);
        context->content = NULL;
    }
    eland_sound_download_pos = 0;
}

//发送http请求 single mode
OSStatus eland_http_file_download(ELAND_HTTP_METHOD method, //POST 或者 GET
                                  char *request_uri,        //uri
                                  char *host_name,          //host
                                  char *http_body,          //BODY
                                  _download_type_t download_type)
{
    OSStatus err = kGeneralErr;
    uint32_t http_req_all_len;
    char ipstr[20] = {0};
    HTTPHeader_t *httpHeader = NULL;
    http_context_t context = {NULL, 0};
    struct sockaddr_in addr;
    int http_fd = -1;
    int ret = 0;
    int ssl_errno = 0;
    mico_ssl_t client_ssl = NULL;
    fd_set readfds;
    struct timeval t = {1, HTTP_YIELD_TMIE * 1000};

    if (http_body)
        http_req_all_len = strlen(http_body) + 1024; //为head部分预留1024字节 need free
    else
        http_req_all_len = 1024; //为head部分预留1024字节 need free
    eland_http_requeset = malloc(http_req_all_len);
    if (eland_http_requeset == NULL)
    {
        client_log("[ERROR]malloc error!!! malloc len is %ld", http_req_all_len);
        return kGeneralErr;
    }
    memset(eland_http_requeset, 0, http_req_all_len);
    if (method != HTTP_POST && method != HTTP_GET)
    {
        client_log("http method error!");
        err = kGeneralErr;
        goto exit;
    }
    mico_rtos_lock_mutex(&http_send_setting_mutex); //这个锁 锁住的资源比较多
    /******set request mode******/
    if (method == HTTP_POST)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_POST, request_uri);
    }
    else if (method == HTTP_GET)
    {
        sprintf(eland_http_requeset, "%s %s HTTP/1.1\r\n", HTTP_HEAD_METHOD_GET, request_uri);
    }
    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Host: %s\r\n", host_name); //增加hostname
    //sprintf(eland_http_requeset + strlen(eland_http_requeset), "Keep-Alive：300\r\n");                     //增加Connection设置
    sprintf(eland_http_requeset + strlen(eland_http_requeset), "Connection: Keep-Alive\r\n"); //增加Connection设置

    //增加http body部分
    if (http_body != NULL)
    {
        /*增加Content-Length*/
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: %d\r\n\r\n", strlen((const char *)http_body));
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "%s", http_body);
        if (http_body != NULL)
        {
            free(http_body);
            http_body = NULL;
        }
    }
    else
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Length: 0\r\n\r\n"); //增加Content-Length

    client_log("start dns analysis, domain:%s", host_name);
    err = usergethostbyname(host_name, (uint8_t *)ipstr, sizeof(ipstr));
    if (err != kNoErr)
    {
        client_log("dns error!!! doamin:%s", host_name);
        err = kGeneralErr;
        goto exit;
    }
    /*HTTPHeaderCreateWithCallback set some callback functions */
    if (download_type == DOWNLOAD_OID)
        httpHeader = HTTPHeaderCreateWithCallback(1024, onReceivedData_oid, onClearData, &context);
    else if (download_type == DOWNLOAD_OTA)
        httpHeader = HTTPHeaderCreateWithCallback(1024, onReceivedData_ota, onClearData, &context);
    else
        goto exit;
    if (httpHeader == NULL)
    {
        client_log("HTTPHeaderCreateWithCallback() error");
        err = kGeneralErr;
        goto exit;
    }
    http_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ipstr);
    addr.sin_port = htons(ELAND_HTTP_PORT_SSL); //HTTP SSL端口 443

    //设置tcp keep_alive 参数
    ret = user_set_tcp_keepalive(http_fd,
                                 ELAND_HTTP_SEND_TIME_OUT,   //http tcp 發送超時
                                 ELAND_HTTP_RECV_TIME_OUT,   //http tcp 接收超時
                                 ELAND_HTTP_KEEP_IDLE_TIME,  //http tcp 如该连接在xx秒内没有任何数据往来,则进行此TCP层的探测
                                 ELAND_HTTP_KEEP_INTVL_TIME, // 探测发包间隔为x秒
                                 ELAND_HTTP_KEEP_COUNT);     // 尝试探测的次数.如果第1次探测包就收到响应了,则后2次的不再发
    if (ret < 0)
    {
        err = kGeneralErr;
        client_log("user_set_tcp_keepalive() error ret:%d", ret);
        goto exit;
    }
    client_log("start connect");
    err = connect(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");
    // 設置 證書
    ssl_set_client_cert(certificate, private_key);
    ssl_set_client_version(TLS_V1_2_MODE);
    client_log("start ssl_connect");
    client_ssl = ssl_connect(http_fd, strlen(capem), capem, &ssl_errno);
    require_action(client_ssl != NULL, exit, {err = kGeneralErr; client_log("https ssl_connnect error, errno = %d", ssl_errno); });
    /* Send HTTP Request */
    ret = ssl_send(client_ssl, eland_http_requeset, strlen((const char *)eland_http_requeset));
    if (ret > 0)
    {
        client_log("ssl_send success [%d] [%d]", strlen((const char *)eland_http_requeset), ret);
        client_log("%s", eland_http_requeset);
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
            if (httpHeader->statusCode >= 500)
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                break;
            }
            if (httpHeader->statusCode == 400) //認證錯誤
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                break;
            }

            if (httpHeader->statusCode == 404) //沒找到文件
            {
                client_log("[FAILURE]eland http response fail, code:%d", httpHeader->statusCode);
                break;
            }
            if (httpHeader->statusCode == 204) //文件為空 不需要播放
            {
                client_log("[SUCCESS]eland http responsed, code:%d", httpHeader->statusCode);
                break;
            }

            //只有code正确才解析返回数据,错误情况下解析容易造成内存溢出
            if (httpHeader->statusCode == 200) //正常應答
            {
                //PrintHTTPHeader(httpHeader);
                err = SocketReadHTTPSBody(client_ssl, httpHeader); /*get body data*/
                client_log("data lenth = %ld", (uint32_t)context.content_length);
                require_noerr(err, exit);
            }
            break;
        }
        case EWOULDBLOCK:
            break;

        case kNoSpaceErr:
            client_log("SocketReadHTTPSHeader kNoSpaceErr");
            break;
        case kConnectionErr:
            client_log("SocketReadHTTPSHeader kConnectionErr");
            break;
        default:
            client_log("ERROR: HTTP Header parse error: %d", err);
            break;
        }
    }
exit:
    if (client_ssl)
    {
        ssl_close(client_ssl);
        client_ssl = NULL;
    }

    SocketClose(&http_fd);

    /*free request buf*/
    if (eland_http_requeset != NULL)
    {
        free(eland_http_requeset);
        eland_http_requeset = NULL;
    }
    HTTPHeaderDestory(&httpHeader);
    mico_rtos_unlock_mutex(&http_send_setting_mutex); //锁必须要等到response队列返回之后才能释放
    return err;
}

/*one request may receive multi reply */
static OSStatus onReceivedData_oid(struct _HTTPHeader_t *inHeader, uint32_t inPos, uint8_t *inData,
                                   size_t inLen, void *inUserContext)
{
    OSStatus err = kNoErr;
    mscp_result_t result = MSCP_RST_ERROR;
    http_context_t *context = inUserContext;
    static AUDIO_STREAM_PALY_S fm_stream;
    static uint8_t fm_test_cnt;

    if (inHeader->chunkedData == false)
        client_log("This is not a chunked data");
    //Extra data with a content length value
    if (inPos == 0 && context->content == NULL)
    {
        context->content = calloc(1501, sizeof(uint8_t));
        require_action(context->content, exit, err = kNoMemoryErr);
        context->content_length = inHeader->contentLength;
        fm_stream.type = AUDIO_STREAM_TYPE_MP3;
        fm_stream.stream_id = alarm_stream.stream_id;
        fm_stream.total_len = context->content_length;
        fm_stream.pdata = (const uint8_t *)context->content;
    }
    memcpy(context->content, inData, inLen);
    fm_stream.stream_len = (uint16_t)(inLen & 0xFFFF); //len

    if ((++fm_test_cnt) >= 10)
    {
        fm_test_cnt = 0;
        client_log("fm_stream.type[%d],fm_stream.stream_id[%d],fm_stream.total_len[%d],fm_stream.stream_len[%d]",
                   (int)fm_stream.type, (int)fm_stream.stream_id, (int)fm_stream.total_len, (int)fm_stream.stream_len);
    }
audio_transfer:
    err = audio_service_stream_play(&result, &fm_stream);
    if (err != kNoErr)
    {
        client_log("audio_stream_play() error!!!!");
        return false;
    }
    else
    {
        if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
        {
            client_log("new slave set pause!!!");
            mico_rtos_thread_msleep(1000); //time set 1000ms!!!
            goto audio_transfer;
        }
        else
        {
            err = kNoErr;
        }
    }

exit:
    return err;
}
/*one request may receive multi reply */
static OSStatus onReceivedData_ota(struct _HTTPHeader_t *inHeader, uint32_t inPos, uint8_t *inData,
                                   size_t inLen, void *inUserContext)
{
    OSStatus err = kNoErr;
    http_context_t *context = inUserContext;

    if (inHeader->chunkedData == false)
        client_log("This is not a chunked data");
    //Extra data with a content length value
    if (inPos == 0 && context->content == NULL)
    {
        context->content = calloc(1, sizeof(uint8_t));
        require_action(context->content, exit, err = kNoMemoryErr);
        context->content_length = inHeader->contentLength;
        eland_ota_data_init(context->content_length);
        client_log("This is not a chunked data");
    }
    eland_ota_data_write(inData, inLen);
exit:
    return err;
}
