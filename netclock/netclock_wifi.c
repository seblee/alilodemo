/*
 * netclock_wifi.c
 *
 *  Created on: 2017年7月14日
 *      Author: ceeu
 */
#include "mico.h"
#include "netclock_wifi.h"
#define WifiSet_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)
void Start_wifi_Station_SoftSP_Thread(wlanInterfaceTypedef wifi_Mode)
{
    if (wifi_Mode == Station)
    {
        mico_rtos_create_thread(NULL, MICO_NETWORK_WORKER_PRIORITY, "wifi station",
                                Wifi_station_threed, 0x900, (mico_thread_arg_t)NULL);
    }
    else if (wifi_Mode == Soft_AP)
    {
        mico_rtos_create_thread(NULL, MICO_NETWORK_WORKER_PRIORITY, "wifi Soft_AP",
                                Wifi_SoftAP_threed, 0x900, (mico_thread_arg_t)NULL);
    }
    mico_thread_sleep(3); //3秒 等待连接完成
}

void Wifi_station_threed(mico_thread_arg_t arg)
{
    network_InitTypeDef_adv_st wNetConfigAdv;

    mico_rtos_lock_mutex(&WifiMutex);

    micoWlanSuspend();
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
    WifiSet_log("connecting to %s...", wNetConfigAdv.ap_info.ssid);
    micoWlanStartAdv(&wNetConfigAdv);

    mico_thread_sleep(3); //3秒 等待连接完成
    mico_rtos_unlock_mutex(&WifiMutex);
    mico_rtos_delete_thread(NULL);
}
void Wifi_SoftAP_threed(mico_thread_arg_t arg)
{
    network_InitTypeDef_st wNetConfig;
    mico_rtos_lock_mutex(&WifiMutex);
    WifiSet_log("Soft_ap_Server");

    micoWlanSuspend();
    memset(&wNetConfig, 0x0, sizeof(network_InitTypeDef_st));
    strcpy((char *)wNetConfig.wifi_ssid, ELAND_AP_SSID);
    strcpy((char *)wNetConfig.wifi_key, ELAND_AP_KEY);
    wNetConfig.wifi_mode = Soft_AP;
    wNetConfig.dhcpMode = DHCP_Server;
    wNetConfig.wifi_retry_interval = 100;
    strcpy((char *)wNetConfig.local_ip_addr, "192.168.0.1");
    strcpy((char *)wNetConfig.net_mask, "255.255.255.0");
    strcpy((char *)wNetConfig.dnsServer_ip_addr, "192.168.0.1");
    WifiSet_log("ssid:%s  key:%s", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    micoWlanStart(&wNetConfig);
    mico_rtos_get_semaphore(&wifi_SoftAP_Sem, MICO_WAIT_FOREVER);
    mico_rtos_unlock_mutex(&WifiMutex);
    mico_rtos_delete_thread(NULL);
}
