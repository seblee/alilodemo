/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-17 17:12:26
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-27 15:49:11
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/inc/audio_service.h"
#include "../alilodemo/inc/http_file_download.h"
#include "../alilodemo/inc/robot_event.h"
#include "mico.h"
#include "netclock_wifi.h"
#include "netclock_uart.h"
#include "netclock_httpd.h"
#include "eland_tcp.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define WifiSet_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_queue_t wifistate_queue = NULL;

/* Private function prototypes -----------------------------------------------*/
static void Wifi_SoftAP_threed(mico_thread_arg_t arg);
static void recorde_IPStatus(wlanInterfaceTypedef type);
/* Private functions ---------------------------------------------------------*/

void micoNotify_WifiStatusHandler(WiFiEvent status, void *const inContext)
{
    msg_wify_queue my_message;
    __msg_function_t eland_cmd = SEND_LINK_STATE_08;

    switch (status)
    {
    case NOTIFY_STATION_UP:
        WifiSet_log("Wi-Fi STATION connected.");
        mico_rtos_set_semaphore(&wifi_netclock);
        recorde_IPStatus(Station);
        mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
        /*Send wifi state station*/
        my_message.value = Wify_Station_Connect_Successed;
        mico_rtos_push_to_queue(&wifistate_queue, &my_message, 20);
        /*send cmd to lcd*/
        break;
    case NOTIFY_STATION_DOWN:
        WifiSet_log("Wi-Fi STATION disconnected.");
        SendElandStateQueue(WifyDisConnected);
        break;
    case NOTIFY_AP_UP:
        WifiSet_log("Wi-Fi Soft_AP ready!");
        mico_rtos_set_semaphore(&wifi_SoftAP_Sem);
        recorde_IPStatus(Soft_AP);
        SendElandStateQueue(APStatusStart);
        break;
    case NOTIFY_AP_DOWN:
        SendElandStateQueue(APStatusClosed);
        WifiSet_log("uAP disconnected.");
        break;
    default:
        break;
    }
}
void micoNotify_WifiConnectFailedHandler(OSStatus err, void *arg)
{
    msg_wify_queue my_message;
    WifiSet_log("Wi-Fi STATION connecte failed");
    my_message.value = Wify_Station_Connect_Failed;
    if (wifistate_queue != NULL)
    {
        if (!mico_rtos_is_queue_empty(&wifistate_queue))
            mico_rtos_pop_from_queue(&wifistate_queue, &my_message, 20);
        my_message.value = Wify_Station_Connect_Failed;
        mico_rtos_push_to_queue(&wifistate_queue, &my_message, 20);
    }
}
static void recorde_IPStatus(wlanInterfaceTypedef type)
{
    IPStatusTypedef IPStatus_Cache;
    micoWlanGetIPStatus(&IPStatus_Cache, type);
    memset(netclock_des_g->ip_address, 0, sizeof(netclock_des_g->ip_address));
    sprintf(netclock_des_g->ip_address, IPStatus_Cache.ip);
    netclock_des_g->dhcp_enabled = IPStatus_Cache.dhcp;
    memset(netclock_des_g->subnet_mask, 0, sizeof(netclock_des_g->subnet_mask));
    sprintf(netclock_des_g->subnet_mask, IPStatus_Cache.mask);
    memset(netclock_des_g->default_gateway, 0, sizeof(netclock_des_g->default_gateway));
    sprintf(netclock_des_g->default_gateway, IPStatus_Cache.gate);
}
OSStatus Start_wifi_Station_SoftSP_Thread(wlanInterfaceTypedef wifi_Mode)
{
    OSStatus err = kNoErr;
    if (wifi_Mode == Station)
    {
        err = mico_rtos_create_thread(NULL, MICO_NETWORK_WORKER_PRIORITY, "wifi station",
                                      Wifi_station_threed, 0x500, (mico_thread_arg_t)NULL);
    }
    else if (wifi_Mode == Soft_AP)
    {
        mico_rtos_set_semaphore(&TCP_Stop_Sem);

        // Wifi_SoftAP_fun();
        err = mico_rtos_create_thread(NULL, MICO_NETWORK_WORKER_PRIORITY, "wifi Soft_AP",
                                      Wifi_SoftAP_threed, 0x1000, (mico_thread_arg_t)NULL);
    }
    return err;
}

