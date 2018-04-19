/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:34
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-16 16:01:13
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "eland_alarm.h"
#include "audio_service.h"
#include "eland_sound.h"
#include "netclock.h"
#include "eland_tcp.h"
#include "netclock_uart.h"
#include "eland_http_client.h"
/* Private define ------------------------------------------------------------*/
#define CONFIG_ALARM_DEBUG
#ifdef CONFIG_ALARM_DEBUG
#define alarm_log(M, ...) custom_log("alarm", M, ##__VA_ARGS__)
#else
#define alarm_log(...)
#endif /* ! CONFIG_ALARM_DEBUG */
/*********/
#define ALARM_DATA_SIZE (uint16_t)(sizeof(__elsv_alarm_data_t) * 50 + 1)
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_semaphore_t alarm_update = NULL;
mico_semaphore_t alarm_skip_sem = NULL;
mico_queue_t http_queue = NULL;

_eland_alarm_list_t alarm_list;
_elsv_holiday_t holiday_list;
_alarm_stream_t alarm_stream;
mico_thread_t play_voice_thread = NULL;

_alarm_off_history_t off_history;
/* Private function prototypes -----------------------------------------------*/
static uint32_t GET_current_second(void);
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list);
void Alarm_Manager(uint32_t arg);
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD);

static void play_voice(mico_thread_arg_t arg);
static void alarm_operation(__elsv_alarm_data_t *alarm);
static void get_alarm_utc_second(__elsv_alarm_data_t *alarm);
static bool today_is_holiday(DATE_TIME_t *date, uint8_t offset);
static void combing_alarm(_eland_alarm_list_t *list, __elsv_alarm_data_t **alarm_nearest);
static OSStatus set_alarm_history_send_sem(void);
/* Private functions ---------------------------------------------------------*/
OSStatus Start_Alarm_service(void)
{
    OSStatus err;
    /*init holiday list*/
    memset(&holiday_list, 0, sizeof(_elsv_holiday_t));
    err = mico_rtos_init_mutex(&holiday_list.holidaymutex);
    require_noerr(err, exit);
    /**init alarm list**/
    memset(&alarm_list, 0, sizeof(_eland_alarm_list_t));
    /**apply alarm memory**/
    alarm_list.alarm_lib = calloc(ALARM_DATA_SIZE, sizeof(uint8_t));
    alarm_log("alarm_data memory size:%d", ALARM_DATA_SIZE);
    err = mico_rtos_init_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_list.AlarmSerialMutex);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&alarm_list.state.AlarmStateMutex);
    require_noerr(err, exit);
    alarm_stream.stream_id = audio_service_system_generate_stream_id();
    err = mico_rtos_init_mutex(&alarm_stream.stream_Mutex);
    require_noerr(err, exit);
    alarm_list.alarm_nearest = NULL;
    /*need to ergonic alarm*/
    err = mico_rtos_init_semaphore(&alarm_update, 5);
    require_noerr(err, exit);
    //eland_state_queue
    err = mico_rtos_init_queue(&http_queue, "http_queue", sizeof(_download_type_t), 5); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    err = mico_rtos_init_semaphore(&alarm_skip_sem, 1);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&off_history.off_Mutex);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager",
                                  Alarm_Manager, 0x800, 0);
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    __elsv_alarm_data_t *alarm_nearest = NULL;
    _alarm_list_state_t alarm_state;
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
        set_display_alarm_serial(get_waiting_alarm_serial());
        alarm_log("get alarm_nearest alarm_id:%s", alarm_nearest->alarm_id);
    }
    else
    {
        set_display_alarm_serial(0);
        alarm_log("NO alarm_nearest ");
    }
    while (1)
    {
        combing_alarm(&alarm_list, &alarm_nearest);
        alarm_state = get_alarm_state();
        if ((alarm_state == ALARM_IDEL) || (alarm_state == ALARM_SKIP))
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
    }
    mico_rtos_delete_thread(NULL);
}
_alarm_list_state_t get_alarm_state(void)
{
    return alarm_list.state.state;
}
void set_alarm_state(_alarm_list_state_t state)
{
    iso8601_time_t iso8601_time;
    if (state == ALARM_ING)
    {
        mico_time_get_iso8601_time(&iso8601_time);
        alarm_off_history_record_time(ALARM_ON, &iso8601_time);
    }
    mico_rtos_lock_mutex(&alarm_list.state.AlarmStateMutex);
    alarm_list.state.state = state;
    mico_rtos_unlock_mutex(&alarm_list.state.AlarmStateMutex);
    if ((state == ALARM_ING) || (state == ALARM_SNOOZ_STOP))
        eland_push_uart_send_queue(ALARM_SEND_0B);
}
static void init_alarm_stream(__elsv_alarm_data_t *alarm, uint8_t sound_type)
{
    mico_rtos_lock_mutex(&alarm_stream.stream_Mutex);
    memset(alarm_stream.alarm_id, 0, ALARM_ID_LEN + 1);
    alarm_stream.sound_type = sound_type;

    if ((alarm->alarm_pattern == 3) ||
        (sound_type == SOUND_FILE_OFID))
        alarm_stream.stream_count = 1;
    else
        alarm_stream.stream_count = 0xffff;

    switch (sound_type)
    {
    case SOUND_FILE_VID:
        memcpy(alarm_stream.alarm_id, alarm->voice_alarm_id, strlen(alarm->voice_alarm_id));
        break;
    case SOUND_FILE_SID:
        memcpy(alarm_stream.alarm_id, &(alarm->alarm_sound_id), sizeof(alarm->alarm_sound_id));
        break;
    case SOUND_FILE_OFID:
        memcpy(alarm_stream.alarm_id, alarm->alarm_off_voice_alarm_id, sizeof(alarm->alarm_off_voice_alarm_id));
        break;
    case SOUND_FILE_DEFAULT:
        memcpy(alarm_stream.alarm_id, ALARM_ID_OF_SIMPLE_CLOCK, strlen(ALARM_ID_OF_SIMPLE_CLOCK));
        break;
    default:
        break;
    }
    mico_rtos_unlock_mutex(&alarm_stream.stream_Mutex);
}

