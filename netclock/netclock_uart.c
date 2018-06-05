/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-10-31 17:39:58
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-04 10:31:50
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
#include "flash_kh25.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define CONFIG_UART_DEBUG
#ifdef CONFIG_UART_DEBUG
#define Eland_uart_log(M, ...) custom_log("UART", M, ##__VA_ARGS__)
#else
#define Eland_uart_log(...)
#endif /* ! CONFIG_UART_DEBUG */

#define UART_BUFFER_LENGTH 1024
#define UART_ONE_PACKAGE_LENGTH 512
#define STACK_SIZE_UART_RECV_THREAD 0x1000

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
//static void uart_service(uint32_t arg);
static void Eland_H01_Send(uint8_t *Cache);
static void Eland_H02_Send(uint8_t *Cache);
static void Eland_H03_Send(uint8_t *Cache);
static void Eland_H04_Send(uint8_t *Cache);
static void ELAND_H05_Send(uint8_t *Cache);
static void ELAND_H06_Send(uint8_t *Cache);
static void ELAND_H08_Send(uint8_t *Cache);
static void ELAND_H0A_Send(uint8_t *Cache);
static void ELAND_H0B_Send(uint8_t *Cache);
static void ELAND_H0C_Send(uint8_t *Cache);
static void ELAND_H0D_Send(uint8_t *Cache);
static void ELAND_H0E_Send(uint8_t *Cache);
static void MODH_Opration_02H(uint8_t *usart_rec);
static void MODH_Opration_04H(uint8_t *usart_rec);
static void MODH_Opration_xxH(__msg_function_t funtype, uint8_t *usart_rec);
static void MODH_Opration_0AH(uint8_t *usart_rec);
static void timer100_key_handle(void *arg);
static void uart_thread_DDE(uint32_t arg);
static OSStatus elandUsartSendData(uint8_t *data, uint32_t lenth);
static void Opration_Packet(uint8_t *data);

static void set_eland_state(Eland_Status_type_t state);
/* Private functions ---------------------------------------------------------*/

OSStatus start_uart_service(void)
{
    OSStatus err = kNoErr;
    mico_uart_config_t uart_config;
    mico_Context_t *mico_context;
    uint32_t ota_arg;
    /*Register user function for MiCO nitification: WiFi status changed*/
    err = mico_rtos_init_mutex(&eland_mode_state.state_mutex);
    require_noerr(err, exit);

    /*wait mcu passed bootloader*/
    mico_rtos_thread_msleep(1100);
    /*creat mcu_ota thread*/

    err = mico_rtos_create_thread(&MCU_OTA_thread, MICO_APPLICATION_PRIORITY, "mcu_ota_thread", mcu_ota_thread, 0x500, (uint32_t)&ota_arg);
    require_noerr(err, exit);
    /*wait ota thread*/
    mico_rtos_thread_join(&MCU_OTA_thread);

    //  goto exit;
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
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "uart_thread_DDE", uart_thread_DDE, 0x1000, 0);
    require_noerr(err, exit);
    /*Register queue receive thread*/
    err = mico_rtos_get_semaphore(&Is_usart_complete_sem, MICO_WAIT_FOREVER); //two threads
    require_noerr(err, exit);
    err = mico_rtos_get_semaphore(&Is_usart_complete_sem, MICO_WAIT_FOREVER); //two times
    require_noerr(err, exit);

    err = mico_rtos_deinit_semaphore(&Is_usart_complete_sem);
    require_noerr(err, exit);

    SendElandStateQueue(ElandBegin);
    Eland_uart_log("start usart timer");
    /*configuration usart timer*/
    err = mico_init_timer(&timer100_key, 100, timer100_key_handle, NULL); //100ms 讀取一次
    require_noerr(err, exit);
    /*start key read timer*/
    err = mico_start_timer(&timer100_key); //開始定時器
    require_noerr(err, exit);

