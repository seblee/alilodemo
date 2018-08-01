/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-02-25 14:34:13
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-07-10 15:15:28
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
#include "default_bin.h"
/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define CONFIG_SOUND_DEBUG
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

const uint16_t _area_sector[7] = {SECTOR_AREA0, SECTOR_AREA1, SECTOR_AREA2, SECTOR_AREA3, SECTOR_AREA4, SECTOR_AREA5, SECTOR_AREA6};
const uint16_t _start_sector[7] = {START_SECTOR_AREA0, START_SECTOR_AREA1, START_SECTOR_AREA2, START_SECTOR_AREA3, START_SECTOR_AREA4, START_SECTOR_AREA5, START_SECTOR_AREA6};
const uint32_t _start_address[7] = {START_ADDRESS_AREA0, START_ADDRESS_AREA1, START_ADDRESS_AREA2, START_ADDRESS_AREA3, START_ADDRESS_AREA4, START_ADDRESS_AREA5, START_ADDRESS_AREA6};
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

    memset(sound_list, 0, sizeof(_sound_file_lib_t));
    for (i = 0; i < 7; i++)
    {
        /**clear exist_flag**/
        sound_list->exist_flag &= ~(1 << i);
        sector_count = _start_sector[i];
        flash_kh25_read((uint8_t *)(&alarm_file_temp), KH25L8006_SECTOR_SIZE * sector_count, sizeof(_sound_file_type_t));
        if (strncmp(alarm_file_temp.flag, ALARM_FILE_HEADER_STRING, ALARM_FILE_HEADER_LEN) == 0) //有文件头
        {
            memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
            flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
            if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0) //有文件尾
            {
                memcpy(&(sound_list->lib[i]), &alarm_file_temp, sizeof(_sound_file_type_t));
                sound_list->exist_flag |= (1 << i);
            }
        }
    }

