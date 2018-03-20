/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-12-22 13:35:42
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-12-22 13:36:07
 * @brief   :
 ****************************************************************************
**/
#ifndef _NETCLOCK_OTA_H
#define _NETCLOCK_OTA_H

/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_semaphore_t eland_ota_sem;
extern char ota_url[128];
extern char ota_md5[33];
extern uint32_t ota_offset;
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
void start_ota_thread(void);
void Eland_ota_thread(mico_thread_arg_t args);
OSStatus eland_ota_data_init(uint32_t length);
OSStatus eland_ota_data_write(uint8_t *data, uint16_t len);
void eland_ota_operation(void);
OSStatus eland_ota(void);

#endif
