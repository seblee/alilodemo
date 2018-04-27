/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-12-27 15:50:32
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-22 11:01:44
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
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
#include "eland_sound.h"
#include "eland_tcp.h"

/* Private define ------------------------------------------------------------*/
#define CONFIG_ELAND_DEBUG
#ifdef CONFIG_ELAND_DEBUG
#define Eland_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)
#else
#define Eland_log(...)
#endif /* ! CONFIG_ELAND_DEBUG */
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
ELAND_DES_S *netclock_des_g = NULL;
_ELAND_DEVICE_t *device_state = NULL;

//http client mutex
mico_mutex_t http_send_setting_mutex = NULL;
json_object *ElandJsonData = NULL;
json_object *DeviceJsonData = NULL;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

OSStatus netclock_desInit(void)
{
    OSStatus err = kGeneralErr;
    mico_Context_t *context = NULL;
    /*start init system context*/
    context = mico_system_context_init(sizeof(_ELAND_DEVICE_t));

    device_state = (_ELAND_DEVICE_t *)mico_system_context_get_user_data(context);
    require_action_string(device_state != NULL, exit, err = kGeneralErr, "[ERROR]device_state is NULL!!!");

    netclock_des_g = (ELAND_DES_S *)calloc(1, sizeof(ELAND_DES_S) + 1);
    require_action_string(netclock_des_g != NULL, exit, err = kGeneralErr, "[ERROR]netclock_des_g is NULL!!!");

    err = mico_rtos_init_mutex(&netclock_des_g->des_mutex);
    require_noerr(err, exit);
    mico_rtos_lock_mutex(&netclock_des_g->des_mutex);
    //数据结构体初始化
    err = CheckNetclockDESSetting();
    require_noerr(err, exit);

    /* Start MiCO system functions according to mico_config.h*/
    err = mico_system_init(context);
    require_noerr(err, exit);

    eland_get_mac(netclock_des_g->mac_address);

    Eland_log("local firmware version:%s", FIRMWARE_REVISION);
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    return kNoErr;
exit:
    if (device_state != NULL)
    {
        memset(device_state, 0, sizeof(ELAND_DES_S));
        mico_system_context_update(mico_system_context_get());
    }
    if (netclock_des_g != NULL)
    {
        free(netclock_des_g);
        memset(netclock_des_g, 0, sizeof(ELAND_DES_S));
    }
    Eland_log("netclock_des_g init err");
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    return kGeneralErr;
}

//啟動Netclock net work
OSStatus start_eland_service(void)
{
    OSStatus err = kGeneralErr;

    err = mico_rtos_init_mutex(&http_send_setting_mutex);
    require_noerr(err, exit);

exit:
    Eland_log(" err = %d", err);
    return err;
}

