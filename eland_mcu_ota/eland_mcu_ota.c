/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-05 14:51:50
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-17 13:37:01
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "eland_mcu_ota.h"
#include "stm8_bin.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
//#define CONFIG_OTA_DEBUG
#ifdef CONFIG_OTA_DEBUG
#define mcu_ota_log(M, ...) custom_log("mcu_ota", M, ##__VA_ARGS__)
#else
#define mcu_ota_log(...)
#endif /* ! CONFIG_OTA_DEBUG */

#define OTA_SEND_BUFFER_LEN 150
#define OTA_UART_BUFFER_LENGTH 256
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_thread_t MCU_OTA_thread = NULL;
//new add
volatile ring_buffer_t user_rx_buffer;
volatile uint8_t user_rx_data[OTA_UART_BUFFER_LENGTH] = {0};
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/*
 * uart init
*/
static OSStatus OTA_uart_init(void)
{
    mico_uart_config_t uart_config;
    memset(&uart_config, 0, sizeof(mico_uart_config_t));

    uart_config.baud_rate = 115200;
    uart_config.data_width = DATA_WIDTH_8BIT;
    uart_config.parity = EVEN_PARITY;
    uart_config.stop_bits = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
    uart_config.flags = UART_WAKEUP_DISABLE;

    ring_buffer_init((ring_buffer_t *)&user_rx_buffer, (uint8_t *)user_rx_data, OTA_UART_BUFFER_LENGTH);
    MicoUartInitialize(MICO_UART_2, &uart_config, (ring_buffer_t *)&user_rx_buffer);
    return kNoErr;
}

/*
 * get copy to buf, return len
*/
static size_t OTA_uart_get_one_packet(uint8_t *inBuf, int inBufLen, uint8_t timeout)
{
    int datalen;
    while (1)
    {
        if (MicoUartRecv(MICO_UART_2, inBuf, inBufLen, timeout) == kNoErr)
        {
            return inBufLen;
        }
        else
        {
            datalen = MicoUartGetLengthInBuffer(MICO_UART_2);
            if (datalen)
            {
                MicoUartRecv(MICO_UART_2, inBuf, datalen, timeout);
                return datalen;
            }
            else
                return 0;
        }
    }
}
/*
 * command once, Uart Send and recv
 */
static OSStatus stm8_ota_command_once(uint8_t *inBuf, int inBufLen, uint8_t timeout)
{
    int recv_len;

    MicoUartSend(MICO_UART_2, inBuf, inBufLen);
    recv_len = OTA_uart_get_one_packet(inBuf, OTA_SEND_BUFFER_LEN, timeout);
    if (recv_len <= 0)
        return kGeneralErr;
    if (inBuf[0] == 0x79)
        return kNoErr;
    else
        return kGeneralErr;
}
/*
 * Checksum   XOR
 */
static uint8_t stm8_ota_Checksum(uint8_t *inBuf, int inBufLen)
{
    uint8_t j, Checksum = 0;

    for (j = 0; j < inBufLen; j++)
        Checksum = Checksum ^ inBuf[j];
    return Checksum;
}

