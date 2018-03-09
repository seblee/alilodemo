/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-02-25 14:34:13
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-02-25 14:48:26
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "eland_sound.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
//#define CONFIG_SOUND_DEBUG
#ifdef CONFIG_SOUND_DEBUG
#define sound_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)
#else
#define sound_log(...)
#endif /* ! CONFIG_SOUND_DEBUG */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_mutex_t eland_sound_mutex = NULL; //flash 調用保護

_http_w_r_struct_t HTTP_W_R_struct = {NULL, NULL};

_sound_file_lib_t sound_file_list;
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

OSStatus start_eland_flash_service(void)
{
    OSStatus err = kGeneralErr;
    /*初始化互斥信号量*/
    err = mico_rtos_init_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    HTTP_W_R_struct.alarm_w_r_queue = (_sound_read_write_type_t *)calloc(sizeof(_sound_read_write_type_t), sizeof(uint8_t));

    err = mico_rtos_init_mutex(&HTTP_W_R_struct.mutex);
    require_noerr(err, exit);

    /*start init eland SPI*/
    err = flash_kh25_init();
    require_noerr(err, exit);

    err = sound_file_sort(&sound_file_list);
    require_noerr(err, exit);

exit:
    return err;
}

OSStatus sound_file_sort(_sound_file_lib_t *sound_list)
{
    OSStatus err;
    uint32_t sector_count = 0;
    _sound_file_type_t alarm_file_temp;
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    if (sound_list->lib)
        free(sound_list->lib);
    memset(sound_list, 0, sizeof(_sound_file_lib_t));

    do
    {
        flash_kh25_read((uint8_t *)(&alarm_file_temp), KH25L8006_SECTOR_SIZE * sector_count, sizeof(_sound_file_type_t));
        if (strncmp(alarm_file_temp.flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN)) //沒尋到標誌
        {
            sector_count++;
        }
        else //有文件標誌
        {
            if (sound_list->file_number == 0)
            {
                sound_list->sector_start = sector_count * KH25L8006_SECTOR_SIZE;
                sound_list->lib = (_sound_file_type_t *)calloc(1, sizeof(_sound_file_type_t));
                memcpy(sound_list->lib, &alarm_file_temp, sizeof(_sound_file_type_t));
                sound_list->file_number++;
            }
            else
            {
                sound_list->lib = (_sound_file_type_t *)realloc(sound_list->lib, ((sound_list->file_number + 1) * sizeof(_sound_file_type_t)));
                memcpy((sound_list->lib + sound_list->file_number), &alarm_file_temp, sizeof(_sound_file_type_t));
                sound_list->file_number++;
            }
            sector_count += (alarm_file_temp.file_len + sizeof(_sound_file_type_t)) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_file_temp.file_len + sizeof(_sound_file_type_t)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
            sound_list->sector_end = sector_count * KH25L8006_SECTOR_SIZE;
        }
    } while (sector_count < KH25_FLASH_FILE_COUNT);

exit:
    sound_log("read file file_number:%d", sound_list->file_number);
    err = mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}
