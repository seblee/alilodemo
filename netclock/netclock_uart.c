/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-10-31 17:39:58
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-27 17:59:53
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "netclock_uart.h"
#include "mico.h"
#include "netclock_httpd.h"
#include "netclockconfig.h"
#include "../alilodemo/inc/audio_service.h"
#include "eland_mcu_ota.h"
#include "eland_alarm.h"
#include "netclock_wifi.h"
#include "eland_alarm.h"
#include "eland_tcp.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define Eland_uart_log(format, ...) custom_log("netclock_uart", format, ##__VA_ARGS__)

#define UART_BUFFER_LENGTH 1024
#define UART_ONE_PACKAGE_LENGTH 512
#define STACK_SIZE_UART_RECV_THREAD 0x2000

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
volatile ring_buffer_t rx_buffer;
volatile uint8_t rx_data[UART_BUFFER_LENGTH];

mico_queue_t eland_uart_receive_queue = NULL; //eland usart
mico_queue_t eland_uart_CMD_queue = NULL;     //eland usart
mico_queue_t eland_state_queue = NULL;        //eland usart
mico_semaphore_t Is_usart_complete_sem = NULL;
mico_timer_t timer100_key;
__ELAND_MODE_STATE_t eland_mode_state = {ELAND_MODE_NONE, ElandNone, NULL};
/* Private function prototypes -----------------------------------------------*/
static void uart_service(uint32_t arg);
static void Eland_H02_Send(uint8_t *Cache);
static void Eland_H03_Send(uint8_t *Cache);
static void Eland_H04_Send(uint8_t *Cache);
static void ELAND_H05_Send(uint8_t *Cache);
static void ELAND_H06_Send(uint8_t *Cache);
static void ELAND_H08_Send(uint8_t *Cache);
static void ELAND_H10_Send(uint8_t *Cache);
static void MODH_Opration_02H(uint8_t *usart_rec);
static void MODH_Opration_04H(uint8_t *usart_rec);
static void MODH_Opration_xxH(__msg_function_t funtype, uint8_t *usart_rec);
static void MODH_Opration_10H(uint8_t *usart_rec);
static void timer100_key_handle(void *arg);
static void uart_thread_DDE(uint32_t arg);
static OSStatus elandUsartSendData(uint8_t *data, uint32_t lenth);
static void Opration_Packet(uint8_t *data);

static uint16_t get_eland_mode_state(void);
static void set_eland_mode(_ELAND_MODE_t mode);
static void set_eland_state(Eland_Status_type_t state);
static void reset_eland_flash_para(void);
/* Private functions ---------------------------------------------------------*/

OSStatus start_uart_service(void)
{
    OSStatus err = kNoErr;
    /*Register user function for MiCO nitification: WiFi status changed*/
    err = mico_rtos_init_mutex(&eland_mode_state.state_mutex);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "uart_service", uart_service, STACK_SIZE_UART_RECV_THREAD, 0);
exit:
    return err;
}

/*************************/
static void uart_service(uint32_t arg)
{
    OSStatus err = kNoErr;
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    uint32_t ota_arg;
    __msg_function_t eland_cmd = KEY_FUN_NONE;
    /*wait mcu passed bootloader*/
    mico_rtos_thread_msleep(1100);
    /*creat mcu_ota thread*/

    err = mico_rtos_create_thread(&MCU_OTA_thread, MICO_APPLICATION_PRIORITY, "mcu_ota_thread", mcu_ota_thread, STACK_SIZE_UART_RECV_THREAD, (uint32_t)&ota_arg);
    require_noerr(err, exit);
    /*wait ota thread*/
    mico_rtos_thread_join(&MCU_OTA_thread);

    if (ota_arg == kGeneralErr)
        Eland_uart_log("mcu ota err");
    // goto exit;
    //eland 状态数据互锁
    err = mico_rtos_init_semaphore(&Is_usart_complete_sem, 2);
    //初始化eland_uart_CMD_queue 發送 CMD 消息 初始化eland uart 發送 send 消息
    err = mico_rtos_init_queue(&eland_uart_CMD_queue, "eland_uart_CMD_queue", sizeof(__msg_function_t), 5); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    //eland_state_queue
    err = mico_rtos_init_queue(&eland_state_queue, "eland_state_queue", sizeof(Eland_Status_type_t), 5); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    //初始化eland uart 發送 receive 消息 CMD
    err = mico_rtos_init_queue(&eland_uart_receive_queue, "eland_uart_receive_queue", sizeof(__msg_function_t), 1); //只容纳一个成员 传递的只是地址
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
    /*read mcu rtc time*/
    eland_cmd = TIME_READ_04;
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
    Eland_uart_log("start usart timer");
    /*configuration usart timer*/
    err = mico_init_timer(&timer100_key, 100, timer100_key_handle, NULL); //100ms 讀取一次
    require_noerr(err, exit);
    /*start key read timer*/
    err = mico_start_timer(&timer100_key); //開始定時器
    require_noerr(err, exit);
    //Eland 设置状态
    SendElandStateQueue(ElandBegin);
exit:
    mico_rtos_delete_thread(NULL);
}

