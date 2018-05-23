/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-02-25 14:34:13
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-16 16:11:14
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "eland_sound.h"
#include "netclock_ota.h"
#include "eland_alarm.h"
#include "netclock_wifi.h"
#include "audio_service.h"
#include "error_bin.h"
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
static bool is_sound_file_usable(_sound_file_type_t *sound_file, _eland_alarm_list_t *alarm_list);
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
    uint32_t sector_count = 0, sound_id = 0;
    uint8_t i;
    _sound_file_type_t alarm_file_temp;
    char string_buff[strlen(ALARM_FILE_END_STRING)];
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    sound_log("sound_file_sort");
    if (sound_list->lib)
        free(sound_list->lib);
    memset(sound_list, 0, sizeof(_sound_file_lib_t));
    do
    {
        flash_kh25_read((uint8_t *)(&alarm_file_temp), KH25L8006_SECTOR_SIZE * sector_count, sizeof(_sound_file_type_t));
        if (strncmp(alarm_file_temp.flag, ALARM_FILE_HEADER_STRING, ALARM_FILE_HEADER_LEN) == 0) //有文件头
        {
            memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
            flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
            if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0) //有文件尾
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
                    for (i = 0; i < sound_list->file_number; i++) //去重
                    {
                        if (memcmp((sound_list->lib + i)->alarm_ID, alarm_file_temp.alarm_ID, ALARM_ID_LEN) == 0)
                            goto exit;
                    }
                    sound_list->lib = (_sound_file_type_t *)realloc(sound_list->lib, ((sound_list->file_number + 1) * sizeof(_sound_file_type_t)));
                    memcpy((sound_list->lib + sound_list->file_number), &alarm_file_temp, sizeof(_sound_file_type_t));
                    sound_list->file_number++;
                }
                sector_count += (alarm_file_temp.file_len + sizeof(_sound_file_type_t)) / KH25L8006_SECTOR_SIZE;
                sector_count += (((alarm_file_temp.file_len + sizeof(_sound_file_type_t)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
                sound_list->sector_end = sector_count * KH25L8006_SECTOR_SIZE;
            }
            else
                break;
        }
        else
            break;
    } while (sector_count < KH25_FLASH_FILE_COUNT);