static int ota_start_command(void)
{
    uint8_t inDatabuf[50];
    uint8_t outDatabuf[50];
    uint8_t times = 10;
    int recv_len;
    mico_uart_config_t uart_config;

    memset(&uart_config, 0, sizeof(mico_uart_config_t));
    memset(&inDatabuf, 0, sizeof(inDatabuf));
    memset(&outDatabuf, 0, sizeof(outDatabuf));

    uart_config.baud_rate = 115200;
    uart_config.data_width = DATA_WIDTH_8BIT;
    uart_config.parity = NO_PARITY;
    uart_config.stop_bits = STOP_BITS_1;
    uart_config.flow_control = FLOW_CONTROL_DISABLED;

    ring_buffer_init((ring_buffer_t *)&user_rx_buffer, (uint8_t *)user_rx_data, OTA_UART_BUFFER_LENGTH);
    MicoUartInitialize(MICO_UART_2, &uart_config, (ring_buffer_t *)&user_rx_buffer);
    /*check firmware version*/
    inDatabuf[0] = 0x55;
    inDatabuf[1] = 0x07;
    inDatabuf[2] = 0x01;
    inDatabuf[3] = 0x00;
    inDatabuf[4] = 0xaa;
    do
    {
        times--;
        MicoUartSend(MICO_UART_2, inDatabuf, 5);
        mico_rtos_thread_msleep(2);
        recv_len = MicoUartGetLengthInBuffer(MICO_UART_2);
        if (recv_len)
            MicoUartRecv(MICO_UART_2, outDatabuf, recv_len, 2);
        inDatabuf[3] = recv_len;
        if (recv_len > 0)
        {
            if ((outDatabuf[0] == 0x55) &&
                (outDatabuf[1] == 0x07) &&
                (outDatabuf[8] == 0xaa))
            {
                if (!strncmp(MCU_REVISION, (char const *)&outDatabuf[3], 5))
                    return 2;
                else
                    break;
                // sscanf((char const *)&outDatabuf[3], "%02d.%02d", &version_major_cache, &version_minor_cache);
                // if ((version_major_cache == MCU_VERSION_MAJOR) && (version_minor_cache == MCU_VERSION_MINOR))
                //     return 2;
                // else
                //     break;
            }
            else
                memset(&outDatabuf, 0, sizeof(outDatabuf));
        }
        mico_rtos_thread_msleep(10);
    } while (times > 0);

    if (times == 0)
        return kGeneralErr;
    /*start update firmware*/
    inDatabuf[0] = 0x55;
    inDatabuf[1] = 0x09;
    inDatabuf[2] = 0x00;
    inDatabuf[3] = 0xaa;
    memset(outDatabuf, 0, sizeof(outDatabuf));
    do
    {
        times--;
        MicoUartSend(MICO_UART_2, inDatabuf, 4);
        recv_len = OTA_uart_get_one_packet(outDatabuf, OTA_SEND_BUFFER_LEN, 2);
        if (recv_len > 0)
        {
            if (outDatabuf[0] == 0x55)
                break;
            else
                outDatabuf[0] = 0;
        }
        mico_rtos_thread_msleep(10);
    } while (times > 0);

    if (times)
        return kNoErr;
    else
        return kGeneralErr;
}

/*
 * Thread fun
 */