void set_alarm_stream_state(_stream_state_t state)
{
    mico_rtos_lock_mutex(&alarm_stream.stream_Mutex);
    alarm_stream.state = state;
    mico_rtos_unlock_mutex(&alarm_stream.stream_Mutex);
}
_stream_state_t get_alarm_stream_state(void)
{
    return alarm_stream.state;
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
    if (elsv_alarm_data->alarm_repeat == 0)
        get_alarm_utc_second(elsv_alarm_data);

    alarm_mcu_data->color = elsv_alarm_data->alarm_color;
    alarm_mcu_data->snooze_enabled = elsv_alarm_data->snooze_enabled;
    alarm_mcu_data->skip_flag = elsv_alarm_data->skip_flag;

    if (elsv_alarm_data->alarm_repeat == 1)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x7f;
    }
    else if (elsv_alarm_data->alarm_repeat == 2)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x3e;
    }
    else if (elsv_alarm_data->alarm_repeat == 3)
    {
        alarm_eland_data->alarm_on_days_of_week = 0x41;
    }
    else if (elsv_alarm_data->alarm_repeat == 4)
    {
        alarm_eland_data->alarm_on_days_of_week = 0;
        for (i = 0; i < 7; i++)
        {
            if (elsv_alarm_data->alarm_on_days_of_week[i] == '1')
                alarm_eland_data->alarm_on_days_of_week |= (1 << i);
        }
    }
    else if (elsv_alarm_data->alarm_repeat == 5)
    {
    }
    else
    {
        alarm_eland_data->alarm_on_days_of_week = 0;
    }
}
OSStatus elsv_alarm_data_init_MCU(_alarm_mcu_data_t *alarm_mcu_data)
{
    OSStatus err = kGeneralErr;
    __elsv_alarm_data_t alarm_data_cache;
    if (alarm_mcu_data == NULL)
        goto exit;
    memset(&alarm_data_cache, 0, sizeof(__elsv_alarm_data_t));
    memcpy(&alarm_data_cache.alarm_data_for_mcu, alarm_mcu_data, sizeof(_alarm_mcu_data_t));
    strcpy(alarm_data_cache.alarm_id, ALARM_ID_OF_SIMPLE_CLOCK);
    alarm_data_cache.alarm_color = 0;
    sprintf(alarm_data_cache.alarm_time, "%02d:%02d:%02d", alarm_mcu_data->moment_time.hr, alarm_mcu_data->moment_time.min, alarm_mcu_data->moment_time.sec);
    alarm_data_cache.snooze_enabled = 1;
    alarm_data_cache.snooze_count = 3;
    alarm_data_cache.snooze_interval_min = 10;
    alarm_data_cache.alarm_pattern = 4;
    alarm_data_cache.alarm_sound_id = 1;
    alarm_data_cache.alarm_volume = 80;
    alarm_data_cache.alarm_continue_min = 15;
    alarm_data_cache.alarm_repeat = 1;
    elsv_alarm_data_sort_out(&alarm_data_cache);
    err = alarm_list_clear(&alarm_list);
    require_noerr(err, exit);
    alarm_list_add(&alarm_list, &alarm_data_cache);
exit:
    return err;
}
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list)
{
    uint8_t i, j;
    __elsv_alarm_data_t *elsv_alarm_data = NULL;
    mico_utc_time_t utc_time = 0;
    __elsv_alarm_data_t elsv_alarm_data_temp;
    list->list_refreshed = false;
    list->state.alarm_stoped = false;
    set_alarm_state(ALARM_IDEL);
    alarm_log("alarm_number = %d", list->alarm_number);
    if (list->alarm_number == 0)
    {
        elsv_alarm_data = NULL;
        goto exit;
    }
    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_repeat)
            get_alarm_utc_second(list->alarm_lib + i);
    }
    /*****Sequence*****/
    for (i = 0; i < list->alarm_number - 1; i++)
    {
        for (j = 0; j < list->alarm_number - i - 1; j++)
        {
            if ((list->alarm_lib + j)->alarm_data_for_eland.moment_second >
                (list->alarm_lib + j + 1)->alarm_data_for_eland.moment_second)
            {
                memcpy(&elsv_alarm_data_temp, list->alarm_lib + j + 1, sizeof(__elsv_alarm_data_t));
                memcpy(list->alarm_lib + j + 1, list->alarm_lib + j, sizeof(__elsv_alarm_data_t));
                memcpy(list->alarm_lib + j, &elsv_alarm_data_temp, sizeof(__elsv_alarm_data_t));
            }
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

OSStatus alarm_sound_scan(void)
{
    OSStatus err = kNoErr;
    uint8_t scan_count = 0;
    uint8_t i;

scan_again:
    scan_count++;
    alarm_log("alarm_number:%d", alarm_list.alarm_number);
    for (i = 0; i < alarm_list.alarm_number; i++)
    {
        alarm_log("alarm:%d", i);
        if (((alarm_list.alarm_lib + i)->alarm_pattern == 1) ||
            ((alarm_list.alarm_lib + i)->alarm_pattern == 3))
        {
            alarm_log("SID:%ld", (alarm_list.alarm_lib + i)->alarm_sound_id);
            err = alarm_sound_download(alarm_list.alarm_lib + i, SOUND_FILE_SID);
            require_noerr(err, exit);
        }
        if (((alarm_list.alarm_lib + i)->alarm_pattern == 2) ||
            ((alarm_list.alarm_lib + i)->alarm_pattern == 3))
        {
            alarm_log("VID:%s", (alarm_list.alarm_lib + i)->voice_alarm_id);
            err = alarm_sound_download(alarm_list.alarm_lib + i, SOUND_FILE_VID);
            require_noerr(err, exit);
        }

        if (strlen((alarm_list.alarm_lib + i)->alarm_off_voice_alarm_id) > 30)
        {
            alarm_log("OFID:%s", (alarm_list.alarm_lib + i)->alarm_off_voice_alarm_id);
            err = alarm_sound_download(alarm_list.alarm_lib + i, SOUND_FILE_OFID);
            require_noerr(err, exit);
        }
    }
exit:
    if ((err != kNoErr) && (scan_count < 5))
    {
        mico_rtos_thread_sleep(5);
        goto scan_again;
    }
    return err;
}

OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;
    err = mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
    if (AlarmList->alarm_number == 0)
    {
        memcpy(AlarmList->alarm_lib, inData, sizeof(__elsv_alarm_data_t));
        AlarmList->alarm_number++;
    }
    else
    {
        for (i = 0; i < AlarmList->alarm_number; i++)
        {
            if (strcmp((AlarmList->alarm_lib + i)->alarm_id, inData->alarm_id) == 0)
            {
                memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, i * sizeof(__elsv_alarm_data_t));
                memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
                goto exit;
            }
        }
        /**a new alarm**/
        alarm_log("alarm_id is new");
        memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, AlarmList->alarm_number * sizeof(__elsv_alarm_data_t));
        memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
        AlarmList->alarm_number++;
    }
