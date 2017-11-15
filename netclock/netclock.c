#include "hal_alilo_rabbit.h"
#include "netclockconfig.h"
#include "netclock.h"
#include "netclock_httpd.h"
#include "json_c/json.h"
#include "netclock_wifi.h"
#include "mico.h"
#include "netclock_uart.h"
#include "netclock_ota.h"
#include "eland_http_client.h"
#include "SocketUtils.h"
#include "flash_kh25.h"

#define Eland_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

ELAND_DES_S *netclock_des_g = NULL;
static bool NetclockInitSuccess = false;

static mico_semaphore_t elandservicestart = NULL;
//http client mutex
static mico_mutex_t http_send_setting_mutex = NULL;
json_object *ElandJsonData = NULL;
json_object *DeviceJsonData = NULL;
json_object *AlarmJsonData = NULL;

//发送http请求
//发送http请求
OSStatus eland_push_http_req_mutex(ELAND_HTTP_METHOD method,                           //POST 或者 GET
                                   ELAND_HTTP_REQUEST_TYPE request_type,               //eland request type
                                   char *request_uri,                                  //uri
                                   char *host_name,                                    //host
                                   char *http_body,                                    //BODY
                                   ELAND_HTTP_RESPONSE_SETTING_S *user_http_response); //response 指針

//设置请求参数
OSStatus set_eland_http_request(ELAND_HTTP_REQUEST_SETTING_S *http_req, //request 指針
                                ELAND_HTTP_METHOD method,               //請求方式 POST/GET
                                ELAND_HTTP_REQUEST_TYPE request_type,   //eland request type
                                char *request_uri,                      //URI 路徑
                                char *host_name,                        //host 主機
                                char *http_body,                        //數據 body
                                uint32_t http_req_id);                  //請求 id

//發送 http請求信號量
static OSStatus push_http_req_to_queue(ELAND_HTTP_REQUEST_SETTING_S *http_req);
//从队列中获取请求
OSStatus get_http_res_from_queue(ELAND_HTTP_RESPONSE_SETTING_S *http_res,
                                 ELAND_HTTP_REQUEST_TYPE request_type, //eland request type
                                 uint32_t id);
//eland init 線程
static void NetclockInit(mico_thread_arg_t arg);
//eland 情報登錄
static OSStatus eland_device_info_login(void);
//eland 情報取得
static OSStatus eland_device_info_get(void);
//eland 下載數據
static OSStatus alarm_sound_download(void);
//eland 鬧鐘開始通知
static OSStatus eland_alarm_start_notice(void);
//eland 鬧鐘OFF履歷登錄
static OSStatus alarm_off_record_entry(void);
//eland OTA 固件升級開始通知
static OSStatus ota_update_start_notice(void);

static uint32_t eland_http_request_count = 0;
static uint32_t eland_http_success_count = 0;

//生成一个http回话id
static uint32_t
generate_http_session_id(void)
{
    static uint32_t id = 1;

    return id++;
}
OSStatus netclock_desInit(void)
{
    OSStatus err = kGeneralErr;
    if (false == CheckNetclockDESSetting())
    {
        //结构体覆盖
        Eland_log("[NOTICE]recovery settings!!!!!!!");
        err = Netclock_des_recovery();
        require_noerr(err, exit);
    }
    Eland_log("local firmware version:%s", Eland_Firmware_Version);
    return kNoErr;
exit:
    Eland_log("netclock_des_g init err");
    if (netclock_des_g != NULL)
    {
        memset(netclock_des_g, 0, sizeof(ELAND_DES_S));
        mico_system_context_update(mico_system_context_get());
    }
    return kGeneralErr;
}
/**/
OSStatus InitNetclockService(void)
{
    OSStatus err = kGeneralErr;
    mico_Context_t *context = NULL;

    context = mico_system_context_get();
    require_action_string(context != NULL, exit, err = kGeneralErr, "[ERROR]context is NULL!!!");

    netclock_des_g = (ELAND_DES_S *)mico_system_context_get_user_data(context);
    require_action_string(netclock_des_g != NULL, exit, err = kGeneralErr, "[ERROR]netclock_des_g is NULL!!!");
    err = kNoErr;
exit:
    return err;
}
//啟動Netclock net work
OSStatus start_eland_service(void)
{
    OSStatus err = kGeneralErr;

    NetclockInitSuccess = false;
    err = mico_rtos_init_semaphore(&elandservicestart, 1);
    require_noerr(err, exit);

    /* create eland init thread */
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "eland_init", NetclockInit, 0x800, (uint32_t)NULL);
    require_noerr_string(err, exit, "ERROR: Unable to start the eland_init thread");

    mico_rtos_get_semaphore(&elandservicestart, MICO_WAIT_FOREVER); //wait until get semaphore

