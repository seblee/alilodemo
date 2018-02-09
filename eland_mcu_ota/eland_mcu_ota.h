/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-05 14:51:54
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-05 15:12:16
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_MCU_OTA_H_
#define __ELAND_MCU_OTA_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define MCU_VERSION_MAJOR 1
#define MCU_VERSION_MINOR 3
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_thread_t MCU_OTA_thread;
/* Private function prototypes -----------------------------------------------*/
void mcu_ota_thread(mico_thread_arg_t arg);
/* Private functions ---------------------------------------------------------*/

#endif /*__ELAND_MCU_OTA_H_*/
