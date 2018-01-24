/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:34
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-01-12 17:15:43
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/

#include "eland_alarm.h"
#include "audio_service.h"
#include "eland_sound.h"
/* Private define ------------------------------------------------------------*/
#define alarm_log(format, ...) custom_log("alarm", format, ##__VA_ARGS__)
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
_eland_alarm_list_t alarm_list;
_alarm_stream_t alarm_stream;
mico_thread_t play_voice_thread = NULL;
/* Private function prototypes -----------------------------------------------*/
static uint32_t GET_current_second(void);
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list);
static void set_alarm_state(_alarm_list_state_t state);
static _alarm_list_state_t get_alarm_state(void);
void Alarm_Manager(uint32_t arg);
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD);
static void set_alarm_stream_pos(uint32_t pos);
static void play_voice(mico_thread_arg_t arg);
static void alarm_operation(__elsv_alarm_data_t *alarm);

/* Private functions ---------------------------------------------------------*/
OSStatus Start_Alarm_service(void)
{
    OSStatus err;
    __elsv_alarm_data_t elsv_alarm_data;
    uint8_t cache[11] = {0x00, 0x1e, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memset(&alarm_list, 0, sizeof(_eland_alarm_list_t));
    err = mico_rtos_init_mutex(&alarm_list.AlarmListMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_list.state.AlarmStateMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_stream.stream_Mutex);
    require_noerr(err, exit);

    elsv_alarm_data_init_MCU(&elsv_alarm_data, (_alarm_mcu_data_t *)cache);
    elsv_alarm_data_sort_out(&elsv_alarm_data);
    alarm_list_add(&alarm_list, &elsv_alarm_data);

    alarm_stream.stream_id = audio_service_system_generate_stream_id();
    set_alarm_stream_pos(0);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager", Alarm_Manager, 0x500, 0);
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    __elsv_alarm_data_t *alarm_nearest = NULL;
    uint32_t current_seconds = 0;
    uint8_t i;

    for (i = 0; i < alarm_list.alarm_number; i++)
        elsv_alarm_data_sort_out(alarm_list.alarm_lib + i);
    alarm_nearest = Alarm_ergonic_list(&alarm_list);
    if (alarm_nearest)
    {
        alarm_log("get alarm_nearest alarm_id:%s", alarm_nearest->alarm_id);
    }
    else
        alarm_log("NO alarm_nearest ");
    while (1)
    {
        if ((alarm_list.list_refreshed) || (alarm_list.state.alarm_stoped == true))
        {
            /**refresh point**/
            alarm_nearest = Alarm_ergonic_list(&alarm_list);
            if (alarm_nearest)
            {
                alarm_log("get alarm_nearest alarm_id:%s", alarm_nearest->alarm_id);
            }
            else
                alarm_log("NO alarm_nearest ");
        }
        if (get_alarm_state() == ALARM_IDEL)
        {
            if (alarm_nearest)
            {
                current_seconds = GET_current_second();

                if (current_seconds >= alarm_nearest->alarm_data_for_eland.moment_second)
                {
                    alarm_log("##### start alarm ######");
                    alarm_operation(alarm_nearest);
                    /**operation output alarm**/
                }
            }
        }
        /* check current time per second */
        mico_rtos_thread_sleep(1);
    }

    mico_rtos_delete_thread(NULL);
}
static _alarm_list_state_t get_alarm_state(void)
{
    return alarm_list.state.state;
}
static void set_alarm_state(_alarm_list_state_t state)
{
    mico_rtos_lock_mutex(&alarm_list.state.AlarmStateMutex);
    alarm_list.state.state = state;
    mico_rtos_unlock_mutex(&alarm_list.state.AlarmStateMutex);
}
static void init_alarm_stream(char *alarm_id)
{
    mico_rtos_lock_mutex(&alarm_stream.stream_Mutex);
    memset(alarm_stream.alarm_id, 0, ALARM_ID_LEN + 1);
    alarm_stream.data_pos = 0;
    memcpy(alarm_stream.alarm_id, alarm_id, ALARM_ID_LEN);
    mico_rtos_unlock_mutex(&alarm_stream.stream_Mutex);
}
static uint32_t get_alarm_stream_pos(void)
{
    return alarm_stream.data_pos;
}
static void set_alarm_stream_pos(uint32_t pos)
{
    mico_rtos_lock_mutex(&alarm_stream.stream_Mutex);
    alarm_stream.data_pos = pos;
    mico_rtos_unlock_mutex(&alarm_stream.stream_Mutex);
}

static uint32_t GET_current_second(void)
{
    uint32_t current_seconds;
    mico_rtc_time_t cur_time = {0};
    MicoRtcGetTime(&cur_time); //返回新的时间值
    current_seconds = (uint32_t)cur_time.hr * 3600 +
                      (uint32_t)cur_time.min * 60 +
                      (uint32_t)cur_time.sec;
    return current_seconds;
}
/**elsv alarm data init**/
void elsv_alarm_data_sort_out(__elsv_alarm_data_t *elsv_alarm_data)
{
    _alarm_eland_data_t *alarm_eland_data = NULL;
    _alarm_mcu_data_t *alarm_mcu_data = NULL;
    int ho, mi, se;
    uint8_t i;
    mico_rtos_lock_mutex(&alarm_list.AlarmListMutex);
    set_alarm_state(ALARM_SORT);
    if (elsv_alarm_data == NULL)
        goto exit;
    alarm_eland_data = &(elsv_alarm_data->alarm_data_for_eland);
    alarm_mcu_data = &(elsv_alarm_data->alarm_data_for_mcu);
    sscanf((const char *)(&(elsv_alarm_data->alarm_time)), "%02d:%02d:%02d", &ho, &mi, &se);

    alarm_eland_data->moment_second = (uint32_t)ho * 3600 + (uint32_t)mi * 60 + (uint32_t)se;
    alarm_mcu_data->moment_time.hr = ho;
    alarm_mcu_data->moment_time.min = mi;
    alarm_mcu_data->moment_time.sec = se;

    alarm_eland_data->color = elsv_alarm_data->alarm_color;
    alarm_mcu_data->color = elsv_alarm_data->alarm_color;

    if (elsv_alarm_data->snooze_enabled)
    {
        alarm_eland_data->snooze_count = elsv_alarm_data->snooze_count;
        alarm_mcu_data->snooze_count = elsv_alarm_data->snooze_count;
    }
    else
    {
        alarm_eland_data->snooze_count = 0;
        alarm_mcu_data->snooze_count = 0;
    }

    if (elsv_alarm_data->alarm_repeat == 1)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x7f;
        alarm_mcu_data->alarm_on_days_of_week = 0x7f;
    }
    else if (elsv_alarm_data->alarm_repeat == 2)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x3e;
        alarm_mcu_data->alarm_on_days_of_week = 0x3e;
    }
    else if (elsv_alarm_data->alarm_repeat == 3)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x41;
        alarm_mcu_data->alarm_on_days_of_week = 0x41;
    }
    else if (elsv_alarm_data->alarm_repeat == 4)
    {
        alarm_eland_data->alarm_on_days_of_week = 0;
        for (i = 0; i < 7; i++)
        {
            if (elsv_alarm_data->alarm_on_days_of_week[i] == '1')
                alarm_eland_data->alarm_on_days_of_week |= (1 << i);
        }
        alarm_mcu_data->alarm_on_days_of_week = alarm_eland_data->alarm_on_days_of_week;
    }
    else
    {
        alarm_eland_data->alarm_on_days_of_week = 0;
        alarm_mcu_data->alarm_on_days_of_week = 0;
    }