exit:
    mico_rtos_deinit_semaphore(&elandservicestart); //銷毀 不再使用的信號量

    if (NetclockInitSuccess == true)
    {
        Eland_log("eland init success!");
        err = kNoErr;
    }
    else
    {
        Eland_log("eland init failure!");
        err = kGeneralErr;
    }
    return err;
}
//闹钟初始化
static void NetclockInit(mico_thread_arg_t arg)
{
    UNUSED_PARAMETER(arg);
    OSStatus err = kGeneralErr;

    require_string(get_wifi_status() == true, exit, "wifi is not connect");
    /*初始化互斥信号量*/
    err = mico_rtos_init_mutex(&http_send_setting_mutex);
    require_noerr(err, exit);

    //开启http client 后台线程
    err = start_client_serrvice(); //内部有队列初始化,需要先执行 //ssl登录等
    require_noerr(err, exit);

    //startsNetclockinit:

    //1.eland 情报同步到雲端(同步设备版本、硬件型号到云端)
    //err = eland_device_info_login(); //暫時做測試用
    require_noerr(err, exit);

    //2.eland 情報從雲端獲取
    //err = eland_device_info_get(); //暫時做測試用
    require_noerr(err, exit);

    //3.eland test  下載音頻到flash
    err = alarm_sound_download(); //暫時做測試用
    require_noerr(err, exit);

    //4.eland 鬧鐘開始通知
    //err = eland_alarm_start_notice(); //暫時做測試用
    require_noerr(err, exit);

    //5.eland 鬧鐘OFF履歷登錄
    //err = alarm_off_record_entry(); //暫時做測試用
    require_noerr(err, exit);

    //6.eland OTA開始通知
    //err = ota_update_start_notice(); //暫時做測試用
    require_noerr(err, exit);

    NetclockInitSuccess = true;

exit:
    if (NetclockInitSuccess == true)
        mico_rtos_set_semaphore(&elandservicestart); //wait until get semaphore

    // if (err != kNoErr)
    // {
    //     _bonjour();
    // }

    mico_rtos_delete_thread(NULL);
    return;
}

//检查从flash读出的数据是否正确,不正确就恢复出厂设置
bool CheckNetclockDESSetting(void)
{
    char firmware[64] = {0};
    sprintf(firmware, "%s", Eland_Firmware_Version);
    /*check Eland_ID*/
    if (0 != strcmp(netclock_des_g->eland_id, Eland_ID))
    {
        Eland_log("ElandID change!");
        return false;
    }
    /*check ElandFirmwareVersion*/
    if (0 != strcmp(netclock_des_g->firmware_version, firmware))
    {
        Eland_log("ElandFirmwareVersion changed!");
        return false;
    }

    /*check MAC Address*/
    if (strlen(netclock_des_g->mac_address) != DEVICE_MAC_LEN)
    {
        Eland_log("MAC Address err!");
        return false;
    }

    if (netclock_des_g->IsActivate == true) //设备已激活
    {
        /*check UserID */
        if ((netclock_des_g->timezone_offset_sec < Timezone_offset_sec_Min) ||
            (netclock_des_g->timezone_offset_sec > Timezone_offset_sec_Max))
        {
            Eland_log("ElandZoneOffset is error!");
            return false;
        }
    }
    return true;
}
OSStatus Netclock_des_recovery(void)
{
    unsigned char mac[10] = {0};

    memset(netclock_des_g, 0, sizeof(ELAND_DES_S)); //全局变量清空
    mico_system_context_update(mico_system_context_get());

    netclock_des_g->IsActivate = false;
    netclock_des_g->IsHava_superuser = false;
    netclock_des_g->IsRecovery = false;

    memcpy(netclock_des_g->eland_id, Eland_ID, strlen(Eland_ID));                                     //Eland唯一识别的ID
    memcpy(netclock_des_g->firmware_version, Eland_Firmware_Version, strlen(Eland_Firmware_Version)); //设置设备软件版本号
    wlan_get_mac_address(mac);                                                                        //MAC地址
    memset(netclock_des_g->mac_address, 0, sizeof(netclock_des_g->mac_address));                      //MAC地址
    sprintf(netclock_des_g->mac_address, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Eland_log("device_mac:%s", netclock_des_g->mac_address);

    mico_system_context_update(mico_system_context_get());

    return kNoErr;
}
/****start softAP event wait******/
void start_HttpServer_softAP_thread(void)
{

    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Para_config",
                            ElandParameterConfiguration, 0x1000, (uint32_t)NULL);
}