exit:
    return err;
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
    case SEND_ELAND_ERR_01:
        MODH_Opration_xxH(*(data + 1), data);
        break;
    case KEY_READ_02:
        MODH_Opration_02H(data);
        break;
    case TIME_SET_03:
        MODH_Opration_xxH(*(data + 1), data);
        break;
    case TIME_READ_04:
        MODH_Opration_04H(data);
        break;
    case ELAND_STATES_05:
    case SEND_FIRM_WARE_06:
    case SEND_LINK_STATE_08:
        MODH_Opration_xxH(*(data + 1), data);
        break;
    case ALARM_READ_0A:
        MODH_Opration_0AH(data);
        break;
    case ALARM_SEND_0B:
    case ELAND_DATA_0C:
        MODH_Opration_xxH(*(data + 1), data);
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
        case SEND_ELAND_ERR_01: /* read key value*/
            Eland_H01_Send(inDataBuffer);
            break;
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
        case ALARM_READ_0A: /* READ MCU ALARM*/
            ELAND_H0A_Send(inDataBuffer);
            break;
        case ALARM_SEND_0B: /* SEND ELAND ALARM*/
            ELAND_H0B_Send(inDataBuffer);
            break;
        case ELAND_DATA_0C: /* SEND ELAND DATA TO MCU */
            ELAND_H0C_Send(inDataBuffer);
            break;
        case ELAND_RESET_0D: /* RESET SYSTEM */
            ELAND_H0D_Send(inDataBuffer);
            break;
        case ELAND_DELETE_0E: /* RESET SYSTEM */
            ELAND_H0E_Send(inDataBuffer);
            break;
        default:
            break;
        }
    }
exit:
    mico_rtos_delete_thread(NULL);
}

void SendElandStateQueue(Eland_Status_type_t value)
{
    Eland_Status_type_t state;
    set_eland_state(value);
    if (eland_state_queue == NULL)
        return;
    if (mico_rtos_is_queue_full(&eland_state_queue)) //if full pick out data then update state
        mico_rtos_pop_from_queue(&eland_state_queue, &state, 0);
    state = value;

    mico_rtos_push_to_queue(&eland_state_queue, &state, 0);
    eland_push_uart_send_queue(ELAND_STATES_05);
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
    eland_push_uart_send_queue(eland_cmd);
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

//time set 03
static void Eland_H01_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    __eland_error_t el_error;
    el_error = eland_error(false, EL_ERROR_NONE);
    *Cache = Uart_Packet_Header;
    *(Cache + 1) = SEND_ELAND_ERR_01;
    *(Cache + 2) = 1;
    *(Cache + 3) = el_error;
    *(Cache + 4) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 5);
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)SEND_ELAND_ERR_01)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
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

    sended_times--;
    err = elandUsartSendData(Cache, len);
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);
    if (received_cmd == (__msg_function_t)KEY_READ_02)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
        // if (sended_times > 0)
        //     goto start_send;
    }
