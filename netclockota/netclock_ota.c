/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2017-12-22 13:35:01
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2017-12-22 17:09:05
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "netclock_ota.h"
#include "ota_server.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define ota_log(M, ...) custom_log("OTA", M, ##__VA_ARGS__)

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_semaphore_t eland_ota_sem = NULL;
mico_logic_partition_t *ota_partition = NULL;
ota_server_context_t *ota_context = NULL;
uint32_t ota_offset = 0;

static CRC16_Context crc_context;
static md5_context md5;

char ota_url[128];
char ota_md5[33];

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
static void hex2str(uint8_t *hex, int hex_len, char *str)
{
    int i = 0;
    for (i = 0; i < hex_len; i++)
    {
        sprintf(str + i * 2, "%02x", hex[i]);
    }
}
static void upper2lower(char *str, int len)
{
    int i = 0;
    for (i = 0; i < len; i++)
    {
        if ((*(str + i) >= 'A') && (*(str + i) <= 'Z'))
        {
            *(str + i) += 32;
        }
    }
}
static void ota_server_status_handler(OTA_STATE_E state, float progress)
{
    switch (state)
    {
    case OTA_LOADING:
        ota_log("ota server is loading, progress %.2f%%", progress);
        break;
    case OTA_SUCCE:
        ota_log("ota server daemons success");
        break;
    case OTA_FAIL:
        ota_log("ota server daemons failed");
        break;
    default:
        break;
    }
}
static void ota_server_progress_set(OTA_STATE_E state)
{
    float progress = 0.00;

    progress = (float)ota_context->download_state.download_begin_pos / ota_context->download_state.download_len;
    progress = progress * 100;
    ota_server_status_handler(state, progress);
}
/***************啟動 OTA thread*****************************/
void start_ota_thread(void)
{
    ota_log("ota Start!");
    mico_system_notify_remove_all(mico_notify_WIFI_STATUS_CHANGED);
    mico_system_notify_remove_all(mico_notify_WiFI_PARA_CHANGED);
    mico_system_notify_remove_all(mico_notify_DHCP_COMPLETED);
    mico_system_notify_remove_all(mico_notify_WIFI_CONNECT_FAILED);
    mico_system_notify_remove_all(mico_notify_EASYLINK_WPS_COMPLETED);

    /*********************OTA thread*****************************/
    ota_server_start(ota_url, ota_md5, ota_server_status_handler);
}

OSStatus eland_ota_data_init(uint32_t length)
{
    OSStatus err;

    ota_log("ota Start!");
    mico_system_notify_remove_all(mico_notify_WIFI_STATUS_CHANGED);
    mico_system_notify_remove_all(mico_notify_WiFI_PARA_CHANGED);
    mico_system_notify_remove_all(mico_notify_DHCP_COMPLETED);
    mico_system_notify_remove_all(mico_notify_WIFI_CONNECT_FAILED);
    mico_system_notify_remove_all(mico_notify_EASYLINK_WPS_COMPLETED);

    if (ota_context != NULL)
    {
        if (ota_context->download_url.url != NULL)
        {
            free(ota_context->download_url.url);
            ota_context->download_url.url = NULL;
        }
        free(ota_context);
        ota_context = NULL;
    }

    ota_context = malloc(sizeof(ota_server_context_t));
    require_action(ota_context, exit, err = kNoMemoryErr);
    memset(ota_context, 0x00, sizeof(ota_server_context_t));

    if (strlen(ota_md5) != 0)
    {
        ota_log("ota_md5 %d", strlen(ota_md5));
        ota_context->ota_check.is_md5 = true;
        memcpy(ota_context->ota_check.md5, ota_md5, OTA_MD5_LENTH);
        upper2lower(ota_context->ota_check.md5, OTA_MD5_LENTH);
        InitMd5(&md5);
    }
    ota_partition = MicoFlashGetInfo(MICO_PARTITION_OTA_TEMP);

    ota_offset = 0;
    MicoFlashErase(MICO_PARTITION_OTA_TEMP, 0x0, ota_partition->partition_length);

    CRC16_Init(&crc_context);
    ota_context->download_state.download_len = length;
    ota_context->ota_control = OTA_CONTROL_START;
exit:
    return err;
}
OSStatus eland_ota_data_write(uint8_t *data, uint16_t len)
{
    OSStatus err = kNoErr;

    if (len == 0)
        return err;

    ota_context->download_state.download_begin_pos += len;
    CRC16_Update(&crc_context, data, len);
    if (ota_context->ota_check.is_md5 == true)
    {
        Md5Update(&md5, data, len);
    }

    MicoFlashWrite(MICO_PARTITION_OTA_TEMP, &ota_offset, (uint8_t *)data, len);

    ota_server_progress_set(OTA_LOADING);

    if (ota_context->ota_control == OTA_CONTROL_PAUSE)
    {
        while (1)
        {
            if (ota_context->ota_control != OTA_CONTROL_PAUSE)
                break;
            mico_thread_msleep(100);
        }
    }

    if (ota_context->ota_control == OTA_CONTROL_STOP)
    {
        err = kUnsupportedErr;
    }

    return err;
}

void eland_ota_operation(void)
{
    uint16_t crc16 = 0;
    char md5_value[16] = {0};
    char md5_value_string[33] = {0};

    if (ota_context->download_state.download_len == ota_context->download_state.download_begin_pos)
    {
        CRC16_Final(&crc_context, &crc16);
        if (ota_context->ota_check.is_md5 == true)
        {
            Md5Final(&md5, (unsigned char *)md5_value);
            hex2str((uint8_t *)md5_value, 16, md5_value_string);
        }
        if (memcmp(md5_value_string, ota_context->ota_check.md5, OTA_MD5_LENTH) == 0)
        {
            ota_log("OTA md5, Calculation:%s, Get:%s", md5_value_string, ota_context->ota_check.md5);
            ota_server_progress_set(OTA_SUCCE);
            mico_ota_switch_to_new_fw(ota_context->download_state.download_len, crc16);
            mico_system_power_perform(mico_system_context_get(), eState_Software_Reset);
        }
        else
        {
            ota_log("OTA md5 check err, Calculation:%s, Get:%s", md5_value_string, ota_context->ota_check.md5);
            ota_server_progress_set(OTA_FAIL);
        }
    }

    if (ota_context != NULL)
    {
        if (ota_context->download_url.url != NULL)
        {
            free(ota_context->download_url.url);
            ota_context->download_url.url = NULL;
        }
        free(ota_context);
        ota_context = NULL;
    }

    ota_log("ota server will delete");
}