void ElandParameterConfiguration(mico_thread_arg_t args)
{
    msg_queue my_message;
    flagHttpdServerAP = 1;
    while (1)
    {
        /*********wait for softap event***********/
        mico_rtos_get_semaphore(&httpServer_softAP_event_Sem, MICO_WAIT_FOREVER);
        flagHttpdServerAP = 2;
        Eland_log("Soft_ap_Server");
        Start_wifi_Station_SoftSP_Thread(Soft_AP);
        my_message.type = Queue_ElandState_type;
        my_message.value = ElandAPStatus;
        mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);

        /* start http server thread */
        Eland_httpd_start();
    }
    mico_rtos_delete_thread(NULL);
}
//获取网络连接状态
bool get_wifi_status(void)
{
    LinkStatusTypeDef link_status;

    memset(&link_status, 0, sizeof(link_status));

    micoWlanGetLinkStatus(&link_status);

    return (bool)(link_status.is_connected);
}

const char Eland_Data[11] = {"ElandData"};

OSStatus InitUpLoadData(char *OutputJsonstring)
{
    OSStatus err = kNoErr;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    if ((ElandJsonData != NULL) || (AlarmJsonData != NULL) || (DeviceJsonData != NULL))
    {
        free_json_obj(&ElandJsonData);
        free_json_obj(&DeviceJsonData);
        free_json_obj(&AlarmJsonData);
    }
    ElandJsonData = json_object_new_object();
    require_action_string(ElandJsonData, exit, err = kNoMemoryErr, "create ElandJsonData object error!");
    DeviceJsonData = json_object_new_object();
    require_action_string(DeviceJsonData, exit, err = kNoMemoryErr, "create DeviceJsonData object error!");

    Eland_log("Begin add DeviceJsonData object");
    json_object_object_add(DeviceJsonData, "eland_id", json_object_new_string(Eland_ID));
    json_object_object_add(DeviceJsonData, "user_id", json_object_new_string(netclock_des_g->user_id));
    json_object_object_add(DeviceJsonData, "eland_name", json_object_new_string(netclock_des_g->eland_name));
    json_object_object_add(DeviceJsonData, "timezone_offset_sec", json_object_new_int(netclock_des_g->timezone_offset_sec));
    json_object_object_add(DeviceJsonData, "serial_number", json_object_new_string(netclock_des_g->serial_number));
    json_object_object_add(DeviceJsonData, "firmware_version", json_object_new_string(Eland_Firmware_Version));
    json_object_object_add(DeviceJsonData, "mac_address", json_object_new_string(netclock_des_g->mac_address));
    json_object_object_add(DeviceJsonData, "dhcp_enabled", json_object_new_int(netclock_des_g->dhcp_enabled));
    json_object_object_add(DeviceJsonData, "ip_address", json_object_new_string(netclock_des_g->ip_address));
    json_object_object_add(DeviceJsonData, "subnet_mask", json_object_new_string(netclock_des_g->subnet_mask));
    json_object_object_add(DeviceJsonData, "default_gateway", json_object_new_string(netclock_des_g->default_gateway));
    json_object_object_add(DeviceJsonData, "time_display_format", json_object_new_int(netclock_des_g->time_display_format));
    json_object_object_add(DeviceJsonData, "brightness_normal", json_object_new_int(netclock_des_g->brightness_normal));
    json_object_object_add(DeviceJsonData, "brightness_night", json_object_new_int(netclock_des_g->brightness_night));
    json_object_object_add(DeviceJsonData, "night_mode_enabled", json_object_new_int(netclock_des_g->night_mode_enabled));
    json_object_object_add(DeviceJsonData, "night_mode_begin_time", json_object_new_string(netclock_des_g->night_mode_begin_time));
    json_object_object_add(DeviceJsonData, "night_mode_end_time", json_object_new_string(netclock_des_g->night_mode_end_time));
    json_object_object_add(DeviceJsonData, "firmware_update_download_url", json_object_new_string(netclock_des_g->firmware_update_download_url));
    json_object_object_add(DeviceJsonData, "firmware_md5", json_object_new_string(netclock_des_g->firmware_update_download_url));

    json_object_object_add(ElandJsonData, "device", DeviceJsonData);

    generate_data = json_object_to_json_string(ElandJsonData);
    require_action_string(generate_data != NULL, exit, err = kNoMemoryErr, "create generate_data string error!");
    generate_data_len = strlen(generate_data);
    memcpy(OutputJsonstring, generate_data, generate_data_len);

exit:
    free_json_obj(&ElandJsonData);
    free_json_obj(&DeviceJsonData);
    return err;
}
OSStatus init_alarm_off_record_json(char *OutputJsonstring)
{
    OSStatus err = kNoErr;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    json_object *alarm_off_record_json = NULL;
    json_object *alarmhistory = NULL;

    alarm_off_record_json = json_object_new_object();
    require_action_string(alarm_off_record_json, exit, err = kNoMemoryErr, "create alarm_off_record_json object error!");
    alarmhistory = json_object_new_object();
    require_action_string(alarmhistory, exit, err = kNoMemoryErr, "create alarmhistory object error!");

    Eland_log("Begin add alarmhistory object");
    json_object_object_add(alarmhistory, "alarm_id", json_object_new_string("0"));
    json_object_object_add(alarmhistory, "alarm_on_datetime", json_object_new_string("2017-9-18 14:43:00"));
    json_object_object_add(alarmhistory, "alarm_off_datetime", json_object_new_string("2017-9-18 14:43:44"));
    json_object_object_add(alarmhistory, "alarm_off_reason", json_object_new_int(3));
    json_object_object_add(alarmhistory, "snooze_datetime", json_object_new_string("2017-9-18 14:53:00"));

    json_object_object_add(alarm_off_record_json, "history", alarmhistory);

    generate_data = json_object_to_json_string(alarm_off_record_json);
    require_action_string(generate_data != NULL, exit, err = kNoMemoryErr, "create generate_data string error!");
    generate_data_len = strlen(generate_data);
    memcpy(OutputJsonstring, generate_data, generate_data_len);

exit:
    free_json_obj(&alarm_off_record_json);
    free_json_obj(&alarmhistory);
    return err;
}