//检查从flash读出的数据是否正确,不正确就恢复出厂设置
OSStatus CheckNetclockDESSetting(void)
{
    OSStatus err = kGeneralErr;
/*check Eland_ID*/
start_Check:
    Eland_log("CheckNetclockDESSetting");
    if (device_state->IsAlreadySet == true) //have already set factory info
    {
        if ((device_state->eland_id == 0) || (device_state->eland_id > 999999))
        {
            Eland_log("eland_id wrong");
            device_state->IsAlreadySet = 0;
            goto start_Check;
        }
        if (strlen(device_state->serial_number) == 0)
        {
            Eland_log("serial_number wrong");
            goto exit;
        }

        if (device_state->IsActivate == true) //设备已激活
        {
            Eland_log("IsActivate");
            if (strlen(device_state->user_id) == 0)
            {
                device_state->IsActivate = false;
                Eland_log("user_id wrong");
                goto exit;
            }
        }
        else
        {
            mico_Context_t *context = NULL;
            Eland_log("Is not Activate clear the wifi para");
            context = mico_system_context_get();
            if (context->micoSystemConfig.configured != unConfigured)
            {
                Eland_log("Is not Activate clear the wifi para");
                context->micoSystemConfig.configured = unConfigured;
                mico_system_context_update(context);
            }
        }
    }
    else
    {
        Eland_log("device not set");
        memset(device_state, 0, sizeof(_ELAND_DEVICE_t));
        device_state->eland_id = 6;
        sprintf(device_state->serial_number, "AM1A8%06ld", device_state->eland_id);
        device_state->IsAlreadySet = true;

        Eland_log("eland_id %ld", device_state->eland_id);
        Eland_log("serial_number %s", device_state->serial_number);
        goto start_Check;
    }
    /***initialize by device flash***/
    netclock_des_g->eland_id = device_state->eland_id; // des_g_Temp.eland_id; //Eland唯一识别的ID
    memcpy(netclock_des_g->user_id, device_state->user_id, user_id_len);
    memcpy(netclock_des_g->serial_number, device_state->serial_number, serial_number_len);
    memcpy(netclock_des_g->eland_name, device_state->eland_name, eland_name_Len);
    /***initialize by device flash***/
    netclock_des_g->timezone_offset_sec = device_state->timezone_offset_sec;

    if ((device_state->area_code > 142) || (device_state->area_code == 0))
        netclock_des_g->area_code = 43;
    else
        netclock_des_g->area_code = device_state->area_code;
    memcpy(netclock_des_g->firmware_version, FIRMWARE_REVISION, strlen(FIRMWARE_REVISION)); //设置设备软件版本号
    netclock_des_g->dhcp_enabled = device_state->dhcp_enabled;
    memcpy(netclock_des_g->ip_address, device_state->ip_address, ip_address_Len);
    memcpy(netclock_des_g->subnet_mask, device_state->subnet_mask, ip_address_Len);
    memcpy(netclock_des_g->default_gateway, device_state->default_gateway, ip_address_Len);
    memcpy(netclock_des_g->primary_dns, device_state->primary_dns, ip_address_Len);
    /***initialization to default value***/
    netclock_des_g->time_display_format = 1;
    netclock_des_g->brightness_normal = 80;
    netclock_des_g->brightness_night = 20;
    netclock_des_g->led_normal = 80;
    netclock_des_g->led_night = 20;
    netclock_des_g->notification_volume_normal = 40;
    netclock_des_g->notification_volume_night = 10;
    netclock_des_g->night_mode_enabled = 0;
    strncpy(netclock_des_g->night_mode_begin_time, "22:00:00", Time_Format_Len);
    strncpy(netclock_des_g->night_mode_end_time, "06:00:00", Time_Format_Len);

    // Eland_log("area_code %d", offsetof(_ELAND_DEVICE_t, area_code));
    // Eland_log("sizeof %d", sizeof(_ELAND_DEVICE_t));

    return kNoErr;
exit:
    err = Netclock_des_recovery();
    require_noerr(err, exit);
    return kGeneralErr;
}
void eland_get_mac(char *mac_address)
{
    unsigned char mac[10] = {0};
    wlan_get_mac_address(mac); //MAC地址
    memset(mac_address, 0, mac_address_Len);
    sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Eland_log("mac_address:%s", mac_address);
}

OSStatus Netclock_des_recovery(void)
{
    _ELAND_DEVICE_t device_temp;
    mico_Context_t *context = NULL;
    /***clear netclock_des_g***/
    memset(netclock_des_g, 0, sizeof(ELAND_DES_S));
    /***backup device data***/
    memcpy(&device_temp, device_state, sizeof(_ELAND_DEVICE_t));
    /***clear device data***/
    memset(device_state, 0, sizeof(_ELAND_DEVICE_t));
    /***copy para***/
    device_state->eland_id = device_temp.eland_id;
    memcpy(device_state->serial_number, device_temp.serial_number, serial_number_len);
    device_state->timezone_offset_sec = DEFAULT_TIMEZONE;
    device_state->area_code = DEFAULT_AREACODE;
    device_state->dhcp_enabled = 1;

    context = mico_system_context_get();
    /***clear wifi para***/
    if (context->micoSystemConfig.configured != unConfigured)
        context->micoSystemConfig.configured = unConfigured;
    mico_system_context_update(context);
    mico_rtos_thread_msleep(200);
    return kNoErr;
}

