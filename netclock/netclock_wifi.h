/*
 * netclock_wifi.h
 *
 *  Created on: 2017年7月14日
 *      Author: ceeu
 */

#ifndef _NETCLOCK_NETCLOCK_WIFI_H_
#define _NETCLOCK_NETCLOCK_WIFI_H_
#include "mico.h"
#include "netclock.h"

extern mico_mutex_t WifiMutex;
extern mico_semaphore_t wifi_SoftAP_Sem;

void Start_wifi_Station_SoftSP_Thread(wlanInterfaceTypedef wifi_Mode);
void Wifi_station_threed(mico_thread_arg_t arg);
void Wifi_SoftAP_threed(mico_thread_arg_t arg);

#endif /* ALILODEMO_NETCLOCK_NETCLOCK_WIFI_H_ */