exit:
    alarm_log("alarm_add success!");
    AlarmList->list_refreshed = true;
    set_alarm_state(ALARM_ADD);
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

    for (i = 0; i < AlarmList->alarm_number; i++)
    {
        if (strcmp((AlarmList->alarm_lib + i)->alarm_id, inData->alarm_id) == 0)
        {
            alarm_log("get minus alarm_id");
            memmove(AlarmList->alarm_lib + i,
                    AlarmList->alarm_lib + i + 1,
                    (AlarmList->alarm_number - 1 - i) * sizeof(__elsv_alarm_data_t));
            AlarmList->alarm_number--;
            AlarmList->list_refreshed = true;
            break;
        }
    }
    if (AlarmList->alarm_number == 0)
        memset((uint8_t *)(AlarmList->alarm_lib), 0, ALARM_DATA_SIZE);

    err = mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
    alarm_log("unlock AlarmlibMutex");
exit:
    set_alarm_state(ALARM_MINUS);
    return err;
}
OSStatus alarm_list_clear(_eland_alarm_list_t *AlarmList)
{
    OSStatus err = kNoErr;
    alarm_log("lock AlarmlibMutex");
    err = mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);

    AlarmList->alarm_number = 0;
    AlarmList->list_refreshed = true;

    err = mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    require_noerr(err, exit);
    alarm_log("unlock AlarmlibMutex");
exit:
    set_alarm_state(ALARM_MINUS);
    return err;
}

