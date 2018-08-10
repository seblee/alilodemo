/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-03-05 11:31:37
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-22 11:21:11
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
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

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define CONFIG_APPHTTPD_DEBUG
#ifdef CONFIG_APPHTTPD_DEBUG
#define app_httpd_log(M, ...) custom_log("apphttpd", M, ##__VA_ARGS__)
#else
#define app_httpd_log(...)
#endif /* ! CONFIG_APPHTTPD_DEBUG */

#define HTTPD_HDR_DEFORT (HTTPD_HDR_ADD_CONN_CLOSE)

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static bool is_http_init;
static bool is_handlers_registered;
struct httpd_wsgi_call g_app_handlers[];
mico_semaphore_t wifi_netclock = NULL, wifi_SoftAP_Sem = NULL;
mico_semaphore_t http_ssid_event_Sem = NULL;
__http_ssids_list_t wifi_scan_list;
/* Private function prototypes -----------------------------------------------*/
static OSStatus eland_Setting_Json(char *InputJson);
static void eland_wifi_parameter_save(void);
/* Private functions ---------------------------------------------------------*/
static void wifi_manager_scan_complete_cb(ScanResult_adv *pApList, void *arg)
{
    uint8_t i;
    __http_ssids_list_t *list = (__http_ssids_list_t *)arg;
    list->num = pApList->ApNum;
    if (pApList->ApNum > 0)
        list->ssids = calloc(pApList->ApNum, sizeof(__http_ssids_result_t));
    app_httpd_log("ap number:%d", pApList->ApNum);
    for (i = 0; i < pApList->ApNum; i++)
    {
        memcpy(list->ssids[i].ssid, pApList->ApList[i].ssid, 32);
        list->ssids[i].security = (pApList->ApList[i].security > 0) ? 1 : 0;
        list->ssids[i].rssi = pApList->ApList[i].rssi;

        app_httpd_log("ap%d--security:%d,name:%s,rssi=%d ", i,
                      (list->ssids + i)->security,
                      (list->ssids + i)->ssid,
                      (list->ssids + i)->rssi);
    }
    mico_rtos_set_semaphore(&http_ssid_event_Sem);
}
static OSStatus build_ssids_json_string(char *data, __http_ssids_list_t *wifi_list)
{
    uint8_t i;
    OSStatus err = kGeneralErr;
    json_object *Json = NULL;
    json_object *JsonList = NULL;
    json_object *JsonSsidval = NULL;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    uint8_t secure = 0;

    Json = json_object_new_object();
    require_action_string(Json, exit, err = kNoMemoryErr, "create Json object error!");
    JsonList = json_object_new_array();
    require_action_string(JsonList, exit, err = kNoMemoryErr, "create JsonList object error!");
    app_httpd_log("wifi_list->num:%d", wifi_list->num);

    for (i = 0; i < wifi_list->num; i++)
    {
        JsonSsidval = json_object_new_object();
        require_action_string(JsonSsidval, exit, err = kNoMemoryErr, "create JsonSsidval object error!");
        if ((wifi_list->ssids + i)->rssi > RSSI_STATE_STAGE4)
            secure = 100;
        else if ((wifi_list->ssids + i)->rssi < RSSI_STATE_STAGE0)
            secure = 0;
        else
            secure = ((wifi_list->ssids + i)->rssi - RSSI_STATE_STAGE0) * 100 /
                     (RSSI_STATE_STAGE4 - RSSI_STATE_STAGE0);
        app_httpd_log("ap%d--rssi:%d,signal:%d", i, (wifi_list->ssids + i)->rssi, secure);
        json_object_object_add(JsonSsidval, "secure", json_object_new_int((wifi_list->ssids + i)->security));
        json_object_object_add(JsonSsidval, "signal", json_object_new_int((uint32_t)secure));
        json_object_object_add(JsonSsidval, "ssid", json_object_new_string((wifi_list->ssids + i)->ssid));

        json_object_array_add(JsonList, JsonSsidval);
        if (i >= 25)
            break;
    }
    json_object_object_add(Json, "ssids", JsonList);

    generate_data = json_object_to_json_string(Json);
    require_action_string(generate_data != NULL, exit, err = kNoMemoryErr, "create generate_data string error!");
    generate_data_len = strlen(generate_data);
    require_action_string(generate_data_len < HTTP_RESPONSE_LEN, exit, err = kNoMemoryErr, "json data is too long");
    memcpy(data, generate_data, generate_data_len);
    app_httpd_log("data:%s", data);
exit:
    app_httpd_log("exit ");
    free_json_obj(&JsonList);
    free_json_obj(&Json);
    return err;
}
void eland_check_ssid(void)
{
    msg_wify_queue received;
    mico_Context_t *context = NULL;
    /********清空消息隊列*************/
    context = mico_system_context_get();
    while (!mico_rtos_is_queue_empty(&wifistate_queue))
    {
        app_httpd_log("clear wifistate_queue");
        mico_rtos_pop_from_queue(&wifistate_queue, &received, 0);
    }

    app_httpd_log("wifistate_queue is empty");
    /********驗證wifi  ssid password*************/
    Start_wifi_Station_SoftSP_Thread(Station);
    mico_rtos_pop_from_queue(&wifistate_queue, &received, MICO_WAIT_FOREVER);
    if (received.value == Wify_Station_Connect_Successed)
        eland_wifi_parameter_save();
    else
    {
        SendElandStateQueue(WifyConnectedFailed);
        //Eland_httpd_stop(); //stop http server mode
        app_httpd_log("connect wifi failed");
        //Start_wifi_Station_SoftSP_Thread(Soft_AP);
    }
    app_httpd_log("system restart");

    mico_system_power_perform(context, eState_Software_Reset);
    mico_rtos_thread_sleep(2);
    flagHttpdServerAP = 1;
}
/*****************Get_Request******************************************/
static int web_send_Get_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    char *upload_data = NULL;
    uint32_t upload_data_len = HTTP_RESPONSE_LEN;
    app_httpd_log("web_send_Get_Request");
    upload_data = malloc(upload_data_len);
    memset(upload_data, 0, upload_data_len);
    InitUpLoadData(upload_data);

    err = httpd_send_all_header(req, HTTP_RES_200, strlen(upload_data), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)upload_data, strlen(upload_data));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
    SendElandStateQueue(ELAPPConnected);
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
    int buf_size = HTTP_RESPONSE_LEN;
    char *buf = NULL;

    buf = malloc(buf_size + 1);
    memset(buf, 0, buf_size + 1);
    app_httpd_log("post");
    SendElandStateQueue(ELAPPConnected);
    /* read and parse header */
    ret = httpd_get_data(req, buf, buf_size);
    if (strncasecmp(HTTP_CONTENT_JSON_STR, req->content_type, strlen(HTTP_CONTENT_JSON_STR)) == 0) //json data
    {
        app_httpd_log("JSON*************");
        if (req->remaining_bytes == 0)
        {
            app_httpd_log("size = %d,buf = %s", req->body_nbytes, buf);
            /**add json process**/
        }
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));

        mico_thread_sleep(1); //等待傳輸完成
        if (ProcessPostJson(buf) == kNoErr)
        {
            eland_wifi_parameter_save();
            mico_system_power_perform(mico_system_context_get(), eState_Software_Reset);
            app_httpd_log("Json useful");
            //    eland_check_ssid();
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

        eland_ota_operation();
    }
    else
    {
        app_httpd_log("Content-Type: %s", req->content_type);
        app_httpd_log("data: %s", buf);
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
    }
    app_httpd_log("POST OVER");
