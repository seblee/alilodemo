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

#define client_log(M, ...) custom_log("eland_http_clent", M, ##__VA_ARGS__)

static bool is_https_connect = false; //HTTPS 是否连接

static ELAND_HTTP_RESPONSE_SETTING_S eland_http_res; //全局变量 http响应的全局设置
static char *eland_http_requeset = NULL;

mico_queue_t eland_http_request_queue = NULL;  //eland HTTP的发送请求队列
mico_queue_t eland_http_response_queue = NULL; //eland HTTP的接收响应队列

static mico_semaphore_t eland_http_client_InitCompleteSem = NULL;
uint32_t eland_sound_download_pos = 0;
uint32_t eland_sound_download_len = 0;
SOUND_DOWNLOAD_STATUS sound_download_status = SOUND_DOWNLOAD_IDLE;
/**********************************************************/

/**********************************************/

static bool http_get_wifi_status(void);
static OSStatus http_queue_init(void);
static OSStatus http_queue_deinit(void);
static void eland_client_serrvice(mico_thread_arg_t arg);
static void onClearData(struct _HTTPHeader_t *inHeader, void *inUserContext);

/*one request may receive multi reply*/
static OSStatus onReceivedData(struct _HTTPHeader_t *inHeader,
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
    "Content-Type: audio/ x-mp3\r\n"                           \
    "\r\n"                                                     \
    ""
/*******************************************************/

//啟動eland_client 線程，
OSStatus start_client_serrvice(void)
{
    OSStatus err = kGeneralErr;

    err = mico_rtos_init_semaphore(&eland_http_client_InitCompleteSem, 1);
    require_noerr_string(err, exit, "eland_http_client_InitCompleteSem init err");

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

    err = mico_rtos_get_semaphore(&eland_http_client_InitCompleteSem, MICO_WAIT_FOREVER);
    require_noerr_string(err, exit, "eland_http_client_InitCompleteSem get err");

exit:
    if (err != kNoErr)
    {
        http_queue_deinit();
    }
    err = mico_rtos_deinit_semaphore(&eland_http_client_InitCompleteSem);
    require_noerr_string(err, exit, "eland_http_client_InitCompleteSem deinit err");
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

    if ((eland_http_req->eland_request_type == ELAND_DEVICE_INFO_LOGIN) ||
        (eland_http_req->eland_request_type == ELAND_DEVICE_INFO_UPDATE) ||
        (eland_http_req->eland_request_type == ELAND_ALARM_OFF_RECORD_ENTRY))
        sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Type: application/json\r\n"); //增加Content-Type和Connection设置
    //sprintf(eland_http_requeset + strlen(eland_http_requeset), "Content-Type: application/json\r\nConnection: Keepalive\r\n"); //增加Content-Type和Connection设置
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
    ret = user_set_tcp_keepalive(http_fd,
                                 ELAND_HTTP_SEND_TIME_OUT,   //http tcp 發送超時
                                 ELAND_HTTP_RECV_TIME_OUT,   //http tcp 接收超時
                                 ELAND_HTTP_KEEP_IDLE_TIME,  //http tcp 如该连接在xx秒内没有任何数据往来,则进行此TCP层的探测
                                 ELAND_HTTP_KEEP_INTVL_TIME, // 探测发包间隔为x秒
                                 ELAND_HTTP_KEEP_COUNT);     // 尝试探测的次数.如果第1次探测包就收到响应了,则后2次的不再发
    if (ret < 0)
    {
        client_log("user_set_tcp_keepalive() error");
        goto exit;
    }

    client_log("start connect");
    err = connect(http_fd, (struct sockaddr *)&addr, sizeof(addr));
    require_noerr_string(err, exit, "connect https server failed");

    // 設置 證書
    ssl_set_client_cert(certificate, private_key);
    //ssl_version_set(TLS_V1_2_MODE);    //设置SSL版本
    ssl_set_client_version(TLS_V1_2_MODE);

    client_log("start ssl_connect");
    client_ssl = ssl_connect(http_fd, strlen(capem), capem, &ssl_errno);
    require_action(client_ssl != NULL, exit, {err = kGeneralErr; client_log("https ssl_connnect error, errno = %d", ssl_errno); });
    client_log("#####https connect#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
    if (eland_http_client_InitCompleteSem != NULL)
        mico_rtos_set_semaphore(&eland_http_client_InitCompleteSem);
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
    ret = ssl_send(client_ssl, eland_http_requeset, strlen((const char *)eland_http_requeset)); /* Send HTTP Request */

    if (eland_http_requeset != NULL) //释放发送缓冲区
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
        if (sound_download_status == SOUND_DOWNLOAD_START)
        {
            eland_sound_download_len = httpHeader->contentLength;
            sound_download_status = SOUND_DOWNLOAD_CONTINUE;
        }
        switch (err)
        {
        case kNoErr:
        {
            if ((httpHeader->statusCode == -1) || (httpHeader->statusCode >= 500))
            {
                client_log("[ERROR]eland http response error, code:%d", httpHeader->statusCode);
                goto exit; //断开重新连接
            }
            if (httpHeader->statusCode == 400) //認證錯誤
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
                PrintHTTPHeader(httpHeader);
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
    if (eland_sound_download_pos == eland_sound_download_pos)
    {
        sound_download_status = SOUND_DOWNLOAD_IDLE;
    }

    HTTPHeaderClear(httpHeader);
    client_log("#####https send#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
    goto SSL_SEND;
exit:
    set_https_connect_status(false);
    send_response_to_queue(HTTP_CONNECT_ERROR, req_id, 0, NULL); //只有发生连接发生错误的时候才会进入exit中

    if (client_ssl)
    {
        ssl_close(client_ssl);
        client_ssl = NULL;
    }

    SocketClose(&http_fd);
    HTTPHeaderDestory(&httpHeader);

    mico_thread_msleep(200);
    client_log("#####https disconnect#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
    goto HTTP_SSL_START;

    if (eland_http_requeset != NULL)
    {
        free(eland_http_requeset);
        eland_http_requeset = NULL;
    }

    http_queue_deinit();

    client_log("Exit: Client exit with err = %d, fd:%d", err, http_fd);
    mico_rtos_delete_thread(NULL);
    return;
}
/*one request may receive multi reply */
static OSStatus onReceivedData(struct _HTTPHeader_t *inHeader, uint32_t inPos, uint8_t *inData,
                               size_t inLen, void *inUserContext)
{
    OSStatus err = kNoErr;
    http_context_t *context = inUserContext;
    static uint32_t sound_flash_address = 0x001000;
    client_log("inPos = %ld,inLen = %d,flash_address = %ld", inPos, inLen, sound_flash_address);
    if (inHeader->chunkedData == false)
    { //Extra data with a content length value
        if (inPos == 0 && context->content == NULL)
        {
            //context->content = calloc(inHeader->contentLength + 1, sizeof(uint8_t));
            context->content = calloc(1501, sizeof(uint8_t));
            require_action(context->content, exit, err = kNoMemoryErr);
            context->content_length = inHeader->contentLength;
        }
        //memcpy(context->content + inPos, inData, inLen);
        memcpy(context->content, inData, inLen);
        flash_kh25_write_page((uint8_t *)context->content, sound_flash_address, inLen);
        sound_flash_address += inLen;
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
    eland_sound_download_pos = 0;
}

//给response的消息队列发送消息
void send_response_to_queue(ELAND_HTTP_RESPONSE_E status, uint32_t http_id, int32_t status_code, const char *response_body)
{
    OSStatus err = kGeneralErr;
    ELAND_HTTP_RESPONSE_SETTING_S *eland_http_response_temp = NULL, *eland_http_res_p = NULL;
    static uint32_t local_id = 0;

    if (local_id == http_id) //过滤重复id   理论上id是递增的
    {
        return;
    }

    eland_http_res.send_status = status;
    eland_http_res.http_res_id = http_id;
    eland_http_res.status_code = status_code;

    if (response_body != NULL)
    {
        eland_http_res.eland_response_body = malloc(strlen(response_body) + 2);
        require_action_string(eland_http_res.eland_response_body != NULL, exit, err = kNoMemoryErr, "[ERROR]malloc() error!");

        memset(eland_http_res.eland_response_body, 0, strlen(response_body) + 2); //清0

        memcpy(eland_http_res.eland_response_body, response_body, strlen(response_body));
    }
    else
    {
        eland_http_res.eland_response_body = NULL;
    }
    if (false == mico_rtos_is_queue_empty(&eland_http_response_queue))
    {
        client_log("[error]eland_http_response_queue is full");

        err = mico_rtos_pop_from_queue(&eland_http_response_queue, &eland_http_response_temp, 10); //如果满先弹出一个
        require_noerr_action(err, exit, client_log("[error]mico_rtos_pop_from_queue err"));

        if (eland_http_response_temp->eland_response_body != NULL)
        {
            free(eland_http_response_temp->eland_response_body);
            eland_http_response_temp->eland_response_body = NULL;
        }

        eland_http_response_temp = NULL;
    }

    eland_http_res_p = &eland_http_res;
    err = mico_rtos_push_to_queue(&eland_http_response_queue, &eland_http_res_p, 10);
    require_noerr_action(err, exit, client_log("[error]mico_rtos_push_to_queue err"));

    local_id = http_id;

exit:
    if (err != kNoErr)
    {
        if (eland_http_res.eland_response_body != NULL)
        {
            free(eland_http_res.eland_response_body);
            eland_http_res.eland_response_body = NULL;
        }
    }

    return;
}
