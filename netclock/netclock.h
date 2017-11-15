#ifndef NETCLOCK_NETCLOCK_H_
#define NETCLOCK_NETCLOCK_H_
#include "../../alilodemo/netclock/netclockconfig.h"
#include "mico.h"

extern const char Eland_Data[11];
extern json_object *ElandJsonData;
extern ELAND_DES_S *netclock_des_g;
//extern static mico_semaphore_t WifiConnectSem;

OSStatus start_eland_service(void);
OSStatus netclock_desInit(void);

OSStatus InitNetclockService(void);
OSStatus StartNetclockService(void);
bool CheckNetclockDESSetting(void);
OSStatus Netclock_des_recovery(void);
void start_HttpServer_softAP_thread(void);
void ElandParameterConfiguration(mico_thread_arg_t args);

bool get_wifi_status(void);

void free_json_obj(json_object **json_obj);
void destory_upload_data(void);

OSStatus Eland_Rtc_Init(void);

OSStatus InitUpLoadData(char *OutputJsonstring);
OSStatus ProcessPostJson(char *InputJson);

//device sync
#define ELAND_SYNC_STATUS_METHOD HTTP_POST
#define ELAND_SYNC_STATUS_URI ("/")

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

//eland 情報登錄 / download.php ? vid = 01_64kbpse ELAND_DEVICE_INFO_UPDATE_URI("/")
#define ELAND_DEVICE_LOGIN_METHOD HTTP_POST
#define ELAND_DEVICE_LOGIN_URI ("/api/device/regist")

//eland 情報更新
#define ELAND_DEVICE_INFO_UPDATE_METHOD HTTP_POST
#define ELAND_DEVICE_INFO_UPDATE_URI ("/")

//eland 情報取得
#define ELAND_DEVICE_INFO_GET_METHOD HTTP_GET
#define ELAND_DEVICE_INFO_GET_URI ("/api/device/latest?eid=%s&mod=%s")

//eland 鬧鐘聲音取得
#define ELAND_ALARM_GET_METHOD HTTP_GET
#define ELAND_ALARM_GET_URI ("/?vid=%s")

//eland 鬧鐘開始通知
#define ELAND_ALARM_START_NOTICE_METHOD HTTP_POST
#define ELAND_ALARM_START_NOTICE_URI ("/api/alarm/start?eid=%s&aid=%s")

//eland 鬧鐘OFF履歷登錄
#define ELAND_ALARM_OFF_RECORD_ENTRY_METHOD HTTP_POST
#define ELAND_ALARM_OFF_RECORD_ENTRY_URI ("/api/alarm/history")

//eland 固件升級開始通知
#define ELAND_OTA_START_NOTICE_METHOD HTTP_POST
#define ELAND_OTA_START_NOTICE_URI ("/api/device/firmware/update?eid=5")

#define ELAND_DOWN_LOAD_REQUEST                                \
    "GET /api/download.php?vid=taichi_16_024kbps HTTP/1.1\r\n" \
    "Host: 160.16.237.210\r\n"                                 \
    "\r\n"                                                     \
    ""

#endif /* NETCLOCK_NETCLOCK_H_ */