exit:
    for (i = 0; i < 7; i++)
    {
        switch ((sound_list->lib + i)->sound_type)
        {
        case SOUND_FILE_VID:
            sound_log("sound:%d,lenth:%ld,vid :%s", i, (sound_list->lib + i)->file_len, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_SID:
            memcpy(&sound_id, (sound_list->lib + i)->alarm_ID, sizeof(int32_t));
            sound_log("sound:%d,lenth:%ld,sid :%ld", i, (sound_list->lib + i)->file_len, sound_id);
            break;
        case SOUND_FILE_OFID:
            sound_log("sound:%d,lenth:%ld,ofid :%s", i, (sound_list->lib + i)->file_len, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_DEFAULT:
            sound_log("sound:%d,lenth:%ld,default:%s", i, (sound_list->lib + i)->file_len, (sound_list->lib + i)->alarm_ID);
            break;
        case SOUND_FILE_WEATHER_0:
        case SOUND_FILE_WEATHER_A:
        case SOUND_FILE_WEATHER_B:
        case SOUND_FILE_WEATHER_C:
        case SOUND_FILE_WEATHER_D:
        case SOUND_FILE_WEATHER_E:
        case SOUND_FILE_WEATHER_F:
            sound_log("sound:%d,lenth:%ld,weather:%s", i, (sound_list->lib + i)->file_len, (sound_list->lib + i)->alarm_ID);
            break;
        default:
            sound_log("sound:%d,lenth:%ld,type :%d", i, (sound_list->lib + i)->file_len, (sound_list->lib + i)->sound_type);
            break;
        }
    }
    sound_log("flash_capacity:%d", get_flash_capacity());

    err = mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}
OSStatus sound_file_read_write(_sound_file_lib_t *sound_list, _sound_read_write_type_t *alarm_w_r_temp)
{
    uint16_t i;
    OSStatus err = kNoErr;
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
            for (i = 0; i < 7; i++)
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
                        sound_list->reading_point = i;
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
            if (alarm_w_r_temp->write_state == WRITE_ING)
                alarm_file_cache.file_len = (uint32_t)0xffffffff;
            else if (alarm_w_r_temp->write_state == WRITE_END)
                alarm_file_cache.file_len = alarm_w_r_temp->total_len;
            if (alarm_w_r_temp->sound_type == SOUND_FILE_DEFAULT)
            {
                i = 0;
                alarm_file_cache.file_address = _start_address[0] + sizeof(_sound_file_type_t);
                sound_list->exist_flag &= ~(1 << i);
            }
            else
            {
                for (i = 1; i < 7; i++)
                {
                    if ((sound_list->exist_flag & (1 << i)) == 0)
                    {
                        alarm_file_cache.file_address = _start_address[i] + sizeof(_sound_file_type_t);
                        goto check_over;
                    }
                }
                err = kGeneralErr;
                sound_log("flash full");
                goto exit;
            }
        check_over:
            sound_list->writing_point = i;
            alarm_w_r_temp->file_address = alarm_file_cache.file_address;

            memcpy(sound_list->lib + i, &alarm_file_cache, sizeof(_sound_file_type_t));
            flash_kh25_write_page((uint8_t *)(&alarm_file_cache), _start_address[i], sizeof(_sound_file_type_t)); //寫入文件信息
        }

        // sound_log("write sound file data address :%ld", (alarm_w_r_temp->file_address + alarm_w_r_temp->pos));
        flash_kh25_write_page((uint8_t *)alarm_w_r_temp->sound_data,
                              (alarm_w_r_temp->file_address + alarm_w_r_temp->pos),
                              alarm_w_r_temp->len);

        if (alarm_w_r_temp->write_state == WRITE_END)
        {
            alarm_file_cache.file_len = alarm_w_r_temp->total_len;
            (sound_list->lib + sound_list->writing_point)->file_len = alarm_w_r_temp->total_len;
            /*********write file len********/
            flash_kh25_write_page((uint8_t *)(&alarm_file_cache) + sizeof(alarm_file_cache.flag) + sizeof(alarm_file_cache.alarm_ID) + sizeof(alarm_file_cache.sound_type),
                                  _start_address[sound_list->writing_point] + sizeof(alarm_file_cache.flag) + sizeof(alarm_file_cache.alarm_ID) + sizeof(alarm_file_cache.sound_type),
                                  sizeof(uint32_t));
            sound_list->exist_flag |= (1 << sound_list->writing_point);
        }
    }
    else if (alarm_w_r_temp->operation_mode == FILE_REMOVE) //file remove
    {
        sound_log("file remove");
        for (i = 0; i < 7; i++)
        {
            if (memcmp((sound_list->lib + i)->alarm_ID, alarm_w_r_temp->alarm_ID, ALARM_ID_LEN) == 0)
            {
                sound_list->exist_flag &= ~(1 << i);
                sound_log("len = %ld,pos = %ld,address:%ld", alarm_w_r_temp->total_len, alarm_w_r_temp->pos, alarm_w_r_temp->file_address);
            }
        }

        err = kNoErr;
        sound_log("Do not find file");
        goto exit;
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
        sound_file_list.exist_flag &= ~(1 << 0);
        goto exit;
    }
    if (alarm_file_temp.sound_type != SOUND_FILE_DEFAULT)
    {
        err = kGeneralErr;
        sound_file_list.exist_flag &= ~(1 << 0);
        goto exit;
    }
    memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
    flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
    if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)))
    {
        err = kGeneralErr;
        sound_file_list.exist_flag &= ~(1 << 0);
        goto exit;
    }
exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

