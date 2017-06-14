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
#include "mico_app_define.h"
#include "alink_device.h"
#include "alink_config.h"
#include "hal_alilo_rabbit.h"

#define alink_main_log(format, ...)  custom_log("alink_main", format, ##__VA_ARGS__)

static void* alink_post_data_sem = NULL;
static int device_logged = 0;
static void *alink_device_state_mutex;
static int device_status_change = 1;

void alink_device_reset( void )
{
    if ( device_logged )
        alink_factory_reset( );
}

int get_device_state( )
{
    int ret = 0;
    platform_mutex_lock( alink_device_state_mutex );
    ret = device_status_change;
    device_status_change = 0;
    platform_mutex_unlock( alink_device_state_mutex );
    return ret;
}

int set_device_state( int state )
{
    platform_mutex_lock( alink_device_state_mutex );
    device_status_change = state;
    platform_mutex_unlock( alink_device_state_mutex );
    if ( state == 1 )
    {
        platform_semaphore_post( alink_post_data_sem );
        alink_main_log("set_device_state %d", state);
    }
    return state;
}

void cloud_connected( void )
{
    alink_main_log("alink cloud connected!\n");
    device_logged = 1;
}

void cloud_disconnected( void )
{
    alink_main_log("alink cloud disconnected!\n");
    device_logged = 0;
}

//shiner add start
#define SAMPLE_AUDIO_PCM_FILE_NAME	"sample.pcm"
#define SAMPLE_AUDIO_OPUS_FILE_NAME	"sample.opus"
#define SAMPLE_AUDIO_OGG_FILE_NAME	"sample.ogg"

typedef enum {
	AUDIO_FORMAT_PCM = 0,
	AUDIO_FORMAT_OPUS = 1,
	AUDIO_FORMAT_OGG = 2,
	AUDIO_FORMAT_DEFAULT = 0xff
}audio_format_t;


extern int asr_init_server(char *ip, int port, int is_ssl_enable);
static void alink_main( uint32_t arg )
{
    hal_alilo_rabbit_init();
    alink_set_loglevel( ALINK_LL_TRACE );
    lws_set_log_level(0x01FF, NULL);
#ifdef __ALI_ONLINE__
    alink_enable_daily_mode("online/alink.tcp.aliyun.com", 443);//alink.tcp.aliyun.com:443  //online/alink.tcp.aliyun.com
	asr_init_server("mlabs.taobao.com", 443, 1);
#else
	alink_enable_daily_mode(NULL, 0);//daily/wsf.smart.aliyun-inc.com:9999
	asr_init_server("10.218.129.125", 80, 0);
#endif

    alink_device_state_mutex = platform_mutex_init( );
    alink_post_data_sem = platform_semaphore_init( );

    device_logged = 0;

    alink_register_callback( ALINK_CLOUD_CONNECTED, &cloud_connected );
    alink_register_callback( ALINK_CLOUD_DISCONNECTED, &cloud_disconnected );

    /*
     * NOTE: register ALINK_GET/SET_DEVICE_STATUS or ALINK_GET/SET_DEVICE_RAWDATA
     */
#ifdef PASS_THROUGH
    /*
     * TODO: before using callback ALINK_GET/SET_DEVICE_RAWDATA,
     * submit product_model_xxx.lua script to ALINK cloud.
     * ALINKTEST_LIVING_LIGHT_SMARTLED_LUA is done with it.
     */
    alink_register_callback( ALINK_GET_DEVICE_RAWDATA, &cloud_get_device_raw_data );
    alink_register_callback( ALINK_SET_DEVICE_RAWDATA, &cloud_set_device_raw_data );
#else
    alink_register_callback(ALINK_GET_DEVICE_STATUS, &cloud_get_device_json_data);
    alink_register_callback(ALINK_SET_DEVICE_STATUS, &cloud_set_device_json_data);
#endif

    alink_register_callback(ALINK_ASR_GET_RESULT,callback_alink_asr_get_result);

    alink_start( );

    alink_wait_connect( ALINK_WAIT_FOREVER );
    alink_main_log("alink connect succeed. \n");

    start_asr_thread( );

    while ( 1 )
    {
        if ( get_device_state( ) )
        {
            alink_main_log("device status changed, free memory %d", MicoGetMemoryInfo()->free_memory);
#ifdef PASS_THROUGH
            alink_device_post_raw_data( );
#else
            alink_device_post_json_data();
#endif
        }
        platform_semaphore_wait( alink_post_data_sem, 500 );
    }

    alink_main_log("alink exit\n");
    alink_end( );

    platform_mutex_destroy( alink_device_state_mutex );
    platform_semaphore_destroy( alink_post_data_sem );
    mico_rtos_delete_thread( NULL );
}

OSStatus start_alink_emb( void )
{
    return mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "alink main", alink_main,
                                    0x2000,
                                    0 );
}