static void alarm_operation(__elsv_alarm_data_t *alarm)
{
    _alarm_list_state_t current_state;
    int8_t snooze_count;
    mico_utc_time_t utc_time;
    mico_utc_time_t alarm_moment = alarm->alarm_data_for_eland.moment_second;
    bool first_to_snooze = true, first_to_alarming = true;
    mscp_result_t result;
    static uint8_t volume_value = 0;
    uint8_t volume_change_counter = 0;
    iso8601_time_t iso8601_time;
    uint8_t i = 0;
    uint8_t volume_stepup_count = 0;

    if (alarm->alarm_data_for_mcu.skip_flag)
    {
        alarm_list.state.alarm_stoped = true;
        mico_time_get_iso8601_time(&iso8601_time);
        alarm_off_history_record_time(ALARM_OFF_SKIP, &iso8601_time);
        goto exit;
    }

    for (i = 0; i < 32; i++)
        audio_service_volume_down(&result, 1);
    volume_value = 0;

    if (alarm->volume_stepup_enabled == 0)
    {
        for (i = 0; i < (alarm->alarm_volume * 32 / 100 + 1); i++)
        {
            audio_service_volume_up(&result, 1);
            volume_value++;
        }
    }

    if (alarm->snooze_enabled)
        snooze_count = alarm->snooze_count + 1;
    else
        snooze_count = 1;
    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
    set_alarm_state(ALARM_ING);
    snooze_count--;
    do
    {
        current_state = get_alarm_state();
        utc_time = GET_current_second();
        switch (current_state)
        {
        case ALARM_ING:
            if (first_to_alarming)
            {
                /**alarm on notice**/
                TCP_Push_MSG_queue(TCP_HT00_Sem);
                first_to_alarming = false;
                first_to_snooze = true;
            }
            if ((alarm->volume_stepup_enabled) &&
                (++volume_stepup_count > 10))
            {
                volume_stepup_count = 0;
                if ((volume_change_counter++ > 1) &&
                    (volume_value < (alarm->alarm_volume * 32 / 100 + 1)))
                {
                    volume_value++;
                    volume_change_counter = 0;
                    audio_service_volume_up(&result, 1);
                }
            }
            if (utc_time > alarm_moment)
            {
                mico_time_get_iso8601_time(&iso8601_time);
                alarm_off_history_record_time(ALARM_OFF_AUTOOFF, &iso8601_time);
                alarm_log("alarm_time up");
                set_alarm_state(ALARM_SNOOZ_STOP);
            }
            else
                Alarm_Play_Control(alarm, 1); //play with delay
            break;
        case ALARM_ADD:
        case ALARM_MINUS:
        case ALARM_SORT:
        case ALARM_SKIP:
            mico_time_get_iso8601_time(&iso8601_time);
            alarm_off_history_record_time(ALARM_OFF_ALARMOFF, &iso8601_time);
        case ALARM_STOP:
            Alarm_Play_Control(alarm, 0); //stop
            alarm_list.state.alarm_stoped = true;
            break;
        case ALARM_SNOOZ_STOP:
            if (snooze_count)
            {
                if (first_to_snooze)
                {
                    if (alarm->volume_stepup_enabled)
                    {
                        for (i = volume_value; i > 0; i--)
                            audio_service_volume_down(&result, 1);
                        volume_value = 0;
                    }
                    alarm_moment += (uint32_t)alarm->snooze_interval_min * 60;
                    mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)alarm_moment * 1000), &iso8601_time);
                    alarm_off_history_record_time(ALARM_SNOOZE, &iso8601_time);
                    /**add json for tcp**/
                    set_alarm_history_send_sem();
                    Alarm_Play_Control(alarm, 0); //stop
                    first_to_snooze = false;
                    first_to_alarming = true;
                }
                if (utc_time > alarm_moment)
                {
                    alarm_log("alarm_time on again %d", snooze_count);
                    set_alarm_state(ALARM_ING);
                    snooze_count--;
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
        mico_rtos_thread_msleep(50);
    } while (alarm_list.state.alarm_stoped == false);
exit:
    set_alarm_history_send_sem();
}
/***CMD = 1 PALY   CMD = 0 STOP***/
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD)
{
    mscp_result_t result;
    mscp_status_t audio_status;
    static bool isVoice = false;
    static bool voic_stoped = false;

    audio_service_get_audio_status(&result, &audio_status);
    if (CMD) //play
    {
        voic_stoped = false;
        if (get_alarm_stream_state() != STREAM_PLAY)
        {
            alarm_log("audio_status %d", audio_status);
            if (alarm->alarm_pattern == 1)
            {
                init_alarm_stream(alarm, SOUND_FILE_SID);
                set_alarm_stream_state(STREAM_PLAY);
                mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            }
            else if (alarm->alarm_pattern == 2)
            {
                init_alarm_stream(alarm, SOUND_FILE_VID);
                set_alarm_stream_state(STREAM_PLAY);
                mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            }
            else if (alarm->alarm_pattern == 3)
            {
                if (audio_status == MSCP_STATUS_IDLE)
                {
                    if (isVoice)
                    {
                        isVoice = false;
                        init_alarm_stream(alarm, SOUND_FILE_VID);
                        set_alarm_stream_state(STREAM_PLAY);
                        mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
                    }
                    else
                    {
                        isVoice = true;
                        init_alarm_stream(alarm, SOUND_FILE_SID);
                        set_alarm_stream_state(STREAM_PLAY);
                        mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
                    }
                }
            }
            else if (alarm->alarm_pattern == 4)
            {
                init_alarm_stream(alarm, SOUND_FILE_DEFAULT);
                set_alarm_stream_state(STREAM_PLAY);
                mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            }
        }
    }
    else //stop
    {
        if (get_alarm_stream_state() == STREAM_PLAY)
            set_alarm_stream_state(STREAM_STOP);
        if (audio_status != MSCP_STATUS_IDLE)
            audio_service_stream_stop(&result, alarm_stream.stream_id);
        if ((strlen(alarm->alarm_off_voice_alarm_id) > 30) && //alarm off oid
            !voic_stoped)
        {
            voic_stoped = true;
            do
            {
                mico_rtos_thread_msleep(200);
                audio_service_get_audio_status(&result, &audio_status);
            } while (audio_status != MSCP_STATUS_IDLE);
            init_alarm_stream(alarm, SOUND_FILE_OFID);
            set_alarm_stream_state(STREAM_PLAY);
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
        }
    }
}