//获取网络连接状态
bool get_wifi_status(void)
{
    LinkStatusTypeDef link_status;

    memset(&link_status, 0, sizeof(link_status));

    micoWlanGetLinkStatus(&link_status);

    return (bool)(link_status.is_connected);
}

OSStatus InitUpLoadData(char *OutputJsonstring)
{
    OSStatus err = kNoErr;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    if ((ElandJsonData != NULL) || (DeviceJsonData != NULL))
    {
        free_json_obj(&ElandJsonData);
        free_json_obj(&DeviceJsonData);
    }
    ElandJsonData = json_object_new_object();
    require_action_string(ElandJsonData, exit, err = kNoMemoryErr, "create ElandJsonData object error!");
    DeviceJsonData = json_object_new_object();
    require_action_string(DeviceJsonData, exit, err = kNoMemoryErr, "create DeviceJsonData object error!");

    Eland_log("Begin add DeviceJsonData object");
    json_object_object_add(DeviceJsonData, "eland_id", json_object_new_int(netclock_des_g->eland_id));
    json_object_object_add(DeviceJsonData, "user_id", json_object_new_string(netclock_des_g->user_id));
    json_object_object_add(DeviceJsonData, "eland_name", json_object_new_string(netclock_des_g->eland_name));
    json_object_object_add(DeviceJsonData, "timezone_offset_sec", json_object_new_int(netclock_des_g->timezone_offset_sec));
    json_object_object_add(DeviceJsonData, "serial_number", json_object_new_string(netclock_des_g->serial_number));
    json_object_object_add(DeviceJsonData, "firmware_version", json_object_new_string(FIRMWARE_REVISION));
    json_object_object_add(DeviceJsonData, "mac_address", json_object_new_string(netclock_des_g->mac_address));
    json_object_object_add(DeviceJsonData, "dhcp_enabled", json_object_new_int(netclock_des_g->dhcp_enabled));
    json_object_object_add(DeviceJsonData, "ip_address", json_object_new_string(netclock_des_g->ip_address));
    json_object_object_add(DeviceJsonData, "subnet_mask", json_object_new_string(netclock_des_g->subnet_mask));
    json_object_object_add(DeviceJsonData, "default_gateway", json_object_new_string(netclock_des_g->default_gateway));
    json_object_object_add(DeviceJsonData, "primary_dns", json_object_new_string(netclock_des_g->primary_dns));
    json_object_object_add(DeviceJsonData, "time_display_format", json_object_new_int(netclock_des_g->time_display_format));
    json_object_object_add(DeviceJsonData, "brightness_normal", json_object_new_int(netclock_des_g->brightness_normal));
    json_object_object_add(DeviceJsonData, "brightness_night", json_object_new_int(netclock_des_g->brightness_night));
    json_object_object_add(DeviceJsonData, "led_normal", json_object_new_int(netclock_des_g->led_normal));
    json_object_object_add(DeviceJsonData, "led_night", json_object_new_int(netclock_des_g->led_night));
    json_object_object_add(DeviceJsonData, "notification_volume_normal", json_object_new_int(netclock_des_g->notification_volume_normal));
    json_object_object_add(DeviceJsonData, "notification_volume_night", json_object_new_int(netclock_des_g->notification_volume_night));
    json_object_object_add(DeviceJsonData, "night_mode_enabled", json_object_new_int(netclock_des_g->night_mode_enabled));
    json_object_object_add(DeviceJsonData, "night_mode_begin_time", json_object_new_string(netclock_des_g->night_mode_begin_time));
    json_object_object_add(DeviceJsonData, "night_mode_end_time", json_object_new_string(netclock_des_g->night_mode_end_time));
    json_object_object_add(DeviceJsonData, "area_code", json_object_new_int(netclock_des_g->area_code));

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
    return;
}
//解析接收到的数据包
OSStatus ProcessPostJson(char *InputJson)
{
    json_object *ReceivedJsonCache = NULL;
    json_object *ElandJsonCache = NULL;

    mico_rtos_lock_mutex(&netclock_des_g->des_mutex);
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
        else if (!strcmp(key1, "eland_id"))
        {
            netclock_des_g->eland_id = json_object_get_int(val1);
            Eland_log("eland_id:%ld", netclock_des_g->eland_id);
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
        else if (!strcmp(key, "area_code"))
        {
            netclock_des_g->area_code = json_object_get_int(val);
        }
        else if (!strcmp(key, "timezone_offset_sec"))
        {
            netclock_des_g->timezone_offset_sec = json_object_get_int(val);
        }
        else if (!strcmp(key, "dhcp_enabled"))
        {
            netclock_des_g->dhcp_enabled = json_object_get_int(val);
        }
        else if (!strcmp(key, "ip_address"))
        {
            memset(netclock_des_g->ip_address, 0, ip_address_Len);
            sprintf(netclock_des_g->ip_address, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "subnet_mask"))
        {
            memset(netclock_des_g->subnet_mask, 0, ip_address_Len);
            sprintf(netclock_des_g->subnet_mask, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "default_gateway"))
        {
            memset(netclock_des_g->default_gateway, 0, ip_address_Len);
            sprintf(netclock_des_g->default_gateway, "%s", json_object_get_string(val));
        }
        else if (!strcmp(key, "primary_dns"))
        {
            memset(netclock_des_g->primary_dns, 0, ip_address_Len);
            sprintf(netclock_des_g->primary_dns, "%s", json_object_get_string(val));
        }
    }
    /**refresh flash inside**/
    eland_update_flash();
    free_json_obj(&ReceivedJsonCache); //只要free最顶层的那个就可以
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    return kNoErr;
exit:
    mico_rtos_unlock_mutex(&netclock_des_g->des_mutex);
    free_json_obj(&ReceivedJsonCache);
    return kGeneralErr;
}

