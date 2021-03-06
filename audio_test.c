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
#include "../alilodemo/inc/http_file_download.h"
#include "mico.h"
#include "netclock_uart.h"
#include "flash_kh25.h"
#include "eland_sound.h"
#include "netclock.h"

//#define CONFIG_AUDIO_DEBUG
#ifdef CONFIG_AUDIO_DEBUG
#define test_log(M, ...) custom_log("audio", M, ##__VA_ARGS__)
#else
#define test_log(...)
#endif /* ! CONFIG_AUDIO_DEBUG */

void player_test_thread(mico_thread_arg_t arg)
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
            hal_url_fileDownload_start(URL_FILE_DNLD);
            test_log("file download status:  ");
            flagAudioPlay = 2;
            break;
        case 2:
            hal_url_fileDownload_pause();
            audio_service_stream_pause(&result);
            test_log("file pause status:  ");
            flagAudioPlay = 3;
            break;
        case 3:
            hal_url_fileDownload_continue();
            test_log("file continue status:  ");
            audio_service_stream_continue(&result);
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
    mscp_result_t result = MSCP_RST_ERROR;
    while (1)
    {
        mico_rtos_get_semaphore(&urlPalyStreamStop_Sem, MICO_WAIT_FOREVER);

        hal_url_fileDownload_stop();
        test_log("file stop status");

        audio_service_stream_stop(&result, audio_service_system_generate_stream_id());
        flagAudioPlay = 1;
    }
}

OSStatus start_test_thread(void)
{
    OSStatus err = kNoErr;

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "URL Thread", url_fileDownload_test_thread, 0x1500, 0);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "url PlayStop Thread", url_paly_stop_thread, 0x1500, 0);
    require_noerr(err, exit);

exit:
    return err;
}
