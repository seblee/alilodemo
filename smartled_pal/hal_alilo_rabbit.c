#include "audio_service.h"
#include "robot_event.h"
#include "hal_alilo_rabbit.h"

#define hal_log(format, ...)  custom_log("HAL", format, ##__VA_ARGS__)

mico_semaphore_t recordKeyPress_Sem;
bool recordKeyStatus = KEY_RELEASE;
uint8_t mic_record_id = 0;
uint8_t audio_play_id = 0;

static void _recordKeyAction_cb( ROBOT_USER_EVENT event, void *data)
{
    mscp_result_t result = MSCP_RST_ERROR;
    hal_log(">>>>>>>>>> event: %d >>>>>>>>>>", event);
    switch(event)
    {
        case ROBOT_EVENT_KEY_AI_START:
            hal_log("++++++++++++++++++++++");
            recordKeyStatus = KEY_PRESS;
            mico_rtos_set_semaphore(&recordKeyPress_Sem);
            break;
        case ROBOT_EVENT_KEY_AI_STOP:
            hal_log("----------------------");
            recordKeyStatus = KEY_RELEASE;
            audio_service_mic_record_stop(&result, mic_record_id);
            break;
        default:
            recordKeyStatus = KEY_RELEASE;
            hal_log("**********************");
            break;
    }
}


static OSStatus user_key_init(void)
{
    return robot_event_service_start( _recordKeyAction_cb );
}

OSStatus hal_alilo_rabbit_init(void)
{
    OSStatus err;
    err = mico_rtos_init_semaphore(&recordKeyPress_Sem, 1);
    require_noerr(err, exit);

    err = audio_service_init();
    require_noerr(err, exit);

    err = user_key_init();
    require_noerr(err, exit);

    exit:
        return err;
}

int32_t hal_getVoiceData( uint8_t* voice_buf, uint32_t voice_buf_len)
{
    OSStatus err = kGeneralErr;
    AUDIO_MIC_RECORD_USER_PARAM_S record_user_param;
    mscp_result_t result = MSCP_RST_ERROR;
    hal_log(">>>>>>>>>>>>>>>>>>>");
    record_user_param.user_data = (uint8_t *) voice_buf;
    record_user_param.user_data_len = voice_buf_len;

    err = audio_service_mic_record_get_result( &result, mic_record_id,
                                               &record_user_param );

    if ( err != kNoErr )
    {
        return -1;
    } else
    {
        if ( result == MSCP_RST_SUCCESS )
        {
            hal_log( "read speex_ogg voice len:%d", record_user_param.cur_len );
            return record_user_param.cur_len;
        } else
        {
            return -1;
        }
    }
}

bool hal_player_test(const char *data, uint32_t data_len, uint32_t file_total_len)
{
    OSStatus err = kGeneralErr;
    mscp_result_t result = MSCP_RST_ERROR;
    AUDIO_STREAM_PALY_S audio_stream_s_p;
    hal_log(">>>>>>>>>>>>>>>>>> hal_player_test");

    audio_play_id = audio_service_system_generate_stream_id();
    audio_stream_s_p.type = AUDIO_STREAM_TYPE_MP3;
    audio_stream_s_p.pdata = (const uint8_t*)data;
    audio_stream_s_p.stream_id = audio_play_id;
    audio_stream_s_p.total_len = file_total_len;
    audio_stream_s_p.stream_len = (uint16_t)(data_len & 0xFFFF);

    err = audio_service_stream_play(&result, &audio_stream_s_p);
    if(err != kNoErr)
    {
        hal_log("audio played fail");
        return false;
    }
    hal_log("audio playing ......");
    return true;
}