//eland 通信情報取得
OSStatus eland_communication_info_get(void)
{
    OSStatus err = kGeneralErr;
    json_object *ReceivedJsonCache = NULL, *ServerJsonCache = NULL;
    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));

    err = eland_http_request(ELAND_COMMUNICATION_INFO_UPDATE_METHOD, //請求方式
                             ELAND_COMMUNICATION_INFO_GET,           //請求類型
                             ELAND_COMMUNICATION_INFO_UPDATE_URI,    //URI參數
                             ELAND_HTTP_DOMAIN_NAME,                 //主機域名
                             NULL,                                   //request body
                             &user_http_res);                        //response 接收緩存
    require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        ReceivedJsonCache = json_tokener_parse((const char *)(user_http_res.eland_response_body));
        if (ReceivedJsonCache == NULL)
        {
            Eland_log("json_tokener_parse error");
            err = kGeneralErr;
            goto exit;
        }
        //解析Server data
        ServerJsonCache = json_object_object_get(ReceivedJsonCache, "server");
        if ((ServerJsonCache == NULL) || ((json_object_get_object(ServerJsonCache)->head) == NULL))
        {
            Eland_log("get ServerJsonCache error");
            goto exit;
        }
        json_object_object_foreach(ServerJsonCache, key, val)
        {
            if (!strcmp(key, "ip_address"))
            {
                memset(netclock_des_g->tcpIP_host, 0, ip_address_Len);
                sprintf(netclock_des_g->tcpIP_host, "%s", json_object_get_string(val));
            }
            else if (!strcmp(key, "port"))
            {
                netclock_des_g->tcpIP_port = json_object_get_int(val);
            }
        }
        free_json_obj(&ReceivedJsonCache);
        free_json_obj(&ServerJsonCache);
        Eland_log("<===== eland_communication_info_get end <======");
    }
    else
    {
        Eland_log("eland login error %ld", user_http_res.status_code);
        err = kGeneralErr;
        goto exit;
    }

exit:
    if (user_http_res.eland_response_body != NULL) //释放资源
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== device_sync_status error <======");
    }
    else
        SendElandStateQueue(HTTP_Get_HOST_INFO);
    return err;
}

