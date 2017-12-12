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
#include "netclock_httpd.h"
#include "../alilodemo/inc/audio_service.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define Eland_uart_log(format, ...) custom_log("netclock_uart", format, ##__VA_ARGS__)

#define UART_BUFFER_LENGTH 2048
#define UART_ONE_PACKAGE_LENGTH 1024
#define STACK_SIZE_UART_RECV_THREAD 0x500

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
volatile ring_buffer_t rx_buffer;
volatile uint8_t rx_data[UART_BUFFER_LENGTH];

mico_mutex_t ElandUartMutex = NULL; //uart独立访问

mico_queue_t eland_uart_receive_queue = NULL; //eland usart
mico_queue_t eland_uart_CMD_queue = NULL;     //eland usart
mico_queue_t eland_satae_queue = NULL;        //eland usart

mico_semaphore_t Is_usart_complete_sem = NULL;

mico_timer_t timer100_key;
uint8_t count_key_time = 0;

/* Private function prototypes -----------------------------------------------*/
static void Eland_H02_Send(void);
static void Eland_H03_Send(void);
static void Eland_H04_Send(void);
static void ELAND_H05_Send(void);
static void MODH_Opration_02H(uint8_t *usart_rec);
static void MODH_Opration_03H(uint8_t *usart_rec);
static void MODH_Opration_04H(uint8_t *usart_rec);
static void MODH_Opration_05H(uint8_t *usart_rec);
static void timer100_key_handle(void *arg);
static void uart_thread_DDE(uint32_t arg);
static OSStatus elandUsartSendData(uint8_t *data, uint32_t lenth);
static void Opration_Packet(uint8_t *data);
/* Private functions ---------------------------------------------------------*/