exit:
    sound_log("read file file_number:%d", sound_list->file_number);
    for (i = 0; i < sound_list->file_number; i++)
    {
        switch ((sound_list->lib + i)->sound_type)
        {
        case SOUND_FILE_VID:
            sound_log("sound:%d,vid :%s", i, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_SID:
            memcpy(&sound_id, (sound_list->lib + i)->alarm_ID, sizeof(int32_t));
            sound_log("sound:%d,sid :%ld", i, sound_id);
            break;
        case SOUND_FILE_OFID:
            sound_log("sound:%d,ofid :%s", i, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_DEFAULT:
            sound_log("sound:%d,default:%s", i, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_WEATHER_0:
        case SOUND_FILE_WEATHER_E:
        case SOUND_FILE_WEATHER_F:
            sound_log("sound:%d,weather:%s", i, (sound_list->lib + i)->alarm_ID);
            break;
        default:
            sound_log("sound:%d,type :%d", i, (sound_list->lib + i)->sound_type);
            break;
        }
    }
    sound_log("flash_capacity:%ld", get_flash_capacity());

    err = mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}
OSStatus sound_file_read_write(_sound_file_lib_t *sound_list, _sound_read_write_type_t *alarm_w_r_temp)
{
    uint16_t i;
    OSStatus err = kNoErr;
    uint32_t sector_count = 0;
    _sound_file_type_t alarm_file_cache;
    char string_buff[strlen(ALARM_FILE_END_STRING)];

    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    if (alarm_w_r_temp->operation_mode == FILE_READ) //讀數據
    {
        // sound_log("read file number_file:%d", sound_list->file_number);
        if (alarm_w_r_temp->pos == 0) //文件數據流首次讀取
        {
            sound_log("file read");
            for (i = 0; i < sound_list->file_number; i++)
            {
                if (memcmp((sound_list->lib + i)->alarm_ID, alarm_w_r_temp->alarm_ID, ALARM_ID_LEN) == 0)
                {
                    memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
                    flash_kh25_read((uint8_t *)(string_buff), (sound_list->lib + i)->file_len + (sound_list->lib + i)->file_address, strlen(ALARM_FILE_END_STRING));
                    if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0)
                    {
                        alarm_w_r_temp->total_len = (sound_list->lib + i)->file_len;
                        alarm_w_r_temp->file_address = (sound_list->lib + i)->file_address;
                        sound_log("len = %ld,pos = %ld,address:%ld", alarm_w_r_temp->total_len, alarm_w_r_temp->pos, alarm_w_r_temp->file_address);
                        goto start_read_sound;
                    }
                    else
                        break;
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
    }
    else if (alarm_w_r_temp->operation_mode == FILE_WRITE) //寫數據
    {
        if (alarm_w_r_temp->pos == 0) //文件數據流首次寫入
        {
            sound_log("file write");
            sprintf((char *)(alarm_file_cache.flag), "%s", ALARM_FILE_HEADER_STRING);
            memset((char *)(alarm_file_cache.alarm_ID), 0, ALARM_ID_LEN);
            memcpy((char *)(alarm_file_cache.alarm_ID), alarm_w_r_temp->alarm_ID, ALARM_ID_LEN);

            alarm_file_cache.sound_type = alarm_w_r_temp->sound_type;
            alarm_file_cache.file_len = alarm_w_r_temp->total_len;
            alarm_file_cache.file_address = sound_list->sector_end + sizeof(_sound_file_type_t);
            alarm_w_r_temp->file_address = alarm_file_cache.file_address;

            sound_log("write sound file info address:%ld", sound_list->sector_end);
            sector_count = (alarm_w_r_temp->total_len + alarm_w_r_temp->file_address + strlen(ALARM_FILE_END_STRING)) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_w_r_temp->total_len + alarm_w_r_temp->file_address + strlen(ALARM_FILE_END_STRING)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
            if (sector_count >= KH25_FLASH_FILE_COUNT)
            {
                err = kGeneralErr;
                sound_log("flash full");
                goto exit;
            }
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
            memcpy(sound_list->lib + sound_list->file_number - 1, &alarm_file_cache, sizeof(_sound_file_type_t));
            flash_kh25_write_page((uint8_t *)(&alarm_file_cache), sound_list->sector_end, sizeof(_sound_file_type_t)); //寫入文件信息
            sound_list->sector_end = KH25L8006_SECTOR_SIZE * sector_count;
        }
        //  sound_log("write sound file data address :%ld", (alarm_w_r_temp->file_address + alarm_w_r_temp->pos));
        flash_kh25_write_page((uint8_t *)alarm_w_r_temp->sound_data,
                              (alarm_w_r_temp->file_address + alarm_w_r_temp->pos),
                              alarm_w_r_temp->len);
    }
    else if (alarm_w_r_temp->operation_mode == FILE_REMOVE) //file remove
    {
        sound_log("file remove");
        for (i = 0; i < sound_list->file_number; i++)
        {
            if (memcmp((sound_list->lib + i)->alarm_ID, alarm_w_r_temp->alarm_ID, ALARM_ID_LEN) == 0)
            {
                memset((sound_list->lib + i)->alarm_ID, 0, ALARM_ID_LEN);
                sound_log("len = %ld,pos = %ld,address:%ld", alarm_w_r_temp->total_len, alarm_w_r_temp->pos, alarm_w_r_temp->file_address);
                goto start_remove_sound;
            }
        }

        err = kNoErr;
        sound_log("Do not find file");
        goto exit;

    start_remove_sound:
        flash_kh25_write_page((uint8_t *)(sound_list->lib + i), (sound_list->lib + i)->file_address - sizeof(_sound_file_type_t), sizeof(_sound_file_type_t)); //寫入文件信息
    }
exit:
    if (err != kNoErr)
        sound_log("err = %d", err);

    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

OSStatus SOUND_CHECK_DEFAULT_FILE(void)
{
    OSStatus err = kGeneralErr;
    _sound_file_type_t alarm_file_temp;
    char string_buff[strlen(ALARM_FILE_END_STRING)];
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    flash_kh25_read((uint8_t *)(&alarm_file_temp), 0, sizeof(_sound_file_type_t));
    if (strncmp(alarm_file_temp.alarm_ID, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK))) //沒尋到標誌
    {
        err = kGeneralErr;
        goto exit;
    }
    if (alarm_file_temp.sound_type != SOUND_FILE_DEFAULT)
    {
        err = kGeneralErr;
        goto exit;
    }
    memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
    flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
    if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)))
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
    OSStatus err = kGeneralErr;
    uint16_t sector_count = 0;
    _sound_file_type_t alarm_file_temp;
    char string_buff[strlen(ALARM_FILE_END_STRING)];
    mico_rtos_lock_mutex(&eland_sound_mutex);

    flash_kh25_read((uint8_t *)(&alarm_file_temp), 0, sizeof(_sound_file_type_t));
    if (strncmp(alarm_file_temp.alarm_ID, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK)) == 0)
    {
        memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
        flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
        if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0)
        {
            sector_count += (alarm_file_temp.file_len + sizeof(_sound_file_type_t) + strlen(ALARM_FILE_END_STRING)) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_file_temp.file_len + sizeof(_sound_file_type_t) + strlen(ALARM_FILE_END_STRING)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
            if (sound_file_list.lib)
                sound_file_list.lib = realloc(sound_file_list.lib, sizeof(_sound_file_type_t));
            else
                sound_file_list.lib = calloc(sizeof(_sound_file_type_t), sizeof(uint8_t));
            memcpy(sound_file_list.lib, &alarm_file_temp, sizeof(_sound_file_type_t));
            sound_file_list.file_number = 1;
            sound_file_list.sector_start = 0;
            sound_file_list.sector_end = sector_count * KH25L8006_BLOCK_SIZE;
        }
    }
    else
    {
        if (sound_file_list.lib)
        {
            free(sound_file_list.lib);
            sound_file_list.lib = NULL;
        }
        sound_file_list.sector_start = 0;
        sound_file_list.sector_end = 0;
        sound_file_list.file_number = 0;
    }

    flash_kh25_sector_erase((uint32_t)sector_count * KH25L8006_SECTOR_SIZE);
    sound_log("sound_file numberr :%d", sound_file_list.file_number);
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