//eland 鬧鐘聲音取得
OSStatus alarm_sound_download(__elsv_alarm_data_t *alarm, uint8_t sound_type)
{
    OSStatus err = kGeneralErr;

    ELAND_HTTP_RESPONSE_SETTING_S user_http_res;
    char uri_str[100] = {0};
    //  _sound_read_write_type_t **alarm_w_r_queue = NULL;
    uint8_t *flashdata = NULL;
    memset(&user_http_res, 0, sizeof(ELAND_HTTP_RESPONSE_SETTING_S));
    /******check sound*************/
    flashdata = malloc(10);
    mico_rtos_lock_mutex(&HTTP_W_R_struct.mutex);
    memset(HTTP_W_R_struct.alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
    if (sound_type == SOUND_FILE_SID)
        memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, &(alarm->alarm_sound_id), sizeof(alarm->alarm_sound_id));
    else if (sound_type == SOUND_FILE_VID)
        memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->voice_alarm_id, strlen(alarm->voice_alarm_id));
    else if (sound_type == SOUND_FILE_OFID)
        memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
    else if (sound_type == SOUND_FILE_DEFAULT)
        memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, ALARM_ID_OF_SIMPLE_CLOCK, strlen(ALARM_ID_OF_SIMPLE_CLOCK));
    else if (sound_type == SOUND_FILE_WEATHER_0)
    {
        if (strstr(alarm->voice_alarm_id, "00000000"))
            memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->voice_alarm_id, strlen(alarm->voice_alarm_id));
        else if (strstr(alarm->alarm_off_voice_alarm_id, "00000000"))
            memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
        else
            goto exit;
        Eland_log("00000000:%s", HTTP_W_R_struct.alarm_w_r_queue->alarm_ID);
    }
    else if (sound_type == SOUND_FILE_WEATHER_E)
    {
        if (strstr(alarm->alarm_off_voice_alarm_id, "eeeeeeee"))
            memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
        else
            goto exit;
        Eland_log("eeeeeeee:%s", HTTP_W_R_struct.alarm_w_r_queue->alarm_ID);
    }
    else if (sound_type == SOUND_FILE_WEATHER_F)
    {
        if (strstr(alarm->voice_alarm_id, "ffffffff"))
            memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->voice_alarm_id, strlen(alarm->voice_alarm_id));
        else if (strstr(alarm->alarm_off_voice_alarm_id, "ffffffff"))
            memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
        else
            goto exit;
        Eland_log("ffffffff:%s", HTTP_W_R_struct.alarm_w_r_queue->alarm_ID);
    }
    else
        goto exit;
    HTTP_W_R_struct.alarm_w_r_queue->sound_type = sound_type;
    HTTP_W_R_struct.alarm_w_r_queue->is_read = true;
    HTTP_W_R_struct.alarm_w_r_queue->sound_data = flashdata;
    HTTP_W_R_struct.alarm_w_r_queue->pos = 0;
    HTTP_W_R_struct.alarm_w_r_queue->len = 8;
    err = sound_file_read_write(&sound_file_list, HTTP_W_R_struct.alarm_w_r_queue);
    if (err == kNoErr) //文件已下載
    {
        Eland_log("ERRNONE");
        goto exit;
    }
    /*ready for uri*/
    memset(uri_str, 0, 100);
    if (sound_type == SOUND_FILE_SID)
        sprintf(uri_str, ELAND_SOUND_SID_URI, netclock_des_g->eland_id, alarm->alarm_sound_id);
    else if (sound_type == SOUND_FILE_VID)
        sprintf(uri_str, ELAND_SOUND_VID_URI, netclock_des_g->eland_id, alarm->voice_alarm_id);
    else if (sound_type == SOUND_FILE_OFID)
        sprintf(uri_str, ELAND_SOUND_OFID_URI, netclock_des_g->eland_id, alarm->alarm_off_voice_alarm_id);
    else if (sound_type == SOUND_FILE_DEFAULT)
        sprintf(uri_str, ELAND_SOUND_DEFAULT_URI);
    else if (sound_type == SOUND_FILE_WEATHER_0)
        sprintf(uri_str, ELAND_WEATHER_0_URI, netclock_des_g->eland_id);
    else if (sound_type == SOUND_FILE_WEATHER_E)
        sprintf(uri_str, ELAND_WEATHER_E_URI, netclock_des_g->eland_id);
    else if (sound_type == SOUND_FILE_WEATHER_F)
        sprintf(uri_str, ELAND_WEATHER_F_URI, netclock_des_g->eland_id);
    else
        goto exit;
    Eland_log("=====> alarm_sound_download send ======>");

    err = eland_http_request(ELAND_DOWN_LOAD_METHOD, //請求方式
                             ELAND_ALARM_GET,        //請求類型
                             uri_str,                //URI參數
                             ELAND_HTTP_DOMAIN_NAME, //主機域名
                             NULL,                   //request body
                             &user_http_res);        //response 接收緩存
                                                     // require_noerr(err, exit);
    //处理返回结果
    if (user_http_res.status_code == 200)
    {
        Eland_log("<===== alarm_sound_download end <======");
    }
    else if (user_http_res.status_code == 204)
    {
        Eland_log("<===== alarm_sound_download end 204<======");
        eland_error(true, EL_HTTP_204);
    }
    else if (user_http_res.status_code == 400)
    {
        eland_error(true, EL_HTTP_400);
        Eland_log("alarm_sound_download error %ld", user_http_res.status_code);
        err = kGeneralErr;
    }
    else
    {
        eland_error(true, EL_HTTP_OTHER);
        Eland_log("alarm_sound_download error %ld", user_http_res.status_code);
        err = kGeneralErr;
    }
