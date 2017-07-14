#include "../alilodemo/audio_test.h"
#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/mico_app_define.h"
#include "mico.h"
#include "netclockconfig.h"
#include "netclock.h"

#define app_netclock_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

static mico_semaphore_t wifi_netclock = NULL;

/* MICO system callback: Restore default configuration provided by application */

void micoNotify_WifiStatusHandler(WiFiEvent status, void *const inContext)
{
    IPStatusTypedef *IPStatus_Cache = NULL;
    switch (status)
    {
    case NOTIFY_STATION_UP:
        app_netclock_log("Wi-Fi STATION connected.");
        mico_rtos_set_semaphore(&wifi_netclock);
        IPStatus_Cache = malloc(sizeof(IPStatusTypedef));
        micoWlanGetIPStatus(IPStatus_Cache, Station);
        memset(netclock_des_g->ElandIPstr, 0, sizeof(netclock_des_g->ElandIPstr));
        sprintf(netclock_des_g->ElandIPstr, IPStatus_Cache->ip);
        netclock_des_g->ElandDHCPEnable = IPStatus_Cache->dhcp;
        memset(netclock_des_g->ElandSubnetMask, 0, sizeof(netclock_des_g->ElandSubnetMask));
        sprintf(netclock_des_g->ElandSubnetMask, IPStatus_Cache->mask);
        memset(netclock_des_g->ElandDefaultGateway, 0, sizeof(netclock_des_g->ElandDefaultGateway));
        sprintf(netclock_des_g->ElandDefaultGateway, IPStatus_Cache->gate);
        free(IPStatus_Cache);
        break;
    case NOTIFY_STATION_DOWN:
        app_netclock_log("Wi-Fi STATION disconnected.");

        break;
    case NOTIFY_AP_UP:
        app_netclock_log("Wi-Fi Soft_AP ready!");
        IPStatus_Cache = malloc(sizeof(IPStatusTypedef));
        micoWlanGetIPStatus(IPStatus_Cache, Soft_AP);
        memset(netclock_des_g->ElandIPstr, 0, sizeof(netclock_des_g->ElandIPstr));
        sprintf(netclock_des_g->ElandIPstr, IPStatus_Cache->ip);
        netclock_des_g->ElandDHCPEnable = IPStatus_Cache->dhcp;
        memset(netclock_des_g->ElandSubnetMask, 0, sizeof(netclock_des_g->ElandSubnetMask));
        sprintf(netclock_des_g->ElandSubnetMask, IPStatus_Cache->mask);
        memset(netclock_des_g->ElandDefaultGateway, 0, sizeof(netclock_des_g->ElandDefaultGateway));
        sprintf(netclock_des_g->ElandDefaultGateway, IPStatus_Cache->gate);
        free(IPStatus_Cache);
        break;
    case NOTIFY_AP_DOWN:
        app_netclock_log("uAP disconnected.");
        break;
    default:
        break;
    }
}

void ssl_log(const int logLevel, const char *const logMessage)
{
    app_netclock_log("%s\r\n", logMessage);
}

int application_start(void)
{
    app_log_trace();
    OSStatus err = kNoErr;
    mico_Context_t *mico_context;
    app_netclock_log("app start");
    mico_rtos_init_semaphore(&wifi_netclock, 1);

    /*Register user function for MiCO nitification: WiFi status changed */
    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED,
                                      (void *)micoNotify_WifiStatusHandler, NULL);
    require_noerr(err, exit);

    mico_context = mico_system_context_init(sizeof(ELAND_DES_S));
    /*int fog v2 service*/
    app_netclock_log("init_netclock_service");
    err = InitNetclockService();
    require_noerr(err, exit);
    app_netclock_log("mico_system_init");
    /* Start MiCO system functions according to mico_config.h*/
    err = mico_system_init(mico_context);

    require_noerr(err, exit);

    err = netclock_desInit(); //数据结构体初始化
    require_noerr(err, exit);

    /* Wait for wlan connection*/
    app_netclock_log("wait for wifi on");
    mico_rtos_get_semaphore(&wifi_netclock, MICO_WAIT_FOREVER);
    app_netclock_log("wifi connected successful");

    err = hal_alilo_rabbit_init();
    require_noerr(err, exit);

    err = start_test_thread();
    require_noerr(err, exit);

exit:
    mico_system_notify_remove(mico_notify_WIFI_STATUS_CHANGED,
                              (void *)micoNotify_WifiStatusHandler);
    mico_rtos_deinit_semaphore(&wifi_netclock);
    mico_rtos_delete_thread(NULL);

    return err;
}
