#include "mico.h"
#include "eland_sound.h"
#define sound_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

static mico_mutex_t eland_sound_mutex = NULL; //flash 調用保護
mico_queue_t eland_sound_R_W_queue = NULL;    //flash sound 讀寫命令队列
mico_queue_t eland_sound_reback_queue = NULL; //flash sound 讀寫完成返回队列

static mico_semaphore_t sound_service_InitCompleteSem = NULL; //sound service 初始化完成信號量

_sound_file_type_t *eland_sound_point = NULL; //音頻鏈接指針句柄
uint32_t sound_sector_start = 0;
uint32_t sound_sector_end = 0;

static void eland_flash_service(mico_thread_arg_t arg);

OSStatus start_eland_flash_service(void)
{
    OSStatus err = kGeneralErr;
    /*初始化互斥信号量*/
    err = mico_rtos_init_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    /*初始化消息隊列 flash sound file write*/
    err = mico_rtos_init_queue(&eland_sound_R_W_queue, "sound_read_write_queue", sizeof(_sound_read_write_type_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    /*初始化消息隊列 flash sound file write call back*/
    err = mico_rtos_init_queue(&eland_sound_reback_queue, "eland_sound_reback_queue", sizeof(_sound_callback_type_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    err = mico_rtos_init_semaphore(&sound_service_InitCompleteSem, 1);
    require_noerr_string(err, exit, "sound_service_InitCompleteSem init err");
    /* create eland init thread */
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "eland_flash_service", eland_flash_service, 0x800, (uint32_t)NULL);
    require_noerr_string(err, exit, "ERROR: Unable to start the eland_init thread");

    err = mico_rtos_get_semaphore(&sound_service_InitCompleteSem, MICO_WAIT_FOREVER);
    require_noerr_string(err, exit, "kh25_InitCompleteSem get err");

    err = mico_rtos_deinit_semaphore(&sound_service_InitCompleteSem);
    require_noerr_string(err, exit, "kh25_InitCompleteSem deinit err");

exit:
    return err;
}

static void eland_flash_service(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    uint32_t sector_count = 0;
    SOUND_FILE_SCAN_STATUS get_soundfileflag = FILE_SCAN_START;

    _sound_file_type_t *alarm_file_cache = NULL;
    _sound_read_write_type_t *alarm_w_r_queue = NULL;
    _sound_callback_type_t *alarm_r_w_callbcke_queue = NULL;

    alarm_file_cache = (_sound_file_type_t *)calloc(sizeof(_sound_file_type_t), sizeof(uint8_t));

    /*start init eland SPI*/
    start_spi_test_service();
    require_noerr(err, exit);

    //整理內容指針
    do
    {
        //sound_log("check sector %ld", sector_count);
        flash_kh25_read((uint8_t *)alarm_file_cache, KH25L8006_SECTOR_SIZE * sector_count, sizeof(_sound_file_type_t));

        if (strncmp(alarm_file_cache->flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN)) //沒尋到標誌
        {
            if (get_soundfileflag == FILE_SCAN_START)
            {
                sector_count++;
                if (sector_count == KH25_FLASH_FILE_COUNT)
                {
                    sound_sector_start = 0;
                    sound_sector_end = 0;
                    sound_log("flash is empty");
                    break;
                }
            }
            if (get_soundfileflag == FIND_A_FILE)
                get_soundfileflag = GET_FILE_SCAN_START;
            if (get_soundfileflag == GET_FILE_SCAN_START)
            {
                sector_count++;
                if (sector_count == KH25_FLASH_FILE_COUNT)
                    sector_count = 0;
            }
            if (get_soundfileflag == GET_FILE_START)
            {
                get_soundfileflag = GET_FILE_END;
                sound_sector_end = sector_count * KH25L8006_SECTOR_SIZE;
                sound_log("flash scan over");
                break;
            }
        }
        else //有文件標誌
        {
            sound_log("%s", alarm_file_cache->alarm_ID);
            switch (get_soundfileflag)
            {
            case FILE_SCAN_START:
                get_soundfileflag = FIND_A_FILE;
                break;
            case GET_FILE_SCAN_START:
                get_soundfileflag = GET_FILE_START;
                sound_sector_start = sector_count * KH25L8006_SECTOR_SIZE;
                eland_sound_point = (_sound_file_type_t *)calloc(1, sizeof(_sound_file_type_t));
                memcpy((uint8_t *)eland_sound_point + sizeof(*eland_sound_point) - sizeof(_sound_file_type_t), alarm_file_cache, sizeof(_sound_file_type_t));
                break;
            case GET_FILE_START:
                eland_sound_point = (_sound_file_type_t *)realloc(eland_sound_point, (sizeof(*eland_sound_point) + sizeof(_sound_file_type_t)));
                memcpy((uint8_t *)eland_sound_point + sizeof(*eland_sound_point) - sizeof(_sound_file_type_t), alarm_file_cache, sizeof(_sound_file_type_t));
                break;
            default:
                break;
            }
            sector_count += (alarm_file_cache->file_len + sizeof(_sound_file_type_t)) / KH25L8006_SECTOR_SIZE;
            sector_count += (((alarm_file_cache->file_len + sizeof(_sound_file_type_t)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;

            if (sector_count >= KH25_FLASH_FILE_COUNT)
                sector_count = sector_count - KH25_FLASH_FILE_COUNT;
        }
        memset(alarm_file_cache, 0, sizeof(_sound_file_type_t));
    } while (sector_count < KH25_FLASH_FILE_COUNT);

    if (alarm_file_cache != NULL)
    {
        free(alarm_file_cache);
        alarm_file_cache = NULL;
    }
    mico_rtos_set_semaphore(&sound_service_InitCompleteSem); //wait until get semaphore
    while (1)
    {
    waitfor_w_r_queue:
        err = mico_rtos_pop_from_queue(&eland_sound_R_W_queue, &alarm_w_r_queue, MICO_WAIT_FOREVER);
        require_noerr(err, exit);
        alarm_r_w_callbcke_queue = (_sound_callback_type_t *)calloc(sizeof(_sound_callback_type_t), sizeof(uint8_t)); //接收時候記得free

        if (alarm_w_r_queue->is_read == true) //讀數據
        {
            if (alarm_w_r_queue->pos == 0) //文件數據流首次讀取
            {
                alarm_file_cache = eland_sound_point;
                do
                {
                    if (strncmp(alarm_file_cache->alarm_ID, alarm_w_r_queue->alarm_ID, ALARM_ID_LEN) == 0) //找到文件
                    {
                        sound_log("alarm_ID:%s", alarm_file_cache->alarm_ID);
                        alarm_w_r_queue->total_len = alarm_file_cache->file_len;
                        alarm_w_r_queue->file_address = alarm_file_cache->file_address;
                        sound_log("total_len = %ld,sound_flash_pos = %ld", alarm_w_r_queue->total_len, alarm_w_r_queue->pos);
                        goto start_read_sound;
                    }
                    else
                    {
                        alarm_file_cache++;
                        sound_log("no_file");
                    }
                } while ((uint8_t *)alarm_file_cache < (uint8_t *)eland_sound_point + sizeof(*eland_sound_point));
                memcpy(alarm_r_w_callbcke_queue->alarm_ID, alarm_w_r_queue->alarm_ID, ALARM_ID_LEN);
                alarm_r_w_callbcke_queue->read_write_err = FILE_NOT_FIND;
                err = mico_rtos_push_to_queue(&eland_sound_reback_queue, alarm_r_w_callbcke_queue, 10);
                sound_log("no_file");
                //require_noerr_action(err, exit, sound_log("[error]mico_rtos_push_to_queue err"));
                goto waitfor_w_r_queue;
            }
        start_read_sound:
            if ((alarm_w_r_queue->pos + SOUND_STREAM_DEFAULT_LENGTH) < alarm_w_r_queue->total_len)
                alarm_w_r_queue->len = SOUND_STREAM_DEFAULT_LENGTH;
            else
                alarm_w_r_queue->len = alarm_w_r_queue->total_len - alarm_w_r_queue->pos;

            flash_kh25_read((uint8_t *)alarm_w_r_queue->sound_data, alarm_w_r_queue->file_address + alarm_w_r_queue->pos, alarm_w_r_queue->len);
            memcpy(alarm_r_w_callbcke_queue->alarm_ID, alarm_w_r_queue->alarm_ID, ALARM_ID_LEN);
            alarm_r_w_callbcke_queue->read_write_err = ERRNONE;
            err = mico_rtos_push_to_queue(&eland_sound_reback_queue, &alarm_r_w_callbcke_queue, 10);
            sound_log("inlen = %ld,sound_flash_pos = %ld", alarm_w_r_queue->len, alarm_w_r_queue->pos);
            //require_noerr_action(err, exit, sound_log("[error]mico_rtos_push_to_queue err"));
        }
        else //寫數據
        {
            sound_log("sound write");
            if (alarm_w_r_queue->pos == 0) //文件數據流首次寫入
            {
                if (eland_sound_point == NULL)
                    eland_sound_point = (_sound_file_type_t *)calloc(1, sizeof(_sound_file_type_t));
                else
                    eland_sound_point = (_sound_file_type_t *)realloc(eland_sound_point, (sizeof(*eland_sound_point) + sizeof(_sound_file_type_t)));

                alarm_file_cache = (_sound_file_type_t *)calloc(1, sizeof(_sound_file_type_t));
                sprintf((char *)(alarm_file_cache->flag), "%s", ALARM_FILE_FLAG_STRING);
                sprintf((char *)(alarm_file_cache->alarm_ID), "%s", alarm_w_r_queue->alarm_ID);

                alarm_file_cache->file_len = alarm_w_r_queue->total_len;
                alarm_file_cache->file_address = sound_sector_end + sizeof(_sound_file_type_t);
                memcpy((uint8_t *)((uint8_t *)eland_sound_point + sizeof(*eland_sound_point) - sizeof(_sound_file_type_t)), (uint8_t *)alarm_file_cache, sizeof(_sound_file_type_t));

                sound_log("write sound file info address:%ld", sound_sector_end);
                flash_kh25_write_page((uint8_t *)alarm_file_cache, sound_sector_end, sizeof(_sound_file_type_t)); //寫入文件信息

                if (alarm_file_cache != NULL)
                {
                    free(alarm_file_cache);
                    alarm_file_cache = NULL;
                }
            }
            sound_log("write sound file data address :%ld", (sound_sector_end + sizeof(_sound_file_type_t) + alarm_w_r_queue->pos));
            flash_kh25_write_page((uint8_t *)alarm_w_r_queue->sound_data, (sound_sector_end + sizeof(_sound_file_type_t) + alarm_w_r_queue->pos), alarm_w_r_queue->len); //寫入數據
            //memset(alarm_w_r_queue->sound_data, 0, alarm_w_r_queue->len);
            if ((alarm_w_r_queue->pos + alarm_w_r_queue->len) == alarm_w_r_queue->total_len)
            {
                sector_count = (alarm_w_r_queue->total_len + sound_sector_end + sizeof(_sound_file_type_t)) / KH25L8006_SECTOR_SIZE;
                sector_count += (((alarm_w_r_queue->total_len + sound_sector_end + sizeof(_sound_file_type_t)) % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;

                sound_sector_end = KH25L8006_SECTOR_SIZE * sector_count;
                sound_log("sound_sector_end:%ld", sound_sector_end);
            }
            memcpy(alarm_r_w_callbcke_queue->alarm_ID, alarm_w_r_queue->alarm_ID, ALARM_ID_LEN);
            alarm_r_w_callbcke_queue->read_write_err = ERRNONE;
            err = mico_rtos_push_to_queue(&eland_sound_reback_queue, &alarm_r_w_callbcke_queue, 10);
            check_string((err == kNoErr), "callbace err");
            //require_noerr_action(err, exit, sound_log("[error]mico_rtos_push_to_queue err"));
        }
    }

exit:
    mico_rtos_delete_thread(NULL);
}