OSStatus sound_file_read_write(_sound_file_lib_t *sound_list, _sound_read_write_type_t *alarm_w_r_temp)
{
    uint16_t i;
    OSStatus err = kGeneralErr;
    uint32_t sector_count = 0;
    _sound_file_type_t alarm_file_cache;
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    if (alarm_w_r_temp->is_read == true) //讀數據
    {
        sound_log("read file number_file:%d", sound_list->file_number);
        if (alarm_w_r_temp->pos == 0) //文件數據流首次讀取
        {
            for (i = 0; i < sound_list->file_number; i++)
            {
                if ((sound_list->lib + i)->sound_type == alarm_w_r_temp->sound_type)
                {
                    if (memcmp((sound_list->lib + i)->alarm_ID, alarm_w_r_temp->alarm_ID, ALARM_ID_LEN) == 0)
                    {
                        alarm_w_r_temp->total_len = (sound_list->lib + i)->file_len;
                        alarm_w_r_temp->file_address = (sound_list->lib + i)->file_address;
                        sound_log("total_len = %ld,sound_flash_pos = %ld", alarm_w_r_temp->total_len, alarm_w_r_temp->pos);
                        goto start_read_sound;
                    }
                }
            }
            err = kGeneralErr;
            sound_log("Do not find file");
            goto exit;
        }
    start_read_sound:
        if (alarm_w_r_temp->len > (alarm_w_r_temp->total_len - alarm_w_r_temp->pos))
            alarm_w_r_temp->len = alarm_w_r_temp->total_len - alarm_w_r_temp->pos;
        flash_kh25_read((uint8_t *)alarm_w_r_temp->sound_data, alarm_w_r_temp->file_address + alarm_w_r_temp->pos, alarm_w_r_temp->len);
        sound_log("inlen = %ld,sound_flash_pos = %ld", alarm_w_r_temp->len, alarm_w_r_temp->pos);
    }
    else //寫數據
    {
        sound_log("file write");
        if (alarm_w_r_temp->pos == 0) //文件數據流首次寫入
        {
            if (sound_list->file_number == 0)
            {
                sound_list->lib = (_sound_file_type_t *)calloc(1, sizeof(_sound_file_type_t));
                sound_list->file_number++;
            }
            else
            {
                sound_list->lib = (_sound_file_type_t *)realloc(sound_list->lib, ((sound_list->file_number + 1) * sizeof(_sound_file_type_t)));
                sound_list->file_number++;
            }

            sprintf((char *)(alarm_file_cache.flag), "%s", ALARM_FILE_FLAG_STRING);
            memcpy((char *)(alarm_file_cache.alarm_ID), alarm_w_r_temp->alarm_ID, ALARM_ID_LEN);

            alarm_file_cache.sound_type = alarm_w_r_temp->sound_type;
            alarm_file_cache.file_len = alarm_w_r_temp->total_len;
            alarm_file_cache.file_address = sound_list->sector_end + sizeof(_sound_file_type_t);
            alarm_w_r_temp->file_address = alarm_file_cache.file_address;
            memcpy(sound_list->lib + sound_list->file_number - 1, &alarm_file_cache, sizeof(_sound_file_type_t));

            sound_log("write sound file info address:%ld", sound_list->sector_end);
            flash_kh25_write_page((uint8_t *)(&alarm_file_cache), sound_list->sector_end, sizeof(_sound_file_type_t)); //寫入文件信息
            sector_count = (alarm_w_r_temp->total_len + alarm_w_r_temp->file_address) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_w_r_temp->total_len + alarm_w_r_temp->file_address) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
            if (sector_count >= KH25_FLASH_FILE_COUNT)
            {
                err = kGeneralErr;
                sound_log("flash full");
                goto exit;
            }
            sound_list->sector_end = KH25L8006_SECTOR_SIZE * sector_count;
        }

        sound_log("write sound file data address :%ld", (alarm_w_r_temp->file_address + alarm_w_r_temp->pos));
        flash_kh25_write_page((uint8_t *)alarm_w_r_temp->sound_data,
                              (alarm_w_r_temp->file_address + alarm_w_r_temp->pos),
                              alarm_w_r_temp->len);
    }
exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

OSStatus SOUND_CHECK_DEFAULT_FILE(void)
{
    OSStatus err = kGeneralErr;
    _sound_file_type_t alarm_file_temp;
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    flash_kh25_read((uint8_t *)(&alarm_file_temp), 0, sizeof(_sound_file_type_t));
    if (strncmp(alarm_file_temp.flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN)) //沒尋到標誌
    {
        err = kGeneralErr;
        goto exit;
    }
    if (alarm_file_temp.sound_type != SOUND_FILE_DEFAULT)
    {
        err = kGeneralErr;
        goto exit;
    }
exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

OSStatus SOUND_FILE_CLEAR(void)
{
    KH25L8006_SECTOR_SIZE
    OSStatus err = kGeneralErr;
    uint8_t block_count;
    uint16_t sector_count;
    mico_rtos_lock_mutex(&eland_sound_mutex);

    flash_kh25_read((uint8_t *)(&alarm_file_temp), 0, sizeof(_sound_file_type_t));
    if (strncmp(alarm_file_temp.flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN) == 0)
    {
        sound_file_list.file_number = 1;
        sector_count += (alarm_file_temp.file_len + sizeof(_sound_file_type_t)) / KH25L8006_SECTOR_SIZE;
        sector_count += (((alarm_file_temp.file_len + sizeof(_sound_file_type_t)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
    }
    else
        sound_file_list.file_number = 0;

    for (block_count = 0; block_count < KH25_BLOCKCOUNT - 1; block_count++)
    {
        flash_kh25_block_erase((uint32_t)block_count * KH25L8006_BLOCK_SIZE)
    }
    sector_count = 16 * (KH25_BLOCKCOUNT - 1);
    for (sector_count = sector_count; sector_count < KH25_FLASH_FILE_COUNT; sector_count++)
    {
        flash_kh25_sector_erase((uint32_t)sector_count * KH25L8006_BLOCK_SIZE)
    }

exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}