void mcu_ota_thread(mico_thread_arg_t arg)
{
    int i, stm8FW_len, stm8FW_page, OTA_Step_No = 0;
    uint8_t *inDataBuffer;
    int *err = (int *)arg;

    inDataBuffer = malloc(OTA_SEND_BUFFER_LEN);
    require(inDataBuffer, exit);

    mcu_ota_log("uart init ....");
    while (1)
    {
        switch (OTA_Step_No)
        {
        case 0:
            /*The bootloader polls all peripherals waiting for a synchronization
                     byte/message (SYNCHR = 0x7F) within a timeout of 1 second. If a timeout occurs,
                     either the Flash program memory is virgin in which case it waits for a synchronization
                     byte/message in an infinite loop through a software reset, or the Flash program
                     memory is not virgin and the bootloader restores the registersï¿½ï¿½ reset status and jumps
                     to the memory address given by the reset vector (located at 0x00 8000).*/

            *err = ota_start_command();
            if (*err == 2)
                goto exit;
            else if (*err == kGeneralErr)
            {
                goto exit_err;
                // for (i = 0; i < 30; i++)
                // {
                //     MicoUartSend(MICO_UART_2, err, 1);
                // }
                // mico_rtos_thread_sleep(5);
                // MicoGpioInitialize(MCU_POWER_GPIO, OUTPUT_OPEN_DRAIN_NO_PULL);
                // mico_rtos_thread_sleep(1);
                // *err += 1;
                // MicoUartSend(MICO_UART_2, err, 1);
                // mico_rtos_thread_sleep(1);
                // /***stm8 mcu reset  (power off-->power on)**/
                // for (i = 0; i < 230; i++)
                // {
                //     *err += 1;
                //     MicoUartSend(MICO_UART_2, err, 1);
                //     mico_rtos_thread_sleep(1);
                //     MicoGpioOutputLow(MCU_POWER_GPIO);
                //     mico_rtos_thread_sleep(1);
                //     MicoGpioOutputHigh(MCU_POWER_GPIO);
                // }
            }
            OTA_uart_init();
            mico_rtos_thread_msleep(50);
            inDataBuffer[0] = 0x7f; //Synchronization byte
            if (kNoErr != stm8_ota_command_once(inDataBuffer, 1, 2))
                goto exit_err;
            OTA_Step_No = 1;
            break;
        case 1:
            /*Get Command
                Gets the version and the allowed commands supported by the current version of
                     the bootloader*/

            inDataBuffer[0] = 0x00;
            inDataBuffer[1] = 0xff; //Get command
            if (kNoErr != stm8_ota_command_once(inDataBuffer, 2, 2))
                goto exit_err;
            OTA_Step_No = 2;
            break;
        case 2:
            /*Write Command
                Writes up to 128 bytes to RAM or the Flash program memory/data EEPROM
                       starting from an address specified by the host
                The host sends the bytes as follows
                Byte 1:  0x31  - Command ID
                Byte 2:  0xCE  - Complement
                      Wait for ACK or NACK......
                Bytes 3-6:  The start address (32-bit address)
                Byte 3 = MSB
                Byte 6 = LSB
                Byte 7:  Checksum = XOR (byte 3, byte 4, byte 5, byte 6)
                      Wait for ACK or NACK......
                Byte 8:  The number of bytes to be received -1: N = 0 ... 127
                If N > 127, a cmd_error occurs in the bootloader.
                N+1 bytes: Max 128 data bytes
                Checksum byte: XOR (N,[N+1 data bytes])
                      Wait for ACK or NACK......*/

            stm8FW_len = sizeof(FW_C_TABLE);
            stm8FW_page = stm8FW_len / 128;
            if (stm8FW_len % 128 != 0)
                stm8FW_page += 1; //if Not enough one page, make up one page
            for (i = 0; i < stm8FW_page; i++)
            {
                inDataBuffer[0] = 0x31;
                inDataBuffer[1] = 0xce; //Write Command
                if (kNoErr != stm8_ota_command_once(inDataBuffer, 2, 2))
                    goto exit_err;

                inDataBuffer[0] = 0x00;
                inDataBuffer[1] = 0x00;
                inDataBuffer[2] = 0x80 + i / 2;
                if (i % 2 == 0)
                    inDataBuffer[3] = 0x00;
                else
                    inDataBuffer[3] = 0x80; //flash address
                inDataBuffer[4] = stm8_ota_Checksum(inDataBuffer, 4);
                if (kNoErr != stm8_ota_command_once(inDataBuffer, 5, 2))
                    goto exit_err;

                inDataBuffer[0] = 0x7f;
                memcpy(&inDataBuffer[1], &FW_C_TABLE[i * 128], 128); //flash data
                inDataBuffer[129] = stm8_ota_Checksum(inDataBuffer, 129);
                if (kNoErr != stm8_ota_command_once(inDataBuffer, 130, 10))
                    goto exit_err;
            }
            OTA_Step_No = 3;
            break;
        case 3:
            /*Go Command
                Jumps to an address specified by the host to execute a loaded code
                The host sends the bytes as follows
                Byte 1:  0x21  - Command ID
                Byte 2:  0xDE - Complement
                     Wait for ACK or NACK......
                Bytes 3-6:  The start address (32-bit address)
                Byte 3 = MSB
                Byte 6 = LSB
                Byte 7: Checksum = XOR (byte 3, byte 4, byte 5, byte 6).
                     Wait for ACK or NACK......*/

            inDataBuffer[0] = 0x21;
            inDataBuffer[1] = 0xde; //Go command
            if (kNoErr != stm8_ota_command_once(inDataBuffer, 2, 2))
                goto exit_err;

            inDataBuffer[0] = 0x00;
            inDataBuffer[1] = 0x00;
            inDataBuffer[2] = 0x80;
            inDataBuffer[3] = 0x00; //flash address
            inDataBuffer[4] = stm8_ota_Checksum(inDataBuffer, 4);
            if (kNoErr != stm8_ota_command_once(inDataBuffer, 5, 2))
                goto exit_err;
            OTA_Step_No = 4;
            mico_rtos_thread_msleep(1100);
            break;
        case 4:
            goto exit;
            break;
        default:
            break;
        }
    }

exit_err:
    *err = kGeneralErr;
    mcu_ota_log("stm8 flash program Fail ....");
exit:
    mcu_ota_log("stm8 flash program OK ....");
    if (inDataBuffer)
        free(inDataBuffer);

    mico_rtos_delete_thread(NULL);
}
