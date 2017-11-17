/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-10-31 17:37:53
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-10-31 17:42:45
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "../alilodemo/audio_test.h"
#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/mico_app_define.h"
#include "mico.h"
#include "netclockconfig.h"
#include "netclock.h"
#include "netclock_uart.h"
#include "netclock_wifi.h"
#include "flash_kh25.h"
#include "eland_sound.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define app_netclock_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_mutex_t WifiMutex = NULL;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

void ssl_log(const int logLevel, const char *const logMessage)
{
    app_netclock_log("%s\r\n", logMessage);
}

int application_start(void)
{
    app_log_trace();
    OSStatus err = kNoErr;
    mico_Context_t *mico_context;
    app_netclock_log(">>>>>>>>>>>>>>>>>> app start >>>>>>>>>>>>>>>>>>>>>>>>>");
    /*init Wify Notify ,queue and semaphore*/
    err = ElandWifyStateNotifyInit();
    /*Register elandstate_queue: elandstate for uart*/
    err = mico_rtos_init_queue(&elandstate_queue, "elandstate_queue", sizeof(msg_queue), 3);

    err = Eland_Rtc_Init();
    /*start init uart & start service*/
    start_uart_service();

    /*start init system context*/
    mico_context = mico_system_context_init(sizeof(ELAND_DES_S));

    /*init fog v2 service*/
    app_netclock_log("init_netclock_service");
    err = InitNetclockService();
    require_noerr(err, exit);

    err = netclock_desInit(); //数据结构体初始化
    require_noerr(err, exit);

    err = hal_alilo_rabbit_init();
    require_noerr(err, exit);

    /* Start MiCO system functions according to mico_config.h*/
    err = mico_system_init(mico_context);
    require_noerr(err, exit);

    /****start softAP event wait******/
    start_HttpServer_softAP_thread();

    /*start init eland SPI*/
    err = start_eland_flash_service();
    require_noerr(err, exit);

    /* Wait for wlan connection*/
    //app_netclock_log("wait for wifi on");
    mico_rtos_get_semaphore(&wifi_netclock, MICO_WAIT_FOREVER);
    app_netclock_log("wifi connected successful");

    /*start eland HTTP service */
    //err = start_eland_service();
    require_noerr(err, exit);

    //err = start_test_thread();
    require_noerr(err, exit);

exit:
    mico_rtos_delete_thread(NULL);
    return err;
}
