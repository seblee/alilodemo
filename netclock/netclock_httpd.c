/**
 ******************************************************************************
 * @file    app_https.c
 * @author  QQ DING
 * @version V1.0.0
 * @date    1-September-2015
 * @brief   The main HTTPD server initialization and wsgi handle.
 ******************************************************************************
 *
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
 *
 ******************************************************************************
 */
#include "hal_alilo_rabbit.h"
#include "netclock_httpd.h"
#include "netclock.h"
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include "mico.h"
#include "httpd_priv.h"
#include "netclock_wifi.h"
#include "netclock_uart.h"
#include "netclock_ota.h"

//#define CONFIG_APPHTTPD_DEBUG
#ifdef CONFIG_APPHTTPD_DEBUG
#define app_httpd_log(M, ...) custom_log("apphttpd", M, ##__VA_ARGS__)
#else
#define app_httpd_log(...)
#endif /* ! CONFIG_APPHTTPD_DEBUG */

#define HTTPD_HDR_DEFORT (HTTPD_HDR_ADD_CONN_CLOSE)
static bool is_http_init;
static bool is_handlers_registered;
struct httpd_wsgi_call g_app_handlers[];
mico_semaphore_t wifi_netclock = NULL, wifi_SoftAP_Sem = NULL;
static void eland_check_ssid(void)
{
    msg_wify_queue received;
    mico_Context_t *context = NULL;
    /********清空消息隊列*************/
    while (!mico_rtos_is_queue_empty(&wifistate_queue))
    {
        app_httpd_log("clear wifistate_queue");
        mico_rtos_pop_from_queue(&wifistate_queue, &received, MICO_WAIT_FOREVER);
    }
    app_httpd_log("wifistate_queue is empty");
    /********驗證wifi  ssid password*************/
    Start_wifi_Station_SoftSP_Thread(Station);
    mico_rtos_pop_from_queue(&wifistate_queue, &received, MICO_WAIT_FOREVER);
    if (received.value == Wify_Station_Connect_Successed)
    {
        app_httpd_log("Wifi parameter is correct");
        device_state->IsActivate = true;
        app_httpd_log("save wifi para,update flash"); //save
        context = mico_system_context_get();
        strncpy(context->micoSystemConfig.ssid, netclock_des_g->Wifissid, ElandSsid_Len);
        strncpy(context->micoSystemConfig.key, netclock_des_g->WifiKey, ElandKey_Len);
        strncpy(context->micoSystemConfig.user_key, netclock_des_g->WifiKey, ElandKey_Len);
        context->micoSystemConfig.keyLength = strlen(context->micoSystemConfig.key);
        context->micoSystemConfig.user_keyLength = strlen(context->micoSystemConfig.key);
        context->micoSystemConfig.channel = 0;
        memset(context->micoSystemConfig.bssid, 0x0, 6);
        context->micoSystemConfig.security = SECURITY_TYPE_AUTO;
        if (netclock_des_g->dhcp_enabled == 1)
            context->micoSystemConfig.dhcpEnable = true; /* Fetch Ip address from DHCP server */
        else
        {
            context->micoSystemConfig.dhcpEnable = false; /* Fetch Ip address from DHCP server */
            memcpy(context->micoSystemConfig.localIp, netclock_des_g->ip_address, 16);
            memcpy(context->micoSystemConfig.netMask, netclock_des_g->subnet_mask, 16);
            memcpy(context->micoSystemConfig.gateWay, netclock_des_g->default_gateway, 16);
            memcpy(context->micoSystemConfig.dnsServer, netclock_des_g->dnsServer, 16);
        }

        context->micoSystemConfig.configured = allConfigured;
        //mico_system_context_update(context);

        if (strncmp(ota_url, "\0", 1) != 0)
        {
            if (strncmp(ota_md5, "\0", 1) != 0)
            {
                start_ota_thread();
            }
            else
            {
                memset(ota_url, 0, sizeof(ota_url));
                memset(ota_md5, 0, sizeof(ota_md5));
            }
        }
        //app_httpd_log("system restart");
        //mico_system_power_perform(context, eState_Software_Reset);
    }
    Eland_httpd_stop(); //stop http server mode
    flagHttpdServerAP = 1;
    // else
    // {
    //     app_httpd_log("connect wifi failed");
    //     Start_wifi_Station_SoftSP_Thread(Soft_AP);
    //     SendElandStateQueue(ElandWifyConnectedFailed);
    // }
}
/*****************Get_Request******************************************/
static int web_send_Get_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    char *upload_data = NULL;
    uint32_t upload_data_len = 1024;
    app_httpd_log("web_send_Get_Request");
    upload_data = malloc(upload_data_len);
    memset(upload_data, 0, upload_data_len);
    InitUpLoadData(upload_data);

    err = httpd_send_all_header(req, HTTP_RES_200, strlen(upload_data), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)upload_data, strlen(upload_data));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));

