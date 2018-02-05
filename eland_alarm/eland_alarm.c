/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:34
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-02-04 18:11:50
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/

#include "eland_alarm.h"
#include "audio_service.h"
#include "eland_sound.h"
#include "netclock.h"
#include "eland_tcp.h"
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
static void get_alarm_utc_second(__elsv_alarm_data_t *alarm);
/* Private functions ---------------------------------------------------------*/
OSStatus Start_Alarm_service(void)
{
    OSStatus err;
    memset(&alarm_list, 0, sizeof(_eland_alarm_list_t));
    err = mico_rtos_init_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_list.AlarmSerialMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_list.state.AlarmStateMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_stream.stream_Mutex);
    require_noerr(err, exit);

    alarm_stream.stream_id = audio_service_system_generate_stream_id();
    set_alarm_stream_pos(0);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager",
                                  Alarm_Manager, 0x3000, 0);
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    __elsv_alarm_data_t *alarm_nearest = NULL;
    mico_utc_time_t utc_time = 0;
    uint8_t i;
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < alarm_list.alarm_number; i++)
        elsv_alarm_data_sort_out(alarm_list.alarm_lib + i);
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);

    alarm_log("start Alarm_Manager ");
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
            alarm_log("list_refreshed");
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
                utc_time = GET_current_second();
                if (utc_time >= alarm_nearest->alarm_data_for_eland.moment_second)
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
static void init_alarm_stream(char *alarm_id, uint8_t sound_type)
{
    mico_rtos_lock_mutex(&alarm_stream.stream_Mutex);
    memset(alarm_stream.alarm_id, 0, ALARM_ID_LEN + 1);
    alarm_stream.data_pos = 0;
    memcpy(alarm_stream.alarm_id, alarm_id, ALARM_ID_LEN);
    alarm_stream.sound_type = sound_type;
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

static mico_utc_time_t GET_current_second(void)
{
    mico_utc_time_t utc_time;
    mico_time_get_utc_time(&utc_time);
    return utc_time;
}
/**elsv alarm data init**/
void elsv_alarm_data_sort_out(__elsv_alarm_data_t *elsv_alarm_data)
{
    _alarm_eland_data_t *alarm_eland_data = NULL;
    _alarm_mcu_data_t *alarm_mcu_data = NULL;
    int ho, mi, se;
    uint8_t i;
    set_alarm_state(ALARM_SORT);
    if (elsv_alarm_data == NULL)
        return;
    alarm_eland_data = &(elsv_alarm_data->alarm_data_for_eland);
    alarm_mcu_data = &(elsv_alarm_data->alarm_data_for_mcu);
    sscanf((const char *)(&(elsv_alarm_data->alarm_time)), "%02d:%02d:%02d", &ho, &mi, &se);

    alarm_mcu_data->moment_time.hr = ho;
    alarm_mcu_data->moment_time.min = mi;
    alarm_mcu_data->moment_time.sec = se;
    // if (elsv_alarm_data->alarm_repeat == 0)
    //      get_alarm_utc_second(elsv_alarm_data);

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
    mico_utc_time_t utc_time = 0;
    OSStatus err = kNoErr;
start_ergonic_list:
    list->list_refreshed = false;
    list->state.alarm_stoped = false;
    set_alarm_state(ALARM_IDEL);
    if (list->alarm_number == 0)
    {
        alarm_log("alarm_number = 0");
        elsv_alarm_data = NULL;
        goto exit;
    }
    utc_time = GET_current_second();
    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_repeat)
            get_alarm_utc_second(list->alarm_lib + i);
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second < utc_time)
        {
            alarm_list_minus(list, list->alarm_lib + i);
            goto start_ergonic_list;
        }
        if (((list->alarm_lib + i)->alarm_pattern == 1) ||
            ((list->alarm_lib + i)->alarm_pattern == 3) ||
            ((list->alarm_lib + i)->alarm_pattern == 4))
        {
            alarm_log("SID:%d", i);
            err = alarm_sound_download(list->alarm_lib + i, SOUND_FILE_SID);
            require_noerr_action(err, exit, elsv_alarm_data = NULL);
        }
        if (((list->alarm_lib + i)->alarm_pattern == 2) ||
            ((list->alarm_lib + i)->alarm_pattern == 3))
        {
            alarm_log("VID:%d", i);
            err = alarm_sound_download(list->alarm_lib + i, SOUND_FILE_VID);
            require_noerr_action(err, exit, elsv_alarm_data = NULL);
        }
        if ((list->alarm_lib + i)->alarm_pattern == 4)
        {
            alarm_log("OID:%d", i);
            err = alarm_sound_download(list->alarm_lib + i, SOUND_FILE_OID);
            require_noerr_action(err, exit, elsv_alarm_data = NULL);
        }
    }
    utc_time = GET_current_second();
    alarm_log("utc_time:%ld", utc_time);
    set_waiting_alarm_serial(0);
    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second < utc_time)
            continue;
        else
        {
            elsv_alarm_data = list->alarm_lib + i;
            set_waiting_alarm_serial(i);
            break;
        }
    }
    if (elsv_alarm_data == NULL)
        return NULL;

    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second < utc_time)
            continue;
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second <
            elsv_alarm_data->alarm_data_for_eland.moment_second)
        {
            elsv_alarm_data = list->alarm_lib + i;
            set_waiting_alarm_serial(i);
        }
    }