void uart_recv_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    int8_t recvlen;
    uint8_t *inDataBuffer;
    __msg_function_t received_cmd = KEY_FUN_NONE;

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
        MODH_Opration_xxH(TIME_SET_03, data);
        break;
    case TIME_READ_04:
        MODH_Opration_04H(data);
        break;
    case ELAND_STATES_05:
        MODH_Opration_xxH(ELAND_STATES_05, data);
        break;
    case SEND_FIRM_WARE_06:
        MODH_Opration_xxH(SEND_FIRM_WARE_06, data);
        break;
    case SEND_LINK_STATE_08:
        MODH_Opration_xxH(SEND_LINK_STATE_08, data);
        break;
    case ALARM_READ_10:
        MODH_Opration_10H(data);
        break;
    default:
        break;
    }
}
static void uart_thread_DDE(uint32_t arg)
{
    OSStatus err = kNoErr;
    __msg_function_t eland_cmd = KEY_FUN_NONE;

    uint8_t *inDataBuffer;

    inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
    require(inDataBuffer, exit);
    memset(inDataBuffer, 0, UART_ONE_PACKAGE_LENGTH);

    mico_rtos_set_semaphore(&Is_usart_complete_sem);
    while (1)
    {
        Eland_uart_log("start usart timer %d", err);
        err = mico_rtos_pop_from_queue(&eland_uart_CMD_queue, &eland_cmd, MICO_WAIT_FOREVER);

        if (err != kNoErr)
            continue;
        switch (eland_cmd)
        {
        case KEY_READ_02: /* read key value*/
            Eland_H02_Send(inDataBuffer);
            break;
        case TIME_SET_03: /* set MCU_RTC time*/
            Eland_H03_Send(inDataBuffer);
            break;
        case TIME_READ_04: /* READ MCU TIME*/
            Eland_H04_Send(inDataBuffer);
            break;
        case ELAND_STATES_05: /* SEND ELAND STATES*/
            ELAND_H05_Send(inDataBuffer);
            break;
        case SEND_FIRM_WARE_06: /* SEND ELAND FIRMWARE*/
            ELAND_H06_Send(inDataBuffer);
            break;
        case SEND_LINK_STATE_08: /* SEND WIFI LINK STATE*/
            ELAND_H08_Send(inDataBuffer);
            break;
        case ALARM_READ_10: /* READ MCU ALARM*/
            ELAND_H10_Send(inDataBuffer);
            break;
        default:
            break;
        }
        mico_rtos_thread_msleep(5);
    }
exit:
    mico_rtos_delete_thread(NULL);
}

void SendElandStateQueue(Eland_Status_type_t value)
{
    Eland_Status_type_t state = value;
    __msg_function_t eland_cmd = ELAND_STATES_05;
    set_eland_state(value);

    if (eland_state_queue == NULL)
        return;
    if (mico_rtos_is_queue_full(&eland_state_queue)) //if full pick out data then update state
        mico_rtos_pop_from_queue(&eland_state_queue, &state, 0);
    else
    {
        state = value;
        mico_rtos_push_to_queue(&eland_state_queue, &state, 10);
    }
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
}
static void timer100_key_handle(void *arg)
{
    static uint16_t Timer_count = 0;
    __msg_function_t eland_cmd = KEY_READ_02;
    if (Timer_count++ > 50)
    {
        Timer_count = 0;
        eland_cmd = SEND_LINK_STATE_08;
    }
    else
        eland_cmd = KEY_READ_02;
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 10);
}

// static void Eland_Key_destroy_timer(void)
// {
//     mico_stop_timer(&timer100_key);
//     mico_deinit_timer(&timer100_key);
// }
extern mico_mutex_t stdio_tx_mutex;
static OSStatus elandUsartSendData(uint8_t *data, uint32_t lenth)
{
    OSStatus err;
#ifndef MICO_SYSTEM_LOG_INFO_DISABLE
    err = mico_rtos_lock_mutex(&stdio_tx_mutex);
    require_noerr(err, exit);
#endif
    err = MicoUartSend(MICO_UART_2, data, lenth);
    require_noerr(err, exit);
#ifndef MICO_SYSTEM_LOG_INFO_DISABLE
    err = mico_rtos_unlock_mutex(&stdio_tx_mutex);
    require_noerr(err, exit);
#endif
exit:
    return err;
}

