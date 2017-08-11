#include "netclock_ota.h"
#include "ota_server.h"

#define ota_log(M, ...) custom_log("OTA", M, ##__VA_ARGS__)

mico_semaphore_t eland_ota_sem = NULL;
uint8_t eland_ota = 0;
char ota_url[128];
char ota_md5[33];

static void ota_server_status_handler(OTA_STATE_E state, float progress)
{
    switch (state)
    {
    case OTA_LOADING:
        ota_log("ota server is loading, progress %.2f%%", progress);
        break;
    case OTA_SUCCE:
        ota_log("ota server daemons success");
        break;
    case OTA_FAIL:
        ota_log("ota server daemons failed");
        break;
    default:
        break;
    }
}
/***************啟動 OTA thread*****************************/
void start_ota_thread(void)
{
    mico_system_notify_remove_all(mico_notify_WIFI_STATUS_CHANGED);
    mico_system_notify_remove_all(mico_notify_WiFI_PARA_CHANGED);
    mico_system_notify_remove_all(mico_notify_DHCP_COMPLETED);
    mico_system_notify_remove_all(mico_notify_WIFI_CONNECT_FAILED);
    mico_system_notify_remove_all(mico_notify_EASYLINK_WPS_COMPLETED);

    /*********************OTA thread*****************************/
    ota_server_start(ota_url, ota_md5, ota_server_status_handler);
}
