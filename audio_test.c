/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */

#include "../alilodemo/audio_test.h"
#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/inc/http_file_download.h"
#include "mico.h"
#include "netclock_uart.h"
#include "flash_kh25.h"
#include "eland_sound.h"
#include "netclock.h"

#define test_log(format, ...) custom_log("ASR", format, ##__VA_ARGS__)

mico_semaphore_t flash_play_Sem = NULL;

static void player_flash_thread(mico_thread_arg_t arg)
{
    AUDIO_STREAM_PALY_S flash_read_stream;
    OSStatus err = kGeneralErr;
    mscp_result_t result = MSCP_RST_ERROR;
    audio_play_id = audio_service_system_generate_stream_id();
    uint32_t data_pos = 0;
    uint8_t *flashdata = NULL;
    // _sound_read_write_type_t *alarm_w_r_queue = NULL;
    // _sound_callback_type_t *alarm_r_w_callbcke_queue = NULL;

    ADUIO_SYSTEM_STATE_S Aduio_state;
    memset(&Aduio_state, 0, sizeof(ADUIO_SYSTEM_STATE_S));

    test_log("player_flash_thread");
    flashdata = malloc(2001);
    audio_play_id = audio_service_system_generate_stream_id();
    flash_read_stream.type = AUDIO_STREAM_TYPE_MP3;
    flash_read_stream.pdata = flashdata;
    flash_read_stream.stream_id = audio_play_id;
    flash_read_stream.total_len = 264717;

// alarm_w_r_queue = (_sound_read_write_type_t *)calloc(sizeof(_sound_read_write_type_t), sizeof(uint8_t));
// memset(alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
// memcpy(alarm_w_r_queue->alarm_ID, "taichi_16_064kbps", strlen("taichi_16_064kbps"));
// alarm_w_r_queue->is_read = true;
// alarm_w_r_queue->sound_data = flashdata;
wait_sem:
    test_log("wait_sem");
    mico_rtos_get_semaphore(&flash_play_Sem, MICO_WAIT_FOREVER);
    data_pos = 0;
falsh_read_start:

    //alarm_w_r_queue->pos = data_pos;
    //test_log("inlen = %ld,sound_flash_pos = %ld", alarm_w_r_queue->len, alarm_w_r_queue->pos);

    // err = mico_rtos_push_to_queue(&eland_sound_R_W_queue, &alarm_w_r_queue, 10);
    // require_noerr(err, exit);
    // err = mico_rtos_pop_from_queue(&eland_sound_reback_queue, &alarm_r_w_callbcke_queue, MICO_WAIT_FOREVER);
    // require_noerr(err, exit);
    /*****************************************************/
    if ((flash_read_stream.total_len - data_pos) > 1500)
    {
        flash_kh25_read(flashdata, data_pos + 0x001000, 1500);
        flash_read_stream.stream_len = 1500;
    }
    else
    {
        flash_kh25_read(flashdata, data_pos + 0x001000, flash_read_stream.total_len - data_pos);
        flash_read_stream.stream_len = (flash_read_stream.total_len - data_pos);
    }

    /*****************************************************/

    // if (alarm_r_w_callbcke_queue->read_write_err == ERRNONE)
    // {
    //     test_log("ERRNONE");
    //     if (data_pos == 0)
    //     {
    //         flash_read_stream.total_len = alarm_w_r_queue->total_len;
    //     }
    //     flash_read_stream.stream_len = alarm_w_r_queue->len;
    // }
    // else if (alarm_r_w_callbcke_queue->read_write_err == FILE_NOT_FIND)
    // {
    //     test_log("FILE_NOT_FIND");
    //     goto exit;
    // }
    // else
    // {
    //     test_log("ELSE %d", (uint8_t)alarm_r_w_callbcke_queue->read_write_err);
    // }
    test_log("type[%d],stream_id[%d],total_len[%d],stream_len[%d] data_pos[%ld]",
             (int)flash_read_stream.type, (int)flash_read_stream.stream_id,
             (int)flash_read_stream.total_len, (int)flash_read_stream.stream_len, data_pos);
    // test_log("free_callback_queue");
    // free(alarm_r_w_callbcke_queue);
    data_pos += flash_read_stream.stream_len;

audio_transfer:
    err = audio_service_stream_play(&result, &flash_read_stream);

    if (err != kNoErr)
    {
        test_log("audio_stream_play() error!!!!");
        goto exit;
    }
    else
    {
        if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
        {
            test_log("new slave set pause!!!");
            mico_rtos_thread_msleep(1000); //time set 1000ms!!!
            goto audio_transfer;
        }
        else
        {
            err = kNoErr;
        }
    }

    if (data_pos < flash_read_stream.total_len)
    {
        goto falsh_read_start;
        test_log("state is HTTP_FILE_DOWNLOAD_STATE_SUCCESS !");
    }

exit:
    mico_rtos_get_semaphore(&flash_play_Sem, 10);
    goto wait_sem;

    // free(flashdata);
    // free(alarm_w_r_queue);

    mico_rtos_delete_thread(NULL);
}