//read key 02
static void Eland_H02_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    micoMemInfo_t *memory_info;
    static uint8_t timesend02 = 0;
    uint8_t len;

    if (timesend02 > 5)
    {
        timesend02 = 0;
        memory_info = MicoGetMemoryInfo();
        *Cache = Uart_Packet_Header;
        *(Cache + 1) = KEY_READ_02;
        *(Cache + 2) = sizeof(int) + sizeof(int);
        *((int *)(Cache + 3)) = memory_info->num_of_chunks;
        *((int *)(Cache + 3 + sizeof(int))) = memory_info->free_memory;
        *(Cache + 3 + sizeof(int) + sizeof(int)) = Uart_Packet_Trail;
        len = 4 + sizeof(int) + sizeof(int);
    }
    else
    {
        timesend02++;
        *Cache = Uart_Packet_Header;
        *(Cache + 1) = KEY_READ_02;
        *(Cache + 2) = 1;
        *(Cache + 3) = timesend02;
        *(Cache + 4) = Uart_Packet_Trail;
        len = 4 + 1;
    }
    // *Cache = Uart_Packet_Header;
    // *(Cache + 1) = KEY_READ_02;
    // *(Cache + 2) = 1;
    // *(Cache + 3) = timesend02++;
    // *(Cache + 4) = Uart_Packet_Trail;
    // len = 4 + 1;

start_send:
    sended_times--;
    err = elandUsartSendData(Cache, len);
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)KEY_READ_02)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
//time set 03
static void Eland_H03_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    mico_rtc_time_t cur_time;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;

    MicoRtcGetTime(&cur_time); //返回新的时间值
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = TIME_SET_03;
    *(Cache + 2) = sizeof(mico_rtc_time_t);
    memcpy((Cache + 3), &cur_time, sizeof(mico_rtc_time_t));
    *(Cache + 3 + sizeof(mico_rtc_time_t)) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4 + sizeof(mico_rtc_time_t));
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)TIME_SET_03)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
//read time 04
static void Eland_H04_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = TIME_READ_04;
    *(Cache + 2) = 1;
    *(Cache + 3) = 0;
    *(Cache + 4) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)TIME_READ_04)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
//ELAND_STATES_05
static void ELAND_H05_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    Eland_Status_type_t state = ElandNone;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    err = mico_rtos_pop_from_queue(&eland_state_queue, &state, 0);
    if (err != kNoErr)
        return;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ELAND_STATES_05;
    *(Cache + 2) = 1;
    *(Cache + 3) = (uint8_t)state;
    *(Cache + 4) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)ELAND_STATES_05)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H06_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = SEND_FIRM_WARE_06;
    *(Cache + 2) = strlen(Eland_Firmware_Version);
    sprintf((char *)(Cache + 3), "%s", Eland_Firmware_Version);
    *(Cache + 3 + strlen(Eland_Firmware_Version)) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4 + strlen(Eland_Firmware_Version));
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)SEND_FIRM_WARE_06)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}

