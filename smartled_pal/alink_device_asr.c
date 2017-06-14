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

#include "mico.h"
#include "alink_device.h"
#include "hal_alilo_rabbit.h"
#include "http_file_download.h"
#include "hal_alilo_rabbit.h"

#define asr_log(format, ...)  custom_log("ASR", format, ##__VA_ARGS__)
//#define test_log(format, ...)

static FILE_DOWNLOAD_CONTEXT g_file_download_context_user = NULL;
static PLAYER_OPTION_S stream_play_opt;
static uint8_t ai_stream_play_id = 0;

static void file_download_state_cb(void *context, HTTP_FILE_DOWNLOAD_STATE_E state, uint32_t progress, uint32_t user_args)
{
    //    FILE_DOWNLOAD_CONTEXT_S *file_download_context = (FILE_DOWNLOAD_CONTEXT_S *)context;

    PLAYER_OPTION_S *s_player_option_p = (PLAYER_OPTION_S *)user_args;

    if (state == HTTP_FILE_DOWNLOAD_STATE_LOADING)
    {
        asr_log("loading.... %ld %%, user_args = %ld", progress, user_args);
    }
    else
    {
        //        stream_play_log("state = %s, user_args = %ld", user_file_download_state_string[state], user_args);
    }

    s_player_option_p->file_download_status_e = state;

    if (state == HTTP_FILE_DOWNLOAD_STATE_SUCCESS)
    {
        s_player_option_p->file_download_status_e = HTTP_FILE_DOWNLOAD_STATE_SUCCESS;
        asr_log("state is HTTP_FILE_DOWNLOAD_STATE_SUCCESS!");
    }
    else if (state == HTTP_FILE_DOWNLOAD_STATE_FAILED)
    {
        s_player_option_p->file_download_status_e = HTTP_FILE_DOWNLOAD_STATE_FAILED;
        asr_log("state is HTTP_FILE_DOWNLOAD_STATE_FAILED!!!");
    }

    return;
}

void deal_with_url_result( char *url )
{
//    char filename[] = "1.mp3";
//
//    media_play_set_source_decoder_type( MEDIA_SOURCE_NET, filename );
//    http_trans_set_url( HTTP_TYPE_GET, url );
//    media_play_cmd_start( );

    stream_play_opt.type = AUDIO_STREAM_TYPE_MP3;
    ai_stream_play_id = audio_service_system_generate_stream_id();
    stream_play_opt.stream_player_id = ai_stream_play_id;

    http_file_download_start((FILE_DOWNLOAD_CONTEXT *)(&g_file_download_context_user),
                             (const char*)url,
                             file_download_state_cb,
                             NULL,
                             (uint32_t)&stream_play_opt);
}