exit:

    /**************/
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
    //释放资源
    free(flashdata);

    if (user_http_res.eland_response_body != NULL)
    {
        free(user_http_res.eland_response_body);
        user_http_res.eland_response_body = NULL;
    }
    if (err != kNoErr)
    {
        Eland_log("<===== alarm_sound_download error <======");
    }

    return err;
}
OSStatus Eland_Rtc_Init(void)
{
    OSStatus status = kNoErr;
    mico_rtc_time_t cur_time = {0};
    iso8601_time_t iso8601_time;
    mico_utc_time_t utc_time;
    mico_utc_time_ms_t current_utc = 0;
    DATE_TIME_t date_time;

    cur_time.year = 18; //设置时间
    cur_time.month = 1;
    cur_time.date = 21;
    cur_time.weekday = 4;
    cur_time.hr = 14;
    cur_time.min = 29;
    cur_time.sec = 50;

    date_time.iYear = 2000 + cur_time.year;
    date_time.iMon = (int16_t)cur_time.month;
    date_time.iDay = (int16_t)cur_time.date;
    date_time.iHour = (int16_t)cur_time.hr;
    date_time.iMin = (int16_t)cur_time.min;
    date_time.iSec = (int16_t)cur_time.sec;
    date_time.iMsec = 0;
    current_utc = GetSecondTime(&date_time);
    current_utc *= 1000;
    status = MicoRtcSetTime(&cur_time); //初始化 RTC 时钟的时间
    mico_time_set_utc_time_ms(&current_utc);

    mico_time_get_iso8601_time(&iso8601_time);
    Eland_log("Current time: %.26s", (char *)&iso8601_time);
    mico_time_get_utc_time(&utc_time);
    Eland_log("utc_time:%ld", utc_time);

    return status;
}