exit:
    mico_rtos_unlock_mutex(&alarm_list.AlarmListMutex);
}
void elsv_alarm_data_init_MCU(__elsv_alarm_data_t *elsv_alarm_data, _alarm_mcu_data_t *alarm_mcu_data)
{
    if ((elsv_alarm_data == NULL) || (alarm_mcu_data == NULL))
        return;
    memset(elsv_alarm_data, 0, sizeof(__elsv_alarm_data_t));
    memcpy(&elsv_alarm_data->alarm_data_for_mcu, alarm_mcu_data, sizeof(_alarm_mcu_data_t));
    strcpy(elsv_alarm_data->alarm_id, ALARM_ID_OF_SIMPLE_CLOCK);
    elsv_alarm_data->alarm_color = 0;
    sprintf(elsv_alarm_data->alarm_time, "%02d:%02d:%02d", alarm_mcu_data->moment_time.hr, alarm_mcu_data->moment_time.min, alarm_mcu_data->moment_time.sec);
    elsv_alarm_data->snooze_enabled = 1;
    elsv_alarm_data->snooze_count = 3;
    elsv_alarm_data->snooze_interval_min = 2;
    elsv_alarm_data->alarm_pattern = 1;
    elsv_alarm_data->alarm_sound_id = 1;
    elsv_alarm_data->alarm_volume = 80;
    elsv_alarm_data->alarm_continue_min = 1; //測試用 1分鐘
    elsv_alarm_data->alarm_repeat = 1;

    elsv_alarm_data->alarm_data_for_eland.moment_second = (uint32_t)alarm_mcu_data->moment_time.hr * 3600 +
                                                          (uint32_t)alarm_mcu_data->moment_time.min * 60 +
                                                          (uint32_t)alarm_mcu_data->moment_time.sec;
    elsv_alarm_data->alarm_data_for_eland.color = alarm_mcu_data->color;
    elsv_alarm_data->alarm_data_for_eland.snooze_count = 0;
    elsv_alarm_data->alarm_data_for_eland.alarm_on_days_of_week = 0x7f;

    // alarm_log("alarm_id:%s", elsv_alarm_data->alarm_id);
    // alarm_log("alarm_color:%d", elsv_alarm_data->alarm_color);
    // alarm_log("alarm_time:%s", elsv_alarm_data->alarm_time);
    // alarm_log("alarm_off_dates:%s", elsv_alarm_data->alarm_off_dates);
    // alarm_log("snooze_enabled:%d", elsv_alarm_data->snooze_enabled);
    // alarm_log("snooze_count:%d", elsv_alarm_data->snooze_count);
    // alarm_log("snooze_interval_min:%d", elsv_alarm_data->snooze_interval_min);
    // alarm_log("alarm_pattern:%d", elsv_alarm_data->alarm_pattern);
    // alarm_log("alarm_sound_id:%ld", elsv_alarm_data->alarm_sound_id);
    // alarm_log("voice_alarm_id:%s", elsv_alarm_data->voice_alarm_id);
    // alarm_log("alarm_off_voice_alarm_id:%s", elsv_alarm_data->alarm_off_voice_alarm_id);
    // alarm_log("alarm_volume:%d", elsv_alarm_data->alarm_volume);
    // alarm_log("volume_stepup_enabled:%d", elsv_alarm_data->volume_stepup_enabled);
    // alarm_log("alarm_continue_min:%d", elsv_alarm_data->alarm_continue_min);
    // alarm_log("alarm_repeat:%d", elsv_alarm_data->alarm_repeat);
    // alarm_log("alarm_on_dates:%s", elsv_alarm_data->alarm_on_dates);
    // alarm_log("alarm_on_days_of_week:%s", elsv_alarm_data->alarm_on_days_of_week);
}
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list)
{
    uint8_t i;
    __elsv_alarm_data_t *elsv_alarm_data = NULL;
    uint32_t current_seconds = 0;

    list->list_refreshed = false;
    list->state.alarm_stoped = false;
    set_alarm_state(ALARM_IDEL);
    if (list->alarm_number == 0)
        return NULL;
    elsv_alarm_data = list->alarm_lib;
    current_seconds = GET_current_second();

    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second < current_seconds)
            continue;
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second <
            elsv_alarm_data->alarm_data_for_eland.moment_second)
        {
            elsv_alarm_data = list->alarm_lib + i;
        }
    }

    if (elsv_alarm_data->alarm_data_for_eland.moment_second > current_seconds)
        return elsv_alarm_data;
    else
        return NULL;
}
OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;
    __elsv_alarm_data_t *p = AlarmList->alarm_lib;
    mico_rtos_lock_mutex(&alarm_list.AlarmListMutex);
    set_alarm_state(ALARM_ADD);

    if (AlarmList->alarm_number == 0)
    {
        alarm_log("alarm is first");
        AlarmList->alarm_lib = calloc(sizeof(__elsv_alarm_data_t), sizeof(uint8_t));
        require_action((AlarmList->alarm_lib != NULL), exit, err = kNoMemoryErr);
        memcpy(AlarmList->alarm_lib, inData, sizeof(__elsv_alarm_data_t));
        AlarmList->list_refreshed = true;
        AlarmList->alarm_number++;
    }
    else
    {
        for (i = 0; i < AlarmList->alarm_number; i++)
        {
            if (strcmp((AlarmList->alarm_lib + i)->alarm_id, inData->alarm_id) == 0)
            {
                alarm_log("alarm_id is repeated");
                memcpy(((uint8_t *)(AlarmList->alarm_lib) + i * sizeof(__elsv_alarm_data_t)), inData, sizeof(__elsv_alarm_data_t));
                AlarmList->list_refreshed = true;
                goto exit;
            }
        }
        /**a new alarm**/
        alarm_log("alarm_id is new");
        if (i == AlarmList->alarm_number)
        {
            AlarmList->alarm_lib = realloc(AlarmList->alarm_lib, (AlarmList->alarm_number + 1) * sizeof(__elsv_alarm_data_t));
            if (AlarmList->alarm_lib)
            {
                memcpy(((uint8_t *)(AlarmList->alarm_lib) + i * sizeof(__elsv_alarm_data_t)), inData, sizeof(__elsv_alarm_data_t));
                AlarmList->list_refreshed = true;
                AlarmList->alarm_number++;
            }
            else
            {
                AlarmList->alarm_lib = p;
                err = kNoMemoryErr;
            }
        }
    }
