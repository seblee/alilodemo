#include "../alilodemo/hal_alilo_rabbit.h"
#include "../alilodemo/inc/audio_service.h"
#include "../alilodemo/inc/http_file_download.h"
#include "../alilodemo/inc/robot_event.h"
#include "netclock_uart.h"
#include "mico.h"
#include "platform.h"
#include "netclock_httpd.h"
#define hal_log(format, ...) custom_log("HAL", format, ##__VA_ARGS__)

mico_semaphore_t recordKeyPress_Sem;
mico_semaphore_t urlFileDownload_Sem;
mico_semaphore_t urlPalyStreamStop_Sem;
bool recordKeyStatus = KEY_RELEASE;
uint8_t mic_record_id = 0;
uint8_t audio_play_id = 0;
uint8_t flag_mic_start = 0;
uint8_t flagAudioPlay = 0;
uint8_t flagHttpdServerAP = 0;

extern void PlatformEasyLinkButtonClickedCallback(void);
extern void PlatformEasyLinkButtonLongPressedCallback(void);

static uint16_t fm_test_cnt = 0;
static PLAYER_OPTION_S stream_play_opt;
static uint8_t ai_stream_play_id = 0;
static FILE_DOWNLOAD_CONTEXT g_file_download_context_user = NULL;

static void _recordKeyAction_cb(ROBOT_USER_EVENT event, void *data)
{
    mscp_result_t result = MSCP_RST_ERROR;
    OSStatus err = kNoErr;
    err = err;
    hal_log(">>>>>>>>>> event: %d >>>>>>>>>>", event);
    switch (event)
    {
    case ROBOT_EVENT_KEY_AUTO_SLEEP_ON_OFF:
        if ((flagAudioPlay == 2) || (flagAudioPlay == 3))
        {
            mico_rtos_set_semaphore(&urlPalyStreamStop_Sem);
            mico_rtos_thread_sleep(1);
        }
        PlatformEasyLinkButtonLongPressedCallback();
        break;
    case ROBOT_EVENT_KEY_AI_START:
        hal_log("mic record start ...");
        recordKeyStatus = KEY_PRESS;
        mico_rtos_set_semaphore(&recordKeyPress_Sem);
        break;

    case ROBOT_EVENT_KEY_AI_STOP:
        recordKeyStatus = KEY_RELEASE;
        if (flag_mic_start)
        {
            hal_log("mic record stop ...");
            flag_mic_start = 0;
            audio_service_mic_record_stop(&result, mic_record_id);
            hal_log("audio_service_mic_record_stop >>> err:%d, result:%d", err, result);
        }
        break;

    case ROBOT_EVENT_KEY_NET_CONFIG:
        hal_log("############## easylink call back ################");
        PlatformEasyLinkButtonClickedCallback();
        break;

    case ROBOT_EVENT_KEY_URL_FILEDNLD:
        hal_log("url file download start ...");
        if (flagAudioPlay != 0)
            mico_rtos_set_semaphore(&urlFileDownload_Sem);
        break;
    case ROBOT_EVENT_KEY_PLAY_PAUSE:
        if ((flagAudioPlay == 2) || (flagAudioPlay == 3))
            mico_rtos_set_semaphore(&urlPalyStreamStop_Sem);
        break;
    case ROBOT_EVENT_KEY_CHILD_LOCK:

        break;
    default:
        recordKeyStatus = KEY_RELEASE;
        if (flag_mic_start)
        {
            hal_log("mic record stop ...");
            flag_mic_start = 0;
            audio_service_mic_record_stop(&result, mic_record_id);
            hal_log("audio_service_mic_record_stop >>> err:%d, result:%d", err, result);
        }
        break;
    }
}

