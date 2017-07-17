/**
 ******************************************************************************
 * @file    hello_world.c
 * @author  William Xu
 * @version V1.0.0
 * @date    21-May-2015
 * @brief   First MiCO application to say hello world!
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

#include "mico.h"
#include "netclockconfig.h"
#include "netclock.h"

#define app_netclock_log(format, ...) custom_log("APP", format, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")
static mico_semaphore_t wifi_netclock = NULL;

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
        app_netclock_log("AP OK");
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

int application_start(void)
{
    app_log_trace();
    OSStatus err = kNoErr;
    mico_Context_t *mico_context;
    app_netclock_log("app start");
    err = mico_rtos_init_semaphore(&wifi_netclock, 1);
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

    app_netclock_log("wait for wifi on");
    mico_rtos_get_semaphore(&wifi_netclock, MICO_WAIT_FOREVER);

    app_netclock_log("start netclock service");
    err = StartNetclockService();
    require_noerr(err, exit);

/* Output on debug serial port */
exit:
    app_netclock_log("HELLO WORLD!");

    /* Trigger MiCO system led available on most MiCOKit */

    while (1)
    {
        MicoGpioOutputTrigger(MICO_SYS_LED);
        mico_thread_sleep(1);
    }
}