//释放JSON对象
void free_json_obj(json_object **json_obj)
{
    if (*json_obj != NULL)
    {
        json_object_put(*json_obj); // free memory of json object
        *json_obj = NULL;
    }

    return;
}
//8.注销外设数据
void destory_upload_data(void)
{
    free_json_obj(&ElandJsonData);
    free_json_obj(&AlarmJsonData);
    return;
}
//解析接收到的数据包
OSStatus ProcessPostJson(char *InputJson)
{
    json_object *ReceivedJsonCache = NULL, *ElandJsonCache = NULL;
    if (*InputJson != '{')
    {
        Eland_log("error:received err json format data");
        goto exit;
    }
    ReceivedJsonCache = json_tokener_parse((const char *)(InputJson));
    if (ReceivedJsonCache == NULL)
    {
        Eland_log("json_tokener_parse error");
        goto exit;
    }
    json_object_object_foreach(ReceivedJsonCache, key1, val1)
    {
        if (!strcmp(key1, "ssid"))
        {
            memset(netclock_des_g->Wifissid, 0, sizeof(netclock_des_g->Wifissid));
            sprintf(netclock_des_g->Wifissid, "%s", json_object_get_string(val1));
            Eland_log("ssid = %s", netclock_des_g->Wifissid);
            if (!strncmp(netclock_des_g->Wifissid, "\0", 1))
            {
                Eland_log("Wifissid not Available");
                goto exit;
            }
        }
        else if (!strcmp(key1, "password"))
        {
            memset(netclock_des_g->WifiKey, 0, sizeof(netclock_des_g->WifiKey));
            sprintf(netclock_des_g->WifiKey, "%s", json_object_get_string(val1));
            Eland_log("password = %s", netclock_des_g->WifiKey);
            if (!strncmp(netclock_des_g->WifiKey, "\0", 1))
            {
                Eland_log("WifiKey not Available");
                goto exit;
            }
        }
    }
    Eland_log("process device");
    //解析Elanddata
    ElandJsonCache = json_object_object_get(ReceivedJsonCache, "device");
    if ((ElandJsonCache == NULL) || ((json_object_get_object(ElandJsonCache)->head) == NULL))
    {
        Eland_log("get ElandJsonCache error");
        goto exit;
    }
    //解析ElandDATA
    json_object_object_foreach(ElandJsonCache, key, val)
    {
        if (!strcmp(key, "user_id"))
        {
            memset(netclock_des_g->user_id, 0, sizeof(netclock_des_g->user_id));
            sprintf(netclock_des_g->user_id, "%s", json_object_get_string(val));
            if (!strncmp(netclock_des_g->user_id, "\0", 1))
            {
                Eland_log("user_id  = %s", netclock_des_g->user_id);
                goto exit;
            }
        }
        else if (!strcmp(key, "eland_name"))
        {
            memset(netclock_des_g->eland_name, 0, sizeof(netclock_des_g->eland_name));
            sprintf(netclock_des_g->eland_name, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "timezone_offset_sec"))
        {
            netclock_des_g->timezone_offset_sec = json_object_get_int(val);
        }
        else if (!strcmp(key, "firmware_update_download_url"))
        {
            memset(ota_url, 0, sizeof(ota_url));
            sprintf(ota_url, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "firmware_md5"))
        {
            memset(ota_md5, 0, sizeof(ota_md5));
            sprintf(ota_md5, "%s", json_object_get_string(val));
        }
    }
    free_json_obj(&ReceivedJsonCache); //只要free最顶层的那个就可以
    return kNoErr;
exit:
    free_json_obj(&ReceivedJsonCache);
    return kGeneralErr;
}