/*************************/
void start_uart_service(void)
{
    OSStatus err = kNoErr;
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    //eland 状态数据互锁

    err = mico_rtos_init_mutex(&ElandUartMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_semaphore(&Is_usart_complete_sem, 2);

    //初始化eland_uart_CMD_queue 發送 CMD 消息 初始化eland uart 發送 send 消息
    err = mico_rtos_init_queue(&eland_uart_CMD_queue, "eland_uart_CMD_queue", sizeof(eland_usart_cmd_t), 3); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    //eland_satae_queue
    err = mico_rtos_init_queue(&eland_satae_queue, "eland_satae_queue", sizeof(Eland_Status_type_t), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    //初始化eland uart 發送 receive 消息 CMD
    err = mico_rtos_init_queue(&eland_uart_receive_queue, "eland_uart_receive_queue", sizeof(eland_usart_cmd_t), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    Eland_uart_log("start uart");

    /*Register uart thread*/

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
    Eland_uart_log("start thread");

    /*UART receive thread*/
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uart_recv_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);
    require_noerr(err, exit);
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "uart_thread_DDE", uart_thread_DDE, STACK_SIZE_UART_RECV_THREAD, 0);
    require_noerr(err, exit);
    /*Register queue receive thread*/
    err = mico_rtos_get_semaphore(&Is_usart_complete_sem, MICO_WAIT_FOREVER); //two threads
    require_noerr(err, exit);
    err = mico_rtos_get_semaphore(&Is_usart_complete_sem, MICO_WAIT_FOREVER); //two times
    require_noerr(err, exit);
    err = mico_rtos_deinit_semaphore(&Is_usart_complete_sem);
    require_noerr(err, exit);

    Eland_uart_log("start usart timer");
    err = mico_init_timer(&timer100_key, 100, timer100_key_handle, NULL); //100ms 讀取一次
    require_noerr(err, exit);

    err = mico_start_timer(&timer100_key); //開始定時器
    require_noerr(err, exit);
    //Eland 设置状态
    SendElandStateQueue(ElandBegin);
exit:
    return;
}

void uart_recv_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    int8_t recvlen;
    uint8_t *inDataBuffer;
    eland_usart_cmd_t received_cmd = ELAND_CMD_NONE;

    inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
    require(inDataBuffer, exit);
    memset(inDataBuffer, 0, UART_ONE_PACKAGE_LENGTH);

    Eland_uart_log("start receive");
    mico_rtos_set_semaphore(&Is_usart_complete_sem);
    while (1)
    {
    start_receive_paket:
        recvlen = uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
        Eland_uart_log("#####start connect#####:num_of_chunks:%d,allocted_memory:%d, free:%d, total_memory:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->allocted_memory, MicoGetMemoryInfo()->free_memory, MicoGetMemoryInfo()->total_memory);
        if (recvlen <= 0)
        {
            continue;
        }
        else
        {
            Opration_Packet(inDataBuffer);
            received_cmd = *(inDataBuffer + 1);
            memset(inDataBuffer, 0, recvlen);
            err = mico_rtos_push_to_queue(&eland_uart_receive_queue, &received_cmd, 10);
            require_noerr_action(err, start_receive_paket, Eland_uart_log("[error]mico_rtos_push_to_queue err"));
        }
    }
exit:
    if (inDataBuffer != NULL)
    {
        free(inDataBuffer);
        inDataBuffer = NULL;
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
    require((*(p + datalen) == Uart_Packet_Trail), exit);

    return datalen + 4; //返回帧的长度
exit:
    return -1;
}
static void Opration_Packet(uint8_t *data)
{
    switch (*(data + 1))
    {
    case KEY_READ_02:
        MODH_Opration_02H(data);
        break;
    case TIME_SET_03:
        MODH_Opration_03H(data);
        break;
    case TIME_READ_04:
        MODH_Opration_04H(data);
        break;
    case ELAND_STATES_05:
        MODH_Opration_05H(data);
        break;
    default:
        break;
    }
}
static void uart_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    eland_usart_cmd_t eland_cmd = ELAND_CMD_NONE;
    mico_rtos_set_semaphore(&Is_usart_complete_sem);
    while (1)
    {
        Eland_uart_log("start usart timer %d", err);
        err = mico_rtos_pop_from_queue(&eland_uart_CMD_queue, &eland_cmd, MICO_WAIT_FOREVER);

        if (err != kNoErr)
            continue;
        switch (eland_cmd)
        {
        case ELAND_SEND_CMD_02H: //read key value
            Eland_H02_Send();
            break;
        case ELAND_SEND_CMD_03H: //set MCU_RTC time
            Eland_H03_Send();
            break;
        case ELAND_SEND_CMD_04H: //
            Eland_H04_Send();
            break;
        case ELAND_SEND_CMD_05H: //
            ELAND_H05_Send();
            count_key_time++;
            break;
        default:
            break;
        }
    }
    mico_rtos_delete_thread(NULL);
}

void SendElandStateQueue(Eland_Status_type_t value)
{
    Eland_Status_type_t state = value;
    eland_usart_cmd_t eland_cmd = ELAND_SEND_CMD_05H;
    if (eland_satae_queue == NULL)
        return;
    if (mico_rtos_is_queue_full(&eland_satae_queue)) //if full pick out data then update state
        mico_rtos_pop_from_queue(&eland_satae_queue, &state, 0);
    else
    {
        state = value;
        mico_rtos_push_to_queue(&eland_satae_queue, &state, 10);
    }

    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
}
static void timer100_key_handle(void *arg)
{
    eland_usart_cmd_t eland_cmd = ELAND_SEND_CMD_02H;

    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 10);
}

// static void Eland_Key_destroy_timer(void)
// {
//     mico_stop_timer(&timer100_key);
//     mico_deinit_timer(&timer100_key);
// }

static OSStatus elandUsartSendData(uint8_t *data, uint32_t lenth)
{
    OSStatus err;
    // err = mico_rtos_lock_mutex(&stdio_tx_mutex);
    // require_noerr(err, exit);
    err = MicoUartSend(MICO_UART_2, data, lenth);
    require_noerr(err, exit);
    // err = mico_rtos_unlock_mutex(&stdio_tx_mutex);
    // require_noerr(err, exit);
exit:
    return err;
}

//read key 02
static void Eland_H02_Send(void)
{
    OSStatus err = kGeneralErr;
    uint8_t *Cache;
    eland_usart_cmd_t received_cmd = ELAND_CMD_NONE;
    Cache = calloc(5, sizeof(uint8_t));

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = KEY_READ_02;
    *(Cache + 2) = 1;
    *(Cache + 3) = count_key_time;
    *(Cache + 4) = Uart_Packet_Trail;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == KEY_READ_02)
        err = kNoErr;

