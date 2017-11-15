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
    ElandBegin = 0,
    ElandAPStatus,
    ElandHttpServerStatus,
    ElandWifyConnectedSuccessed,
    ElandWifyConnectedFailed,
    ElandAliloPlay,
    ElandAliloPause,
    ElandAliloStop,
} Eland_Status_type;

typedef enum {
    Queue_UTC_type,
    Queue_ElandState_type,
} msg_queue_type;

typedef struct __msg_queue
{
    msg_queue_type type;
    uint32_t value;
} msg_queue;

typedef struct __msg_send_queue
{
    uint16_t length;
    uint8_t *data;
} __msg_send_queue_t;

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
    KEY_READ_02 = 0X02,
    TIME_SET_03,
    TIME_READ_04,
} __msg_function_t;

typedef enum {
    ELAND_CMD_NONE,     /*空命令*/
    ELAND_SEND_CMD_02H, /*發送02H命令*/
    ELAND_SEND_CMD_03H, /*發送03H命令*/
    ELAND_SEND_CMD_04H, /*發送04H命令*/
} eland_usart_cmd_t;
/* Private define ------------------------------------------------------------*/
#define Uart_Packet_Header (uint8_t)(0x55)
#define Uart_Packet_Trail (uint8_t)(0xaa)
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_queue_t elandstate_queue;

/* Private function prototypes -----------------------------------------------*/
void start_uart_service(void);
void uart_recv_thread_DDE(uint32_t arg);
void uart_send_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void StateReceivethread(mico_thread_arg_t arg);
void SendElandQueue(msg_queue_type Type, uint32_t value);

OSStatus Eland_Uart_Push_Uart_Send_Mutex(__msg_send_queue_t *DataBody);
/* Private functions ---------------------------------------------------------*/

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
