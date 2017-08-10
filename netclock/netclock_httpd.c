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
#define app_httpd_log(M, ...) custom_log("apphttpd", M, ##__VA_ARGS__)

#define HTTPD_HDR_DEFORT (HTTPD_HDR_ADD_CONN_CLOSE)
static bool is_http_init;
static bool is_handlers_registered;
struct httpd_wsgi_call g_app_handlers[];
mico_semaphore_t wifi_netclock = NULL, wifi_SoftAP_Sem = NULL, httpServer_softAP_event_Sem = NULL;

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
    int buf_size = 1024;
    char *buf = NULL;
    char post_back_body[20] = {"post response body"};
    mico_Context_t *context = NULL;
    bool para_succ = false;
    msg_queue my_message;
    msg_wify_queue received;

    app_httpd_log("web_send_Post_Request");
    buf = malloc(buf_size);
    memset(buf, 0, buf_size);
    err = httpd_get_data(req, buf, buf_size);
    app_httpd_log("size = %d,buf = %s", req->body_nbytes, buf);
    app_httpd_log(">>>>>>>>web_send_Post_Request>>>>>>>>>>>");
    err = httpd_send_all_header(req, HTTP_RES_200, strlen(post_back_body), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)post_back_body, strlen(post_back_body));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
    mico_thread_sleep(3); //等待傳輸完成
    if (ProcessPostJson(buf) == kNoErr)
    {
        app_httpd_log("Json useful");
        /********清空消息隊列*************/
        while (!mico_rtos_is_queue_empty(&wifistate_queue))
            mico_rtos_pop_from_queue(&wifistate_queue, &received, MICO_WAIT_FOREVER);
        /********驗證wifi  ssid password*************/
        Start_wifi_Station_SoftSP_Thread(Station);
        mico_rtos_pop_from_queue(&wifistate_queue, &received, MICO_WAIT_FOREVER);

        if (received.value == Wify_Station_Connect_Successed)
        {
            my_message.type = Queue_ElandState_type;
            my_message.value = ElandWifyConnectedSuccessed;
            mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);

            app_httpd_log("Wifi parameter is correct");
            flagHttpdServerAP = 1;
            netclock_des_g->IsActivate = true;
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
            context->micoSystemConfig.dhcpEnable = true;

            context->micoSystemConfig.configured = allConfigured;
            mico_system_context_update(context);

            app_httpd_log("system restart");
            mico_system_power_perform(context, eState_Software_Reset);
            Eland_httpd_stop(); //stop http server mode
        }
        else
        {
            app_httpd_log("connect wifi failed");

            Start_wifi_Station_SoftSP_Thread(Soft_AP);
            my_message.type = Queue_ElandState_type;
            my_message.value = ElandWifyConnectedFailed;
            mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);
        }
    }
    else
    {
        app_httpd_log("Json error");
    }

    if (para_succ == true)
    {
    }
    free(buf);
    app_httpd_log("POST OVER");
    buf = NULL;
exit:
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
    msg_queue my_message;
    err = _app_httpd_start();
    require_noerr(err, exit);

    if (is_handlers_registered == false)
    {
        app_http_register_handlers();
        is_handlers_registered = true;
    }
    my_message.type = Queue_ElandState_type;
    my_message.value = ElandHttpServerStatus;
    mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);
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