static void play_voice(mico_thread_arg_t arg)
{
    _alarm_stream_t *stream = (_alarm_stream_t *)arg;
    AUDIO_STREAM_PALY_S flash_read_stream;
    mscp_status_t audio_status;
    OSStatus err = kGeneralErr;
    mscp_result_t result = MSCP_RST_ERROR;
    uint32_t data_pos = 0;
    uint8_t *flashdata = NULL;
    _sound_read_write_type_t alarm_w_r_queue;
    _stream_state_t stream_state;

    alarm_log("player_flash_thread");
    flashdata = malloc(SOUND_STREAM_DEFAULT_LENGTH + 1);
start_play_voice:
    stream_state = get_alarm_stream_state();
    switch (stream_state)
    {
    case STREAM_STOP:
        goto exit;
        break;
    case STREAM_PLAY:
        audio_service_get_audio_status(&result, &audio_status);
        if (audio_status == MSCP_STATUS_STREAM_PLAYING)
        {
            mico_rtos_thread_msleep(20);
            goto start_play_voice;
        }
        break;
    default:
        goto exit;
        break;
    }

    flash_read_stream.type = AUDIO_STREAM_TYPE_MP3;
    flash_read_stream.pdata = flashdata;
    flash_read_stream.stream_id = stream->stream_id;
    data_pos = 0;

    memset(&alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
    memcpy(alarm_w_r_queue.alarm_ID, stream->alarm_id, ALARM_ID_LEN + 1);
    alarm_w_r_queue.sound_type = stream->sound_type;
    alarm_w_r_queue.is_read = true;
    alarm_w_r_queue.sound_data = flashdata;
    alarm_w_r_queue.len = SOUND_STREAM_DEFAULT_LENGTH;
    alarm_log("len = %ld,pos = %ld",
              alarm_w_r_queue.total_len, alarm_w_r_queue.pos);
    stream->stream_count--;
falsh_read_start:
    alarm_w_r_queue.pos = data_pos;
    err = sound_file_read_write(&sound_file_list, &alarm_w_r_queue);
    require_noerr(err, exit);
    if (data_pos == 0)
    {
        flash_read_stream.total_len = alarm_w_r_queue.total_len;
    }
    flash_read_stream.stream_len = alarm_w_r_queue.len;

audio_transfer:
    if (get_alarm_stream_state() == STREAM_PLAY)
    {
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
    }
    if (stream->stream_count)
        goto start_play_voice;
exit:
    if (get_alarm_stream_state() == STREAM_STOP)
    {
        audio_service_get_audio_status(&result, &audio_status);
        if (audio_status != MSCP_STATUS_IDLE)
            audio_service_stream_stop(&result, alarm_stream.stream_id);
    }
    free(flashdata);
    set_alarm_stream_state(STREAM_IDEL);
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
    alarm_log("alarm_on_days_of_week:%s", alarm_data->alarm_on_days_of_week);
    alarm_log("\r\alarm_data_for_eland");
    alarm_log("moment_second:%ld", alarm_data->alarm_data_for_eland.moment_second);
    alarm_log("color:%d", alarm_data->alarm_color);
    alarm_log("snooze_count:%d", alarm_data->snooze_count);
    alarm_log("alarm_on_days_of_week:%d", alarm_data->alarm_data_for_eland.alarm_on_days_of_week);
    alarm_log("\r\n_alarm_mcu_data_t");
    alarm_log("color:%d", alarm_data->alarm_data_for_mcu.color);
    alarm_log("snooze_count:%d", alarm_data->alarm_data_for_mcu.snooze_enabled);
    alarm_log("alarm_on_days_of_week:%d", alarm_data->alarm_data_for_eland.alarm_on_days_of_week);
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
    eland_push_uart_send_queue(ALARM_SEND_0B);
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

uint8_t get_alarm_number(void)
{
    return alarm_list.alarm_number;
}

static void get_alarm_utc_second(__elsv_alarm_data_t *alarm)
{
    DATE_TIME_t date_time;
    mico_utc_time_t utc_time = 0;
    struct tm *currentTime;
    uint8_t week_day;
    int8_t i;

    utc_time = GET_current_second();
    currentTime = localtime((const time_t *)&utc_time);

    date_time.iYear = 1900 + currentTime->tm_year;
    date_time.iMon = (int16_t)(currentTime->tm_mon + 1);
    date_time.iDay = (int16_t)currentTime->tm_mday;
    date_time.iHour = (int16_t)alarm->alarm_data_for_mcu.moment_time.hr;
    date_time.iMin = (int16_t)alarm->alarm_data_for_mcu.moment_time.min;
    date_time.iSec = (int16_t)alarm->alarm_data_for_mcu.moment_time.sec;
    date_time.iMsec = 0;
    week_day = currentTime->tm_wday + 1;

    switch (alarm->alarm_repeat)
    {
    case 0: //只在新数据导入时候计算一次
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        if (alarm->alarm_data_for_eland.moment_second < utc_time)
            alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
        break;
    case 1:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        if (alarm->alarm_data_for_eland.moment_second < utc_time)
            alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
        break;
    case 2:
        i = 0;
        do
        {
            if (((week_day + i) == 1) ||
                ((week_day + i) == 7) ||
                today_is_holiday(&date_time, i))
            {
                i++;
                continue;
            }
            break;
        } while (1);
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + SECOND_ONE_DAY * i;
        alarm_log("utc_time:%ld", alarm->alarm_data_for_eland.moment_second);
        break;
    case 3:
        for (i = 0; i < 7; i++)
        {
            if ((((week_day + i - 1) % 7) == 0) || (((week_day + i - 1) % 7) == 6))
            {
                alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + i * SECOND_ONE_DAY;
                if (alarm->alarm_data_for_eland.moment_second < utc_time)
                    continue;
                else
                    break;
            }
        }
    case 4:
        for (i = 0; i < 7; i++)
        {
            if (alarm->alarm_data_for_eland.alarm_on_days_of_week & (1 << ((week_day + i - 1) % 7)))
            {
                alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time) + (i * SECOND_ONE_DAY);
                if (alarm->alarm_data_for_eland.moment_second < utc_time)
                    continue;
                else
                    break;
            }
        }
        break;
    case 5:
        alarm_log("utc_time:%ld", utc_time);
        for (i = 0; i < ALARM_ON_DATES_COUNT; i++)
        {
            if (alarm->alarm_on_dates[i] == 0)
                break;
            alarm_log("alarm_on_dates%d:%d", i, alarm->alarm_on_dates[i]);
            date_time.iYear = 2000 + alarm->alarm_on_dates[i] / SIMULATE_DAYS_OF_YEAR;
            date_time.iDay = alarm->alarm_on_dates[i] % SIMULATE_DAYS_OF_YEAR;
            date_time.iMon = date_time.iDay / SIMULATE_DAYS_OF_MONTH;
            date_time.iDay = date_time.iDay % SIMULATE_DAYS_OF_MONTH;

            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
            if (alarm->alarm_data_for_eland.moment_second > utc_time)
                break;
            alarm_log("date:%d-%02d-%02d", date_time.iYear, date_time.iMon, date_time.iDay);
            alarm_log("moment_second:%ld", alarm->alarm_data_for_eland.moment_second);
        }
        break;
    default:
        alarm->alarm_data_for_eland.moment_second = 0;
        break;
    }
    /*******Calculate date for mcu*********/
    UCT_Convert_Date(&(alarm->alarm_data_for_eland.moment_second), &(alarm->alarm_data_for_mcu.moment_time));
    if ((utc_time < alarm->alarm_data_for_eland.moment_second) &&
        ((alarm->alarm_data_for_eland.moment_second - utc_time) < SECOND_ONE_DAY))
        alarm->alarm_data_for_mcu.next_alarm = 1;
    else
        alarm->alarm_data_for_mcu.next_alarm = 0;
    alarm_log("%04d-%02d-%02d %02d:%02d:%02d",
              alarm->alarm_data_for_mcu.moment_time.year + 2000,
              alarm->alarm_data_for_mcu.moment_time.month,
              alarm->alarm_data_for_mcu.moment_time.date,
              alarm->alarm_data_for_mcu.moment_time.hr,
              alarm->alarm_data_for_mcu.moment_time.min,
              alarm->alarm_data_for_mcu.moment_time.sec);
    alarm_log("alarm_id:%s", alarm->alarm_id);
    alarm_log("moment_second:%ld", alarm->alarm_data_for_eland.moment_second);
}

void alarm_off_history_record_time(alarm_off_history_record_t type, iso8601_time_t *iso8601_time)
{
    uint8_t alarm_serial;
    if (get_alarm_history_data_state() == WAIT_UPLOAD)
        return;
    mico_rtos_lock_mutex(&off_history.off_Mutex);
    switch (type)
    {
    case ALARM_ON:
        memset(&off_history.HistoryData, 0, sizeof(AlarmOffHistoryData_t));
        alarm_serial = get_waiting_alarm_serial();
        memcpy(off_history.HistoryData.alarm_id,
               (alarm_list.alarm_lib + alarm_serial)->alarm_id, ALARM_ID_LEN);
        memcpy(&off_history.HistoryData.alarm_on_datetime, iso8601_time, 19);
        off_history.state = IDEL_UPLOAD;
        break;
    case ALARM_OFF_SNOOZE:
    case ALARM_OFF_ALARMOFF:
    case ALARM_OFF_AUTOOFF:
        memcpy(&off_history.HistoryData.alarm_off_datetime, iso8601_time, 19);
        off_history.HistoryData.alarm_off_reason = type;
        off_history.state = READY_UPLOAD;
        break;
    case ALARM_OFF_SKIP:
        memset(&off_history.HistoryData, 0, sizeof(AlarmOffHistoryData_t));
        alarm_serial = get_waiting_alarm_serial();
        memcpy(off_history.HistoryData.alarm_id,
               (alarm_list.alarm_lib + alarm_serial)->alarm_id, ALARM_ID_LEN);
        memcpy(&off_history.HistoryData.alarm_on_datetime, iso8601_time, 19);
        off_history.HistoryData.alarm_off_reason = type;
        off_history.state = READY_UPLOAD;
        break;
    case ALARM_SNOOZE:
        memcpy(&off_history.HistoryData.snooze_datetime, iso8601_time, 19);
        break;

    default:
        break;
    }
    mico_rtos_unlock_mutex(&off_history.off_Mutex);
}

OSStatus alarm_off_history_json_data_build(AlarmOffHistoryData_t *HistoryData, char *json_buff)
{
    OSStatus err = kNoErr;
    json_object *TempJsonData = NULL;
    json_object *AlarmOffHistoryData = NULL;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;

    TempJsonData = json_object_new_object();
    require_action_string(TempJsonData, exit, err = kNoMemoryErr, "create TempJsonData object error!");
    AlarmOffHistoryData = json_object_new_object();
    require_action_string(AlarmOffHistoryData, exit, err = kNoMemoryErr, "create AlarmOffHistoryData object error!");

    alarm_log("Begin add AlarmOffHistoryData object");
    json_object_object_add(AlarmOffHistoryData, "alarm_id", json_object_new_string(HistoryData->alarm_id));
    json_object_object_add(AlarmOffHistoryData, "alarm_on_datetime", json_object_new_string((char *)&HistoryData->alarm_on_datetime));
    json_object_object_add(AlarmOffHistoryData, "alarm_off_datetime", json_object_new_string((char *)&HistoryData->alarm_off_datetime));
    json_object_object_add(AlarmOffHistoryData, "alarm_off_reason", json_object_new_int(HistoryData->alarm_off_reason));
    json_object_object_add(AlarmOffHistoryData, "snooze_datetime", json_object_new_string((char *)&HistoryData->snooze_datetime));
    json_object_object_add(TempJsonData, "AlarmOffHistoryData", AlarmOffHistoryData);

    generate_data = json_object_to_json_string(TempJsonData);
    require_action_string(generate_data != NULL, exit, err = kNoMemoryErr, "create generate_data string error!");
    generate_data_len = strlen(generate_data);
    memcpy(json_buff, generate_data, generate_data_len);
    // alarm_log("history_json_buff:%s", json_buff);
exit:
    free_json_obj(&TempJsonData);
    free_json_obj(&AlarmOffHistoryData);
    return err;
}
HistoryDatastate_t get_alarm_history_data_state(void)
{
    return off_history.state;
}
void set_alarm_history_data_state(HistoryDatastate_t value)
{
    mico_rtos_lock_mutex(&off_history.off_Mutex);
    off_history.state = value;
    mico_rtos_unlock_mutex(&off_history.off_Mutex);
}
AlarmOffHistoryData_t *get_alarm_history_data(void)
{
    return &off_history.HistoryData;
}
/**
 ****************************************************************************
 * @Function : RTC_Weekday_TypeDef CaculateWeekDay(int y, int m, int d)
 * @File     : rtc.c
 * @Program  : y:year m:month d:day
 * @Created  : 2017/12/16 by seblee
 * @Brief    : Caculat get week
 * @Version  : V1.0
**/
uint8_t CaculateWeekDay(int y, int m, int d)
{
    int iWeek;
    if (m == 1 || m == 2)
    {
        m += 12;
        y--;
    }
    iWeek = (d + 2 * m + 3 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
    return (iWeek < 6) ? (uint8_t)(iWeek + 1) : 0;
}
/**
 ****************************************************************************
 * @Function : void Alarm_build_JSON(char *json_str)
 * @File     : rtc.c
 * @Program  : y:year m:month d:day
 * @Created  : 2017/12/16 by seblee
 * @Brief    : Caculat get week
 * @Version  : V1.0
**/
OSStatus Alarm_build_JSON(char *json_str)
{
    OSStatus err = kNoErr;
    json_object *Json = NULL;
    json_object *AlarmJson = NULL;
    json_object *AlarmOnDaysJson = NULL;
    uint8_t alarm_now_serial;
    __elsv_alarm_data_t *alarm_now;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    uint8_t i;
    char stringbuf[20];

    Json = json_object_new_object();
    alarm_now_serial = get_waiting_alarm_serial();
    alarm_now = (alarm_list.alarm_lib + alarm_now_serial);
    require_action_string(Json, exit, err = kNoMemoryErr, "create Json object error!");
    AlarmJson = json_object_new_object();
    require_action_string(AlarmJson, exit, err = kNoMemoryErr, "create AlarmJson object error!");
    AlarmOnDaysJson = json_object_new_array();
    require_action_string(AlarmJson, exit, err = kNoMemoryErr, "create AlarmJson object error!");

    alarm_log("Begin add AlarmJson object");
    json_object_object_add(AlarmJson, "alarm_id", json_object_new_string(alarm_now->alarm_id));
    json_object_object_add(AlarmJson, "alarm_color", json_object_new_int(alarm_now->alarm_color));
    json_object_object_add(AlarmJson, "alarm_time", json_object_new_string(alarm_now->alarm_time));
    json_object_object_add(AlarmJson, "alarm_off_dates", json_object_new_string(alarm_now->alarm_off_dates));
    json_object_object_add(AlarmJson, "snooze_enabled", json_object_new_int(alarm_now->snooze_enabled));
    json_object_object_add(AlarmJson, "snooze_count", json_object_new_int(alarm_now->snooze_count));
    json_object_object_add(AlarmJson, "snooze_interval_min", json_object_new_int(alarm_now->snooze_interval_min));
    json_object_object_add(AlarmJson, "alarm_pattern", json_object_new_int(alarm_now->alarm_pattern));
    json_object_object_add(AlarmJson, "alarm_sound_id", json_object_new_int(alarm_now->alarm_sound_id));

    json_object_object_add(AlarmJson, "voice_alarm_id", json_object_new_string(alarm_now->voice_alarm_id));
    json_object_object_add(AlarmJson, "alarm_off_voice_alarm_id", json_object_new_string(alarm_now->alarm_off_voice_alarm_id));
    json_object_object_add(AlarmJson, "alarm_volume", json_object_new_int(alarm_now->alarm_volume));
    json_object_object_add(AlarmJson, "volume_stepup_enabled", json_object_new_int(alarm_now->volume_stepup_enabled));
    json_object_object_add(AlarmJson, "alarm_continue_min", json_object_new_int(alarm_now->alarm_continue_min));
    json_object_object_add(AlarmJson, "alarm_repeat", json_object_new_int(alarm_now->alarm_repeat));
    for (i = 0; i < ALARM_ON_DATES_COUNT; i++)
    {
        if (alarm_now->alarm_on_dates[i] == 0)
            break;
        memset(stringbuf, 0, 20);
        sprintf(stringbuf, "%d-%02d-%02d",
                alarm_now->alarm_on_dates[i] / SIMULATE_DAYS_OF_YEAR + 2000,
                alarm_now->alarm_on_dates[i] % SIMULATE_DAYS_OF_YEAR / 32,
                alarm_now->alarm_on_dates[i] % SIMULATE_DAYS_OF_YEAR % 32);
        json_object_array_add(AlarmOnDaysJson, json_object_new_string(stringbuf));
    }
    json_object_object_add(AlarmJson, "alarm_on_dates", AlarmOnDaysJson);
    json_object_object_add(AlarmJson, "alarm_on_days_of_week", json_object_new_string(alarm_now->alarm_on_days_of_week));
    json_object_object_add(AlarmJson, "skip_flag", json_object_new_int(alarm_now->skip_flag));

    json_object_object_add(Json, "alarm", AlarmJson);

    generate_data = json_object_to_json_string(Json);
    require_action_string(generate_data != NULL, exit, err = kNoMemoryErr, "create generate_data string error!");
    generate_data_len = strlen(generate_data);
    memcpy(json_str, generate_data, generate_data_len);

exit:
    free_json_obj(&Json);
    return err;
}
/**
 ****************************************************************************
 * @Function : static bool today_is_holiday(DATE_TIME_t *date, uint8_t offset)
 * @File     : eland_alarm.c
 * @Program  : date:today
 *             offset: =0 today  =1 tomorrow.....
 * @Created  : 2018-3-14 by seblee
 * @Brief    :  
 * @Version  : V1.0
**/
static bool today_is_holiday(DATE_TIME_t *date, uint8_t offset)
{
    uint16_t Cache = 0;
    uint8_t i;
    Cache = (date->iYear % 100) * SIMULATE_DAYS_OF_YEAR +
            date->iMon * SIMULATE_DAYS_OF_MONTH +
            date->iDay +
            offset;
    for (i = 0; i < holiday_list.number; i++)
    {
        if (Cache == *(holiday_list.list + i))
            return true;
    }

    return false;
}

void UCT_Convert_Date(uint32_t *utc, mico_rtc_time_t *time)
{
    struct tm *currentTime;
    currentTime = localtime((const time_t *)utc);
    time->sec = currentTime->tm_sec;
    time->min = currentTime->tm_min;
    time->hr = currentTime->tm_hour;
    time->date = currentTime->tm_mday;
    time->weekday = currentTime->tm_wday + 1;
    time->month = currentTime->tm_mon + 1;
    time->year = (currentTime->tm_year + 1900) % 100;
}

static void combing_alarm(_eland_alarm_list_t *list, __elsv_alarm_data_t **alarm_nearest)
{
    OSStatus err = kGeneralErr;
    // err = mico_rtos_get_semaphore(&alarm_skip_sem, 0);
    // if (err == kNoErr)
    // {
    //     if (list->alarm_skip_flag)
    //     {
    //         set_alarm_state(ALARM_IDEL);
    //         list->alarm_skip_flag = false;
    //     }
    //     else
    //     {
    //         set_alarm_state(ALARM_SKIP);
    //         list->alarm_skip_flag = true;
    //     }
    //     if (*alarm_nearest)
    //     {
    //         (*alarm_nearest)->alarm_data_for_mcu.skip_flag = get_alarm_skip_flag();
    //         (*alarm_nearest)->skip_flag = get_alarm_skip_flag();
    //         if ((*alarm_nearest)->skip_flag == 1)
    //             TCP_Push_MSG_queue(TCP_HT02_Sem);
    //         else
    //             TCP_Push_MSG_queue(TCP_HT02_Sem);
    //     }
    //     eland_push_uart_send_queue(ALARM_SEND_0B);
    // }
    err = mico_rtos_get_semaphore(&alarm_update, 500);
    if (list->list_refreshed ||
        list->state.alarm_stoped ||
        (err == kNoErr))
    {
        /**refresh point**/
        alarm_log("list_refreshed");
        if (list->list_refreshed || list->state.alarm_stoped)
            list->alarm_skip_flag = false;
        *alarm_nearest = Alarm_ergonic_list(list);

        if (*alarm_nearest)
        {
            eland_push_http_queue(DOWNLOAD_SCAN);
            set_display_alarm_serial(get_waiting_alarm_serial());
            alarm_log("get alarm_nearest alarm_id:%s", (*alarm_nearest)->alarm_id);
        }
        else
        {
            alarm_log("NO alarm_nearest ");
            set_display_alarm_serial(0);
        }
    }
}
uint8_t get_alarm_skip_flag(void)
{
    if (alarm_list.alarm_skip_flag)
        return (uint8_t)0x01;
    else
        return (uint8_t)0x00;
}
OSStatus alarm_sound_oid(void)
{
    OSStatus err;
    uint8_t i;
    char uri_str[100] = {0};
    mscp_result_t result = MSCP_RST_ERROR;
    mscp_status_t audio_status;
    uint8_t oid_volume = 0;

    set_alarm_stream_state(STREAM_PLAY);
    for (i = 0; i < 33; i++)
        audio_service_volume_down(&result, 1);
    oid_volume = get_notification_volume();
    for (i = 0; i < (oid_volume * 32 / 100 + 1); i++)
    {
        audio_service_volume_up(&result, 1);
    }

    /******************/
    sprintf(uri_str, ELAND_SOUND_OID_URI, netclock_des_g->eland_id, (uint32_t)1);
    alarm_log("uri_str:%s", uri_str);
    err = eland_http_file_download(ELAND_DOWN_LOAD_METHOD, uri_str, ELAND_HTTP_DOMAIN_NAME, NULL, DOWNLOAD_OID);
    require_noerr(err, exit);
    /******************/
check_audio:
    require_action_string(get_alarm_stream_state() != STREAM_STOP, exit, err = kGeneralErr, "user set stoped!");
    audio_service_get_audio_status(&result, &audio_status);
    if (audio_status == MSCP_STATUS_STREAM_PLAYING)
    {
        mico_rtos_thread_msleep(500);
        alarm_log("oid playing");
        goto check_audio;
    }
exit:
    if (audio_status != MSCP_STATUS_IDLE)
    {
        alarm_log("stop playing");
        audio_service_stream_stop(&result, alarm_stream.stream_id);
    }
    set_alarm_stream_state(STREAM_IDEL);
    alarm_log("play stopped");

    return err;
}

static OSStatus set_alarm_history_send_sem(void)
{
    OSStatus err = kNoErr;
    if (get_alarm_history_data_state() == READY_UPLOAD)
    {
        set_alarm_history_data_state(WAIT_UPLOAD);
        TCP_Push_MSG_queue(TCP_HT01_Sem);
    }
    return err;
}
OSStatus check_default_sound(void)
{
    OSStatus err = kNoErr;
    __elsv_alarm_data_t alarm_simple;
check_start:
    alarm_log("check default sound");
    err = SOUND_CHECK_DEFAULT_FILE();
    if (err != kNoErr)
    {
        alarm_log("SOUND_FILE_CLEAR");
        SOUND_FILE_CLEAR();
        memset(&alarm_simple, 0, sizeof(__elsv_alarm_data_t));
        alarm_log("download default file");
        err = alarm_sound_download(&alarm_simple, SOUND_FILE_DEFAULT);
        require_noerr(err, exit);
        goto check_start;
    }
exit:
    if (err == kNoErr)
        alarm_log("check ok");
    return err;
}

void eland_push_http_queue(_download_type_t msg)
{
    _download_type_t http_msg = msg;
    mico_rtos_push_to_queue(&http_queue, &http_msg, 10);
}
