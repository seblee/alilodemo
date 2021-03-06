/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-01 10:13:33
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-06-13 10:34:01
 * @brief   :
 ****************************************************************************
**/
#ifndef _NETCLOCK_NETCLOCKUART_H_
#define _NETCLOCK_NETCLOCKUART_H_

/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/
//状态宏定义
/*Eland 状态*/
typedef enum
{
    ElandNone = 0,
    ElandBegin,
    APStatusStart,
    APStatusClosed,
    APServerStart,
    HttpServerStop,
    ELAPPConnected,
    WifyConnected,
    WifyDisConnected,
    WifyConnectedFailed,
    HTTP_Get_HOST_INFO,
    TCP_CN00,
    CONNECTED_NET,
    TCP_DV00,
    TCP_AL00,
    TCP_HD00,
    TCP_HC00,
} Eland_Status_type_t;
typedef enum
{
    KEY_Set = (uint16_t)0x0001,       /*!< SET */
    KEY_Reset = (uint16_t)0x0002,     /*!< RESET */
    KEY_Add = (uint16_t)0x0004,       /*!< UP   */
    KEY_Minus = (uint16_t)0x0008,     /*!< DOWN  */
    KEY_MON = (uint16_t)0x0010,       /*!< offline clock alarm off*/
    KEY_AlarmMode = (uint16_t)0x0020, /*!< offline alarm on*/
    KEY_Wifi = (uint16_t)0x0040,      /*!< ON line*/
    KEY_Snooze = (uint16_t)0x0080,    /*!< SNOOZE     */
    KEY_Alarm = (uint16_t)0x0100,     /*!< ALARM_OFF   */
    KEY_TEST = (uint16_t)0x0200,      /*!< TEST    */
} KEY_State_TypeDef;

typedef enum
{
    EL_ERROR_NONE = 0x00, /*error none */
    EL_HTTP_TIMEOUT,      /*http time out*/
    EL_HTTP_204,          /*http 204*/
    EL_HTTP_400,          /*http 400*/
    EL_HTTP_OTHER,        /*http other error*/
    EL_FLASH_READ,        /*flash read error*/
    EL_AUDIO_PLAY,        /*audio play error*/
    EL_MAIN_THREAD,       /*file download  error*/
    EL_FLASH_OK,          /*file download  error*/
    EL_FLASH_ERR,         /*file download  error*/
} __eland_error_t;

typedef enum
{
    EL_RAM_READ = 0x00, /*READ RAM*/
    EL_RAM_WRITE,       /*WRITE RAM*/
} _eland_ram_rw_t;

typedef enum
{
    KEY_FUN_NONE = 0x00, /* 空命令*/
    SEND_ELAND_ERR_01,   /* SEND ELAND ERROR CODE*/
    KEY_READ_02,         /* READ MCU KEY STATE*/
    TIME_SET_03,         /* SEND ELAND TIME*/
    TIME_READ_04,        /* READ MCU TIME*/
    ELAND_STATES_05,     /* SEND ELAND STATES*/
    SEND_FIRM_WARE_06,   /* SEND ELAND FIRMWARE VERSION*/
    REND_FIRM_WARE_07,   /* READ MUC FIRMWARE VERSION*/
    SEND_LINK_STATE_08,  /* SEND WIFI LINK STATE*/
    MCU_FIRM_WARE_09,    /* START MCU FIRM WARE UPDATE*/
    ALARM_READ_0A,       /* READ MCU ALARM*/
    ALARM_SEND_0B,       /* SEND NEXT ALARM STATE*/
    ELAND_DATA_0C,       /* SEND ELAND DATA TO MCU*/
    ELAND_RESET_0D,      /* RESET SYSTEM */
    ELAND_DELETE_0E,     /* DEVICE DATA DELETE */
} __msg_function_t;

typedef enum
{
    REFRESH_NONE = 0,
    REFRESH_TIME,
    REFRESH_ALARM,
    REFRESH_ELAND_DATA,
    REFRESH_MAX,
} MCU_code_type_t;
typedef enum
{
    LEVEL0 = (uint8_t)0x00,
    LEVEL1 = (uint8_t)0x08,
    LEVEL2 = (uint8_t)0x0C,
    LEVEL3 = (uint8_t)0x0E,
    LEVEL4 = (uint8_t)0x0F,
    LEVELNUM = (uint8_t)0xFF,
} LCD_Wifi_Rssi_t;

typedef enum _eland_mode
{
    ELAND_MODE_NONE = (uint8_t)0x00,
    ELAND_CLOCK_MON,
    ELAND_CLOCK_ALARM,
    ELAND_NC,
    ELAND_NA,
    ELAND_AP,
    ELAND_OTA,
    ELAND_TEST,
    ELAND_MODE_MAX,
} _ELAND_MODE_t;

typedef struct eland_mode_state
{
    _ELAND_MODE_t eland_mode;
    Eland_Status_type_t eland_status;
    mico_mutex_t state_mutex;
} __ELAND_MODE_STATE_t;

typedef struct eland_data_2_mcu
{
    int8_t time_display_format;
    int8_t night_mode_enabled;
    int8_t brightness_normal;
    int8_t brightness_night;
    int8_t led_normal;
    int8_t led_night;
    uint32_t night_mode_begin_time;
    uint32_t night_mode_end_time;
} __ELAND_DATA_2_MCU_t;

typedef enum
{
    Thread_UART_recv_dead = (uint16_t)0x0001,       /*!< uart thread is flag of dead */
    Thread_TCP_dead = (uint16_t)0x0002,             /*!< tcp thread is flag of dead */
    Thread_UART_ELAND_test_dead = (uint16_t)0x2000, /*!< ELAND_test is flag of dead */
    Thread_UART_ELAND_AP_dead = (uint16_t)0x4000,   /*!< uart SOFTAP thread is flag of dead */
    Thread_TCP_FW01_dead = (uint16_t)0x8000,        /*!< tcp TCP_FW01 is flag of dead */
} Thread_State_dead_TypeDef;
extern uint16_t Thread_State_dead;
/* Private define ------------------------------------------------------------*/
#define Uart_Packet_Header (uint8_t)(0x55)
#define Uart_Packet_Trail (uint8_t)(0xaa)
#define USART_RESEND_MAX_TIMES 3

#define RSSI_STATE_STAGE0 (int16_t)(-85)
#define RSSI_STATE_STAGE1 (int16_t)(-80)
#define RSSI_STATE_STAGE2 (int16_t)(-75)
#define RSSI_STATE_STAGE3 (int16_t)(-70)
#define RSSI_STATE_STAGE4 (int16_t)(-65)

#define ELAND_KEY_COUNT 10
#define ELAND_CHECKOUT_SPEED 20
/* Private macro -------------------------------------------------------------*/
extern mico_queue_t eland_uart_CMD_queue; //eland usart
extern mico_utc_time_t eland_current_utc;
/* Private function prototypes -----------------------------------------------*/
void set_eland_mode(_ELAND_MODE_t mode);
uint16_t get_eland_mode_state(void);
_ELAND_MODE_t get_eland_mode(void);
OSStatus start_uart_service(void);
void uart_recv_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void SendElandStateQueue(Eland_Status_type_t value);
void reset_eland_flash_para(__msg_function_t msg);
void eland_push_uart_send_queue(__msg_function_t cmd);
__eland_error_t eland_error(_eland_ram_rw_t write_error, __eland_error_t error);
/* Private functions ---------------------------------------------------------*/

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