exit:
    mico_rtos_unlock_mutex(&alarm_list.AlarmListMutex);
    return err;
}
void alarm_list_minus(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    uint8_t i;
    mico_rtos_lock_mutex(&alarm_list.AlarmListMutex);
    set_alarm_state(ALARM_MINUS);
    if (AlarmList->alarm_number == 0)
    {
        if (AlarmList->alarm_lib)
            free(AlarmList->alarm_lib);
    }
    else
    {
        for (i = 0; i < AlarmList->alarm_number; i++)
        {
            if (strcmp((AlarmList->alarm_lib + i)->alarm_id, inData->alarm_id) == 0)
            {
                memmove(AlarmList->alarm_lib + i,
                        AlarmList->alarm_lib + i + 1,
                        (AlarmList->alarm_number - 1 - i) * sizeof(__elsv_alarm_data_t));
                if (AlarmList->alarm_number > 1)
                    AlarmList->alarm_lib = realloc(AlarmList->alarm_lib, (AlarmList->alarm_number - 1) * sizeof(__elsv_alarm_data_t));
                else
                {
                    free(AlarmList->alarm_lib);
                    AlarmList->alarm_lib = NULL;
                }
                AlarmList->alarm_number--;
                AlarmList->list_refreshed = true;
            }
        }
    }
    mico_rtos_unlock_mutex(&alarm_list.AlarmListMutex);
}