static void ELAND_H08_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    uint16_t mode_state_temp = 0;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    LCD_Wifi_Rssi_t rssi_level = LEVELNUM;
    LinkStatusTypeDef LinkStatus_Cache;
    micoWlanGetLinkStatus(&LinkStatus_Cache);

    if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE4)
        rssi_level = LEVEL4;
    else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE3)
        rssi_level = LEVEL3;
    else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE2)
        rssi_level = LEVEL2;
    else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE1)
        rssi_level = LEVEL1;
    else
        rssi_level = LEVEL0;
    mode_state_temp = get_eland_mode_state();
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = SEND_LINK_STATE_08;
    *(Cache + 2) = 3;
    *(Cache + 3) = rssi_level;
    *(Cache + 4) = (uint8_t)(mode_state_temp >> 8);
    *(Cache + 5) = (uint8_t)(mode_state_temp & 0xff);
    *(Cache + 6) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 7);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)SEND_LINK_STATE_08)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H10_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ALARM_READ_10;
    *(Cache + 2) = 0;
    *(Cache + 3) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)ALARM_READ_10)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(10);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void MODH_Opration_02H(uint8_t *usart_rec)
{
    static uint16_t Key_Count = 0, Key_Restain = 0;
    static uint16_t Reset_key_Restain_Trg, Reset_key_Restain_count;
    static uint16_t KEY_Add_Count_Trg, KEY_Add_Count_count;
    static uint16_t Set_Minus_Restain_Trg, Set_Minus_Restain_count;
    uint16_t ReadData;
    __msg_function_t eland_cmd = SEND_FIRM_WARE_06;

    Key_Count = (uint16_t)((*(usart_rec + 3)) << 8) | *(usart_rec + 4);
    Key_Restain = (uint16_t)((*(usart_rec + 5)) << 8) | *(usart_rec + 6);

    /*analysis key NA mode key value*/
    if ((Key_Restain & KEY_Set) && (Key_Restain & KEY_Minus))
        ReadData = (KEY_Set | KEY_Minus);
    else
        ReadData = 0;
    Set_Minus_Restain_Trg = ReadData & (ReadData ^ Set_Minus_Restain_count);
    Set_Minus_Restain_count = ReadData;
    /*analysis key reset mode key value*/
    ReadData = Key_Restain & KEY_Reset;
    Reset_key_Restain_Trg = ReadData & (ReadData ^ Reset_key_Restain_count);
    Reset_key_Restain_count = ReadData;
    if (Reset_key_Restain_Trg)
    {
        reset_eland_flash_para();
    }

    if (Key_Count & 0x0030) //ELAND_CLOCK_MON or ELAND_CLOCK_ALARM
    {
        switch ((MCU_Refresh_type_t)(*(usart_rec + 7)))
        {
        case REFRESH_TIME:
            eland_cmd = TIME_READ_04;
            mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
            break;
        case REFRESH_ALARM:
            eland_cmd = ALARM_READ_10;
            mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
            break;
        default:
            break;
        }
    }
    else //eland net clock mode
    {
        if (Set_Minus_Restain_Trg == (KEY_Set | KEY_Minus))
        {
            Start_wifi_Station_SoftSP_Thread(Soft_AP);
            set_eland_mode(ELAND_NA);
        }
    }

    ReadData = Key_Count & KEY_Add;
    KEY_Add_Count_Trg = ReadData & (ReadData ^ KEY_Add_Count_count);
    KEY_Add_Count_count = ReadData;
    if (KEY_Add_Count_Trg)
    {
        eland_cmd = SEND_FIRM_WARE_06;
        // mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
    }
}

static void MODH_Opration_04H(uint8_t *usart_rec)
{
    mico_rtc_time_t cur_time = {0};
    uint8_t *rec_data;
    rec_data = usart_rec;
    mico_utc_time_ms_t current_utc = 0;
    DATE_TIME_t date_time;

    memcpy(&cur_time, (rec_data + 3), sizeof(mico_rtc_time_t));

    date_time.iYear = 2000 + cur_time.year;
    date_time.iMon = (int16_t)cur_time.month;
    date_time.iDay = (int16_t)cur_time.date;
    date_time.iHour = (int16_t)cur_time.hr;
    date_time.iMin = (int16_t)cur_time.min;
    date_time.iSec = (int16_t)cur_time.sec;
    date_time.iMsec = 0;
    current_utc = GetSecondTime(&date_time);
    current_utc *= 1000;
    MicoRtcSetTime(&cur_time); //初始化 RTC 时钟的时间
    mico_time_set_utc_time_ms(&current_utc);
}

static void MODH_Opration_xxH(__msg_function_t funtype, uint8_t *usart_rec)
{
}
static void MODH_Opration_10H(uint8_t *usart_rec)
{
    _alarm_mcu_data_t cache;
    __elsv_alarm_data_t elsv_alarm_data;
    memcpy(&cache, (usart_rec + 3), sizeof(_alarm_mcu_data_t));
    elsv_alarm_data_init_MCU(&elsv_alarm_data, &cache);
}

static uint16_t get_eland_mode_state(void)
{
    uint16_t Cache = 0;
    Cache = eland_mode_state.eland_mode << 16;
    Cache |= eland_mode_state.eland_status;
    return Cache;
}
static void set_eland_mode(_ELAND_MODE_t mode)
{
    mico_rtos_lock_mutex(&eland_mode_state.state_mutex);
    eland_mode_state.eland_mode = mode;
    mico_rtos_unlock_mutex(&eland_mode_state.state_mutex);
}
static void set_eland_state(Eland_Status_type_t state)
{
    mico_rtos_lock_mutex(&eland_mode_state.state_mutex);
    eland_mode_state.eland_status = state;
    mico_rtos_unlock_mutex(&eland_mode_state.state_mutex);
}

static void reset_eland_flash_para(void)
{
    OSStatus err = kNoErr;
    err = Netclock_des_recovery();
    err = err;
    //MicoSystemReboot();
}