void callback_alink_asr_get_result( char *params, int len )
{
    char *nlp_ret = NULL, *server_data = NULL, *SwitchAudio = NULL, *data = NULL,*ttsUrl = NULL, *playUrl = NULL, *serviceDataMap = NULL, *tts_url=NULL, *tts_data=NULL;
    char *temp = NULL, *result=NULL;
    int ret_len = 0;
    char url[256] = { 0 };
    bool is_play = false;

    asr_log("memshow %d", MicoGetMemoryInfo( )->free_memory);

    if ( params != NULL )
    {
        asr_log("\r\n");
        asr_log("get resut: %s , len : %d\n",params,len);

        temp = strstr(params, "data");
        if( temp != NULL ){
            temp = temp + strlen("data") +2;
            asr_log("%s", temp);
            if ( (result = json_get_value_by_name( (char *) temp, strlen(temp), "result", &ret_len,NULL ))!= NULL )
            {
                if ( (tts_data = json_get_value_by_name( (char *) result, strlen(result), "data", &ret_len,NULL ))!= NULL )
                {
                    if ( (nlp_ret = json_get_value_by_name( (char *) tts_data, strlen(tts_data), "data", &ret_len,NULL ))!= NULL )
                    {
                        strncpy( url, nlp_ret, ret_len );
                        is_play = true;
                        goto PLAY;
                    }
                }
            }
        }

        if ( (nlp_ret = json_get_value_by_name( (char *) params, len, "nlp_ret", &ret_len,NULL ))!= NULL )
        {
            if ( (ttsUrl = json_get_value_by_name( (char *) nlp_ret + 1, strlen( params ),"ttsUrl",&ret_len, NULL ))!= NULL )
            {
                strncpy( url, ttsUrl, ret_len );
                is_play = true;
                goto PLAY;
            }
        }

        if ( (server_data = json_get_value_by_name( (char *) params, len, "service_data",&ret_len,NULL ))!= NULL )
        {
            if ( (serviceDataMap = json_get_value_by_name( (char *) server_data, strlen(server_data),"serviceDataMap",&ret_len,NULL ))!= NULL )
            {
                if ( (SwitchAudio = json_get_value_by_name( (char *) serviceDataMap,strlen( serviceDataMap ),"SwitchAudio", &ret_len,NULL ))!= NULL )
                {
                    if ( (data = json_get_value_by_name( (char *) SwitchAudio,strlen( SwitchAudio ),"data", &ret_len,NULL ))!= NULL )
                    {
                        if ( (playUrl = json_get_value_by_name( (char *) data, strlen( data ),"playUrl",&ret_len, NULL ))!= NULL )
                        {
                            strncpy( url, playUrl, ret_len );
                            is_play = true;
                        }
                    }
                }
            }

            if ( (ttsUrl = json_get_value_by_name( (char *) server_data, strlen(server_data),"ttsUrl",&ret_len,NULL ))!= NULL )
            {
                if ( (tts_url = json_get_value_by_name( (char *) ttsUrl+1, strlen(ttsUrl),"tts_url",&ret_len,NULL ))!= NULL )
                {
                    strncpy( url, tts_url, ret_len );
                    is_play = true;
                }
            }
        }

        PLAY:
        if ( is_play == true )
        {
            asr_log("play url: %s", url);
            deal_with_url_result( url );
        }
    }
}

void asr_thread( mico_thread_arg_t arg )
{
    char buf[1024];
    int buf_len = 0;
    mscp_result_t result = MSCP_RST_ERROR;
    AUDIO_MIC_RECORD_START_S mic_record;

    asr_log("ASR Test Thread created. \n");

    mico_rtos_thread_sleep(2);
    alink_asr_send_buf("你好我是苏泊尔电压力锅，请问有什么需要为您服务么",strlen("你好我是苏泊尔电压力锅，请问有什么需要为您服务么"), ASR_MSG_TTS_REQUEST);

    while ( 1 )
    {
        mico_rtos_get_semaphore(&recordKeyPress_Sem, MICO_WAIT_FOREVER);
        asr_log("get audio start.\n");

        ai_mic_record_id = audio_service_system_generate_record_id();
        mic_record.record_id = ai_mic_record_id;
        mic_record.format = AUDIO_MIC_RESULT_FORMAT_SPEEX;
        mic_record.type = AUDIO_MIC_RESULT_TYPE_STREAM;
        audio_service_mic_record_start(&result, &mic_record);
        while ( recordKeyStatus == KEY_PRESS )
        {
            memset( buf, 0, sizeof(buf) );
            buf_len = hal_getVoiceData( (uint8_t*)buf, sizeof(buf) );

            if ( buf_len > 0 )
            {
                asr_log("get audio from mic ,len: %d\n",buf_len);
                alink_asr_send_buf( buf, buf_len, ASR_MSG_AUDIO_DATA );
            }
            mico_rtos_thread_msleep( 100 );
        }
        asr_log("get audio stop\n");
        alink_asr_send_buf( NULL, 0, ASR_MSG_AUDIO_DATA );
    }
}

int start_asr_thread( void )
{
    /* Create a asr test thread */
    return mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "ASR Thread", asr_thread,
                                    0x900,
                                    0 );
}