//eland 情報登錄
static OSStatus eland_device_info_login(void)
{
    OSStatus err = kGeneralErr;
    char *device_info_login_body = NULL;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

    Eland_log("web_send_Get_Request");
    device_info_login_body = malloc(1024);
    memset(device_info_login_body, 0, 1024);
    InitUpLoadData(device_info_login_body);

start_sync_status:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto start_sync_status;
    }

    Eland_log("=====> eland_device_info_login send ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(ELAND_SYNC_STATUS_METHOD, ELAND_DEVICE_INFO_LOGIN, ELAND_DEVICE_LOGIN_URI,
                                    ELAND_HTTP_DOMAIN_NAME, device_info_login_body, &user_http_res);
    require_noerr(err, exit);

    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("eland login successed 200");
    }
    else
    {
        Eland_log("eland login error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }
    Eland_log("request = %ld,success = %ld ",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1);
    goto start_sync_status; //兩秒重複一次

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== device_sync_status error <======");
        mico_thread_msleep(200);
        goto start_sync_status;
    }

    if (device_info_login_body != NULL) //释放资源
    {
        free(device_info_login_body);
        device_info_login_body = NULL;
    }
    return err;
}

//eland 情報取得
static OSStatus eland_device_info_get(void)
{
    OSStatus err = kGeneralErr;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));
    char *request_uri_cache;
    request_uri_cache = malloc(strlen(ELAND_DEVICE_INFO_GET_URI) + strlen("52017-09-14+19%3A45%3A47.289") + 1);
    sprintf(request_uri_cache, ELAND_DEVICE_INFO_GET_URI, "6", "2017-09-20+15%3A06%3A53.289");

DEVICE_INFO_GET_START:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto DEVICE_INFO_GET_START;
    }

    Eland_log("=====> eland_device_info_get start ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(ELAND_DEVICE_INFO_GET_METHOD, //請求方式
                                    ELAND_DEVICE_INFO_GET,        //請求類型
                                    request_uri_cache,            //URI參數
                                    ELAND_HTTP_DOMAIN_NAME,       //主機域名
                                    "",                           //request body
                                    &user_http_res);              //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("<===== eland_device_info_get end <======");
    }
    else
    {
        Eland_log("eland login error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }
    Eland_log("request = %ld,success = %ld",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1);
    goto DEVICE_INFO_GET_START; //兩秒重複一次

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== device_sync_status error <======");
        mico_thread_msleep(200);
        goto DEVICE_INFO_GET_START;
    }

    free(request_uri_cache);
    return err;
}