exit:
    if (Cache != NULL)
        free(Cache);
}
//time set 03
static void Eland_H03_Send(void)
{
    OSStatus err = kGeneralErr;
    uint8_t *Cache;
    mico_rtc_time_t cur_time;

    Cache = calloc(4 + sizeof(mico_rtc_time_t), sizeof(uint8_t));
    MicoRtcGetTime(&cur_time); //返回新的时间值
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = TIME_SET_03;
    *(Cache + 2) = sizeof(mico_rtc_time_t);
    memcpy((Cache + 3), &cur_time, sizeof(mico_rtc_time_t));
    *(Cache + 3 + sizeof(mico_rtc_time_t)) = Uart_Packet_Trail;
    err = elandUsartSendData(Cache, 4 + sizeof(mico_rtc_time_t));
    require_noerr(err, exit);
exit:
    if (Cache != NULL)
        free(Cache);
}
//read time 04
static void Eland_H04_Send(void)
{
    OSStatus err = kGeneralErr;
    uint8_t *Cache;

    Cache = calloc(5, sizeof(uint8_t));

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = TIME_READ_04;
    *(Cache + 2) = 1;
    *(Cache + 3) = count_key_time;
    *(Cache + 4) = Uart_Packet_Trail;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);
exit:
    if (Cache != NULL)
        free(Cache);
}
//ELAND_STATES_05
static void ELAND_H05_Send(void)
{
    OSStatus err = kGeneralErr;
    uint8_t *Cache;
    Eland_Status_type_t state = ElandNone;
    err = mico_rtos_pop_from_queue(&eland_satae_queue, &state, 0);
    if (err != kNoErr)
        return;

    Cache = calloc(5, sizeof(uint8_t));
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ELAND_STATES_05;
    *(Cache + 2) = 1;
    *(Cache + 3) = (uint8_t)state;
    *(Cache + 4) = Uart_Packet_Trail;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);
exit:
    if (Cache != NULL)
        free(Cache);
}
extern void PlatformEasyLinkButtonLongPressedCallback(void);
static void MODH_Opration_02H(uint8_t *usart_rec)
{
    static uint16_t Key_Count = 0, Key_Restain = 0;
    static uint16_t Reset_key_Restain_Trg, Reset_key_Restain_count;
    static uint16_t KEY_Add_Count_Trg, KEY_Add_Count_count;
    static uint16_t KEY_Minus_Count_Trg, KEY_Minus_Count_count;
    static uint16_t Set_Minus_Restain_Trg, Set_Minus_Restain_count;
    uint16_t ReadData;
    eland_usart_cmd_t eland_cmd = ELAND_CMD_NONE;

    Key_Count = (uint16_t)((*(usart_rec + 3)) << 8) | *(usart_rec + 4);
    Key_Restain = (uint16_t)((*(usart_rec + 5)) << 8) | *(usart_rec + 6);

    ReadData = Key_Restain & KEY_Reset;
    Reset_key_Restain_Trg = ReadData & (ReadData ^ Reset_key_Restain_count);
    Reset_key_Restain_count = ReadData;
    if (Reset_key_Restain_Trg)
        PlatformEasyLinkButtonLongPressedCallback();

    ReadData = Key_Count & KEY_Add;
    KEY_Add_Count_Trg = ReadData & (ReadData ^ KEY_Add_Count_count);
    KEY_Add_Count_count = ReadData;
    if (KEY_Add_Count_Trg)
        count_key_time++;

    if ((Key_Restain & KEY_Set) && (Key_Restain & KEY_Minus))
        ReadData = (KEY_Set | KEY_Minus);
    else
        ReadData = 0;
    Set_Minus_Restain_Trg = ReadData & (ReadData ^ Set_Minus_Restain_count);
    Set_Minus_Restain_count = ReadData;
    if (Set_Minus_Restain_Trg == (KEY_Set | KEY_Minus))
    {
        mico_rtos_set_semaphore(&httpServer_softAP_event_Sem); //start Soft_AP mode
        count_key_time++;
    }
}
static void MODH_Opration_03H(uint8_t *usart_rec)
{
}
static void MODH_Opration_04H(uint8_t *usart_rec)
{
    mico_rtc_time_t cur_time = {0};
    uint8_t *rec_data;
    rec_data = usart_rec;
    memcpy(&cur_time, (rec_data + 3), sizeof(mico_rtc_time_t));
    MicoRtcSetTime(&cur_time); //初始化 RTC 时钟的时间
}
static void MODH_Opration_05H(uint8_t *usart_rec)
{
}