void eland_update_flash(void)
{
    bool needupdateflash = false;
    mico_Context_t *context = NULL;
    context = mico_system_context_get();

    if (strncmp(netclock_des_g->serial_number, device_state->serial_number, serial_number_len) != 0)
    {
        memcpy(device_state->serial_number, netclock_des_g->serial_number, serial_number_len);
        needupdateflash = true;
    }
    if (device_state->eland_id != netclock_des_g->eland_id)
    {
        device_state->eland_id = netclock_des_g->eland_id;
        needupdateflash = true;
    }

    if (device_state->dhcp_enabled != netclock_des_g->dhcp_enabled)
    {
        device_state->dhcp_enabled = netclock_des_g->dhcp_enabled;

        if (netclock_des_g->dhcp_enabled == 1)
            context->micoSystemConfig.dhcpEnable = DHCP_Client; /* Fetch Ip address from DHCP server */
        else if (netclock_des_g->dhcp_enabled == 0)
            context->micoSystemConfig.dhcpEnable = DHCP_Disable; /* Fetch Ip address from DHCP server */
        needupdateflash = true;
    }

    if (device_state->dhcp_enabled == 0)
    {
        if (strncmp(device_state->ip_address, netclock_des_g->ip_address, ip_address_Len) != 0)
        {
            needupdateflash = true;
            memcpy(device_state->user_id, netclock_des_g->user_id, user_id_len);
        }
        if ((strncmp(device_state->ip_address, netclock_des_g->ip_address, ip_address_Len) != 0) ||
            (strncmp(device_state->subnet_mask, netclock_des_g->subnet_mask, ip_address_Len) != 0) ||
            (strncmp(device_state->default_gateway, netclock_des_g->default_gateway, ip_address_Len) != 0) ||
            (strncmp(device_state->primary_dns, netclock_des_g->primary_dns, ip_address_Len) != 0))
        {
            needupdateflash = true;
            memcpy(device_state->ip_address, netclock_des_g->ip_address, ip_address_Len);
            memcpy(device_state->subnet_mask, netclock_des_g->subnet_mask, ip_address_Len);
            memcpy(device_state->default_gateway, netclock_des_g->default_gateway, ip_address_Len);
        }
        if (needupdateflash)
        {
            memcpy(context->micoSystemConfig.localIp, netclock_des_g->ip_address, 16);
            memcpy(context->micoSystemConfig.netMask, netclock_des_g->subnet_mask, 16);
            memcpy(context->micoSystemConfig.gateWay, netclock_des_g->default_gateway, 16);
            memcpy(context->micoSystemConfig.dnsServer, netclock_des_g->primary_dns, 16);
        }
    }
    if (strncmp(netclock_des_g->eland_name, device_state->eland_name, eland_name_Len) != 0)
    {
        needupdateflash = true;
        memcpy(device_state->eland_name, netclock_des_g->eland_name, eland_name_Len);
    }
    if (strncmp(netclock_des_g->user_id, device_state->user_id, user_id_len) != 0)
    {
        needupdateflash = true;
        memcpy(device_state->user_id, netclock_des_g->user_id, user_id_len);
    }
    if (netclock_des_g->timezone_offset_sec != device_state->timezone_offset_sec)
    {
        device_state->timezone_offset_sec = netclock_des_g->timezone_offset_sec;
        needupdateflash = true;
    }
    if (netclock_des_g->area_code != device_state->area_code)
    {
        device_state->area_code = netclock_des_g->area_code;
        needupdateflash = true;
    }

    if (needupdateflash == true)
        mico_system_context_update(context);
}

int8_t get_notification_volume(void)
{
    int ho, mi, se;
    uint32_t begin_second, end_second, now_second;
    mico_utc_time_t utc_time;

    sscanf((const char *)(&(netclock_des_g->night_mode_begin_time)), "%02d:%02d:%02d", &ho, &mi, &se);
    begin_second = (uint32_t)ho * SECOND_ONE_HOUR + (uint32_t)mi * SECOND_ONE_MINUTE + (uint32_t)se;
    sscanf((const char *)(&(netclock_des_g->night_mode_end_time)), "%02d:%02d:%02d", &ho, &mi, &se);
    end_second = (uint32_t)ho * SECOND_ONE_HOUR + (uint32_t)mi * SECOND_ONE_MINUTE + (uint32_t)se;

    mico_time_get_utc_time(&utc_time);
    now_second = utc_time % SECOND_ONE_DAY;
    if (netclock_des_g->night_mode_enabled)
    {
        if (end_second < begin_second)
        {
            if (now_second < end_second)
                return netclock_des_g->notification_volume_night;
            else if (now_second < begin_second)
                return netclock_des_g->notification_volume_normal;
            else
                return netclock_des_g->notification_volume_night;
        }
        else
        {
            if (now_second < begin_second)
                return netclock_des_g->notification_volume_normal;
            else if (now_second < end_second)
                return netclock_des_g->notification_volume_night;
            else
                return netclock_des_g->notification_volume_normal;
        }
    }
    else
        return netclock_des_g->notification_volume_normal;
}
