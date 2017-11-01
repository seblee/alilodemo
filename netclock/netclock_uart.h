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

typedef enum {
    KEY_READ_02 = 0X02,
    TIME_SET_03,
    TIME_READ_04,
} __msg_function_t;

/* Private define ------------------------------------------------------------*/
#define Uart_Packet_Header (uint8_t)(0x55)
#define Uart_Packet_Trail (uint8_t)(0xaa)
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern mico_queue_t elandstate_queue;

/* Private function prototypes -----------------------------------------------*/
void start_uart_service(void);
void netclock_uart_thread(mico_thread_arg_t args);
void uart_recv_thread_DDE(uint32_t arg);
void uart_send_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void StateReceivethread(mico_thread_arg_t arg);
void SendElandQueue(msg_queue_type Type, uint32_t value);

OSStatus Eland_Uart_Push_Uart_Send_Mutex(uint8_t *DataBody);
/* Private functions ---------------------------------------------------------*/

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