static void player_test_thread(mico_thread_arg_t arg)
{
    char *buf = malloc(2000);
    char *buf_head = buf;
    int buf_len = 0;
    mscp_result_t result = MSCP_RST_ERROR;
    mscp_status_t audio_status = MSCP_STATUS_ERR;
    AUDIO_MIC_RECORD_START_S mic_record;
    OSStatus err = kNoErr;

    test_log("player_test_thread created.");

    while (1)
    {
        mico_rtos_get_semaphore(&recordKeyPress_Sem, MICO_WAIT_FOREVER);

        buf = buf_head;
        memset(buf, 0, 2000);

        mic_record_id = audio_service_system_generate_record_id();
        mic_record.record_id = mic_record_id;
        mic_record.format = AUDIO_MIC_RESULT_FORMAT_AMR;
        mic_record.type = AUDIO_MIC_RESULT_TYPE_STREAM;

        test_log("get audio start.\n");

        err = audio_service_get_audio_status(&result, &audio_status);
        test_log("audio_service_get_audio_status >>> err: %d, result:%d, audio_status:%d", err, result, audio_status);
        if (err != kNoErr || result != MSCP_RST_SUCCESS || audio_status != MSCP_STATUS_IDLE)
        {
            if (audio_status != MSCP_STATUS_STREAM_PLAYING)
                continue;
            err = audio_service_stream_stop(&result, audio_play_id);
            test_log("audio_service_stream_stop >>> err: %d, result:%d", err, result);
            if (err != kNoErr || result != MSCP_RST_SUCCESS)
                continue;
            else
            {
                err = audio_service_get_audio_status(&result, &audio_status);
                test_log("audio_service_get_audio_status >>> err: %d, result:%d, audio_status:%d", err, result, audio_status);
                if (err != kNoErr || result != MSCP_RST_SUCCESS || audio_status != MSCP_STATUS_IDLE)
                    continue;
            }
        }

        err = audio_service_mic_record_start(&result, &mic_record);
        test_log("audio_service_mic_record_start >>> err:%d, result:%d", err, result);
        if (err != kNoErr || result != MSCP_RST_SUCCESS)
        {
            test_log("audio_service_mic_record_start >>> ERROR");
            audio_service_stream_stop(&result, audio_play_id);
            mico_thread_msleep(5);
            continue;
        }

        flag_mic_start = 1;

        while (recordKeyStatus == KEY_PRESS)
        {
            buf_len = hal_getVoiceData((uint8_t *)buf, 1024);

            if (buf_len > 0)
            {
                test_log("get audio from mic ,len: %d", buf_len);
                buf += buf_len;
                test_log("audio total len: %d", buf - buf_head);
                if (buf - buf_head >= 1500)
                {
                    test_log("audio record buf is full!!!");
                    recordKeyStatus = KEY_RELEASE;
                    break;
                }
            }
            mico_rtos_thread_msleep(100);
        }
        test_log("get audio stop!");
        err = hal_player_start(buf_head, buf - buf_head, 2000);
        test_log("play result: %d", err);
    }
}

static void url_fileDownload_test_thread(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    mscp_result_t result = MSCP_RST_ERROR;
    test_log("url_fileDownloadtest_thread created.");
    mico_rtos_thread_sleep(1);
    flagAudioPlay = 1;
    while (1)
    {
        mico_rtos_get_semaphore(&urlFileDownload_Sem, MICO_WAIT_FOREVER);
        switch (flagAudioPlay)
        {
        case 1:
            err = hal_url_fileDownload_start(URL_FILE_DNLD);
            test_log("file download status: err = %d", err);
            SendElandQueue(Queue_ElandState_type, ElandAliloPlay);
            flagAudioPlay = 2;
            break;
        case 2:
            err = hal_url_fileDownload_pause();
            audio_service_stream_pause(&result);
            test_log("file pause status: err = %d", err);
            SendElandQueue(Queue_ElandState_type, ElandAliloPause);
            flagAudioPlay = 3;
            break;
        case 3:
            err = hal_url_fileDownload_continue();
            test_log("file continue status: err = %d", err);
            audio_service_stream_continue(&result);
            SendElandQueue(Queue_ElandState_type, ElandAliloPlay);
            flagAudioPlay = 2;
            break;
        default:
            flagAudioPlay = 1;
            break;
        }
    }
}

static void url_paly_stop_thread(mico_thread_arg_t arg)
{
    OSStatus err = kNoErr;
    mscp_result_t result = MSCP_RST_ERROR;
    while (1)
    {
        mico_rtos_get_semaphore(&urlPalyStreamStop_Sem, MICO_WAIT_FOREVER);

        err = hal_url_fileDownload_stop();
        test_log("file stop status: err = %d", err);

        audio_service_stream_stop(&result, audio_service_system_generate_stream_id());
        flagAudioPlay = 1;
        SendElandQueue(Queue_ElandState_type, ElandAliloStop);
    }
}

OSStatus start_test_thread(void)
{
    OSStatus err = kNoErr;
    err = mico_rtos_init_semaphore(&flash_play_Sem, 1);
    require_noerr(err, exit);
    //mico_rtos_set_semaphore(&flash_play_Sem);
    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "ASR Thread", player_flash_thread, 0x900, 0);
    require_noerr(err, exit);

// err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "URL Thread", url_fileDownload_test_thread, 0x1500, 0);
// require_noerr(err, exit);

// err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "url PlayStop Thread", url_paly_stop_thread, 0x1500, 0);
// require_noerr(err, exit);

exit:
    return err;
}