exit:
    if (upload_data != NULL) //回收资源
    {
        free(upload_data);
        upload_data = NULL;
    }
    destory_upload_data(); //回收资源
    return err;
}
/*****************Post_Request******************************************/
static int web_send_Post_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    int ret;
    int buf_size = 1024;
    char *buf = NULL;

    buf = malloc(buf_size + 1);

    memset(buf, 0, buf_size + 1);
    app_httpd_log("post");
    /* read and parse header */
    ret = httpd_get_data(req, buf, buf_size);
    if (strncasecmp(HTTP_CONTENT_JSON_STR, req->content_type, strlen(HTTP_CONTENT_JSON_STR)) == 0) //json data
    {
        app_httpd_log("JSON*************");
        if (req->remaining_bytes == 0)
        {
            //app_httpd_log("size = %d,buf = %s", req->body_nbytes, buf);
            /**add json process**/
        }
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
        SendElandStateQueue(ELAPPConnected);
        mico_thread_sleep(2); //等待傳輸完成
        if (ProcessPostJson(buf) == kNoErr)
        {
            app_httpd_log("Json useful");
            eland_check_ssid();
        }
        else
        {
            app_httpd_log("Json error");
        }
    }
    else if (strncasecmp(HTTP_CONTENT_OCTET_STREAM, req->content_type, strlen(HTTP_CONTENT_OCTET_STREAM)) == 0)
    {
        app_httpd_log("OCTET-STREAM*************");
        app_httpd_log("remain_bytes:%d,total_len:%d", req->remaining_bytes, req->body_nbytes);
        // sprintf(ota_md5, "%s", "350f677d282dea128fab4bf05a508ea1");
        memset(ota_md5, 0, sizeof(ota_md5));
        strncpy(ota_md5, req->hash, strlen(req->hash));

        err = eland_ota_data_init(req->body_nbytes);
        err = eland_ota_data_write((uint8_t *)buf, ret);
        do
        {
            memset(buf, 0, buf_size + 1);
            ret = httpd_get_data(req, buf, buf_size);
            err = eland_ota_data_write((uint8_t *)buf, ret);
            app_httpd_log("remain_bytes:%d,total_len:%d", req->remaining_bytes, req->body_nbytes);
        } while (req->remaining_bytes > 0);
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
        SendElandStateQueue(ELAPPConnected);

        eland_ota_operation();
    }
    else
    {
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
        SendElandStateQueue(ELAPPConnected);
    }
    app_httpd_log("POST OVER");
exit:
    free(buf);
    return err;
}

struct httpd_wsgi_call g_app_handlers[] = {
    {"/", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
    {"/result.htm", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
    {"/setting.htm", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
};

static int g_app_handlers_no = sizeof(g_app_handlers) / sizeof(struct httpd_wsgi_call);

static void app_http_register_handlers()
{
    int rc;
    rc = httpd_register_wsgi_handlers(g_app_handlers, g_app_handlers_no);
    if (rc)
    {
        app_httpd_log("failed to register test web handler");
    }
}

static int _app_httpd_start()
{
    OSStatus err = kNoErr;
    app_httpd_log("initializing web-services");

    /*Initialize HTTPD*/
    if (is_http_init == false)
    {
        err = httpd_init();
        require_noerr_action(err, exit, app_httpd_log("failed to initialize httpd"));
        is_http_init = true;
    }

    /*Start http thread*/
    err = httpd_start();
    if (err != kNoErr)
    {
        app_httpd_log("failed to start httpd thread");
        httpd_shutdown();
    }
exit:
    return err;
}

int Eland_httpd_start(void)
{
    OSStatus err = kNoErr;
    err = _app_httpd_start();
    require_noerr(err, exit);

    if (is_handlers_registered == false)
    {
        app_http_register_handlers();
        is_handlers_registered = true;
    }
    SendElandStateQueue(HttpServerStatus);
exit:
    return err;
}

int Eland_httpd_stop()
{
    OSStatus err = kNoErr;

    /* HTTPD and services */
    app_httpd_log("stopping down httpd");
    err = httpd_stop();
    require_noerr_action(err, exit, app_httpd_log("failed to halt httpd"));

exit:
    return err;
}