exit:
    free(buf);
    app_httpd_log("system restart");
    mico_rtos_thread_msleep(200);
    mico_system_power_perform(mico_system_context_get(), eState_Software_Reset);
    return err;
}
/*****************web_send_Get_ssids_Request******************************************/
static int web_send_Get_ssids_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    char *upload_data = NULL;
    uint32_t upload_data_len = HTTP_RESPONSE_LEN;
    app_httpd_log("Get_ssids_Request");
    SendElandStateQueue(ELAPPConnected);
    upload_data = malloc(upload_data_len);
    memset(upload_data, 0, upload_data_len);
    memset(&wifi_scan_list, 0, sizeof(__http_ssids_list_t));
    micoWlanStartScanAdv();
    err = mico_rtos_get_semaphore(&http_ssid_event_Sem, 5000);

    if (err == kNoErr)
        build_ssids_json_string(upload_data, &wifi_scan_list);
    else
        sprintf(upload_data, HTTPD_JSON_ERR_MSG, "timeout");
    app_httpd_log("#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);

    err = httpd_send_all_header(req, HTTP_RES_200, strlen(upload_data), HTTP_CONTENT_JSON_STR);
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

    err = httpd_send_body(req->sock, (const unsigned char *)upload_data, strlen(upload_data));
    require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
exit:
    if (wifi_scan_list.num > 0)
        free(wifi_scan_list.ssids);
    free(upload_data);
    app_httpd_log("#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);

    return err;
}
/*****************web_send_Post_setting_Request******************************************/
static int web_send_Post_setting_Request(httpd_request_t *req)
{
    OSStatus err = kNoErr;
    int buf_size = HTTP_RESPONSE_LEN;
    char *buf = NULL;

    buf = malloc(buf_size + 1);

    memset(buf, 0, buf_size + 1);
    app_httpd_log("post");
    SendElandStateQueue(ELAPPConnected);
    /* read and parse header */
    httpd_get_data(req, buf, buf_size);
    if (strncasecmp(HTTP_CONTENT_JSON_STR, req->content_type, strlen(HTTP_CONTENT_JSON_STR)) == 0) //json data
    {
        app_httpd_log("JSON*************");
        if (req->remaining_bytes == 0)
        {
            app_httpd_log("size = %d,buf = %s", req->body_nbytes, buf);
            /**add json process**/
        }
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));

        err = eland_Setting_Json(buf);
        mico_thread_sleep(1); //等待傳輸完成
    }
    else
    {
        app_httpd_log("Content-Type: %s", req->content_type);
        app_httpd_log("data: %s", buf);
        err = httpd_send_all_header(req, HTTP_RES_200, strlen(HTTPD_JSON_SUCCESS), HTTP_CONTENT_JSON_STR);
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting headers."));

        err = httpd_send_body(req->sock, (const unsigned char *)HTTPD_JSON_SUCCESS, strlen(HTTPD_JSON_SUCCESS));
        require_noerr_action(err, exit, app_httpd_log("ERROR: Unable to send http wifisetting body."));
    }
    app_httpd_log("POST OVER");
