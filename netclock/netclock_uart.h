/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-11-01 10:13:33
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-11-01 10:18:16
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
    APStatus,
    APStatusClosed,
    HttpServerStatus,
    HttpServerStop,
    ELAPPConnected,
    WifyConnected,
    WifyDisConnected,
    WifyConnectedFailed,
    ElandAliloPlay,
    ElandAliloPause,
    ElandAliloStop,
    HTTP_Get_HOST_INFO,
    TCPConnectedELSV,
    TCPHealthCheck,
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
    KEY_FUN_NONE = 0x00, /*空命令*/
    KEY_READ_02 = 0X02,
    TIME_SET_03,
    TIME_READ_04,
    ELAND_STATES_05,
    FIRM_WARE_06,
} __msg_function_t;

typedef enum {
    ELAND_CMD_NONE,     /*空命令*/
    ELAND_SEND_CMD_02H, /*發送02H命令*/
    ELAND_SEND_CMD_03H, /*發送03H命令*/
    ELAND_SEND_CMD_04H, /*發送04H命令*/
    ELAND_SEND_CMD_05H, /*發送05H命令*/
    ELAND_SEND_CMD_06H, /*發送05H命令*/
} eland_usart_cmd_t;
/* Private define ------------------------------------------------------------*/
#define Uart_Packet_Header (uint8_t)(0x55)
#define Uart_Packet_Trail (uint8_t)(0xaa)
#define Usart_Packet_Resend_time 5
/* Private macro -------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
void start_uart_service(void);
void uart_recv_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void StateReceivethread(mico_thread_arg_t arg);
void SendElandStateQueue(Eland_Status_type_t value);

/* Private functions ---------------------------------------------------------*/

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
