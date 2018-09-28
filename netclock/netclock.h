/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-17 10:33:38
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-08 11:11:07
 * @brief   :
 ****************************************************************************
**/
#ifndef NETCLOCK_NETCLOCK_H_
#define NETCLOCK_NETCLOCK_H_

/* Private include -----------------------------------------------------------*/
#include "../../alilodemo/netclock/netclockconfig.h"
#include "mico.h"
#include "eland_alarm.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
//eland sound down load
#define ELAND_DOWN_LOAD_METHOD HTTP_GET

#define ELAND_SOUND_SID_URI ("/api/sound/download?eid=%ld&sid=%ld")
#define ELAND_SOUND_VID_URI ("/api/sound/download?eid=%ld&vid=%s")
#define ELAND_SOUND_OFID_URI ("/api/sound/download?eid=%ld&vid=%s")
#define ELAND_SOUND_OID_URI ("/api/sound/download?eid=%ld&oid=%ld")
#define ELAND_WEATHER_0_URI ("/api/sound/download?eid=%ld&vid=00000000-0000-0000-0000-000000000000")
#define ELAND_WEATHER_A_URI ("/api/sound/download?eid=%ld&vid=aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa")
#define ELAND_WEATHER_B_URI ("/api/sound/download?eid=%ld&vid=bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb")
#define ELAND_WEATHER_C_URI ("/api/sound/download?eid=%ld&vid=cccccccc-cccc-cccc-cccc-cccccccccccc")
#define ELAND_WEATHER_D_URI ("/api/sound/download?eid=%ld&vid=dddddddd-dddd-dddd-dddd-dddddddddddd")
#define ELAND_WEATHER_E_URI ("/api/sound/download?eid=%ld&vid=eeeeeeee-eeee-eeee-eeee-eeeeeeeeeeee")
#define ELAND_WEATHER_F_URI ("/api/sound/download?eid=%ld&vid=ffffffff-ffff-ffff-ffff-ffffffffffff")
#define ELAND_SOUND_DEFAULT_URI ("/api/sound/default")

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
extern mico_mutex_t http_send_setting_mutex;
/* Private function prototypes -----------------------------------------------*/
OSStatus start_eland_service(void);
OSStatus netclock_desInit(void);

OSStatus StartNetclockService(void);
OSStatus CheckNetclockDESSetting(void);
void eland_get_mac(char *mac_address);
OSStatus Netclock_des_recovery(void);

bool get_wifi_status(void);

void free_json_obj(json_object **json_obj);
void destory_upload_data(void);

OSStatus Eland_Rtc_Init(void);

OSStatus InitUpLoadData(char *OutputJsonstring);
OSStatus ProcessPostJson(char *InputJson);
OSStatus alarm_sound_download(_sound_download_para_t sound_para, _download_type_t download_type);
OSStatus eland_communication_info_get(void);
bool eland_update_flash(void);
int8_t get_notification_volume(void);
/* Private functions ---------------------------------------------------------*/

//extern static mico_semaphore_t WifiConnectSem;

#endif /* NETCLOCK_NETCLOCK_H_ */