static void alarm_operation(__elsv_alarm_data_t *alarm)
{
    _alarm_list_state_t current_state;
    int8_t snooze_count;
    uint32_t current_seconds;
    uint32_t alarm_moment = alarm->alarm_data_for_eland.moment_second;
    init_alarm_stream(alarm->alarm_id);

    if (alarm->snooze_enabled)
        snooze_count = alarm->snooze_count;
    else
        snooze_count = 1;
    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
    set_alarm_state(ALARM_ING);
    do
    {
        current_state = get_alarm_state();
        current_seconds = GET_current_second();
        switch (current_state)
        {
        case ALARM_ING:
            if (current_seconds > alarm_moment)
            {
                alarm_log("alarm_time up");
                alarm_moment += (uint32_t)alarm->snooze_interval_min * 60 -
                                (uint32_t)alarm->alarm_continue_min * 60;
                set_alarm_state(ALARM_SNOOZ_STOP);
                snooze_count--;
                Alarm_Play_Control(alarm, 0); //stop with delay
            }
            else
                Alarm_Play_Control(alarm, 1); //play with delay
            break;
        case ALARM_ADD:
        case ALARM_MINUS:
        case ALARM_SORT:
        case ALARM_STOP:
            Alarm_Play_Control(alarm, 0); //stop
            alarm_list.state.alarm_stoped = true;
            break;
        case ALARM_SNOOZ_STOP:
            if (snooze_count)
            {
                if (current_seconds > alarm_moment)
                {
                    alarm_log("alarm_time on aagin %d", snooze_count);
                    set_alarm_state(ALARM_ING);
                    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
                }
            }
            else
                set_alarm_state(ALARM_STOP);
            break;
        default:
            alarm_list.state.alarm_stoped = true;
            break;
        }
        mico_rtos_thread_msleep(500);
    } while (alarm_list.state.alarm_stoped == false);
}
/***CMD = 1 PALY   CMD = 0 STOP***/
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD)
{
    mscp_result_t result;
    mscp_status_t audio_status;
    static bool isVoice = false;
    if (CMD) //play
    {
        audio_service_get_audio_status(&result, &audio_status);
        if (audio_status == MSCP_STATUS_IDLE)
        {
            if ((alarm->alarm_pattern == 1) || (alarm->alarm_pattern == 4))
                audio_service_sound_remind_start(&result, 12);
            else if (alarm->alarm_pattern == 2)
            {
                mico_rtos_create_thread(play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x900, (uint32_t)(&alarm_stream));
            }
            else if (alarm->alarm_pattern == 3)
            {
                if (isVoice)
                {
                    isVoice = false;
                    mico_rtos_create_thread(play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x900, (uint32_t)(&alarm_stream));
                }
                else
                {
                    isVoice = true;
                    audio_service_sound_remind_start(&result, 12);
                }
            }
        }
        else if ((audio_status == MSCP_STATUS_STREAM_PLAYING) || (audio_status == MSCP_STATUS_SOUND_REMINDING))
            return;
    }
    else //stop
    {
    }
}

