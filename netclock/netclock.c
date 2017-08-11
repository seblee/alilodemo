#include "hal_alilo_rabbit.h"
#include "netclockconfig.h"
#include "netclock.h"
#include "netclock_httpd.h"
#include "json_c/json.h"
#include "netclock_wifi.h"
#include "mico.h"
#include "netclock_uart.h"
#define Eland_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

ELAND_DES_S *netclock_des_g = NULL;
static bool NetclockInitSuccess = false;

static mico_semaphore_t NetclockInitCompleteSem = NULL;
static mico_semaphore_t WifiConnectSem = NULL;

json_object *ElandJsonData = NULL;
json_object *DeviceJsonData = NULL;
json_object *AlarmJsonData = NULL;

extern void PlatformEasyLinkButtonLongPressedCallback(void);
OSStatus netclock_desInit(void)
{
    OSStatus err = kGeneralErr;
    //    LinkStatusTypeDef *WifiStatus;
    //    msg_wify_queue received;
    //    msg_queue my_message;
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
    require_action_string(netclock_des_g != NULL, exit, err = kGeneralErr, "[ERROR]fog_des_g is NULL!!!");
    err = kNoErr;
exit:
    return err;
}
//闹钟初始化
static void NetclockInit(mico_thread_arg_t arg)
{
    UNUSED_PARAMETER(arg);
    OSStatus err = kGeneralErr;

    require_string(get_wifi_status() == true, exit, "wifi is not connect");
    /*初始化互斥信号量*/
    //err = mico_rtos_init_mutex(&http_send_setting_mutex);
    //require_noerr(err, exit);

    //开启http client 后台线程
    //err = start_fogcloud_http_client(); //内部有队列初始化,需要先执行 //ssl登录等
    require_noerr(err, exit);

//startsNetclockinit:

exit:
    mico_rtos_set_semaphore(&NetclockInitCompleteSem); //wait until get semaphore

    // if (err != kNoErr)
    // {
    //     stop_fog_bonjour();
    // }

    mico_rtos_delete_thread(NULL);
    return;
}
OSStatus StartNetclockService(void)
{
    OSStatus err = kGeneralErr;
    NetclockInitSuccess = false;

    err = mico_rtos_init_semaphore(&NetclockInitCompleteSem, 1);
    require(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "NetclockInit", NetclockInit, 0x1000, (uint32_t)NULL);
    require(err, exit);

    mico_rtos_get_semaphore(&NetclockInitCompleteSem, MICO_WAIT_FOREVER);
exit:
    return err;
}
//检查从flash读出的数据是否正确，不正确就恢复出厂设置
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
                            ElandParameterConfiguration, 0x2000, (uint32_t)NULL);
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

    json_object_object_add(ElandJsonData, "device", DeviceJsonData);

    // peripherals_member = json_object_new_object();
    // require_action_string(peripherals_member, exit, err = kNoMemoryErr, "create peripherals_member error!");
    // json_object_object_add(peripherals_member, "ElandData", ElandJsonData);

    generate_data = json_object_to_json_string(ElandJsonData);
    generate_data_len = strlen(generate_data);
    memcpy(OutputJsonstring, generate_data, generate_data_len);

exit:
    free_json_obj(&ElandJsonData);
    free_json_obj(&DeviceJsonData);
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
static void micoNotify_ConnectFailedHandler(OSStatus err, void *inContext)
{
    Eland_log("join Wlan failed Err: %d", err);
    mico_rtos_set_semaphore(&WifiConnectSem);
}

OSStatus wifiConnectADV(void)
{
    OSStatus err = kNoErr;
    network_InitTypeDef_adv_st wNetConfigAdv;

    err = micoWlanSuspend();
    require_noerr(err, exit);

    mico_rtos_init_semaphore(&WifiConnectSem, 1);

    /* Register user function when wlan connection is faile in one attempt */
    err = mico_system_notify_register(mico_notify_WIFI_CONNECT_FAILED, (void *)micoNotify_ConnectFailedHandler, NULL);
    require_noerr(err, exit);
    /* Initialize wlan parameters */
    memset(&wNetConfigAdv, 0x0, sizeof(wNetConfigAdv));
    strcpy((char *)wNetConfigAdv.ap_info.ssid, netclock_des_g->Wifissid); /* wlan ssid string */
    strcpy((char *)wNetConfigAdv.key, netclock_des_g->WifiKey);           /* wlan key string or hex data in WEP mode */
    wNetConfigAdv.key_len = strlen(netclock_des_g->WifiKey);              /* wlan key length */
    wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;                  /* wlan security mode */
    wNetConfigAdv.ap_info.channel = 0;                                    /* Select channel automatically */
    wNetConfigAdv.dhcpMode = DHCP_Client;                                 /* Fetch Ip address from DHCP server */
    wNetConfigAdv.wifi_retry_interval = 100;                              /* Retry interval after a failure connection */

    /* Connect Now! */
    Eland_log("connecting to %s...", wNetConfigAdv.ap_info.ssid);
    micoWlanStartAdv(&wNetConfigAdv);

    mico_rtos_get_semaphore(&WifiConnectSem, 3000); //等待3秒如果未连接 返回错误

    err = kGeneralErr;
exit:
    return err;
}
