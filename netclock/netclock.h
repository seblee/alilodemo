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

OSStatus InitUpLoadData(char *OutputJsonstring);
OSStatus ProcessPostJson(char *InputJson);

//device sync
#define ELAND_SYNC_STATUS_METHOD HTTP_POST
#define ELAND_SYNC_STATUS_URI ("/")

//eland sound down load
#define ELAND_DOWN_LOAD_METHOD HTTP_GET
#define ELAND_DOWN_LOAD_URI ("/api/download.php?vid=maki_emo_16_024kbps")

#define ELAND_DOWN_LOAD_REQUEST                                \
    "GET /api/download.php?vid=taichi_16_024kbps HTTP/1.1\r\n" \
    "Host: 160.16.237.210\r\n"                                 \
    "\r\n"                                                     \
    ""

#endif /* NETCLOCK_NETCLOCK_H_ */
