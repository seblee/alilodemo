#ifndef _NETCLOCK_NETCLOCK_WIFI_H_
#define _NETCLOCK_NETCLOCK_WIFI_H_
#include "mico.h"
#include "netclock.h"
typedef enum {
    Wify_Station_Connect_Successed,
    Wify_Station_Connect_Failed,
} msg_wify_status;

typedef struct __msg_wiffy_queue
{
    msg_wify_status value;
} msg_wify_queue;

extern mico_mutex_t WifiConfigMutex;
extern mico_semaphore_t wifi_SoftAP_Sem;
extern mico_semaphore_t wifi_netclock;
extern mico_queue_t wifistate_queue;

void Start_wifi_Station_SoftSP_Thread(wlanInterfaceTypedef wifi_Mode);
void Wifi_station_threed(mico_thread_arg_t arg);
void Wifi_SoftAP_threed(mico_thread_arg_t arg);
void micoNotify_WifiConnectFailedHandler(OSStatus err, void *arg);
void micoNotify_WifiStatusHandler(WiFiEvent status, void *const inContext);
OSStatus ElandWifyStateNotifyInit(void);

#endif /* ALILODEMO_NETCLOCK_NETCLOCK_WIFI_H_ */
