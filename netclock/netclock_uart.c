/*
 * netclockuart.c
 *
 *  Created on: 2017年7月27日
 *      Author: ceeu
 */

#include "netclock_uart.h"
#include "mico.h"

#define netclock_uart_log(format, ...) custom_log("netclock_uart", format, ##__VA_ARGS__)

volatile ring_buffer_t rx_buffer;
volatile uint8_t rx_data[2048];
#define UART_BUFFER_LENGTH 2048
#define UART_ONE_PACKAGE_LENGTH 1024
#define STACK_SIZE_UART_RECV_THREAD 0x900

mico_mutex_t ElandStateDateMutex = NULL; //传输数据独立访问
uint16_t ElandStateDate[2];
mico_queue_t elandstate_queue = NULL;

TxDataStc ElandTranmmiteToMcu = {
    0, 0,
};

/*************************/
void start_uart_service(void)
{
    OSStatus err = kNoErr;
    msg_queue my_message;
    //eland 状态数据互锁
    err = mico_rtos_init_mutex(&ElandStateDateMutex);
    require_noerr(err, exit);

    /*Register uart thread*/
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "netclock_uart_thread", netclock_uart_thread, 0x1000, (uint32_t)NULL);
    /*Register queue receive thread*/
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "queue receiver", StateReceivethread, 0x500, 0);
    require_noerr(err, exit);
    //Eland 设置状态
    my_message.type = Queue_ElandState_type;
    my_message.value = ElandBegin;
    mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);
exit:
    return;
}

void netclock_uart_thread(mico_thread_arg_t args)
{
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    /*UART receive thread*/
    mico_context = mico_system_context_get();
    uart_config.baud_rate = 57600;
    uart_config.data_width = DATA_WIDTH_8BIT;
    uart_config.parity = NO_PARITY;
    uart_config.stop_bits = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
    if (mico_context->micoSystemConfig.mcuPowerSaveEnable == true)
        uart_config.flags = UART_WAKEUP_ENABLE;
    else
        uart_config.flags = UART_WAKEUP_DISABLE;
    MicoGpioOutputTrigger(MICO_SYS_LED);
    ring_buffer_init((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH);
    MicoUartInitialize(MICO_UART_2, &uart_config, (ring_buffer_t *)&rx_buffer);

    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uart_recv_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Send", uart_send_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);

    mico_rtos_delete_thread(NULL);
}
void uart_send_thread_DDE(uint32_t arg)
{
    uint8_t *SendBuffToMcu;
    SendBuffToMcu = malloc(sizeof(TxDataStc));
    mico_rtos_lock_mutex(&ElandStateDateMutex);
    ElandTranmmiteToMcu.Elandheader = '{';
    ElandTranmmiteToMcu.ElandTrail = '}';
    ElandTranmmiteToMcu.Elandstatus = ElandBegin;
    ElandTranmmiteToMcu.ElandRtctime.year = 0;
    ElandTranmmiteToMcu.ElandRtctime.month = 1;
    ElandTranmmiteToMcu.ElandRtctime.date = 1;
    ElandTranmmiteToMcu.ElandRtctime.weekday = 7;
    ElandTranmmiteToMcu.ElandRtctime.hr = 0;
    ElandTranmmiteToMcu.ElandRtctime.min = 0;
    ElandTranmmiteToMcu.ElandRtctime.sec = 0;

    mico_rtos_unlock_mutex(&ElandStateDateMutex);

    while (1)
    {
        mico_rtos_lock_mutex(&ElandStateDateMutex);
        memset(SendBuffToMcu, 0, sizeof(TxDataStc));
        memcpy(SendBuffToMcu, &(ElandTranmmiteToMcu), sizeof(TxDataStc));
        MicoUartSend(MICO_UART_2, SendBuffToMcu, sizeof(TxDataStc));
        mico_rtos_unlock_mutex(&ElandStateDateMutex);
        mico_thread_msleep(300); //300ms 发送一次
    }
    if (SendBuffToMcu != NULL)
        free(SendBuffToMcu);
    mico_rtos_delete_thread(NULL);
}

void uart_recv_thread_DDE(uint32_t arg)
{
    int8_t recvlen;
    uint8_t *inDataBuffer;
    inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
    require(inDataBuffer, exit);
    while (1)
    {
        recvlen = uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
        if (recvlen <= 0)
        {
            continue;
        }
        //printf( "\r\n" );
        for (int i = 0; i < recvlen; i++)
        {
            //printf( "%02x ", inDataBuffer[i] );
            MicoUartSend(MICO_UART_2, &(inDataBuffer[i]), 1);
        }
        //printf( "\r\n\r\n" );
        //uart_cmd_process( inDataBuffer, recvlen );
        //mico_rtos_set_semaphore( &postfog_sem );
    }
exit:
    if (inDataBuffer)
        free(inDataBuffer);
    mico_rtos_delete_thread(NULL);
}

int uart_get_one_packet(uint8_t *inBuf, int inBufLen)
{
    OSStatus err = kNoErr;
    char datalen;
    uint8_t *p;
    p = inBuf;
    err = MicoUartRecv(MICO_UART_2, p, 1, MICO_WAIT_FOREVER);
    MicoGpioOutputTrigger(MICO_SYS_LED);
    require_noerr(err, exit);
    require((*p == 0xA0), exit);
    for (int i = 0; i < 7; i++)
    {
        p++;
        err = MicoUartRecv(MICO_UART_2, p, 1, 500);

        require_noerr(err, exit);
    }
    datalen = *p;
    p++;
    err = MicoUartRecv(MICO_UART_2, p, datalen + 1, 500);
    require_noerr(err, exit);
    require(datalen + 9 <= inBufLen, exit);
    return datalen + 9; //返回帧的长度
exit:
    return -1;
}
void StateReceivethread(mico_thread_arg_t arg)
{
    UNUSED_PARAMETER(arg);

    OSStatus err;
    struct tm *currentTime;
    msg_queue received;
    while (1)
    {
        /*Wait until queue has data*/
        err = mico_rtos_pop_from_queue(&elandstate_queue, &received, MICO_WAIT_FOREVER);
        require_noerr(err, exit);
        mico_rtos_lock_mutex(&ElandStateDateMutex);
        if (received.type == Queue_UTC_type)
        {
            currentTime = localtime((const time_t *)&received.value);
            ElandTranmmiteToMcu.ElandRtctime.sec = currentTime->tm_sec;
            ElandTranmmiteToMcu.ElandRtctime.min = currentTime->tm_min;
            ElandTranmmiteToMcu.ElandRtctime.hr = currentTime->tm_hour;
            ElandTranmmiteToMcu.ElandRtctime.date = currentTime->tm_mday;
            ElandTranmmiteToMcu.ElandRtctime.weekday = currentTime->tm_wday;
            ElandTranmmiteToMcu.ElandRtctime.month = currentTime->tm_mon + 1;
            ElandTranmmiteToMcu.ElandRtctime.year = (currentTime->tm_year + 1900) % 100;
        }
        else if (received.type == Queue_ElandState_type)
        {
            ElandTranmmiteToMcu.Elandstatus = (Eland_Status_type)received.value;
        }
        mico_rtos_unlock_mutex(&ElandStateDateMutex);
        //netclock_uart_log( "Received data from queue:value = %d",received.value );
    }

exit:
    if (err != kNoErr)
        netclock_uart_log("Receiver exit with err: %d", err);

    mico_rtos_delete_thread(NULL);
}
void SendElandQueue(msg_queue_type Type, uint32_t value)
{
    msg_queue my_message;
    my_message.type = Type;
    my_message.value = value;
    mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);
}