exit:
    free(buf);
    return err;
}
struct httpd_wsgi_call g_app_handlers[] = {
    {"/", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_Request, NULL, NULL},
    {"/ssids", HTTPD_HDR_DEFORT, 0, web_send_Get_ssids_Request, NULL, NULL, NULL},
    {"/setting", HTTPD_HDR_DEFORT, 0, web_send_Get_Request, web_send_Post_setting_Request, NULL, NULL},
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

    err = mico_rtos_init_semaphore(&http_ssid_event_Sem, 1);
    require_noerr(err, exit);

    err = mico_system_notify_register(mico_notify_WIFI_SCAN_ADV_COMPLETED, (void *)wifi_manager_scan_complete_cb, &wifi_scan_list);
    require_noerr(err, exit);

    if (is_handlers_registered == false)
    {
        app_http_register_handlers();
        is_handlers_registered = true;
    }
exit:
    return err;
}

int Eland_httpd_stop()
{
    OSStatus err = kNoErr;

    err = mico_rtos_deinit_semaphore(&http_ssid_event_Sem);
    require_noerr(err, exit);

    err = mico_system_notify_remove(mico_notify_WIFI_SCAN_ADV_COMPLETED, (void *)wifi_manager_scan_complete_cb);
    require_noerr(err, exit);
    /* HTTPD and services */
    app_httpd_log("stopping down httpd");
    err = httpd_stop();
    require_noerr_action(err, exit, app_httpd_log("failed to halt httpd"));

exit:
    return err;
}

