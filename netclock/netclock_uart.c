/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-10-31 17:39:58
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-11-01 10:47:31
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "netclock_uart.h"
#include "mico.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define Eland_uart_log(format, ...) custom_log("netclock_uart", format, ##__VA_ARGS__)

#define UART_BUFFER_LENGTH 2048
#define UART_ONE_PACKAGE_LENGTH 1024
#define STACK_SIZE_UART_RECV_THREAD 0x900

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
volatile ring_buffer_t rx_buffer;
volatile uint8_t rx_data[UART_BUFFER_LENGTH];

mico_mutex_t ElandUartMutex = NULL; //uart独立访问

uint16_t ElandStateDate[2];
mico_queue_t elandstate_queue = NULL;

mico_queue_t eland_uart_send_queue = NULL;    //eland HTTP的发送请求队列
mico_queue_t eland_uart_receive_queue = NULL; //eland HTTP的接收响应队列

mico_timer_t timer100_key;
/* Private function prototypes -----------------------------------------------*/
static void Eland_Key02_Send(void *arg);
static OSStatus push_usart_to_queue(uint8_t *usart_send);
static OSStatus get_usart_from_queue(void);
/* Private functions ---------------------------------------------------------*/

/*************************/
void start_uart_service(void)
{
    OSStatus err = kNoErr;
    //eland 状态数据互锁

    err = mico_rtos_init_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    //初始化eland uart 發送 send 消息
    err = mico_rtos_init_queue(&eland_uart_send_queue, "eland_uart_send_queue", sizeof(uint8_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    //初始化eland uart 發送 receive 消息
    err = mico_rtos_init_queue(&eland_uart_receive_queue, "eland_uart_receive_queue", sizeof(uint8_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    /*Register uart thread*/
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "netclock_uart_thread", netclock_uart_thread, 0x1000, (uint32_t)NULL);
    /*Register queue receive thread*/

    err = mico_init_timer(&timer100_key, 100, Eland_Key02_Send, NULL); //100ms 讀取一次
    require_noerr(err, exit);

    err = mico_start_timer(&timer100_key); //開始定時器
    require_noerr(err, exit);
    //Eland 设置状态 ※
    SendElandQueue(Queue_ElandState_type, ElandBegin);
exit:
    return;
}

void netclock_uart_thread(mico_thread_arg_t args)
{
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    /*UART receive thread*/
    // mico_context = mico_system_context_get();
    // uart_config.baud_rate = 115200;
    // uart_config.data_width = DATA_WIDTH_8BIT;
    // uart_config.parity = NO_PARITY;
    // uart_config.stop_bits = STOP_BITS_1;
    // uart_config.flow_control = FLOW_CONTROL_DISABLED;
    // if (mico_context->micoSystemConfig.mcuPowerSaveEnable == true)
    //     uart_config.flags = UART_WAKEUP_ENABLE;
    // else
    //     uart_config.flags = UART_WAKEUP_DISABLE;
    // ring_buffer_init((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH);
    // MicoUartInitialize(MICO_UART_2, &uart_config, (ring_buffer_t *)&rx_buffer);

    // mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uart_recv_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Send", uart_send_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);

    mico_rtos_delete_thread(NULL);
}
void uart_send_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    uint8_t *SendBuffToMcu = NULL;

    while (1)
    {
    WaitSend:
        if (SendBuffToMcu != NULL)
            free(SendBuffToMcu);
        err = mico_rtos_pop_from_queue(&eland_uart_send_queue, &SendBuffToMcu, MICO_WAIT_FOREVER);
        require_noerr(err, WaitSend);
        Eland_uart_log("size = %d %02x %02x %02x %02x", sizeof(SendBuffToMcu), *SendBuffToMcu, *(SendBuffToMcu + 1), *(SendBuffToMcu + 2), *(SendBuffToMcu + 3));
        Eland_uart_log("#####https connect#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
        // err = MicoUartSend(MICO_UART_2, SendBuffToMcu, sizeof(SendBuffToMcu));
        // require_noerr(err, WaitSend);
    }

    mico_rtos_delete_thread(NULL);
}

void uart_recv_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    int8_t recvlen;
    uint8_t *inDataBuffer, *DataReceived;
    inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
    require(inDataBuffer, exit);
    memset(inDataBuffer, 0, UART_ONE_PACKAGE_LENGTH);
    while (1)
    {
        recvlen = uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
        if (recvlen <= 0)
        {
            continue;
        }
        else
        {
            DataReceived = calloc(recvlen, sizeof(uint8_t));
            memcpy(DataReceived, inDataBuffer, recvlen);
            memset(inDataBuffer, 0, recvlen);
            err = mico_rtos_push_to_queue(&eland_uart_receive_queue, &DataReceived, 10);
            require_noerr_action(err, exit, Eland_uart_log("[error]mico_rtos_push_to_queue err"));
        }
    }
exit:
    if (inDataBuffer != NULL)
    {
        free(inDataBuffer);
        inDataBuffer = NULL;
    }
    if (DataReceived != NULL)
    {
        free(DataReceived);
        DataReceived = NULL;
    }
    mico_rtos_delete_thread(NULL);
}

int uart_get_one_packet(uint8_t *inBuf, int inBufLen)
{
    OSStatus err = kNoErr;
    char datalen;
    uint8_t *p;
    p = inBuf;
    err = MicoUartRecv(MICO_UART_2, p, 1, MICO_WAIT_FOREVER);
    require_noerr(err, exit);
    require((*p == Uart_Packet_Header), exit);
    for (int i = 0; i < 2; i++)
    {
        p++;
        err = MicoUartRecv(MICO_UART_2, p, 1, 10);
        require_noerr(err, exit);
    }
    datalen = *p;
    p++;
    err = MicoUartRecv(MICO_UART_2, p, datalen + 1, 10);
    require_noerr(err, exit);
    require(datalen + 4 <= inBufLen, exit);

    return datalen + 4; //返回帧的长度
exit:
    return -1;
}

void SendElandQueue(msg_queue_type Type, uint32_t value)
{
    msg_queue my_message;
    my_message.type = Type;
    my_message.value = value;
    mico_rtos_push_to_queue(&elandstate_queue, &my_message, MICO_WAIT_FOREVER);
}
//read key 02
static void Eland_Key02_Send(void *arg)
{
    OSStatus err = kGeneralErr;
    uint8_t *Cache = NULL;
    Cache = calloc(4, sizeof(uint8_t));
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = KEY_READ_02;
    *(Cache + 2) = 0;
    *(Cache + 3) = Uart_Packet_Trail;

    err = Eland_Uart_Push_Uart_Send_Mutex(Cache);
    if (err != kNoErr)
    {
        if (Cache != NULL)
            free(Cache);
    }
}

static void Eland_Key_destroy_timer(void)
{
    mico_stop_timer(&timer100_key);
    mico_deinit_timer(&timer100_key);
}

OSStatus Eland_Uart_Push_Uart_Send_Mutex(uint8_t *DataBody)
{
    OSStatus err = kGeneralErr;
    err = mico_rtos_lock_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    err = push_usart_to_queue(DataBody); //等待返回數據
    require_noerr(err, exit);

// err = get_usart_from_queue(); //等待返回數據
// require_noerr(err, exit);

exit:
    err = mico_rtos_unlock_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    return err;
}

static OSStatus push_usart_to_queue(uint8_t *usart_send)
{
    OSStatus err = kGeneralErr;
    if (mico_rtos_is_queue_full(&eland_uart_send_queue) == true)
    {
        Eland_uart_log("eland_uart_send_queue is full!");
        goto exit;
    }

    err = mico_rtos_push_to_queue(&eland_uart_send_queue, &usart_send, 10);
    require_noerr(err, exit);

    if (err != kNoErr)
    {
        if (usart_send != NULL)
        {
            free(usart_send);
            usart_send = NULL;
        }
        Eland_uart_log("push req to eland_uart_send_queue error");
        goto exit;
    }
    return kNoErr;
exit:
    return err;
}
static OSStatus get_usart_from_queue(void)
{
    OSStatus err = kGeneralErr;
    uint16_t rec_len;
    uint8_t *usart_rec, *usart_sen;
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &usart_rec, 10);
    if (err != kNoErr)
    {
        Eland_uart_log("mico_rtos_pop_from_queue() timeout!!!");
        if (mico_rtos_is_queue_full(&eland_uart_send_queue) == true)
        {
            err = mico_rtos_pop_from_queue(&eland_uart_send_queue, &usart_sen, 10);
            if (err == kNoErr)
            {
                if (usart_sen != NULL)
                {
                    free(usart_sen);
                    usart_sen = NULL;
                }
                Eland_uart_log("push one req from queue!");
            }
        }
        return kGeneralErr;
    }
    rec_len = sizeof(usart_rec);
    if (*(usart_rec + rec_len - 1) == Uart_Packet_Trail)
    {
        switch (*(usart_rec + 1))
        {
        case KEY_READ_02:
            process_02_fun(usart_rec);
            break;
        default:
            break;
        }
    }

    if (usart_rec != NULL)
    {
        free(usart_rec);
        usart_rec = NULL;
    }
    return err;
}

void process_02_fun(uint8_t *usart_rec)
{
}
