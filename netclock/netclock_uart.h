/*
 * netclockuart.h
 *
 *  Created on: 2017年7月27日
 *      Author: ceeu
 */

#ifndef _NETCLOCK_NETCLOCKUART_H_
#define _NETCLOCK_NETCLOCKUART_H_
#include "mico.h"

//状态宏定义
/*Eland 状态*/
typedef enum {
    ElandBegin = 0,
    ElandAPStatus,
    ElandHttpServerStatus,
    ElandWifyConnectedStatus,
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

typedef struct __TxDataStc
{
    uint8_t Elandheader;
    // uint8_t ElandLen;
    Eland_Status_type Elandstatus;
    mico_rtc_time_t ElandRtctime;
    uint8_t ElandTrail;
} TxDataStc;

/*全局变量*/
extern mico_queue_t elandstate_queue;
extern TxDataStc ElandTranmmiteToMcu;

/*函数*/
void start_uart_service(void);
void netclock_uart_thread(mico_thread_arg_t args);
void uart_recv_thread_DDE(uint32_t arg);
void uart_send_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void StateReceivethread(mico_thread_arg_t arg);

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