void file_download(void)
{
    OSStatus err;
    _download_type_t download_type;
    // eland_push_http_queue(DOWNLOAD_OID);
wait_for_queue:
    err = mico_rtos_pop_from_queue(&http_queue, &download_type, MICO_WAIT_FOREVER);
    require_noerr(err, exit);
    mico_rtos_lock_mutex(&HTTP_W_R_struct.mutex);
operation_queue:
    switch (download_type)
    {
    case DOWNLOAD_SCAN:
        alarm_sound_scan();
        break;
    case DOWNLOAD_OID:
        alarm_sound_oid();
        while (!mico_rtos_is_queue_empty(&http_queue))
        {
            err = mico_rtos_pop_from_queue(&http_queue, &download_type, 0);
            require_noerr(err, exit);
            if (download_type != DOWNLOAD_OID)
            {
                sound_log("goto operation_queue");
                goto operation_queue;
            }
        }
        // eland_push_http_queue(DOWNLOAD_OID);
        break;
    case DOWNLOAD_OTA:
        eland_ota();
        break;
    case DOWNLOAD_WEATHER:
        weather_sound_scan();
        break;
    case GO_INTO_AP_MODE:
        Start_wifi_Station_SoftSP_Thread(Soft_AP);
        goto out;
        break;
    default:
        break;
    }
exit:
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
    goto wait_for_queue;
out:
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
}

int32_t get_flash_capacity(void)
{
    int32_t capacity;
    capacity = KH25_CHECK_ADDRESS - sound_file_list.sector_end;
    return capacity;
}

