#ifndef _NETCLOCK_OTA_H
#define _NETCLOCK_OTA_H

#include "mico.h"

/*****************************/
extern mico_semaphore_t eland_ota_sem;
extern char ota_url[128];
extern char ota_md5[33];

/********************************/
void start_ota_thread(void);
void Eland_ota_thread(mico_thread_arg_t args);

/***************************/
#endif