//eland 鬧鐘聲音取得
static OSStatus alarm_sound_download(void)
{
    OSStatus err = kGeneralErr;

    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

ALARM_SOUND_DOWNLOAD_START:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto ALARM_SOUND_DOWNLOAD_START;
    }
    Eland_log("=====> alarm_sound_download send ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(HTTP_GET,               //請求方式
                                    ELAND_ALARM_GET,        //請求類型
                                    ELAND_DOWN_LOAD_URI,    //URI參數
                                    ELAND_HTTP_DOMAIN_NAME, //主機域名
                                    "",                     //request body
                                    &user_http_res);        //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("<===== alarm_sound_download end <======");
    }
    else
    {
        Eland_log("alarm_sound_download error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }
    Eland_log("request = %ld,success = %ld",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1);
    return err;
    goto ALARM_SOUND_DOWNLOAD_START; //兩秒重複一次
exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== alarm_sound_download error <======");
        mico_thread_msleep(200);
        goto ALARM_SOUND_DOWNLOAD_START;
    }

    return err;
}
//eland 鬧鐘開始通知
static OSStatus eland_alarm_start_notice(void)
{
    OSStatus err = kGeneralErr;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));
    char *request_uri_cache;
    request_uri_cache = malloc(strlen(ELAND_ALARM_START_NOTICE_URI) + strlen("50a259f14-f7c5-4128-8dd2-7f8a5cdc4702") + 1);
    sprintf(request_uri_cache, ELAND_ALARM_START_NOTICE_URI, "5", "0a259f14-f7c5-4128-8dd2-7f8a5cdc4702");

ALARM_START_NOTICE_START:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto ALARM_START_NOTICE_START;
    }

    Eland_log("=====> eland_alarm_start_notice start ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(ELAND_ALARM_START_NOTICE_METHOD, //請求方式
                                    ELAND_ALARM_START_NOTICE,        //請求類型
                                    request_uri_cache,               //URI參數
                                    ELAND_HTTP_DOMAIN_NAME,          //主機域名
                                    "",                              //request body
                                    &user_http_res);                 //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("<===== eland_alarm_start_notice successed <======");
    }
    else
    {
        Eland_log("alarm_start_notice error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }
    Eland_log("request = %ld,success = %ld",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1); //兩秒重複一次
    goto ALARM_START_NOTICE_START;

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== alarm_start_notice error <======");
        mico_thread_msleep(200);
        goto ALARM_START_NOTICE_START;
    }
    free(request_uri_cache);
    return err;
}

//eland 鬧鐘OFF履歷登錄
static OSStatus alarm_off_record_entry(void)
{
    OSStatus err = kGeneralErr;
    char *eland_request_body_cache = NULL;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

    eland_request_body_cache = malloc(400);
    memset(eland_request_body_cache, 0, 400);
    init_alarm_off_record_json(eland_request_body_cache);

ALARM_OFF_RECORD_ENTRY_START:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto ALARM_OFF_RECORD_ENTRY_START;
    }

    Eland_log("=====> alarm_off_record_entry start ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(ELAND_ALARM_OFF_RECORD_ENTRY_METHOD, //請求方式
                                    ELAND_ALARM_OFF_RECORD_ENTRY,        //請求類型
                                    ELAND_ALARM_OFF_RECORD_ENTRY_URI,    //URI參數
                                    ELAND_HTTP_DOMAIN_NAME,              //主機域名
                                    eland_request_body_cache,            //request body
                                    &user_http_res);                     //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("<===== alarm_off_record_entry successe <======");
    }
    else
    {
        Eland_log("alarm_off_record_entry error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }

    Eland_log("request = %ld,success = %ld ",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1); //兩秒重複一次
    goto ALARM_OFF_RECORD_ENTRY_START;

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== alarm_off_record_entry error <======");
        mico_thread_msleep(200);
        goto ALARM_OFF_RECORD_ENTRY_START;
    }

    free(eland_request_body_cache);
    return err;
}
//eland OTA 固件升級開始通知
static OSStatus ota_update_start_notice(void)
{
    OSStatus err = kGeneralErr;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

OTA_UPDATE_NOTICE_START:
    while (get_wifi_status() == false)
    {
        Eland_log("https disconnect, eland_device_info_login is waitting...");
        mico_rtos_thread_sleep(2);
        err = kGeneralErr;
        goto exit;
    }
    if (get_https_connect_status() == false)
    {
        Eland_log("https_connect_status is not true");
        mico_rtos_thread_sleep(2);
        goto OTA_UPDATE_NOTICE_START;
    }

    Eland_log("=====> eland_alarm_start_notice start ======>");
    eland_http_request_count++;
    err = eland_push_http_req_mutex(ELAND_OTA_START_NOTICE_METHOD, //請求方式
                                    ELAND_OTA_START_NOTICE,        //請求類型
                                    ELAND_OTA_START_NOTICE_URI,    //URI參數
                                    ELAND_HTTP_DOMAIN_NAME,        //主機域名
                                    "",                            //request body
                                    &user_http_res);               //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        eland_http_success_count++;
        Eland_log("<===== eland_alarm_start_notice successed <======");
    }
    else
    {
        Eland_log("alarm_start_notice error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }
    Eland_log("request = %ld,success = %ld",
              eland_http_request_count, eland_http_success_count);
    //Eland_log("<===== eland_device_info_login success <======");
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    mico_rtos_thread_sleep(1); //兩秒重複一次
    goto OTA_UPDATE_NOTICE_START;

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== alarm_start_notice error <======");
        mico_thread_msleep(200);
        goto OTA_UPDATE_NOTICE_START;
    }
    return err;
}