static void play_voice(mico_thread_arg_t arg)
{
    _alarm_stream_t *stream = (_alarm_stream_t *)arg;

    AUDIO_STREAM_PALY_S flash_read_stream;
    OSStatus err = kGeneralErr;
    mscp_result_t result = MSCP_RST_ERROR;
    uint32_t data_pos = 0;
    uint8_t *flashdata = NULL;
    _sound_read_write_type_t *alarm_w_r_queue = NULL;
    _sound_callback_type_t *alarm_r_w_callbcke_queue = NULL;

    alarm_log("player_flash_thread");
    flashdata = malloc(2001);

    flash_read_stream.type = AUDIO_STREAM_TYPE_MP3;
    flash_read_stream.pdata = flashdata;
    flash_read_stream.stream_id = stream->stream_id;
    data_pos = get_alarm_stream_pos();

    alarm_w_r_queue = (_sound_read_write_type_t *)calloc(sizeof(_sound_read_write_type_t), sizeof(uint8_t));
    memset(alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
    memcpy(alarm_w_r_queue->alarm_ID, stream->alarm_id, ALARM_ID_LEN + 1);
    alarm_w_r_queue->is_read = true;
    alarm_w_r_queue->sound_data = flashdata;

falsh_read_start:
    alarm_w_r_queue->pos = data_pos;
    alarm_log("inlen = %ld,sound_flash_pos = %ld", alarm_w_r_queue->len, alarm_w_r_queue->pos);
    err = mico_rtos_push_to_queue(&eland_sound_R_W_queue, &alarm_w_r_queue, 10);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_sound_reback_queue, &alarm_r_w_callbcke_queue, MICO_WAIT_FOREVER);
    require_noerr(err, exit);

    if (alarm_r_w_callbcke_queue->read_write_err == ERRNONE)
    {
        alarm_log("ERRNONE");
        if (data_pos == 0)
        {
            flash_read_stream.total_len = alarm_w_r_queue->total_len;
        }
        flash_read_stream.stream_len = alarm_w_r_queue->len;
    }
    else if (alarm_r_w_callbcke_queue->read_write_err == FILE_NOT_FIND)
    {
        alarm_log("FILE_NOT_FIND");
        goto exit;
    }
    else
    {
        alarm_log("ELSE %d", (uint8_t)alarm_r_w_callbcke_queue->read_write_err);
    }
    alarm_log("type[%d],stream_id[%d],total_len[%d],stream_len[%d] data_pos[%ld]",
              (int)flash_read_stream.type, (int)flash_read_stream.stream_id,
              (int)flash_read_stream.total_len, (int)flash_read_stream.stream_len, data_pos);
    alarm_log("free_callback_queue");
    free(alarm_r_w_callbcke_queue);

audio_transfer:
    err = audio_service_stream_play(&result, &flash_read_stream);

    if (err != kNoErr)
    {
        alarm_log("audio_stream_play() error!!!!");
        goto exit;
    }
    else
    {
        if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
        {
            alarm_log("new slave set pause!!!");
            mico_rtos_thread_msleep(1000); //time set 1000ms!!!
            goto audio_transfer;
        }
        else
        {
            err = kNoErr;
        }
    }
    data_pos += flash_read_stream.stream_len;
    if (data_pos < flash_read_stream.total_len)
        goto falsh_read_start;
    else
        data_pos = 0;
    alarm_log("state is HTTP_FILE_DOWNLOAD_STATE_SUCCESS !");
exit:
    set_alarm_stream_pos(data_pos);
    free(flashdata);
    free(alarm_w_r_queue);
}