exit:
    return elsv_alarm_data;
}
OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;
    __elsv_alarm_data_t *p = AlarmList->alarm_lib;
    set_alarm_state(ALARM_ADD);
    err = mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
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
    err = mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    return err;
}
OSStatus alarm_list_minus(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;
    alarm_log("lock AlarmlibMutex");
    err = mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
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
                alarm_log("get minus alarm_id");
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
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
exit:
    return err;
}

static void alarm_operation(__elsv_alarm_data_t *alarm)
{
    _alarm_list_state_t current_state;
    int8_t snooze_count;
    mico_utc_time_t utc_time;
    mico_utc_time_t alarm_moment = alarm->alarm_data_for_eland.moment_second;

    if (alarm->snooze_enabled)
        snooze_count = alarm->snooze_count;
    else
        snooze_count = 1;
    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
    set_alarm_state(ALARM_ING);
    do
    {
        current_state = get_alarm_state();
        utc_time = GET_current_second();
        switch (current_state)
        {
        case ALARM_ING:
            if (utc_time > alarm_moment)
            {
                alarm_log("alarm_time up");
                alarm_moment += (uint32_t)alarm->snooze_interval_min * 60;
                alarm_moment -= (uint32_t)alarm->alarm_continue_min * 60;
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
                if (utc_time > alarm_moment)
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
        alarm_log("audio_status %d", audio_status);
        if (audio_status == MSCP_STATUS_IDLE)
        {
            if ((alarm->alarm_pattern == 1) || (alarm->alarm_pattern == 4))
            {
                init_alarm_stream(alarm->alarm_id, SOUND_FILE_SID);
                mico_rtos_create_thread(play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x900, (uint32_t)(&alarm_stream));
            }
            else if (alarm->alarm_pattern == 2)
            {
                init_alarm_stream(alarm->alarm_id, SOUND_FILE_SID);
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
        if (alarm->alarm_pattern == 4) //alarm off oid
        {
        }
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
    alarm_w_r_queue->sound_type = stream->sound_type;
    alarm_w_r_queue->is_read = true;
    alarm_w_r_queue->sound_data = flashdata;

falsh_read_start:
    alarm_w_r_queue->pos = data_pos;
    alarm_log("inlen = %ld,sound_flash_pos = %ld", alarm_w_r_queue->len, alarm_w_r_queue->pos);
    err = mico_rtos_push_to_queue(&eland_sound_R_W_queue, &alarm_w_r_queue, 10);
    require_noerr(err, exit);
    err = mico_rtos_pop_from_queue(&eland_sound_reback_queue, &alarm_r_w_callbcke_queue, 1000);
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
        free(alarm_r_w_callbcke_queue);
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
    alarm_log("state is_SUCCESS !");
exit:
    set_alarm_stream_pos(0);
    free(flashdata);
    free(alarm_w_r_queue);
    mico_rtos_delete_thread(NULL);
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

uint8_t get_next_alarm_serial(uint8_t now_serial)
{
    now_serial++;
    if (now_serial >= alarm_list.alarm_number)
        now_serial = 0;
    return now_serial;
}
uint8_t get_previous_alarm_serial(uint8_t now_serial)
{
    if (alarm_list.alarm_number)
    {
        if ((now_serial >= alarm_list.alarm_number) || (now_serial == 0))
            now_serial = alarm_list.alarm_number - 1;
        else
            now_serial--;
    }
    return now_serial;
}
uint8_t get_waiting_alarm_serial(void)
{
    return alarm_list.alarm_waiting_serial;
}
void set_waiting_alarm_serial(uint8_t now_serial)
{
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_lock_mutex(&alarm_list.AlarmSerialMutex);
    alarm_list.alarm_waiting_serial = now_serial;
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_unlock_mutex(&alarm_list.AlarmSerialMutex);
}

uint8_t get_display_alarm_serial(void)
{
    return alarm_list.alarm_display_serial;
}
void set_display_alarm_serial(uint8_t serial)
{
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_lock_mutex(&alarm_list.AlarmSerialMutex);
    alarm_list.alarm_display_serial = serial;
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_unlock_mutex(&alarm_list.AlarmSerialMutex);
}
_alarm_mcu_data_t *get_alarm_mcu_data(uint8_t serial)
{
    _alarm_mcu_data_t *temp = NULL;
    if (serial >= alarm_list.alarm_number)
        return temp;
    else
        temp = &(alarm_list.alarm_lib + serial)->alarm_data_for_mcu;
    return temp;
}

static void get_alarm_utc_second(__elsv_alarm_data_t *alarm)
{
    DATE_TIME_t date_time;
    mico_rtc_time_t rtc_time = {0};
    mico_utc_time_t utc_time = 0;
    struct tm *currentTime;
    int8_t i;
    MicoRtcGetTime(&rtc_time);
    date_time.iYear = 2000 + rtc_time.year;
    date_time.iMon = (int16_t)rtc_time.month;
    date_time.iDay = (int16_t)rtc_time.date;
    date_time.iHour = (int16_t)alarm->alarm_data_for_mcu.moment_time.hr;
    date_time.iMin = (int16_t)alarm->alarm_data_for_mcu.moment_time.min;
    date_time.iSec = (int16_t)alarm->alarm_data_for_mcu.moment_time.sec;
    date_time.iMsec = 0;
    utc_time = GET_current_second();
    switch (alarm->alarm_repeat)
    {
    case 0: //只在新数据导入时候计算一次
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        mico_time_get_utc_time(&utc_time);
        if (alarm->alarm_data_for_eland.moment_second < utc_time)
            alarm->alarm_data_for_eland.moment_second += 86400;
        currentTime = localtime((const time_t *)&(alarm->alarm_data_for_eland.moment_second));
        alarm->alarm_data_for_mcu.moment_time.sec = currentTime->tm_sec;
        alarm->alarm_data_for_mcu.moment_time.min = currentTime->tm_min;
        alarm->alarm_data_for_mcu.moment_time.hr = currentTime->tm_hour;
        alarm->alarm_data_for_mcu.moment_time.date = currentTime->tm_mday;
        alarm->alarm_data_for_mcu.moment_time.weekday = currentTime->tm_wday;
        alarm->alarm_data_for_mcu.moment_time.month = currentTime->tm_mon + 1;
        alarm->alarm_data_for_mcu.moment_time.year = (currentTime->tm_year + 1900) % 100;
        alarm_log("moment_second %ld", alarm->alarm_data_for_eland.moment_second);
        break;
    case 1:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        break;
    case 2:
        if (rtc_time.weekday < 2)
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + 3600;
        else if (rtc_time.weekday < 7)
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        else
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + 7200;
        break;
    case 3:
        if (rtc_time.weekday < 2)
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        else if (rtc_time.weekday < 7)
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + (7 - rtc_time.weekday) * 3600;
        else
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        break;
    case 4:
        for (i = 0; i < 7; i++)
        {
            if (alarm->alarm_data_for_mcu.alarm_on_days_of_week & (1 << ((rtc_time.weekday + i - 1) % 7)))
                alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + (i * 3600);
        }
        break;
    case 5:
        date_time.iYear = 2000 + alarm->alarm_data_for_mcu.moment_time.year;
        date_time.iMon = (int16_t)alarm->alarm_data_for_mcu.moment_time.month;
        date_time.iDay = (int16_t)alarm->alarm_data_for_mcu.moment_time.date;
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        break;
    default:
        alarm->alarm_data_for_eland.moment_second = 0;
        break;
    }
}