static OSStatus user_key_init(void)
{
    return robot_event_service_start(_recordKeyAction_cb);
}
extern system_context_t *sys_context;
OSStatus hal_alilo_rabbit_init(void)
{
    OSStatus err;
    mscp_result_t result;
    uint8_t i;
    ADUIO_SYSTEM_STATE_S Aduio_state;
    memset(&Aduio_state, 0, sizeof(ADUIO_SYSTEM_STATE_S));

    err = mico_rtos_init_semaphore(&recordKeyPress_Sem, 1);
    require_noerr(err, exit);

    err = mico_rtos_init_semaphore(&urlFileDownload_Sem, 1);
    require_noerr(err, exit);

    err = mico_rtos_init_semaphore(&urlPalyStreamStop_Sem, 1);
    require_noerr(err, exit);

    err = audio_service_init();
    require_noerr(err, exit);

    for (i = 0; i < 35; i++)
        audio_service_volume_down(&result, 1);
    // err = user_key_init();

    //audio_service_sound_remind_start(&result, 1); //我的尝夥伴，快來和我玩耝坧
    //audio_service_sound_remind_start(&result, 2);//請先幫我酝置無線網絡坧
    //audio_service_sound_remind_start(&result, 3); //正在連接無線網絡
    //audio_service_sound_remind_start(&result, 4); //請長安與我尝話坧
    //audio_service_sound_remind_start(&result, 5); //這個啝題有點難，我還在學習
    //audio_service_sound_remind_start(&result, 6); //尝丝起，沒有蝽清，請冝來一次
    //audio_service_sound_remind_start(&result, 7); //網絡連接失敗
    //audio_service_sound_remind_start(&result, 8); //設備正在均級中，請稝等一會坧
    //audio_service_sound_remind_start(&result, 9); //還剩百分之二坝五電針，電針快丝足了
    //audio_service_sound_remind_start(&result, 10); //還剩百分之五電針，快幫我衝充電坧
    //audio_service_sound_remind_start(&result, 11); //微信發逝的蝲音 “嗚”
    //audio_service_sound_remind_start(&result, 12); //門鈴蝲音 “坮噔”
    //audio_service_sound_remind_start(&result, 13); //“坮坮”蝲

exit:
    return err;
}

int32_t hal_getVoiceData(uint8_t *voice_buf, uint32_t voice_buf_len)
{
    OSStatus err = kGeneralErr;
    AUDIO_MIC_RECORD_USER_PARAM_S record_user_param;
    mscp_result_t result = MSCP_RST_ERROR;
    hal_log(">>>>>>>>>>>>>>>>>>>");
    record_user_param.user_data = (uint8_t *)voice_buf;
    record_user_param.user_data_len = voice_buf_len;

    err = audio_service_mic_record_get_result(&result, mic_record_id,
                                              &record_user_param);

    if (err != kNoErr)
    {
        return -1;
    }
    else
    {
        if (result == MSCP_RST_SUCCESS)
        {
            hal_log("read speex_ogg voice len:%d", record_user_param.cur_len);
            return record_user_param.cur_len;
        }
        else
        {
            return -1;
        }
    }
}

bool hal_player_start(const char *data, uint32_t data_len, uint32_t file_total_len)
{
    OSStatus err = kGeneralErr;
    mscp_result_t result = MSCP_RST_ERROR;
    AUDIO_STREAM_PALY_S audio_stream_s_p;
    hal_log(">>>>>>>>>>>>>>>>>> hal_player_test");

    audio_play_id = audio_service_system_generate_stream_id();
    audio_stream_s_p.type = AUDIO_STREAM_TYPE_AMR;
    audio_stream_s_p.pdata = (const uint8_t *)data;
    audio_stream_s_p.stream_id = audio_play_id;
    audio_stream_s_p.total_len = file_total_len;
    audio_stream_s_p.stream_len = (uint16_t)(data_len & 0xFFFF);

    err = audio_service_stream_play(&result, &audio_stream_s_p);
    if (err != kNoErr)
    {
        hal_log("audio played fail, result: %d", result);
        return false;
    }
    hal_log("audio playing ......");
    return true;
}

static void file_download_state_cb(void *context, HTTP_FILE_DOWNLOAD_STATE_E state, uint32_t progress, uint32_t user_args)
{
    //    FILE_DOWNLOAD_CONTEXT_S *file_download_context = (FILE_DOWNLOAD_CONTEXT_S *)context;

    PLAYER_OPTION_S *s_player_option_p = (PLAYER_OPTION_S *)user_args;

    if (state == HTTP_FILE_DOWNLOAD_STATE_LOADING)
    {
        hal_log("loading.... %ld %%, user_args = %ld", progress, user_args);
    }
    else
    {
        //        hal_log("state = %s, user_args = %ld", user_file_download_state_string[state], user_args);
    }

    s_player_option_p->file_download_status_e = state;

    if (state == HTTP_FILE_DOWNLOAD_STATE_SUCCESS)
    {
        s_player_option_p->file_download_status_e = HTTP_FILE_DOWNLOAD_STATE_SUCCESS;
        hal_log("state is HTTP_FILE_DOWNLOAD_STATE_SUCCESS!");
    }
    else if (state == HTTP_FILE_DOWNLOAD_STATE_FAILED)
    {
        s_player_option_p->file_download_status_e = HTTP_FILE_DOWNLOAD_STATE_FAILED;
        hal_log("state is HTTP_FILE_DOWNLOAD_STATE_FAILED!!!");
    }

    return;
}