OSStatus SOUND_FILE_CLEAR(void)
{
    OSStatus err = kGeneralErr;
    _sound_file_type_t alarm_file_temp;
    char string_buff[strlen(ALARM_FILE_END_STRING)];
    mico_rtos_lock_mutex(&eland_sound_mutex);
    memset(&sound_file_list, 0, sizeof(_sound_file_lib_t));
    flash_kh25_read((uint8_t *)(&alarm_file_temp), 0, sizeof(_sound_file_type_t));
    if (strncmp(alarm_file_temp.alarm_ID, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK)) == 0)
    {
        memset(string_buff, 0, strlen(ALARM_FILE_END_STRING));
        flash_kh25_read((uint8_t *)(string_buff), alarm_file_temp.file_len + alarm_file_temp.file_address, strlen(ALARM_FILE_END_STRING));
        if (strncmp(string_buff, ALARM_FILE_END_STRING, strlen(ALARM_FILE_END_STRING)) == 0)
        {
            sound_file_list.exist_flag |= (1 << 0);
            memcpy(sound_file_list.lib, &alarm_file_temp, sizeof(_sound_file_type_t));
            goto exit;
        }
    }
    flash_kh25_sector_erase((uint32_t)0);
exit:
    mico_rtos_unlock_mutex(&eland_sound_mutex);
    return err;
}

void file_download(void)
{
    OSStatus err;
    _download_type_t download_type;
    mico_rtos_thread_sleep(3);
    // eland_push_http_queue(DOWNLOAD_OID);
wait_for_queue:
    sound_log("#####:num_of_chunks:%d, free:%d", MicoGetMemoryInfo()->num_of_chunks, MicoGetMemoryInfo()->free_memory);
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
        break;
    case DOWNLOAD_OTA:
        eland_ota();
        break;
    case DOWNLOAD_0_E_F:
        weather_sound_0_e_f();
        break;
    case DOWNLOAD_C_D:
        weather_sound_c_d();
        break;
    case DOWNLOAD_A:
    case DOWNLOAD_B:
        weather_sound_a_b(download_type);
        break;
    case GO_INTO_AP_MODE:
    case GO_OUT:
        goto out;
        break;
    default:
        break;
    }

    err = mico_rtos_pop_from_queue(&http_queue, &download_type, 0);
    if (err == kNoErr)
        goto operation_queue;
exit:
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
    goto wait_for_queue;
out:
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
}

uint8_t get_flash_capacity(void)
{
    uint8_t capacity = 0, i;
    for (i = 0; i < 7; i++)
    {
        if (sound_file_list.exist_flag & (1 << i))
            capacity++;
    }
    return capacity;
}

OSStatus eland_sound_file_arrange(_sound_file_lib_t *sound_list)
{
    OSStatus err = kGeneralErr;
    uint16_t i;

    err = mico_rtos_lock_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    sound_log("###start arrange flash ####");

    // mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < 7; i++)
    {
        if (!is_sound_file_usable(sound_list->lib + i, &alarm_list))
        {
            memset(sound_list->lib + i, 0, sizeof(_sound_file_type_t));
            sound_list->exist_flag &= ~(1 << i);
        }
        else
            sound_list->exist_flag |= (1 << i);
    }
    // mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    sound_log("###secotr arrange done");
    err = mico_rtos_unlock_mutex(&eland_sound_mutex);
exit:
    return err;
}