OSStatus eland_sound_file_arrange(_sound_file_lib_t *sound_list)
{
    OSStatus err = kGeneralErr;
    _sound_file_lib_t sound_list_temp;
    _sound_file_type_t alarm_file_cache;
    uint32_t sector_count = 0;
    uint16_t i;
    uint8_t *Cache = NULL;
    uint32_t read_address = 0, write_address = 0, read_Threshold = 0;
    bool first_move_flag = false;
    uint8_t data_buff[10] = {0xaa};

    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    memset(&sound_list_temp, 0, sizeof(_sound_file_lib_t));
    sound_log("###start arrange flash number:%d####", sound_list->file_number);
    for (i = 0; i < sound_list->file_number; i++)
    {
        if (is_sound_file_usable(sound_list->lib + i, &alarm_list))
        {
            if (sound_list_temp.file_number == 0)
                sound_list_temp.lib = calloc(1, sizeof(_sound_file_type_t));
            else
                sound_list_temp.lib = realloc(sound_list_temp.lib, (sound_list_temp.file_number + 1) * sizeof(_sound_file_type_t));

            sprintf((char *)(alarm_file_cache.flag), "%s", ALARM_FILE_HEADER_STRING);
            memcpy((char *)(alarm_file_cache.alarm_ID), (sound_list->lib + i)->alarm_ID, ALARM_ID_LEN);

            alarm_file_cache.sound_type = (sound_list->lib + i)->sound_type;
            alarm_file_cache.file_len = (sound_list->lib + i)->file_len;
            alarm_file_cache.file_address = sound_list_temp.sector_end + sizeof(_sound_file_type_t);
            if (alarm_file_cache.file_address != (sound_list->lib + i)->file_address)
            {
                /**data move**/
                sound_log("data move");
                Cache = calloc(1, KH25L8006_SECTOR_SIZE);
                read_address = (sound_list->lib + i)->file_address - sizeof(_sound_file_type_t);
                write_address = alarm_file_cache.file_address - sizeof(_sound_file_type_t);
                read_Threshold = (sound_list->lib + i)->file_address + (sound_list->lib + i)->file_len;
                first_move_flag = true;
                do
                {
                    flash_kh25_read(Cache, read_address, KH25L8006_SECTOR_SIZE);
                    if (first_move_flag)
                    {
                        first_move_flag = false;
                        memcpy(Cache, &alarm_file_cache, sizeof(_sound_file_type_t));
                    }
                    flash_kh25_sector_erase(write_address);
                    flash_kh25_write_page(Cache, write_address, KH25L8006_SECTOR_SIZE);
                    read_address += KH25L8006_SECTOR_SIZE;
                    write_address += KH25L8006_SECTOR_SIZE;
                } while (read_address < read_Threshold);
                free(Cache);
            }
            memcpy(sound_list_temp.lib + sound_list_temp.file_number, &alarm_file_cache, sizeof(_sound_file_type_t));
            sound_list_temp.file_number++;
            sector_count = (alarm_file_cache.file_len + alarm_file_cache.file_address + strlen(ALARM_FILE_END_STRING)) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_file_cache.file_len + alarm_file_cache.file_address + strlen(ALARM_FILE_END_STRING)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
            sound_list_temp.sector_end = KH25L8006_SECTOR_SIZE * sector_count;
        }
        else
        {
            memset(data_buff, 0xaa, sizeof(data_buff));
            flash_kh25_write_page(data_buff, (sound_list->lib + i)->file_address - sizeof(_sound_file_type_t) + 1, 5);
        }
    }
    free(sound_list->lib);
    memcpy(sound_list, &sound_list_temp, sizeof(_sound_file_lib_t));
    sound_list->lib = sound_list_temp.lib;

    sound_log("###secotr arrange done:%d####", sound_list->file_number);
    err = mico_rtos_unlock_mutex(&eland_sound_mutex);
exit:
    return err;
}

static bool is_sound_file_usable(_sound_file_type_t *sound_file, _eland_alarm_list_t *alarm_list)
{
    uint8_t i;
    int32_t sound_id;
    __elsv_alarm_data_t *nearest = NULL;
    switch (sound_file->sound_type)
    {
    case SOUND_FILE_VID:
        for (i = 0; i < alarm_list->alarm_number; i++)
        {
            if ((((alarm_list->alarm_lib + i)->alarm_pattern == 2) ||
                 ((alarm_list->alarm_lib + i)->alarm_pattern == 3)) &&
                (memcmp(sound_file->alarm_ID, (alarm_list->alarm_lib + i)->voice_alarm_id, ALARM_ID_LEN) == 0))
            {
                sound_log("FILE_VID %s is usable", sound_file->alarm_ID);
                goto exit;
            }
        }
        break;
    case SOUND_FILE_SID:
        memcpy(&sound_id, sound_file->alarm_ID, sizeof(sound_id));
        sound_log("FILE_SID %ld", sound_id);
        for (i = 0; i < alarm_list->alarm_number; i++)
        {
            sound_log("alarm_sound_id %ld", (alarm_list->alarm_lib + i)->alarm_sound_id);

            if ((((alarm_list->alarm_lib + i)->alarm_pattern == 1) ||
                 ((alarm_list->alarm_lib + i)->alarm_pattern == 3)) &&
                (sound_id == (alarm_list->alarm_lib + i)->alarm_sound_id))
            {
                sound_log("FILE_SID %ld is usable", sound_id);
                goto exit;
            }
        }
        break;
    case SOUND_FILE_OFID:
        for (i = 0; i < alarm_list->alarm_number; i++)
        {
            if (memcmp(sound_file->alarm_ID, (alarm_list->alarm_lib + i)->alarm_off_voice_alarm_id, ALARM_ID_LEN) == 0)
            {
                sound_log("FILE_OFID %s is usable", sound_file->alarm_ID);
                goto exit;
            }
        }
        break;
    case SOUND_FILE_DEFAULT:
        if (memcmp(sound_file->alarm_ID, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK)) == 0)
        {
            sound_log("FILE_OFID %s is usable", sound_file->alarm_ID);
            goto exit;
        }
        if (sound_file->file_address == sizeof(_sound_file_type_t))
        {
            sound_log("FILE_DEFAULT %s is usable", sound_file->alarm_ID);
            goto exit;
        }
    case SOUND_FILE_WEATHER_0:
    case SOUND_FILE_WEATHER_E:
    case SOUND_FILE_WEATHER_F:
        nearest = get_nearest_alarm();
        if (nearest)
        {
            if (strncmp(sound_file->alarm_ID, nearest->voice_alarm_id, ALARM_ID_LEN) == 0)
            {
                sound_log("FILE_OFID %s is usable", sound_file->alarm_ID);
                goto exit;
            }
            if (strncmp(sound_file->alarm_ID, nearest->alarm_off_voice_alarm_id, ALARM_ID_LEN) == 0)
            {
                sound_log("FILE_OFID %s is usable", sound_file->alarm_ID);
                goto exit;
            }
        }
        break;
    default:
        break;
    }
    return false;