//发送http请求
OSStatus eland_push_http_req_mutex(ELAND_HTTP_METHOD method,                          //POST 或者 GET
                                   ELAND_HTTP_REQUEST_TYPE request_type,              //eland request type
                                   char *request_uri,                                 //uri
                                   char *host_name,                                   //host
                                   char *http_body,                                   //BODY
                                   ELAND_HTTP_RESPONSE_SETTING_S *user_http_response) //response 指針
{
    OSStatus err = kGeneralErr;
    int32_t id = 0;
    ELAND_HTTP_REQUEST_SETTING_S user_http_req;
    ELAND_HTTP_RESPONSE_SETTING_S *eland_http_response_temp_p = NULL;

    mico_rtos_lock_mutex(&http_send_setting_mutex); //这个锁 锁住的资源比较多

    if (false == mico_rtos_is_queue_empty(&eland_http_response_queue))
    { //queue满了  在发生错误的情况下 可能导致 eland_http_response_queue 里面有数据
        Eland_log("[error]want send http request, but eland_http_response_queue is full");
        err = mico_rtos_pop_from_queue(&eland_http_response_queue, &eland_http_response_temp_p, 10); //弹出数据
        require_noerr_string(err, exit, "mico_rtos_pop_from_queue() error");

        if (eland_http_response_temp_p->eland_response_body != NULL)
        {
            free(eland_http_response_temp_p->eland_response_body);
            eland_http_response_temp_p->eland_response_body = NULL;
        }

        eland_http_response_temp_p = NULL;
    }

    id = generate_http_session_id();

    memset(&user_http_req, 0, sizeof(user_http_req));
    err = set_eland_http_request(&user_http_req, method, request_type, request_uri, host_name, http_body, id);
    require_noerr(err, exit);

    err = push_http_req_to_queue(&user_http_req); //发送请求
    require_noerr(err, exit);

    err = get_http_res_from_queue(user_http_response, request_type, id); //等待返回结果
    require_noerr(err, exit);

exit:
    mico_rtos_unlock_mutex(&http_send_setting_mutex); //锁必须要等到response队列返回之后才能释放

    return err;
}

//设置请求参数
OSStatus set_eland_http_request(ELAND_HTTP_REQUEST_SETTING_S *http_req, //request 指針
                                ELAND_HTTP_METHOD method,               //請求方式 POST/GET
                                ELAND_HTTP_REQUEST_TYPE request_type,   //eland request type
                                char *request_uri,                      //URI 路徑
                                char *host_name,                        //host 主機
                                char *http_body,                        //數據 body
                                uint32_t http_req_id)                   //請求 id
{
    memset(http_req, 0, sizeof(ELAND_HTTP_REQUEST_SETTING_S));

    http_req->method = method;
    http_req->eland_request_type = request_type;

    if (strlen(request_uri) > HTTP_REQUEST_REQ_URI_MAX_LEN)
    {
        Eland_log("request_uri is error!");
        memset(http_req, 0, sizeof(ELAND_HTTP_REQUEST_SETTING_S));
        return kGeneralErr;
    }

    if (strlen(host_name) > HTTP_REQUEST_HOST_NAME_MAX_LEN)
    {
        Eland_log("host_name is error!");
        memset(http_req, 0, sizeof(ELAND_HTTP_REQUEST_SETTING_S));
        return kGeneralErr;
    }

    if (http_body != NULL)
    {
        if (strlen(http_body) > HTTP_REQUEST_BODY_MAX_LEN)
        {
            Eland_log("http_body is too long!");
            memset(http_req, 0, sizeof(ELAND_HTTP_REQUEST_SETTING_S));
            return kGeneralErr;
        }
    }

    memcpy(http_req->request_uri, request_uri, strlen(request_uri));
    memcpy(http_req->host_name, host_name, strlen(host_name));

    if (http_body != NULL)
    {
        http_req->http_body = malloc(strlen(http_body) + 1);
        if (http_req->http_body == NULL)
        {
            Eland_log("http body malloc error");
            return kNoMemoryErr;
        }

        memset(http_req->http_body, 0, strlen(http_body) + 1); //清空申请的缓冲区
        memcpy(http_req->http_body, http_body, strlen(http_body));
    }

    http_req->http_req_id = http_req_id;

    return kNoErr;
}
//發送 http請求信號量
static OSStatus push_http_req_to_queue(ELAND_HTTP_REQUEST_SETTING_S *http_req)
{
    OSStatus err = kGeneralErr;

    if (mico_rtos_is_queue_full(&eland_http_request_queue) == true)
    {
        Eland_log("eland_http_request_queue is full!");
        return kGeneralErr;
    }

    err = mico_rtos_push_to_queue(&eland_http_request_queue, &http_req, 10);
    if (err != kNoErr)
    {
        if (http_req->http_body != NULL)
        {
            free(http_req->http_body);
            http_req->http_body = NULL;
        }
        Eland_log("push req to eland_http_request_queue error");
    }

    return err;
}

