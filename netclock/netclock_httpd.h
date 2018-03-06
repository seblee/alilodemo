/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-03-05 11:21:53
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-05 16:09:06
 * @brief   :
 ****************************************************************************
**/
#ifndef NETCLOCK_HTTPD_H_
#define NETCLOCK_HTTPD_H_

/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct _HTTP_SSIDS_RESULT
{
    uint8_t security;
    char ssid[32];
    int16_t rssi;
} __http_ssids_result_t;

typedef struct _HTTP_SSIDS_LIST
{
    uint8_t num;
    __http_ssids_result_t *ssids;
} __http_ssids_list_t;

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_semaphore_t http_ssid_event_Sem;

/* Private function prototypes -----------------------------------------------*/
int Eland_httpd_start(void);
int Eland_httpd_stop();

/* Private functions ---------------------------------------------------------*/

#endif