exit:
    return true;
}

OSStatus alarm_sound_file_check(char *alarm_id)
{
    OSStatus err = kNoErr;
    uint8_t i;
    char string_buff[strlen(ALARM_FILE_END_STRING)];
    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);

    sound_log("file check");
    for (i = 0; i < sound_file_list.file_number; i++)
    {
        if (memcmp((sound_file_list.lib + i)->alarm_ID, alarm_id, ALARM_ID_LEN) == 0)
        {
            memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
            flash_kh25_read((uint8_t *)(string_buff), (sound_file_list.lib + i)->file_len + (sound_file_list.lib + i)->file_address, strlen(ALARM_FILE_END_STRING));
            if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0)
            {
                sound_log("find file len = %ld,address:%ld", (sound_file_list.lib + i)->file_len, (sound_file_list.lib + i)->file_address);
                err = kNoErr;
                goto exit;
            }
            else
                break;
        }
    }
    err = kGeneralErr;
    sound_log("Do not find file");
exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

OSStatus eland_play_oid_error_sound(void)
{
    OSStatus err = kNoErr;
    uint8_t fm_test_cnt = 0;
    AUDIO_STREAM_PALY_S fm_stream;
    mscp_result_t result = MSCP_RST_ERROR;
    mscp_status_t audio_status;
    uint32_t inPos = 0;
    sound_log("error sound");

    fm_stream.type = AUDIO_STREAM_TYPE_MP3;
    fm_stream.stream_id = audio_service_system_generate_stream_id();
    fm_stream.total_len = sizeof(error_sound);
start_start:
    fm_stream.pdata = (const uint8_t *)error_sound + inPos;
    if ((fm_stream.total_len - inPos) > 4000) //len
        fm_stream.stream_len = 4000;
    else
        fm_stream.stream_len = fm_stream.total_len - inPos;
    if ((++fm_test_cnt) >= 10)
    {
        fm_test_cnt = 0;
        sound_log("fm_stream.type[%d],fm_stream.stream_id[%d],fm_stream.total_len[%d],fm_stream.stream_len[%d]",
                  (int)fm_stream.type, (int)fm_stream.stream_id, (int)fm_stream.total_len, (int)fm_stream.stream_len);
    }
audio_transfer:
    require_action_string(get_alarm_stream_state() != STREAM_STOP, exit, err = kGeneralErr, "user set stoped!");
    err = audio_service_stream_play(&result, &fm_stream);
    if (err != kNoErr)
    {
        sound_log("audio_stream_play() error!!!!");
        return false;
    }
    else
    {
        if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
        {
            sound_log("new slave set pause!!!");
            mico_rtos_thread_msleep(500); //time set 1000ms!!!
            goto audio_transfer;
        }
        else
        {
            err = kNoErr;
        }
    }
    inPos += fm_stream.stream_len;
    if (inPos < fm_stream.total_len)
        goto start_start;

check_audio:
    require_action_string(get_alarm_stream_state() != STREAM_STOP, exit, err = kGeneralErr, "user set stoped!");
    audio_service_get_audio_status(&result, &audio_status);
    if (audio_status == MSCP_STATUS_STREAM_PLAYING)
    {
        mico_rtos_thread_msleep(500);
        sound_log("oid playing");
        goto check_audio;
    }
exit:
    audio_service_get_audio_status(&result, &audio_status);
    if (audio_status != MSCP_STATUS_IDLE)
    {
        sound_log("stop playing");
        audio_service_stream_stop(&result, fm_stream.stream_id);
    }

    sound_log("play stopped err:%d", err);
    return err;
}
