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
#define ElandBegin 0x0000
#define ElandAPStatus 0x0001
#define ElandHttpServerStatus 0x0002
#define ElandWifyConnectedStatus 0x0004

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
    uint8_t Elandstatus;
    mico_rtc_time_t ElandRtctime;
    uint8_t ElandTrail;
} TxDataStc;

/*全局变量*/
extern mico_queue_t elandstate_queue;
//extern uint8_t ElandTranmmiteToMcu[10];
extern TxDataStc ElandTranmmiteToMcu;

/*函数*/
void start_uart_service(void);
void netclock_uart_thread(mico_thread_arg_t args);
void uart_recv_thread_DDE(uint32_t arg);
void uart_send_thread_DDE(uint32_t arg);
int uart_get_one_packet(uint8_t *inBuf, int inBufLen);
void StateReceivethread(mico_thread_arg_t arg);

#endif /* _NETCLOCK_NETCLOCKUART_H_ */