//解析接收到的数据包
static OSStatus eland_Setting_Json(char *InputJson)
{
    json_object *ReceivedJsonCache = NULL;
    mico_rtos_lock_mutex(&netclock_des_g->des_mutex);
    if (*InputJson != '{')
    {
        app_httpd_log("error:received err json format data");
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(InputJson));
    if (ReceivedJsonCache == NULL)
    {
        app_httpd_log("json_tokener_parse error");
        goto exit;
    }
    json_object_object_foreach(ReceivedJsonCache, key, val)
    {
        if (!strcmp(key, "eland_id"))
        {
            device_state->eland_id = json_object_get_int(val);
            netclock_des_g->eland_id = device_state->eland_id;
            app_httpd_log("eland_id:%ld", device_state->eland_id);
        }
        else if (!strcmp(key, "serial_number"))
        {
            memset(device_state->serial_number, 0, sizeof(device_state->serial_number));
            sprintf(device_state->serial_number, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->serial_number, device_state->serial_number, sizeof(device_state->serial_number));
        }
        else if (!strcmp(key, "eland_name"))
        {
            memset(device_state->eland_name, 0, sizeof(device_state->eland_name));
            sprintf(device_state->eland_name, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->eland_name, device_state->eland_name, sizeof(device_state->eland_name));
        }
        else if (!strcmp(key, "timezone_offset_sec"))
        {
            device_state->timezone_offset_sec = json_object_get_int(val);
            netclock_des_g->timezone_offset_sec = device_state->timezone_offset_sec;
        }
        else if (!strcmp(key, "user_id"))
        {
            memset(device_state->user_id, 0, sizeof(device_state->user_id));
            sprintf(device_state->user_id, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->user_id, device_state->user_id, sizeof(device_state->user_id));
        }
        else if (!strcmp(key, "dhcp_enabled"))
        {
            device_state->dhcp_enabled = json_object_get_int(val);
            netclock_des_g->dhcp_enabled = device_state->dhcp_enabled;
        }
        else if (!strcmp(key, "ip_address"))
        {
            memset(device_state->ip_address, 0, ip_address_Len);
            sprintf(device_state->ip_address, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->ip_address, device_state->ip_address, sizeof(device_state->ip_address));
        }
        else if (!strcmp(key, "subnet_mask"))
        {
            memset(device_state->subnet_mask, 0, ip_address_Len);
            sprintf(device_state->subnet_mask, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->subnet_mask, device_state->subnet_mask, sizeof(device_state->subnet_mask));
        }
        else if (!strcmp(key, "default_gateway"))
        {
            memset(device_state->default_gateway, 0, ip_address_Len);
            sprintf(device_state->default_gateway, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->default_gateway, device_state->default_gateway, sizeof(device_state->default_gateway));
        }
        else if (!strcmp(key, "primary_dns"))
        {
            memset(device_state->primary_dns, 0, ip_address_Len);
            sprintf(device_state->primary_dns, "%s", json_object_get_string(val));
            memcpy(netclock_des_g->primary_dns, device_state->primary_dns, sizeof(device_state->primary_dns));
        }
        else if (!strcmp(key, "area_code"))
        {
            device_state->area_code = json_object_get_int(val);
            netclock_des_g->area_code = device_state->area_code;
        }
    }
    /***initialization to default value***/
    netclock_des_g->time_display_format = DEFAULT_TIME_FORMAT;
    netclock_des_g->brightness_normal = DEFAULT_BRIGHTNESS_NORMAL;
    netclock_des_g->brightness_night = DEFAULT_BRIGHTNESS_NIGHT;
    netclock_des_g->led_normal = DEFAULT_LED_NORMAL;
    netclock_des_g->led_night = DEFAULT_LED_NIGHT;
    netclock_des_g->notification_volume_normal = DEFAULT_VOLUME_NORMAL;
    netclock_des_g->notification_volume_night = DEFAULT_VOLUME_NIGHT;
    netclock_des_g->night_mode_enabled = DEFAULT_NIGHT_MODE_ENABLE;
    strncpy(netclock_des_g->night_mode_begin_time, DEFAULT_NIGHT_BEGIN, strlen(DEFAULT_NIGHT_BEGIN));
    strncpy(netclock_des_g->night_mode_end_time, DEFAULT_NIGHT_END, strlen(DEFAULT_NIGHT_END));
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    device_state->IsAlreadySet = true;
    device_state->IsActivate = false;
    /**refresh flash inside**/
    mico_system_context_update(mico_system_context_get());
    free_json_obj(&ReceivedJsonCache); //只要free最顶层的那个就可以
    return kNoErr;
exit:
    free_json_obj(&ReceivedJsonCache);
    return kGeneralErr;
}

static void eland_wifi_parameter_save(void)
{
    mico_Context_t *context = NULL;
    context = mico_system_context_get();
    device_state->IsActivate = true;
    app_httpd_log("save wifi para,update flash"); //save
    strncpy(context->micoSystemConfig.ssid, netclock_des_g->Wifissid, ElandSsid_Len);
    strncpy(context->micoSystemConfig.key, netclock_des_g->WifiKey, ElandKey_Len);
    strncpy(context->micoSystemConfig.user_key, netclock_des_g->WifiKey, ElandKey_Len);
    context->micoSystemConfig.keyLength = strlen(context->micoSystemConfig.key);
    context->micoSystemConfig.user_keyLength = strlen(context->micoSystemConfig.key);
    context->micoSystemConfig.channel = 0;
    memset(context->micoSystemConfig.bssid, 0x0, 6);
    context->micoSystemConfig.security = SECURITY_TYPE_AUTO;
    if (netclock_des_g->dhcp_enabled == 1)
        context->micoSystemConfig.dhcpEnable = DHCP_Client; /* Fetch Ip address from DHCP server */
    else if (netclock_des_g->dhcp_enabled == 0)
    {
        context->micoSystemConfig.dhcpEnable = DHCP_Disable; /* Fetch Ip address from DHCP server */
        memcpy(context->micoSystemConfig.localIp, netclock_des_g->ip_address, 16);
        memcpy(context->micoSystemConfig.netMask, netclock_des_g->subnet_mask, 16);
        memcpy(context->micoSystemConfig.gateWay, netclock_des_g->default_gateway, 16);
        memcpy(context->micoSystemConfig.dnsServer, netclock_des_g->primary_dns, 16);
    }
    context->micoSystemConfig.configured = allConfigured;
    mico_system_context_update(context);
    mico_rtos_thread_msleep(400);
}