static bool file_download_data_cb(void *context, const char *data, uint32_t data_len, uint32_t user_args)
{
    AUDIO_STREAM_PALY_S fm_stream;
    OSStatus err = kGeneralErr;
    FILE_DOWNLOAD_CONTEXT_S *file_download_context = (FILE_DOWNLOAD_CONTEXT_S *)context;
    mscp_result_t result = MSCP_RST_ERROR;
    PLAYER_OPTION_S *s_player_option_p = (PLAYER_OPTION_S *)user_args;

    require_string((file_download_context != NULL) && (data_len <= 2000) && (data_len != 0), exit, "data len error");

    fm_stream.type = s_player_option_p->type;
    fm_stream.stream_id = s_player_option_p->stream_player_id;
    fm_stream.total_len = file_download_context->file_info.file_total_len;
    fm_stream.stream_len = (uint16_t)(data_len & 0xFFFF); //len
    fm_stream.pdata = (const uint8_t *)data;

    //    hal_log("############# file_download_data callback ############# ");

    if ((++fm_test_cnt) == 100)
    {
        fm_test_cnt = 0;
        hal_log("fm_stream.type[%d],fm_stream.stream_id[%d],fm_stream.total_len[%d],fm_stream.stream_len[%d]",
                (int)fm_stream.type, (int)fm_stream.stream_id, (int)fm_stream.total_len, (int)fm_stream.stream_len);
    }

audio_transfer:
    err = audio_service_stream_play(&result, &fm_stream);
    if (err != kNoErr)
    {
        hal_log("audio_stream_play() error!!!!");
        return false;
    }
    else
    {
        require_action_string(http_file_download_get_state(&file_download_context) != HTTP_FILE_DOWNLOAD_CONTROL_STATE_STOP, exit, err = kGeneralErr, "user set stop download!");
        if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
        {
            hal_log("new slave set pause!!!");
            mico_rtos_thread_msleep(1000); //time set 1000ms!!!
            goto audio_transfer;
        }
        else
        {
            err = kNoErr;
        }
    }

exit:
    if (err == kNoErr)
    {
        return true;
    }
    else
    {
        return false;
    }
}

OSStatus hal_url_fileDownload_start(char *url)
{
    hal_log(">>>>>>>>>>>>>>>>>>> hal_url_fileDownload_test");
    stream_play_opt.type = AUDIO_STREAM_TYPE_MP3;
    ai_stream_play_id = audio_service_system_generate_stream_id();
    stream_play_opt.stream_player_id = ai_stream_play_id;

    return http_file_download_start((FILE_DOWNLOAD_CONTEXT *)(&g_file_download_context_user),
                                    (const char *)url,
                                    file_download_state_cb,
                                    file_download_data_cb,
                                    (uint32_t)&stream_play_opt);
}
OSStatus hal_url_fileDownload_pause(void)
{
    hal_log(">>>>>>>>>>>>>>>>>>> hal_url_fileDownload_pause");

    return http_file_download_pause((FILE_DOWNLOAD_CONTEXT *)(&g_file_download_context_user));
}
OSStatus hal_url_fileDownload_continue(void)
{
    hal_log(">>>>>>>>>>>>>>>>>>> hal_url_fileDownload_continue");

    return http_file_download_continue((FILE_DOWNLOAD_CONTEXT *)(&g_file_download_context_user));
}
OSStatus hal_url_fileDownload_stop(void)
{
    hal_log(">>>>>>>>>>>>>>>>>>> hal_url_fileDownload_stop");
    return http_file_download_stop((FILE_DOWNLOAD_CONTEXT *)(&g_file_download_context_user), true);
}
