/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-17 10:33:38
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-17 10:34:32
 * @brief   :
 ****************************************************************************
**/
#ifndef NETCLOCK_NETCLOCK_H_
#define NETCLOCK_NETCLOCK_H_

/* Private include -----------------------------------------------------------*/
#include "../../alilodemo/netclock/netclockconfig.h"
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
//eland sound down load
#define ELAND_DOWN_LOAD_METHOD HTTP_GET
//#define ELAND_DOWN_LOAD_URI ("/mockup/browser/sound/Alarm01.mp3")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=maki_emo_16_024kbps")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=maki_emo_16_064kbps")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=maki_emo_16_096kbps")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=maki_emo_16_128kbps")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=01_128kbps")
#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=01_128kbps")
//#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=01_40kbps")

//eland 通信情報取得
#define ELAND_COMMUNICATION_INFO_UPDATE_METHOD HTTP_GET
#define ELAND_COMMUNICATION_INFO_UPDATE_URI ("/api/server/info")

#define ELAND_DOWN_LOAD_REQUEST                                \
    "GET /api/download.php?vid=taichi_16_024kbps HTTP/1.1\r\n" \
    "Host: 160.16.237.210\r\n"                                 \
    "\r\n"                                                     \
    ""

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern const char Eland_Data[11];
extern json_object *ElandJsonData;
extern ELAND_DES_S *netclock_des_g;
extern _ELAND_DEVICE_t *device_state;

/* Private function prototypes -----------------------------------------------*/
OSStatus start_eland_service(void);
OSStatus netclock_desInit(void);

OSStatus StartNetclockService(void);
OSStatus CheckNetclockDESSetting(void);
OSStatus Netclock_des_recovery(void);

bool get_wifi_status(void);

void free_json_obj(json_object **json_obj);
void destory_upload_data(void);

OSStatus Eland_Rtc_Init(void);

OSStatus InitUpLoadData(char *OutputJsonstring);
OSStatus ProcessPostJson(char *InputJson);

/* Private functions ---------------------------------------------------------*/

//extern static mico_semaphore_t WifiConnectSem;

#endif /* NETCLOCK_NETCLOCK_H_ */
