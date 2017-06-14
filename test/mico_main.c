#include "mico.h"
#include "mico_app_define.h"
#include "audio_service.h"
#include "hal_alilo_rabbit.h"

#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

static mico_semaphore_t wait_sem = NULL;

static void micoNotify_WifiStatusHandler( WiFiEvent status, void* const inContext )
{
    switch ( status )
    {
        case NOTIFY_STATION_UP:
            mico_rtos_set_semaphore( &wait_sem );
            break;
        case NOTIFY_STATION_DOWN:
            case NOTIFY_AP_UP:
            case NOTIFY_AP_DOWN:
            break;
    }
}

void ssl_log( const int logLevel, const char * const logMessage )
{
    app_log("%s\r\n", logMessage);
}

int application_start( void )
{
    app_log_trace();
    OSStatus err = kNoErr;
    mico_Context_t* mico_context;
    app_context_t* app_context;
    char version[30];

    mscp_result_t result = MSCP_RST_ERROR;
    AUDIO_MIC_RECORD_START_S mic_record;

    /* Create application context */
    app_context = (app_context_t *) calloc( 1, sizeof(app_context_t) );
    require_action( app_context, exit, err = kNoMemoryErr );

    mico_rtos_init_semaphore( &wait_sem, 1 );

    /*Register user function for MiCO nitification: WiFi status changed */
    err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                       (void *) micoNotify_WifiStatusHandler,
                                       NULL );
    require_noerr( err, exit );

    /* Create mico system context and read application's config data from flash */
    mico_context = mico_system_context_init( sizeof(application_config_t) );
    app_context->appConfig = mico_system_context_get_user_data( mico_context );

    /* mico system initialize */
    err = mico_system_init( mico_context );
    require_noerr( err, exit );

    /* Wait for wlan connection*/
    mico_rtos_get_semaphore( &wait_sem, MICO_WAIT_FOREVER );
    app_log("wifi connected successful");

//  ssl_set_loggingcb(ssl_log);

    err = hal_alilo_rabbit_init();
    require_noerr( err, exit );


    mic_record.record_id = audio_service_system_generate_record_id();
    mic_record.format = AUDIO_MIC_RESULT_FORMAT_SPEEX;
    mic_record.type = AUDIO_MIC_RESULT_TYPE_STREAM;
    while(1)
    {
        mico_rtos_get_semaphore(&recordKeyPress_Sem, MICO_WAIT_FOREVER);
        err = audio_service_mic_record_start(&result, &mic_record);
        app_log("audio_service_mic_record_start >>> err:%d, result:%d", err, result);
        if(err != kNoErr && result != MSCP_RST_SUCCESS)
        {
            app_log("audio_service_mic_record_start >>> ERROR");
            audio_service_mic_record_stop(&result, ai_mic_record_id);
            continue;
        }
    }

    exit:
    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED,
                               (void *) micoNotify_WifiStatusHandler );
    mico_rtos_deinit_semaphore( &wait_sem );
    mico_rtos_delete_thread( NULL );
    return err;
}

