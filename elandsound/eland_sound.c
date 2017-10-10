#include "mico.h"
#include "eland_sound.h"
#define eland_sound_log(M, ...) custom_log("Eland", M, ##__VA_ARGS__)

static mico_mutex_t eland_sound_mutex = NULL; //flash 調用保護
mico_queue_t eland_sound_R_W_queue = NULL;    //flash sound write队列

_sound_file_type_t *eland_sound_point = NULL; //音頻鏈接指針句柄

OSStatus start_eland_flash_service(void)
{
    OSStatus err = kGeneralErr;
    /*初始化互斥信号量*/
    err = mico_rtos_init_mutex(&eland_sound_mutex);
    require_noerr(err, exit);
    /*初始化消息隊列 flash sound file write*/
    err = mico_rtos_init_queue(&eland_sound_R_W_queue, "sound_read_write_queue", sizeof(_sound_read_write_type_t *), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    /* create eland init thread */
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "eland_flash_service", eland_flash_service, 0x800, (uint32_t)NULL);
    require_noerr_string(err, exit, "ERROR: Unable to start the eland_init thread");

exit:
    return err;
}

static void eland_flash_service(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    uint32_t block_count = 1;
    _sound_file_type_t *alarm_file_cache;
    _sound_read_write_type_t *alarm_w_r_queue = NULL;
    alarm_file_cache = (_sound_file_type_t *)calloc(sizeof(_sound_file_type_t), sizeof(uint8_t));
    //memset(&alarm_file_cache, 0, sizeof(alarm_file_cache));
    /*start init eland SPI*/
    start_spi_test_service();
    require_noerr(err, exit);

    //整理內容指針
    do
    {
        flash_kh25_read((uint8_t *)&alarm_file_cache, KH25L8006_SECTOR_SIZE * block_count, sizeof(_sound_file_type_t));

        if (strncmp(alarm_file_cache.flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN)) //沒尋到標誌
            block_count++;
        else //有文件標誌
        {
            if (eland_sound_point == NULL)
                eland_sound_point = (_sound_file_type_t *)calloc(sizeof(_sound_file_type_t), sizeof(uint8_t));
            else
                eland_sound_point = (_sound_file_type_t *)realloc(eland_sound_point, (sizeof(eland_sound_point) + sizeof(_sound_file_type_t)));
            alarm_file_cache->file_address = KH25L8006_SECTOR_SIZE * block_count + sizeof(_sound_file_type_t);

            memcpy((uint8_t *)eland_sound_point + sizeof(eland_sound_point) - sizeof(_sound_file_type_t), alarm_file_cache, sizeof(_sound_file_type_t));

            block_count += alarm_file_cache.file_len / KH25L8006_SECTOR_SIZE;
            block_count += ((alarm_file_cache.file_len % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;
        }
        memset(alarm_file_cache, 0, sizeof(_sound_file_type_t));
    } while (block_count < KH25_BLOCKCOUNT);

    if (alarm_file_cache != NULL)
    {
        free(alarm_file_cache);
        alarm_file_cache = NULL;
    }

    while (1)
    {
        err = mico_rtos_pop_from_queue(&eland_sound_R_W_queue, &alarm_w_r_queue, MICO_WAIT_FOREVER);
        require_noerr(err, exit);

        if (alarm_w_r_queue->is_read == true)
        {
            alarm_file_cache = eland_sound_point;
            if (strncmp(alarm_file_cache, alarm_w_r_queue->alarm_ID, ALARM_ID_LEN) == 0)
            {
            }
        }
        else //寫數據
        {
            do
            {
                if (alarm_w_r_queue->pos = 0)
                {
                    if (eland_sound_point == NULL)
                        eland_sound_point = (_sound_file_type_t *)calloc(sizeof(_sound_file_type_t), sizeof(uint8_t));
                    else
                        eland_sound_point = (_sound_file_type_t *)realloc(eland_sound_point, (sizeof(eland_sound_point) + sizeof(_sound_file_type_t)));

                    alarm_file_cache = (_sound_file_type_t *)((uint8_t *)eland_sound_point + sizeof(eland_sound_point) - sizeof(_sound_file_type_t));

                    memcpy(alarm_file_cache->flag, ALARM_FILE_FLAG_STRING, ALARM_FILE_FLAG_LEN);
                    memcpy(alarm_file_cache->alarm_ID, alarm_w_r_queue->alarm_ID, sizeof(ALARM_ID_LEN));

                    alarm_file_cache->file_len = alarm_w_r_queue->len;
                }

                alarm_file_cache->file_address = KH25L8006_SECTOR_SIZE * block_count + sizeof(_sound_file_type_t);

                memcpy(eland_sound_point + sizeof(eland_sound_point) - sizeof(_sound_file_type_t), alarm_file_cache, sizeof(_sound_file_type_t));

                block_count += alarm_file_cache.file_len / KH25L8006_SECTOR_SIZE;
                block_count += ((alarm_file_cache.file_len % KH25L8006_SECTOR_SIZE) == 0) ? 0 : 1;

            } while (1);
        }
    }

exit:
    mico_rtos_delete_thread(NULL);
}