exit:
    return;
}
//time set 03
static void Eland_H03_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    mico_rtc_time_t cur_time;
    mico_utc_time_t utc_time = 0;
    struct tm *currentTime;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    utc_time = GET_current_second();
    currentTime = localtime((const time_t *)&(utc_time));

    cur_time.sec = currentTime->tm_sec;
    cur_time.min = currentTime->tm_min;
    cur_time.hr = currentTime->tm_hour;
    cur_time.date = currentTime->tm_mday;
    cur_time.weekday = currentTime->tm_wday + 1;
    cur_time.month = currentTime->tm_mon + 1;
    cur_time.year = (currentTime->tm_year + 1900) % 100;

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
        mico_rtos_thread_msleep(5);
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
        mico_rtos_thread_msleep(5);
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
        mico_rtos_thread_msleep(5);
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
    *(Cache + 2) = strlen(FIRMWARE_REVISION);
    sprintf((char *)(Cache + 3), "%s", FIRMWARE_REVISION);
    *(Cache + 3 + strlen(FIRMWARE_REVISION)) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4 + strlen(FIRMWARE_REVISION));
    require_noerr(err, exit);

    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)SEND_FIRM_WARE_06)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
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
    if (LinkStatus_Cache.is_connected)
    {
        if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE4)
            rssi_level = LEVEL4;
        else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE3)
            rssi_level = LEVEL3;
        else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE2)
            rssi_level = LEVEL2;
        else if (LinkStatus_Cache.rssi >= RSSI_STATE_STAGE0)
            rssi_level = LEVEL1;
        else
            rssi_level = LEVEL0;
    }
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
        mico_rtos_thread_msleep(5);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H0A_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ALARM_READ_0A;
    *(Cache + 2) = 0;
    *(Cache + 3) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)ALARM_READ_0A)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H0B_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    _alarm_mcu_data_t *alarm_data_mcu = NULL;
    _alarm_mcu_data_t to_mcu_data;
    _alarm_schedules_t *schedule = NULL;
    if (ELAND_NA == get_eland_mode())
    {
        if (get_schedules_number() == 0)
            alarm_data_mcu = NULL;
        else
        {
            alarm_data_mcu = get_alarm_mcu_data();
            require_quiet(alarm_data_mcu, data_parse_over);
            memcpy(&to_mcu_data, alarm_data_mcu, sizeof(_alarm_mcu_data_t));
            schedule = get_schedule_data();
            require_action(schedule, data_parse_over, alarm_data_mcu = NULL);
            UCT_Convert_Date(&(schedule->utc_second), &(to_mcu_data.moment_time));
            to_mcu_data.alarm_color = schedule->alarm_color;
            to_mcu_data.snooze_enabled = schedule->snooze_enabled;
            alarm_data_mcu = &to_mcu_data;
        }
    }
    else
    {
        alarm_data_mcu = get_alarm_mcu_data();
        if (alarm_data_mcu)
        {
            memcpy(&to_mcu_data, alarm_data_mcu, sizeof(_alarm_mcu_data_t));
            alarm_data_mcu = &to_mcu_data;
        }
    }

data_parse_over:

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ALARM_SEND_0B;
    if (alarm_data_mcu)
    {
        alarm_data_mcu->mode = get_eland_mode();
        alarm_data_mcu->alarm_state = get_alarm_state();
        *(Cache + 2) = sizeof(_alarm_mcu_data_t);
        memcpy(Cache + 3, (uint8_t *)alarm_data_mcu, sizeof(_alarm_mcu_data_t));
    }
    else
        *(Cache + 2) = 0;
    *(Cache + 3 + (*(Cache + 2))) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4 + (*(Cache + 2)));
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)ALARM_SEND_0B)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H0C_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;
    __msg_function_t received_cmd = KEY_FUN_NONE;
    uint8_t sended_times = USART_RESEND_MAX_TIMES;
    __ELAND_DATA_2_MCU_t cache_to_mcu = {0};
    int ho, mi, se;
    cache_to_mcu.time_display_format = netclock_des_g->time_display_format;
    cache_to_mcu.night_mode_enabled = netclock_des_g->night_mode_enabled;
    cache_to_mcu.brightness_normal = netclock_des_g->brightness_normal;
    cache_to_mcu.brightness_night = netclock_des_g->brightness_night;
    cache_to_mcu.led_normal = netclock_des_g->led_normal;
    cache_to_mcu.led_night = netclock_des_g->led_night;
    sscanf((const char *)(&(netclock_des_g->night_mode_begin_time)), "%02d:%02d:%02d", &ho, &mi, &se);
    cache_to_mcu.night_mode_begin_time = (uint32_t)ho * 3600 + (uint32_t)mi * 60 + (uint32_t)se;
    sscanf((const char *)(&(netclock_des_g->night_mode_end_time)), "%02d:%02d:%02d", &ho, &mi, &se);
    cache_to_mcu.night_mode_end_time = (uint32_t)ho * 3600 + (uint32_t)mi * 60 + (uint32_t)se;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ELAND_DATA_0C;
    *(Cache + 2) = sizeof(__ELAND_DATA_2_MCU_t);
    memcpy(Cache + 3, &cache_to_mcu, sizeof(__ELAND_DATA_2_MCU_t));
    *(Cache + 3 + (*(Cache + 2))) = Uart_Packet_Trail;
