/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-05 14:51:54
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-22 09:40:59
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_MCU_OTA_H_
#define __ELAND_MCU_OTA_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define MCU_REVISION "01.16"
/* Define mcu power pin */
#define MCU_POWER_GPIO MICO_GPIO_1

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_thread_t MCU_OTA_thread;
/* Private function prototypes -----------------------------------------------*/
void mcu_ota_thread(mico_thread_arg_t arg);
/* Private functions ---------------------------------------------------------*/

#endif /*__ELAND_MCU_OTA_H_*/
