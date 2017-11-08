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
#include "../alilodemo/audio_test.h"
#include "../alilodemo/hal_alilo_rabbit.h"

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

mico_queue_t elandstate_queue = NULL;

mico_queue_t eland_uart_send_queue = NULL;    //eland HTTP的发送请求队列
mico_queue_t eland_uart_receive_queue = NULL; //eland HTTP的接收响应队列

mico_timer_t timer100_key;

uint8_t count_key_time = 0;
/* Private function prototypes -----------------------------------------------*/
static void Eland_Key02_Send(void *arg);
static OSStatus push_usart_to_queue(__msg_send_queue_t *usart_send);
static OSStatus get_usart_from_queue(void);
void MODH_Read_02H(__msg_send_queue_t *usart_rec);
/* Private functions ---------------------------------------------------------*/

/*************************/
void start_uart_service(void)
{
    OSStatus err = kNoErr;
    //eland 状态数据互锁

    err = mico_rtos_init_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    //初始化eland uart 發送 send 消息
    err = mico_rtos_init_queue(&eland_uart_send_queue, "eland_uart_send_queue", sizeof(__msg_send_queue_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    //初始化eland uart 發送 receive 消息
    err = mico_rtos_init_queue(&eland_uart_receive_queue, "eland_uart_receive_queue", sizeof(__msg_send_queue_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    Eland_uart_log("start uart");

    /*Register uart thread*/
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "netclock_uart_thread", netclock_uart_thread, 0x1000, (uint32_t)NULL);
    /*Register queue receive thread*/

    err = mico_init_timer(&timer100_key, 100, Eland_Key02_Send, NULL); //100ms 讀取一次
    require_noerr(err, exit);

    err = mico_start_timer(&timer100_key); //開始定時器
    require_noerr(err, exit);
    //Eland 设置状态
    SendElandQueue(Queue_ElandState_type, ElandBegin);
exit:
    return;
}

void netclock_uart_thread(mico_thread_arg_t args)
{
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    /*UART receive thread*/
    mico_context = mico_system_context_get();
    uart_config.baud_rate = 115200;
    uart_config.data_width = DATA_WIDTH_8BIT;
    uart_config.parity = NO_PARITY;
    uart_config.stop_bits = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
    if (mico_context->micoSystemConfig.mcuPowerSaveEnable == true)
        uart_config.flags = UART_WAKEUP_ENABLE;
    else
        uart_config.flags = UART_WAKEUP_DISABLE;
    ring_buffer_init((ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH);
    MicoUartInitialize(MICO_UART_2, &uart_config, (ring_buffer_t *)&rx_buffer);
    Eland_uart_log("start receive");

    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uart_recv_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Send", uart_send_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);

    mico_rtos_delete_thread(NULL);
}
void uart_send_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    __msg_send_queue_t *SendBuffToMcu;
    while (1)
    {
    WaitSend:
        if (SendBuffToMcu != NULL)
        {
            if (SendBuffToMcu->data != NULL)
            {
                free(SendBuffToMcu->data);
                SendBuffToMcu->data = NULL;
            }
            free(SendBuffToMcu);
            SendBuffToMcu = NULL;
        }
        err = mico_rtos_pop_from_queue(&eland_uart_send_queue, &SendBuffToMcu, MICO_WAIT_FOREVER);
        require_noerr(err, WaitSend);
        err = MicoUartSend(MICO_UART_2, SendBuffToMcu->data, SendBuffToMcu->length);
        require_noerr(err, WaitSend);
        Eland_uart_log("#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
    }

    mico_rtos_delete_thread(NULL);
}

void uart_recv_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    int8_t recvlen;
    uint8_t *inDataBuffer;
    __msg_send_queue_t *DataReceived;

    inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
    require(inDataBuffer, exit);
    memset(inDataBuffer, 0, UART_ONE_PACKAGE_LENGTH);

    DataReceived = malloc(sizeof(__msg_send_queue_t));
    require(DataReceived, exit);
    memset(DataReceived, 0, sizeof(__msg_send_queue_t));

    //Eland_uart_log("size = %d %02x %02x %02x %02x", sizeof(SendBuffToMcu), *SendBuffToMcu, *(SendBuffToMcu + 1), *(SendBuffToMcu + 2), *(SendBuffToMcu + 3));

    while (1)
    {
        recvlen = uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
        if (recvlen <= 0)
        {
            continue;
        }
        else
        {
            DataReceived->length = recvlen;
            DataReceived->data = calloc(recvlen, sizeof(uint8_t));
            memcpy(DataReceived->data, inDataBuffer, recvlen);
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
        if (DataReceived->data)
        {
            free(DataReceived->data);
            DataReceived->data = NULL;
        }
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
    Eland_uart_log("received  ********");
    err = MicoUartRecv(MICO_UART_2, p, 1, MICO_WAIT_FOREVER);
    Eland_uart_log("received  %02x", *p);

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
    __msg_send_queue_t *msg_send;
    uint8_t *Cache;

    msg_send = (__msg_send_queue_t *)calloc(sizeof(__msg_send_queue_t), sizeof(uint8_t));

    msg_send->length = 5;

    Cache = calloc(msg_send->length, sizeof(uint8_t));
    msg_send->data = Cache;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = KEY_READ_02;
    *(Cache + 2) = 1;
    *(Cache + 3) = count_key_time;
    *(Cache + 4) = Uart_Packet_Trail;
    Eland_uart_log("size of cache %d", sizeof(*Cache));
    err = Eland_Uart_Push_Uart_Send_Mutex(msg_send);
    if (err != kNoErr)
    {
        if (Cache != NULL)
            free(Cache);
        if (msg_send != NULL)
            free(msg_send);
    }
}

static void Eland_Key_destroy_timer(void)
{
    mico_stop_timer(&timer100_key);
    mico_deinit_timer(&timer100_key);
}

OSStatus Eland_Uart_Push_Uart_Send_Mutex(__msg_send_queue_t *DataBody)
{
    OSStatus err = kGeneralErr;
    err = mico_rtos_lock_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    err = push_usart_to_queue(DataBody); //等待返回數據
    require_noerr(err, exit);

    err = get_usart_from_queue(); //等待返回數據
    require_noerr(err, exit);

exit:
    err = mico_rtos_unlock_mutex(&ElandUartMutex);
    require_noerr(err, exit);

    return err;
}

static OSStatus push_usart_to_queue(__msg_send_queue_t *usart_send)
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
            if (usart_send->data != NULL)
            {
                free(usart_send->data);
                usart_send->data = NULL;
            }
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
    __msg_send_queue_t *usart_rec, *usart_sen;
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &usart_rec, 20);
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
                    if (usart_sen->data != NULL)
                    {
                        free(usart_sen->data);
                        usart_sen = NULL;
                    }
                    free(usart_sen);
                    usart_sen = NULL;
                }
                Eland_uart_log("push one req from queue!");
            }
        }
        return kGeneralErr;
    }
    rec_len = usart_rec->length;
    if (*(usart_rec->data + rec_len - 1) == Uart_Packet_Trail)
    {
        switch (*(usart_rec->data + 1))
        {
        case KEY_READ_02:
            MODH_Read_02H(usart_rec);
            break;
        default:
            break;
        }
    }
    else
        err = kGeneralErr;

    if (usart_rec != NULL)
    {
        if (usart_rec->data != NULL)
        {
            free(usart_rec->data);
            usart_rec->data = NULL;
        }
        free(usart_rec);
        usart_rec = NULL;
    }
    return err;
}
extern void PlatformEasyLinkButtonLongPressedCallback(void);
void MODH_Read_02H(__msg_send_queue_t *usart_rec)
{
    static uint16_t Key_Count = 0, Key_Restain = 0;
    static uint16_t Reset_key_Restain_Trg, Reset_key_Restain_count;
    static uint16_t KEY_Snooze_Count_Trg, KEY_Snooze_Count_count;
    static uint16_t KEY_Minus_Count_Trg, KEY_Minus_Count_count;
    static uint16_t KEY_Add_Count_Trg, KEY_Add_Count_count;

    uint16_t ReadData;

    Key_Count = (uint16_t)((*(usart_rec->data + 3)) << 8) | *(usart_rec->data + 4);
    Key_Restain = (uint16_t)((*(usart_rec->data + 5)) << 8) | *(usart_rec->data + 6);

    ReadData = Key_Restain & KEY_Reset;
    Reset_key_Restain_Trg = ReadData & (ReadData ^ Reset_key_Restain_count);
    Reset_key_Restain_count = ReadData;
    if (Reset_key_Restain_Trg)
        PlatformEasyLinkButtonLongPressedCallback();

    ReadData = Key_Count & KEY_Minus;
    KEY_Add_Count_Trg = ReadData & (ReadData ^ KEY_Add_Count_count);
    KEY_Add_Count_count = ReadData;
    if (KEY_Add_Count_Trg)
        count_key_time++;

    ReadData = Key_Count & KEY_Minus;
    KEY_Minus_Count_Trg = ReadData & (ReadData ^ KEY_Minus_Count_count);
    KEY_Minus_Count_count = ReadData;
    if (KEY_Minus_Count_Trg)
        count_key_time++;

    ReadData = Key_Count & KEY_Snooze;
    KEY_Snooze_Count_Trg = ReadData & (ReadData ^ KEY_Snooze_Count_count);
    KEY_Snooze_Count_count = ReadData;
    if (KEY_Snooze_Count_Trg)
    {count_key_time++;
        if (flash_play_Sem != NULL)
            mico_rtos_set_semaphore(&flash_play_Sem);
    }

    if (((KEY_Add | KEY_Minus) & Key_Restain) == (KEY_Add | KEY_Minus))
    {
        if (flagAudioPlay == 1)
            mico_rtos_set_semaphore(&urlFileDownload_Sem);
        if ((flagAudioPlay == 2) || (flagAudioPlay == 3))
            mico_rtos_set_semaphore(&urlPalyStreamStop_Sem);
    }
}
