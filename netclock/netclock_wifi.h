/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-27 14:20:27
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-27 14:21:01
 * @brief   :
 ****************************************************************************
**/
#ifndef _NETCLOCK_NETCLOCK_WIFI_H_
#define _NETCLOCK_NETCLOCK_WIFI_H_

/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "netclock.h"

/* Private typedef -----------------------------------------------------------*/
typedef enum {
    Wify_Station_Connect_Successed,
    Wify_Station_Connect_Failed,
} msg_wify_status;

typedef struct __msg_wiffy_queue
{
    msg_wify_status value;
} msg_wify_queue;

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_mutex_t WifiConfigMutex;
extern mico_semaphore_t wifi_SoftAP_Sem;
extern mico_semaphore_t wifi_netclock;
extern mico_queue_t wifistate_queue;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
OSStatus Start_wifi_Station_SoftSP_Thread(wlanInterfaceTypedef wifi_Mode);
void Wifi_station_threed(mico_thread_arg_t arg);
void micoNotify_WifiConnectFailedHandler(OSStatus err, void *arg);
void micoNotify_WifiStatusHandler(WiFiEvent status, void *const inContext);
OSStatus ElandWifyStateNotifyInit(void);
void Wifi_SoftAP_fun(void);

#endif /* ALILODEMO_NETCLOCK_NETCLOCK_WIFI_H_ */