void Wifi_station_threed(mico_thread_arg_t arg)
{
    network_InitTypeDef_adv_st wNetConfigAdv;

    mico_rtos_lock_mutex(&WifiConfigMutex);
    micoWlanSuspend();
    /* Initialize wlan parameters */
    memset(&wNetConfigAdv, 0x0, sizeof(wNetConfigAdv));
    strcpy((char *)wNetConfigAdv.ap_info.ssid, netclock_des_g->Wifissid); /* wlan ssid string */
    strcpy((char *)wNetConfigAdv.key, netclock_des_g->WifiKey);           /* wlan key string or hex data in WEP mode */
    wNetConfigAdv.key_len = strlen(netclock_des_g->WifiKey);              /* wlan key length */
    wNetConfigAdv.ap_info.security = SECURITY_TYPE_AUTO;                  /* wlan security mode */
    wNetConfigAdv.ap_info.channel = 0;                                    /* Select channel automatically */
    if (netclock_des_g->dhcp_enabled == 1)
        wNetConfigAdv.dhcpMode = DHCP_Client; /* Fetch Ip address from DHCP server */
    else
    {
        wNetConfigAdv.dhcpMode = DHCP_Disable; /* Fetch Ip address from DHCP server */
        memcpy(wNetConfigAdv.local_ip_addr, netclock_des_g->ip_address, 16);
        memcpy(wNetConfigAdv.net_mask, netclock_des_g->subnet_mask, 16);
        memcpy(wNetConfigAdv.gateway_ip_addr, netclock_des_g->default_gateway, 16);
        memcpy(wNetConfigAdv.dnsServer_ip_addr, netclock_des_g->dnsServer, 16);
    }

    wNetConfigAdv.wifi_retry_interval = 100; /* Retry interval after a failure connection */

    /* Connect Now! */
    WifiSet_log("connecting to %s...", wNetConfigAdv.ap_info.ssid);
    micoWlanStartAdv(&wNetConfigAdv);

    mico_rtos_unlock_mutex(&WifiConfigMutex);
    mico_rtos_delete_thread(NULL);
}
static void Wifi_SoftAP_threed(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    network_InitTypeDef_st wNetConfig;

    mico_rtos_thread_sleep(2);
    err = Eland_httpd_start();
    if (err == kNoErr)
        SendElandStateQueue(APServerStart);
    mico_rtos_lock_mutex(&WifiConfigMutex);
    micoWlanSuspend();
    WifiSet_log("Soft_ap_Server");
    memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
    strcpy((char *)wNetConfig.wifi_ssid, ELAND_AP_SSID);
    strcpy((char *)wNetConfig.wifi_key, ELAND_AP_KEY);
    wNetConfig.wifi_mode = Soft_AP;
    wNetConfig.dhcpMode = DHCP_Server;
    wNetConfig.wifi_retry_interval = 100;
    strcpy((char *)wNetConfig.local_ip_addr, ELAND_AP_LOCAL_IP);
    strcpy((char *)wNetConfig.net_mask, ELAND_AP_NET_MASK);
    strcpy((char *)wNetConfig.dnsServer_ip_addr, ELAND_AP_LOCAL_IP);
    WifiSet_log("ssid:%s  key:%s", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    micoWlanStart(&wNetConfig);
    mico_rtos_get_semaphore(&wifi_SoftAP_Sem, MICO_WAIT_FOREVER);
    mico_rtos_unlock_mutex(&WifiConfigMutex);
    mico_rtos_delete_thread(NULL);
}

void Wifi_SoftAP_fun(void)
{
    network_InitTypeDef_st wNetConfig;

    mico_rtos_lock_mutex(&WifiConfigMutex);
    micoWlanSuspend();
    WifiSet_log("Soft_ap_Server");
    memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
    strcpy((char *)wNetConfig.wifi_ssid, ELAND_AP_SSID);
    strcpy((char *)wNetConfig.wifi_key, ELAND_AP_KEY);
    wNetConfig.wifi_mode = Soft_AP;
    wNetConfig.dhcpMode = DHCP_Server;
    wNetConfig.wifi_retry_interval = 100;
    strcpy((char *)wNetConfig.local_ip_addr, ELAND_AP_LOCAL_IP);
    strcpy((char *)wNetConfig.net_mask, ELAND_AP_NET_MASK);
    strcpy((char *)wNetConfig.dnsServer_ip_addr, ELAND_AP_LOCAL_IP);
    WifiSet_log("ssid:%s  key:%s", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    micoWlanStart(&wNetConfig);
    mico_rtos_get_semaphore(&wifi_SoftAP_Sem, MICO_WAIT_FOREVER);
    mico_rtos_unlock_mutex(&WifiConfigMutex);
}

OSStatus ElandWifyStateNotifyInit(void)
{
    OSStatus err;
    /*wifi station 信號針*/
    err = mico_rtos_init_semaphore(&wifi_netclock, 1);
    require_noerr(err, exit);
    /*wifi softAP 信號針*/
    err = mico_rtos_init_semaphore(&wifi_SoftAP_Sem, 1);
    require_noerr(err, exit);
    /*wifi state 消杯隊列*/
    err = mico_rtos_init_queue(&wifistate_queue, "wifistate_queue", sizeof(msg_wify_queue), 3);
    require_noerr(err, exit);
    /*Register user function for MiCO nitification: WiFi status changed*/
    err = mico_rtos_init_mutex(&WifiConfigMutex);
    require_noerr(err, exit);
    /*Register user Wifi set mutex: WiFi status changed*/
    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED,
                                      (void *)micoNotify_WifiStatusHandler, NULL);
    require_noerr(err, exit);
    /*Register user Wifi set mutex: WiFi status changed*/
    err = mico_system_notify_register(mico_notify_WIFI_CONNECT_FAILED,
                                      (void *)micoNotify_WifiConnectFailedHandler, NULL);
    require_noerr(err, exit);

exit:
    return err;
}