//从队列中获取请求
OSStatus get_http_res_from_queue(ELAND_HTTP_RESPONSE_SETTING_S *http_res,
                                 ELAND_HTTP_REQUEST_TYPE request_type, //eland request type
                                 uint32_t id)
{
    OSStatus err = kGeneralErr;
    uint32_t res_body_len = 0;
    ELAND_HTTP_RESPONSE_SETTING_S *eland_http_res_p = NULL;
    ELAND_HTTP_REQUEST_SETTING_S *eland_http_req = NULL;

    memset(http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

    err = mico_rtos_pop_from_queue(&eland_http_response_queue, &eland_http_res_p, MICO_WAIT_FOREVER); // WAIT_HTTP_RES_MAX_TIME);
    if (err != kNoErr)
    {
        http_res->send_status = HTTP_CONNECT_ERROR;
        Eland_log("mico_rtos_pop_from_queue() timeout!!!");
        if (mico_rtos_is_queue_full(&eland_http_request_queue) == true)
        {
            err = mico_rtos_pop_from_queue(&eland_http_request_queue, &eland_http_req, 10);
            if (err == kNoErr)
            {
                if (eland_http_req->http_body != NULL)
                {
                    free(eland_http_req->http_body);
                    eland_http_req->http_body = NULL;
                }
                Eland_log("push one req from queue!");
            }
        }
        return kGeneralErr;
    }

    require_action_string((eland_http_res_p->http_res_id == id), exit, err = kIDErr, "response id is error"); //检查ID

    require_action_string((eland_http_res_p->send_status == HTTP_RESPONSE_SUCCESS), exit, err = kStateErr, "send_state is not success"); //检查是否成功

    res_body_len = strlen(eland_http_res_p->eland_response_body);

    if ((res_body_len == 0) && (request_type != ELAND_ALARM_GET))
    {
        Eland_log("[error]get data is len is 0");
        err = kGeneralErr;
        goto exit;
    }
    // Eland_log("--------------------------------------------------------");
    // Eland_log("eland_response_body:%s", eland_http_res_p->eland_response_body);
    // Eland_log("--------------------------------------------------------");
    if ((request_type == ELAND_DEVICE_INFO_GET) && //只在login時候 做JSON格式检查
        ((eland_http_res_p->eland_response_body[0] != '{') ||
         eland_http_res_p->eland_response_body[res_body_len - 1] != '}'))
    {
        Eland_log("[error]get data is not JSON format!");
        err = kGeneralErr;
        goto exit;
    }

    memcpy(http_res, eland_http_res_p, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

exit:
    if (err != kNoErr)
    { //copy send_status and status_code
        http_res->send_status = eland_http_res_p->send_status;
        http_res->status_code = eland_http_res_p->status_code;
    }

    if (err == kIDErr)
    {
        Eland_log("requese id:%ld, rseponse id:%ld", id, eland_http_res_p->http_res_id);
    }
    else if (err == kStateErr)
    {
        Eland_log("send_state:%d, state_code:%ld", eland_http_res_p->send_status, eland_http_res_p->status_code);
    }

    return err;
}

OSStatus Eland_Rtc_Init(void)
{
    OSStatus status = kNoErr;
    mico_rtc_time_t cur_time = {0};
    cur_time.year = 17; //设置时间
    cur_time.month = 11;
    cur_time.date = 15;
    cur_time.weekday = 4;
    cur_time.hr = 15;
    cur_time.min = 10;
    cur_time.sec = 7;
    status = MicoRtcSetTime(&cur_time); //初始化 RTC 时钟的时间
    return status;
}
