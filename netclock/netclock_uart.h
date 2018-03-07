/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-01 10:13:33
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-27 16:43:34
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
typedef enum {
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
typedef enum {
    KEY_Set = (uint16_t)0x0001,       /*!< 時刻設置 */
    KEY_Reset = (uint16_t)0x0002,     /*!< 軟件復位 */
    KEY_Add = (uint16_t)0x0004,       /*!< 時間＋   */
    KEY_Minus = (uint16_t)0x0008,     /*!< 時間－   */
    KEY_MON = (uint16_t)0x0010,       /*!< mon時間  */
    KEY_AlarmMode = (uint16_t)0x0020, /*!< 鬧鐘模式 */
    KEY_Wifi = (uint16_t)0x0040,      /*!< wifi模式 */
    KEY_Snooze = (uint16_t)0x0080,    /*!< 貪睡     */
    KEY_Alarm = (uint16_t)0x0100,     /*!< 鬧鐘     */
} KEY_State_TypeDef;

typedef enum {
    KEY_FUN_NONE = 0x00, /* 空命令*/
    KEY_READ_02 = 0X02,  /* READ MCU KEY STATE*/
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
} __msg_function_t;

typedef enum {
    REFRESH_NONE = 0,
    REFRESH_TIME,
    REFRESH_ALARM,
    REFRESH_MAX,
} MCU_Refresh_type_t;
typedef enum {
    LEVEL0 = (uint8_t)0x00,
    LEVEL1 = (uint8_t)0x08,
    LEVEL2 = (uint8_t)0x0C,
    LEVEL3 = (uint8_t)0x0E,
    LEVEL4 = (uint8_t)0x0F,
    LEVELNUM = (uint8_t)0xFF,
} LCD_Wifi_Rssi_t;

typedef enum _eland_mode {
    ELAND_MODE_NONE = (uint8_t)0x00,
    ELAND_CLOCK_MON,
    ELAND_CLOCK_ALARM,
    ELAND_NC,
    ELAND_NA,
    ELAND_AP,
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
    uint32_t night_mode_begin_time;
    uint32_t night_mode_end_time;
} __ELAND_DATA_2_MCU_t;

/* Private define ------------------------------------------------------------*/
#define Uart_Packet_Header (uint8_t)(0x55)
#define Uart_Packet_Trail (uint8_t)(0xaa)
#define USART_RESEND_MAX_TIMES 3

#define RSSI_STATE_STAGE0 (int)(-85)
#define RSSI_STATE_STAGE1 (int)(-80)
#define RSSI_STATE_STAGE2 (int)(-75)
#define RSSI_STATE_STAGE3 (int)(-70)
#define RSSI_STATE_STAGE4 (int)(-65)
/* Private macro -------------------------------------------------------------*/
extern mico_queue_t eland_uart_CMD_queue; //eland usart
/* Private function prototypes -----------------------------------------------*/
uint16_t get_eland_mode_state(void);
_ELAND_MODE_t get_eland_mode(void);
OSStatus start_uart_service(void);
void uart_recv_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void SendElandStateQueue(Eland_Status_type_t value);
void reset_eland_flash_para(void);
/* Private functions ---------------------------------------------------------*/

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