start_send:
    sended_times--;
    err = elandUsartSendData(Cache, 4 + (*(Cache + 2)));
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_uart_receive_queue, &received_cmd, 20);
    require_noerr(err, exit);

    if (received_cmd == (__msg_function_t)ELAND_DATA_0C)
        err = kNoErr;
    else //10ms resend mechanism
    {
        mico_rtos_thread_msleep(5);
        if (sended_times > 0)
            goto start_send;
    }
exit:
    return;
}
static void ELAND_H0D_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ELAND_RESET_0D;
    *(Cache + 2) = 0;
    *(Cache + 3 + (*(Cache + 2))) = Uart_Packet_Trail;

    err = elandUsartSendData(Cache, 4 + (*(Cache + 2)));
    require_noerr(err, exit);
    mico_rtos_delete_thread(NULL);
exit:
    return;
}
static void ELAND_H0E_Send(uint8_t *Cache)
{
    OSStatus err = kGeneralErr;

    *Cache = Uart_Packet_Header;
    *(Cache + 1) = ELAND_DELETE_0E;
    *(Cache + 2) = 0;
    *(Cache + 3 + (*(Cache + 2))) = Uart_Packet_Trail;

    err = elandUsartSendData(Cache, 4 + (*(Cache + 2)));
    require_noerr(err, exit);
    mico_rtos_delete_thread(NULL);
exit:
    return;
}
static void MODH_Opration_02H(uint8_t *usart_rec)
{
    static uint16_t Key_Count = 0, Key_Restain = 0;
    static uint16_t Key_Count_Trg = 0, Key_Restain_Trg = 0;
    static uint16_t Key_Count_count = 0, Key_Restain_count = 0;
    iso8601_time_t iso8601_time;
    uint8_t cache = 0;
    static uint16_t time_delay_counter;
    _alarm_list_state_t alarm_status;
    _ELAND_MODE_t eland_mode;

    Key_Count = (uint16_t)((*(usart_rec + 3)) << 8) | *(usart_rec + 4);
    Key_Restain = (uint16_t)((*(usart_rec + 5)) << 8) | *(usart_rec + 6);

    Key_Count_Trg = Key_Count & (Key_Count ^ Key_Count_count);
    Key_Count_count = Key_Count;

    Key_Restain_Trg = Key_Restain & (Key_Restain ^ Key_Restain_count);
    Key_Restain_count = Key_Restain;

    if (Key_Restain_Trg & KEY_Reset)
        reset_eland_flash_para(ELAND_RESET_0D);

    /*alarm control*/
    alarm_status = get_alarm_state();
    eland_mode = get_eland_mode();
    if (alarm_status == ALARM_ING)
    {
        if ((Key_Count_Trg & KEY_Snooze) || (Key_Count_Trg & KEY_Alarm))
        {
            mico_time_get_iso8601_time(&iso8601_time);
            if (Key_Count_Trg & KEY_Snooze)
            {
                set_alarm_state(ALARM_SNOOZ_STOP);
                alarm_off_history_record_time(ALARM_OFF_SNOOZE, &iso8601_time);
            }
            else if (Key_Count_Trg & KEY_Alarm)
            {
                set_alarm_state(ALARM_STOP);
                alarm_off_history_record_time(ALARM_OFF_ALARMOFF, &iso8601_time);
            }
        }
    }
    else if (alarm_status == ALARM_SNOOZ_STOP)
    {
        if (Key_Count_Trg & KEY_Alarm)
            set_alarm_state(ALARM_STOP);
        if ((Key_Count_Trg & KEY_Snooze) &&
            (eland_mode != ELAND_CLOCK_MON) &&
            (eland_mode != ELAND_CLOCK_ALARM) &&
            (eland_mode != ELAND_NA))
        { //sound oid
            if (get_alarm_stream_state() == STREAM_PLAY)
                set_alarm_stream_state(STREAM_STOP);
            else
                eland_push_http_queue(DOWNLOAD_OID);
        }
    }
    else
    {
        if (Key_Restain_Trg & KEY_Alarm) //alarm skip
        {
            set_alarm_state(ALARM_SKIP);
        }
        if (Key_Count_Trg & KEY_Snooze) //sound oid
        {
            if (get_alarm_stream_state() == STREAM_PLAY)
                set_alarm_stream_state(STREAM_STOP);
            else
                eland_push_http_queue(DOWNLOAD_OID);
        }
    }
    /*ELAND_CLOCK_MON or ELAND_CLOCK_ALARM*/
    if ((Key_Count & KEY_MON) | (Key_Count & KEY_AlarmMode))
    {
        switch ((MCU_Refresh_type_t)(*(usart_rec + 7)))
        {
        case REFRESH_TIME:
            eland_push_uart_send_queue(TIME_READ_04);
            break;
        case REFRESH_ALARM:
            eland_push_uart_send_queue(ALARM_READ_0A);
            break;
        default:
            break;
        }
        /********mode operation************/
        switch (eland_mode)
        {
        case ELAND_CLOCK_MON:
            if (Key_Count & KEY_AlarmMode)
            {
                set_eland_mode(ELAND_CLOCK_ALARM);
                eland_push_uart_send_queue(ALARM_READ_0A);
            }
            break;
        case ELAND_CLOCK_ALARM:
            if (Key_Count & KEY_MON)
            {
                alarm_list_clear(&alarm_list);
                set_eland_mode(ELAND_CLOCK_MON);
            }
            break;
        case ELAND_NC:
        case ELAND_NA:
            /**stop tcp communication**/
            set_eland_state(ElandBegin);
            if (Key_Count & KEY_MON)
            {
                alarm_list_clear(&alarm_list);
                set_eland_mode(ELAND_CLOCK_MON);
            } /****alarm mode**********/
            else if (Key_Count & KEY_AlarmMode)
            {
                set_eland_mode(ELAND_CLOCK_ALARM);
                eland_push_uart_send_queue(ALARM_READ_0A);
            }
            break;
        case ELAND_AP:
            /**stop tcp communication**/
            MicoSystemReboot();
            break;
        case ELAND_OTA:
            break;
        default:
            set_eland_mode(ELAND_CLOCK_MON);
            break;
        }
    }
    else if (Key_Count & KEY_Wifi) //eland net clock mode
    {
        switch ((MCU_Refresh_type_t)(*(usart_rec + 7)))
        {
        case REFRESH_TIME:
            eland_push_uart_send_queue(TIME_READ_04);
            break;
        case REFRESH_ALARM:
            eland_push_uart_send_queue(ALARM_SEND_0B);
            break;
        case REFRESH_ELAND_DATA:
            eland_push_uart_send_queue(ELAND_DATA_0C);
            break;
        default:
            break;
        }
        /********mode operation************/
        switch (eland_mode)
        {
        case ELAND_NC:
            if ((Key_Count_Trg & KEY_Add) || (Key_Count_Trg & KEY_Minus))
            {
                set_eland_mode(ELAND_NA);
                time_delay_counter = 0;
                cache = 0;
                if (Key_Count_Trg & KEY_Add)
                    cache = get_next_alarm_serial(cache);
                else
                    cache = get_previous_alarm_serial(cache);
                set_display_na_serial(cache);
            }
            if ((Key_Restain_Trg & KEY_Set) &&
                ((Key_Count & KEY_Add) == 0) &&
                ((Key_Count & KEY_Minus) == 0)) //NA
            {
                set_eland_mode(ELAND_AP);
                time_delay_counter = 0;
                eland_push_http_queue(GO_INTO_AP_MODE);
                if (!get_wifi_status())
                    Start_wifi_Station_SoftSP_Thread(Soft_AP);
            }
            break;
        case ELAND_NA:
            if ((Key_Count_Trg & KEY_Add) || (Key_Count_Trg & KEY_Minus))
            {
                time_delay_counter = 0;
                cache = get_display_na_serial();
                if (Key_Count_Trg & KEY_Add)
                    cache = get_next_alarm_serial(cache);
                else
                    cache = get_previous_alarm_serial(cache);
                set_display_na_serial(cache);
            }
            if ((Key_Count_Trg & KEY_Snooze) ||
                (Key_Count_Trg & KEY_Alarm) ||
                (time_delay_counter++ > 50))
            {
                /******back to nc********/
                set_eland_mode(ELAND_NC);
                set_display_na_serial(SCHEDULE_MAX);
                time_delay_counter = 0;
            }
            break;
        case ELAND_AP:
            if ((Key_Count_Trg & KEY_Add) ||
                (Key_Count_Trg & KEY_Minus) ||
                (Key_Count_Trg & KEY_Set) ||
                (Key_Count_Trg & KEY_Alarm) ||
                (Key_Count_Trg & KEY_Snooze) ||
                (time_delay_counter++ > 6000))
            {
                /******back to nc********/
                set_eland_mode(ELAND_NC);
                mico_rtos_thread_msleep(300);
                MicoSystemReboot();
            }
            break;
        case ELAND_OTA:
            break;
        case ELAND_CLOCK_ALARM:
            alarm_list_clear(&alarm_list);
        case ELAND_CLOCK_MON:
            set_eland_mode(ELAND_NC);
            // MicoSystemReboot();
            break;
        default:
            set_eland_mode(ELAND_NC);
            break;
        }
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
    mico_rtos_lock_mutex(&time_Mutex);
    MicoRtcSetTime(&cur_time); //初始化 RTC 时钟的时间
    mico_time_set_utc_time_ms(&current_utc);
    mico_rtos_unlock_mutex(&time_Mutex);
}

static void MODH_Opration_xxH(__msg_function_t funtype, uint8_t *usart_rec)
{
}
static void MODH_Opration_0AH(uint8_t *usart_rec)
{
    _alarm_mcu_data_t cache;
    memcpy(&cache, (usart_rec + 3), sizeof(_alarm_mcu_data_t));
    elsv_alarm_data_init_MCU(&cache);
}

uint16_t get_eland_mode_state(void)
{
    uint16_t Cache = 0;
    Cache = eland_mode_state.eland_mode << 8;
    Cache |= eland_mode_state.eland_status;
    return Cache;
}
_ELAND_MODE_t get_eland_mode(void)
{
    return eland_mode_state.eland_mode;
}
void set_eland_mode(_ELAND_MODE_t mode)
{
    if (eland_mode_state.state_mutex == NULL)
        return;
    mico_rtos_lock_mutex(&eland_mode_state.state_mutex);
    eland_mode_state.eland_mode = mode;
    mico_rtos_unlock_mutex(&eland_mode_state.state_mutex);
    eland_push_uart_send_queue(SEND_LINK_STATE_08);
}
static void set_eland_state(Eland_Status_type_t state)
{
    if (eland_mode_state.state_mutex != NULL)
        mico_rtos_lock_mutex(&eland_mode_state.state_mutex);
    Eland_uart_log("*****************set_eland_state:%d", state);
    eland_mode_state.eland_status = state;
    mico_rtos_unlock_mutex(&eland_mode_state.state_mutex);
}

void reset_eland_flash_para(__msg_function_t msg)
{
    OSStatus err = kNoErr;
    uint8_t BUF[5] = {0};
    __msg_function_t eland_cmd;
    do
    {
        err = mico_rtos_pop_from_queue(&eland_uart_CMD_queue, &eland_cmd, 0);
    } while (err == kNoErr);
    /**clear time**/
    eland_push_uart_send_queue(msg);
    err = Netclock_des_recovery();
    /**clear sound file in flash**/
    flash_kh25_write_page(BUF, KH25_CHECK_ADDRESS, sizeof(BUF));
    MicoSystemReboot();
}

void eland_push_uart_send_queue(__msg_function_t cmd)
{
    __msg_function_t eland_cmd = cmd;
    mico_rtos_push_to_queue(&eland_uart_CMD_queue, &eland_cmd, 20);
}

__eland_error_t eland_error(bool write_error, __eland_error_t error)
{
    static __eland_error_t error_ram = EL_ERROR_NONE;
    if (write_error)
    {
        error_ram = error;
        eland_push_uart_send_queue(SEND_ELAND_ERR_01);
    }
    return error_ram;
}