void alarm_print(__elsv_alarm_data_t *alarm_data)
{
    alarm_log("alarm_list number:%d", alarm_list.alarm_number);
    alarm_log("\r\n__elsv_alarm_data_t");
    alarm_log("alarm_id:%s", alarm_data->alarm_id);
    alarm_log("alarm_color:%d", alarm_data->alarm_color);
    alarm_log("alarm_time:%s", alarm_data->alarm_time);
    alarm_log("alarm_off_dates:%s", alarm_data->alarm_off_dates);
    alarm_log("snooze_enabled:%d", alarm_data->snooze_enabled);
    alarm_log("snooze_count:%d", alarm_data->snooze_count);
    alarm_log("snooze_interval_min:%d", alarm_data->snooze_interval_min);
    alarm_log("alarm_pattern:%d", alarm_data->alarm_pattern);
    alarm_log("alarm_sound_id:%ld", alarm_data->alarm_sound_id);
    alarm_log("voice_alarm_id:%s", alarm_data->voice_alarm_id);
    alarm_log("alarm_off_voice_alarm_id:%s", alarm_data->alarm_off_voice_alarm_id);
    alarm_log("alarm_volume:%d", alarm_data->alarm_volume);
    alarm_log("volume_stepup_enabled:%d", alarm_data->volume_stepup_enabled);
    alarm_log("alarm_continue_min:%d", alarm_data->alarm_continue_min);
    alarm_log("alarm_repeat:%d", alarm_data->alarm_repeat);
    alarm_log("alarm_on_dates:%s", alarm_data->alarm_on_dates);
    alarm_log("alarm_on_days_of_week:%s", alarm_data->alarm_on_days_of_week);
    alarm_log("\r\alarm_data_for_eland");
    alarm_log("moment_second:%ld", alarm_data->alarm_data_for_eland.moment_second);
    alarm_log("color:%d", alarm_data->alarm_data_for_eland.color);
    alarm_log("snooze_count:%d", alarm_data->alarm_data_for_eland.snooze_count);
    alarm_log("alarm_on_days_of_week:%d", alarm_data->alarm_data_for_eland.alarm_on_days_of_week);
    alarm_log("\r\n_alarm_mcu_data_t");
    alarm_log("color:%d", alarm_data->alarm_data_for_mcu.color);
    alarm_log("snooze_count:%d", alarm_data->alarm_data_for_mcu.snooze_count);
    alarm_log("alarm_on_days_of_week:%d", alarm_data->alarm_data_for_mcu.alarm_on_days_of_week);
    alarm_log("moment_time:%02d-%02d-%02d %d %02d-%02d-%02d",
              alarm_data->alarm_data_for_mcu.moment_time.year,
              alarm_data->alarm_data_for_mcu.moment_time.month,
              alarm_data->alarm_data_for_mcu.moment_time.date,
              alarm_data->alarm_data_for_mcu.moment_time.weekday,
              alarm_data->alarm_data_for_mcu.moment_time.hr,
              alarm_data->alarm_data_for_mcu.moment_time.min,
              alarm_data->alarm_data_for_mcu.moment_time.sec);
}