static bool is_sound_file_usable(_sound_file_type_t *sound_file, _eland_alarm_list_t *alarm_list)
{
    uint8_t i, temp_point = 0;
    int32_t sound_id;
    __elsv_alarm_data_t *nearest = NULL;
    if (alarm_list->alarm_number == 0)
        return false;
    switch (sound_file->sound_type)
    {
    case SOUND_FILE_VID:
        for (i = alarm_list->alarm_now_serial; i < (alarm_list->alarm_now_serial + 3); i++)
        {
            temp_point = i % alarm_list->alarm_number;
            if ((((alarm_list->alarm_lib + temp_point)->alarm_pattern == 2) ||
                 ((alarm_list->alarm_lib + temp_point)->alarm_pattern == 3)) &&
                (memcmp(sound_file->alarm_ID, (alarm_list->alarm_lib + temp_point)->voice_alarm_id, ALARM_ID_LEN) == 0))
            {
                sound_log("FILE_VID %s is usable", sound_file->alarm_ID);
                goto exit;
            }
        }
        break;
    case SOUND_FILE_SID:
        memcpy(&sound_id, sound_file->alarm_ID, sizeof(sound_id));
        sound_log("FILE_SID %ld", sound_id);
        for (i = alarm_list->alarm_now_serial; i < (alarm_list->alarm_now_serial + 3); i++)
        {
            temp_point = i % alarm_list->alarm_number;
            sound_log("alarm_sound_id %ld", (alarm_list->alarm_lib + temp_point)->alarm_sound_id);

            if ((((alarm_list->alarm_lib + temp_point)->alarm_pattern == 1) ||
                 ((alarm_list->alarm_lib + temp_point)->alarm_pattern == 3)) &&
                (sound_id == (alarm_list->alarm_lib + temp_point)->alarm_sound_id))
            {
                sound_log("FILE_SID %ld is usable", sound_id);
                goto exit;
            }
        }
        break;
    case SOUND_FILE_OFID:
        for (i = alarm_list->alarm_now_serial; i < (alarm_list->alarm_now_serial + 3); i++)
        {
            temp_point = i % alarm_list->alarm_number;
            if (memcmp(sound_file->alarm_ID, (alarm_list->alarm_lib + temp_point)->alarm_off_voice_alarm_id, ALARM_ID_LEN) == 0)
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
        break;
    case SOUND_FILE_WEATHER_A:
    case SOUND_FILE_WEATHER_B:
        break;
    case SOUND_FILE_WEATHER_0:
    case SOUND_FILE_WEATHER_C:
    case SOUND_FILE_WEATHER_D:
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
    for (i = 0; i < 7; i++)
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

OSStatus eland_play_rom_sound(_sound_rom_t SOUND)
{
    OSStatus err = kNoErr;
    uint8_t fm_test_cnt = 0;
    AUDIO_STREAM_PALY_S fm_stream;
    mscp_result_t result = MSCP_RST_ERROR;
    mscp_status_t audio_status;
    uint32_t inPos = 0;
    uint8_t oid_volume, i;

    for (i = 0; i < 33; i++)
        audio_service_volume_down(&result, 1);
    if (get_eland_mode() == ELAND_TEST)
        oid_volume = 100;
    else
        oid_volume = get_notification_volume();
    for (i = 0; i < (oid_volume * 32 / 100 + 1); i++)
    {
        audio_service_volume_up(&result, 1);
    }

    audio_service_get_audio_status(&result, &audio_status);
    sound_log("audio_status:%d", audio_status);

    fm_stream.type = AUDIO_STREAM_TYPE_MP3;
    fm_stream.stream_id = audio_service_system_generate_stream_id();
    if (SOUND == ROM_ERROR)
    {
        fm_stream.pdata = (uint8_t *)error_sound;
        fm_stream.total_len = sizeof(error_sound);
    }
    else if (SOUND == ROM_DEFAULT)
    {
        fm_stream.pdata = (uint8_t *)default_sound;
        fm_stream.total_len = sizeof(default_sound);
    }
    else
        goto exit;

start_start:
    if ((fm_stream.total_len - inPos) > SOUND_STREAM_DEFAULT_LENGTH) //len
        fm_stream.stream_len = SOUND_STREAM_DEFAULT_LENGTH;
    else
        fm_stream.stream_len = fm_stream.total_len - inPos;
    if ((++fm_test_cnt) >= 10)
    {
        fm_test_cnt = 0;
        sound_log("type:%d,id:%d,total_len:%d,len:%d,pos:%ld",
                  (int)fm_stream.type, (int)fm_stream.stream_id, (int)fm_stream.total_len, (int)fm_stream.stream_len, inPos);
    }
audio_transfer:
    err = audio_service_stream_play(&result, &fm_stream);
    if (err != kNoErr)
    {
        sound_log("audio_stream_play() error:%d,result:%d", err, result);
        return err;
    }
    else
    {
        require_action_string(get_alarm_stream_state() != STREAM_STOP, exit, err = kGeneralErr, "user set stoped!");
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
    fm_stream.pdata += fm_stream.stream_len;
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
