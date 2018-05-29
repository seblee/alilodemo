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
static bool today_is_alarm_off_day(DATE_TIME_t *date, uint8_t offset, __elsv_alarm_data_t *alarm);
static void combing_alarm(_eland_alarm_list_t *list, __elsv_alarm_data_t **alarm_nearest, _alarm_list_state_t state);
static bool eland_alarm_is_same(__elsv_alarm_data_t *alarm1, __elsv_alarm_data_t *alarm2);
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

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager", Alarm_Manager, 0x800, 0);
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    _alarm_list_state_t alarm_state;
    mico_utc_time_t utc_time = 0;
    OSStatus err;
    uint8_t i;
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < alarm_list.alarm_number; i++)
        elsv_alarm_data_sort_out(alarm_list.alarm_lib + i);
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);

    alarm_log("start Alarm_Manager ");
    alarm_list.list_refreshed = true;

    while (1)
    {
        err = mico_rtos_get_semaphore(&alarm_update, 500);
        alarm_state = get_alarm_state();
        if ((alarm_state == ALARM_ADD) ||
            (alarm_state == ALARM_MINUS) ||
            (alarm_state == ALARM_SORT) ||
            (alarm_state == ALARM_ING) ||
            (alarm_state == ALARM_SNOOZ_STOP) ||
            (alarm_state == ALARM_STOP) ||
            (alarm_state == ALARM_SKIP) ||
            (err == kNoErr))
            combing_alarm(&alarm_list, &(alarm_list.alarm_nearest), alarm_state);
        else if ((alarm_state == ALARM_IDEL) &&
                 (alarm_list.alarm_nearest))
        {
            utc_time = GET_current_second();
            if (((alarm_list.alarm_nearest->alarm_data_for_eland.moment_second - utc_time) < 300) &&
                (alarm_list.alarm_nearest->skip_flag == 0) &&
                alarm_list.weather_need_refreshed)
            {
                alarm_list.weather_need_refreshed = false;
                alarm_log("##### DOWNLOAD_WEATHER ######");
                eland_push_http_queue(DOWNLOAD_WEATHER);
            }

            if (utc_time >= alarm_list.alarm_nearest->alarm_data_for_eland.moment_second)
            {
                alarm_log("##### start alarm ######");
                alarm_operation(alarm_list.alarm_nearest);
                /**operation output alarm**/
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
    bool Alarm_refresh_flag = false;
    if (state == ALARM_ING)
    {
        mico_time_get_iso8601_time(&iso8601_time);
        alarm_off_history_record_time(ALARM_ON, &iso8601_time);
    }
    if ((alarm_list.state.state == ALARM_ING) ||
        (alarm_list.state.state == ALARM_SNOOZ_STOP) ||
        (state == ALARM_ING) ||
        (state == ALARM_SNOOZ_STOP))
    {
        alarm_log("##%d# alarm state refreshed #%d##", alarm_list.state.state, state);
        Alarm_refresh_flag = true;
    }
    mico_rtos_lock_mutex(&alarm_list.state.AlarmStateMutex);
    alarm_list.state.state = state;
    mico_rtos_unlock_mutex(&alarm_list.state.AlarmStateMutex);
    if (Alarm_refresh_flag)
        eland_push_uart_send_queue(ALARM_SEND_0B);
}
static void init_alarm_stream(__elsv_alarm_data_t *alarm, uint8_t sound_type)
{
    OSStatus err = kGeneralErr;
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
        memcpy(alarm_stream.alarm_id, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
        break;
    case SOUND_FILE_DEFAULT:
        memcpy(alarm_stream.alarm_id, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK));
        break;
    default:
        break;
    }
    err = alarm_sound_file_check(alarm_stream.alarm_id);
    if (err != kNoErr)
    {
        memset(alarm_stream.alarm_id, 0, ALARM_ID_LEN + 1);
        strncpy(alarm_stream.alarm_id, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK));
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
    _alarm_mcu_data_t *alarm_mcu_data = NULL;
    int ho, mi, se;
    //  set_alarm_state(ALARM_SORT);
    if (elsv_alarm_data == NULL)
        return;
    alarm_mcu_data = &(elsv_alarm_data->alarm_data_for_mcu);
    sscanf((const char *)(elsv_alarm_data->alarm_time), "%02d:%02d:%02d", &ho, &mi, &se);

    alarm_mcu_data->moment_time.hr = ho;
    alarm_mcu_data->moment_time.min = mi;
    alarm_mcu_data->moment_time.sec = se;
    if (elsv_alarm_data->alarm_repeat == 0)
        get_alarm_utc_second(elsv_alarm_data);

    alarm_mcu_data->alarm_color = elsv_alarm_data->alarm_color;
    alarm_mcu_data->snooze_enabled = elsv_alarm_data->snooze_enabled;
    alarm_mcu_data->skip_flag = elsv_alarm_data->skip_flag;
}
OSStatus elsv_alarm_data_init_MCU(_alarm_mcu_data_t *alarm_mcu_data)
{
    OSStatus err = kGeneralErr;
    __elsv_alarm_data_t alarm_data_cache;
    if (alarm_mcu_data == NULL)
        goto exit;
    memset(&alarm_data_cache, 0, sizeof(__elsv_alarm_data_t));
    memcpy(&alarm_data_cache.alarm_data_for_mcu, alarm_mcu_data, sizeof(_alarm_mcu_data_t));
    strcpy(alarm_data_cache.alarm_id, ALARM_ID_OF_DEFAULT_CLOCK);
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
    alarm_list.list_refreshed = true;
    set_alarm_state(ALARM_ADD);
exit:
    return err;
}
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list)
{
    uint8_t i, j;
    __elsv_alarm_data_t *elsv_alarm_data = NULL;
    __elsv_alarm_data_t *near_alarm_data = NULL;
    mico_utc_time_t utc_time = 0;
    __elsv_alarm_data_t elsv_alarm_data_temp;
    list->list_refreshed = false;
    list->state.alarm_stoped = false;
    list->weather_need_refreshed = true;
    alarm_log("alarm_number = %d", list->alarm_number);
    list->alarm_now_serial = 0;
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
    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second <= utc_time)
            continue;
        else
        {
            list->alarm_now_serial = i;
            elsv_alarm_data = list->alarm_lib + i;
            break;
        }
    }
    if (elsv_alarm_data == NULL)
    {
        elsv_alarm_data = NULL;
        goto exit;
    }

    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second <= utc_time)
            continue;
        if ((list->alarm_lib + i)->alarm_data_for_eland.moment_second <
            elsv_alarm_data->alarm_data_for_eland.moment_second)
        {
            list->alarm_now_serial = i;
            elsv_alarm_data = list->alarm_lib + i;
        }
    }

exit:
    if (elsv_alarm_data)
    {
        near_alarm_data = calloc(sizeof(__elsv_alarm_data_t), sizeof(uint8_t));
        memcpy(near_alarm_data, elsv_alarm_data, sizeof(__elsv_alarm_data_t));
    }

    return near_alarm_data;
}

OSStatus alarm_sound_scan(void)
{
    OSStatus err = kNoErr;
    uint8_t scan_count = 0;
    uint8_t i, temp_point = 0;

    alarm_log("alarm_number:%d", alarm_list.alarm_number);
    if (alarm_list.alarm_number == 0)
        return err;
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    for (i = alarm_list.alarm_now_serial; i < (alarm_list.alarm_now_serial + 3); i++)
    {
        temp_point = i % alarm_list.alarm_number;
        if (((alarm_list.alarm_lib + temp_point)->alarm_pattern == 1) ||
            ((alarm_list.alarm_lib + temp_point)->alarm_pattern == 3))
        {
            // alarm_log("alarm:%d,SID:%ld", temp_point, (alarm_list.alarm_lib + temp_point)->alarm_sound_id);
            scan_count = 0;
        download_sid:
            err = alarm_sound_download(alarm_list.alarm_lib + temp_point, SOUND_FILE_SID);
            if ((err != kNoErr) && (scan_count++ < 3))
            {
                mico_rtos_thread_msleep(500);
                goto download_sid;
            }
        }
        if (((alarm_list.alarm_lib + temp_point)->alarm_pattern == 2) ||
            ((alarm_list.alarm_lib + temp_point)->alarm_pattern == 3))
        {
            // alarm_log("alarm:%d,VID:%s", temp_point, (alarm_list.alarm_lib + temp_point)->voice_alarm_id);
            if (strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "ffffffff") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "eeeeeeee") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "00000000"))
            {
            }
            else
            {
                scan_count = 0;
            download_vid:
                err = alarm_sound_download(alarm_list.alarm_lib + temp_point, SOUND_FILE_VID);
                if ((err != kNoErr) && (scan_count++ < 3))
                {
                    mico_rtos_thread_msleep(500);
                    goto download_vid;
                }
            }
        }
        if (strlen((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id) > 20)
        {
            // alarm_log("alarm:%d,OFID:%s", temp_point, (alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id);
            if (strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "ffffffff") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "eeeeeeee") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "00000000"))
            {
            }
            else
            {
                scan_count = 0;
            download_ofid:
                err = alarm_sound_download(alarm_list.alarm_lib + temp_point, SOUND_FILE_OFID);
                if ((err != kNoErr) && (scan_count++ < 3))
                {
                    mico_rtos_thread_msleep(500);
                    goto download_ofid;
                }
            }
        }
    }
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    return err;
}

OSStatus weather_sound_scan(void)
{
    OSStatus err = kNoErr;
    uint8_t scan_count = 0, sound_type;
    __elsv_alarm_data_t *nearest = NULL;
    nearest = get_nearest_alarm();
    require_quiet(nearest, exit);

    //   alarm_log("nearest_id:%s", nearest->alarm_id);

    if ((nearest->alarm_pattern == 2) ||
        (nearest->alarm_pattern == 3))
    {
        if (strstr(nearest->voice_alarm_id, "ffffffff"))
            sound_type = SOUND_FILE_WEATHER_F;
        else if (strstr(nearest->voice_alarm_id, "00000000"))
            sound_type = SOUND_FILE_WEATHER_0;
        else
            goto checkout_ofid;
        alarm_log("VID:%s", nearest->voice_alarm_id);
        sprintf(nearest->voice_alarm_id + 24, "%ldww", nearest->alarm_data_for_eland.moment_second);
        scan_count = 0;
    weather_vid:
        err = alarm_sound_download(nearest, sound_type);
        if ((err != kNoErr) && (scan_count++ < 3))
        {
            mico_rtos_thread_msleep(500);
            goto weather_vid;
        }
    }
checkout_ofid:

    if (strlen(nearest->alarm_off_voice_alarm_id) > 10)
    {
        alarm_log("VID:%s", nearest->alarm_off_voice_alarm_id);
        if (strstr(nearest->alarm_off_voice_alarm_id, "ffffffff"))
            sound_type = SOUND_FILE_WEATHER_F;
        else if (strstr(nearest->alarm_off_voice_alarm_id, "eeeeeeee"))
            sound_type = SOUND_FILE_WEATHER_E;
        else if (strstr(nearest->alarm_off_voice_alarm_id, "00000000"))
            sound_type = SOUND_FILE_WEATHER_0;
        else
            goto exit;
        sprintf(nearest->alarm_off_voice_alarm_id + 24, "%ldww", nearest->alarm_data_for_eland.moment_second);
        scan_count = 0;
    weather_ofid:
        err = alarm_sound_download(nearest, sound_type);
        if ((err != kNoErr) && (scan_count++ < 3))
        {
            mico_rtos_thread_msleep(500);
            goto weather_ofid;
        }
    }

exit:
    return err;
}

OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;
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
                if (eland_alarm_is_same(AlarmList->alarm_lib + i, inData))
                {
                    alarm_log("duplicate data");
                    err = kGeneralErr;
                    goto exit_nothing_done;
                }
                alarm_log("alarm change");
                memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, i * sizeof(__elsv_alarm_data_t));
                memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
                goto exit;
            }
        }
        /**a new alarm**/
        alarm_log("alarm_id add");
        memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, AlarmList->alarm_number * sizeof(__elsv_alarm_data_t));
        memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
        AlarmList->alarm_number++;
    }
exit:
    alarm_log("alarm_add success!");

exit_nothing_done:
    return err;
}
OSStatus alarm_list_minus(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i;

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

    if (alarm->snooze_enabled)
        snooze_count = alarm->snooze_count + 1;
    else
        snooze_count = 1;
    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
    set_alarm_state(ALARM_ING);

    do
    {
        current_state = get_alarm_state();
        utc_time = GET_current_second();
        if (utc_time < alarm->alarm_data_for_eland.moment_second)
            goto exit;
        switch (current_state)
        {
        case ALARM_ING:
            if (first_to_alarming)
            {
                snooze_count--;
                first_to_alarming = false;
                first_to_snooze = true;
                if (utc_time >= alarm_moment)
                {
                    set_alarm_state(ALARM_SNOOZ_STOP);
                    break;
                }
                /**alarm on notice**/
                TCP_Push_MSG_queue(TCP_HT00_Sem);
                for (i = volume_value; i > 0; i--)
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
            if (utc_time >= alarm_moment)
            {
                mico_time_get_iso8601_time(&iso8601_time);
                alarm_off_history_record_time(ALARM_OFF_AUTOOFF, &iso8601_time);
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
            alarm_log("Alarm_Play_Control: ALARM_STOP");
            Alarm_Play_Control(alarm, 0); //stop
            goto exit;
            break;
        case ALARM_SNOOZ_STOP:
            if (snooze_count)
            {
                if (first_to_snooze)
                {
                    first_to_snooze = false;
                    first_to_alarming = true;
                    alarm_moment = utc_time + (uint32_t)alarm->snooze_interval_min * 60;
                    Alarm_Play_Control(alarm, 0); //stop
                    if (utc_time >= alarm_moment)
                    {
                        set_alarm_state(ALARM_ING);
                        break;
                    }
                    alarm_log("time up alarm_moment:%ld", alarm_moment);
                    mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)alarm_moment * 1000), &iso8601_time);
                    alarm_off_history_record_time(ALARM_SNOOZE, &iso8601_time);
                    /**add json for tcp**/
                    set_alarm_history_send_sem();
                    alarm_log("Alarm_Play_Control: first_to_snooze");
                }
                if (utc_time >= alarm_moment)
                {
                    alarm_moment = utc_time + (uint32_t)alarm->alarm_continue_min * 60;
                    alarm_log("alarm_time on again %d", snooze_count);
                    set_alarm_state(ALARM_ING);
                }
            }
            else
                set_alarm_state(ALARM_STOP);
            break;
        default:
            goto exit;
            break;
        }
        mico_rtos_thread_msleep(50);
    } while (1);
exit:
    set_alarm_history_send_sem();
}
/***CMD = 1 PALY   CMD = 0 STOP***/
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD)
{
    mscp_result_t result;
    mscp_status_t audio_status;
    static bool isVoice = false;
    static uint8_t CMD_bak = 0;

    audio_service_get_audio_status(&result, &audio_status);
    if ((CMD == 1) && (get_alarm_stream_state() != STREAM_PLAY)) //play
    {
        alarm_log("audio_status %d", audio_status);
        if (alarm->alarm_pattern == 1)
        {
            init_alarm_stream(alarm, SOUND_FILE_SID);
            set_alarm_stream_state(STREAM_PLAY);
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            isVoice = false;
        }
        else if (alarm->alarm_pattern == 2)
        {
            init_alarm_stream(alarm, SOUND_FILE_VID);
            set_alarm_stream_state(STREAM_PLAY);
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            isVoice = false;
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
        else if (alarm->alarm_pattern == 4) //default alarm
        {
            init_alarm_stream(alarm, SOUND_FILE_DEFAULT);
            set_alarm_stream_state(STREAM_PLAY);
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
            isVoice = false;
        }
    }
    else if ((CMD == 0) && (CMD_bak == 1)) //stop
    {
        isVoice = false;
        alarm_log("player_flash_thread");
        if (get_alarm_stream_state() == STREAM_PLAY)
            set_alarm_stream_state(STREAM_STOP);
        if (strlen(alarm->alarm_off_voice_alarm_id) > 10) //alarm off oid
        {
            alarm_log("wait for sudio stoped");
            do
            {
                mico_rtos_thread_msleep(200);
                audio_service_get_audio_status(&result, &audio_status);
            } while ((get_alarm_stream_state() != STREAM_IDEL) &&
                     (audio_status != MSCP_STATUS_IDLE));
            alarm_log("*****sudio stoped");
            init_alarm_stream(alarm, SOUND_FILE_OFID);
            set_alarm_stream_state(STREAM_PLAY);
            alarm_log("start play ofid");
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
        }
    }
    CMD_bak = CMD;
}

static void play_voice(mico_thread_arg_t arg)
{
    _alarm_stream_t *stream = (_alarm_stream_t *)arg;
    AUDIO_STREAM_PALY_S flash_read_stream;
    mscp_status_t audio_status;
    OSStatus err = kNoErr;
    mscp_result_t result = MSCP_RST_ERROR;
    uint32_t data_pos = 0;
    uint8_t *flashdata = NULL;
    _stream_state_t stream_state;

    mico_rtos_lock_mutex(&HTTP_W_R_struct.mutex);
    alarm_log("player_flash_thread");
    flashdata = malloc(SOUND_STREAM_DEFAULT_LENGTH + 1);
start_play_voice:
    stream_state = get_alarm_stream_state();
    switch (stream_state)
    {
    case STREAM_STOP:
        audio_service_get_audio_status(&result, &audio_status);
        if (audio_status != MSCP_STATUS_IDLE)
            audio_service_stream_stop(&result, alarm_stream.stream_id);
        goto exit;
        break;
    case STREAM_PLAY:
    case STREAM_IDEL:
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
    if (stream->stream_count)
        stream->stream_count--;
    else
        goto exit;

    flash_read_stream.type = AUDIO_STREAM_TYPE_MP3;
    flash_read_stream.pdata = flashdata;
    flash_read_stream.stream_id = stream->stream_id;
    data_pos = 0;

    memset(HTTP_W_R_struct.alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
    memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, stream->alarm_id, ALARM_ID_LEN + 1);
    HTTP_W_R_struct.alarm_w_r_queue->sound_type = stream->sound_type;
    HTTP_W_R_struct.alarm_w_r_queue->operation_mode = FILE_READ;
    HTTP_W_R_struct.alarm_w_r_queue->sound_data = flashdata;
    HTTP_W_R_struct.alarm_w_r_queue->len = SOUND_STREAM_DEFAULT_LENGTH;
    alarm_log("id:%s", HTTP_W_R_struct.alarm_w_r_queue->alarm_ID);
falsh_read_start:
    HTTP_W_R_struct.alarm_w_r_queue->pos = data_pos;
    err = sound_file_read_write(&sound_file_list, HTTP_W_R_struct.alarm_w_r_queue);
    if (err != kNoErr)
    {
        alarm_log("sound_file_read error!!!!");
        require_noerr_action(err, exit, eland_error(true, EL_FLASH_READ));
    }
    if (data_pos == 0)
    {
        flash_read_stream.total_len = HTTP_W_R_struct.alarm_w_r_queue->total_len;
    }
    flash_read_stream.stream_len = HTTP_W_R_struct.alarm_w_r_queue->len;
audio_transfer:
    if (get_alarm_stream_state() == STREAM_PLAY)
    {
        err = audio_service_stream_play(&result, &flash_read_stream);
        if (err != kNoErr)
        {
            alarm_log("audio_stream_play() error!!!!");
            require_noerr_action(err, exit, eland_error(true, EL_AUDIO_PLAY));
        }
        else
        {
            if (MSCP_RST_PENDING == result || MSCP_RST_PENDING_LONG == result)
            {
                alarm_log("new slave set pause!!!");
                mico_rtos_thread_msleep(200); //time set 1000ms!!!
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
    else
        alarm_log("player stoped %d", get_alarm_stream_state());

    goto start_play_voice;
exit:
    if (err != kNoErr)
        alarm_log("err =%d", err);

    set_alarm_stream_state(STREAM_IDEL);
    mico_rtos_unlock_mutex(&HTTP_W_R_struct.mutex);
    free(flashdata);
    mico_rtos_delete_thread(NULL);
}

void alarm_print(__elsv_alarm_data_t *alarm_data)
{
    alarm_log("alarm_list number:%d", alarm_list.alarm_number);
    alarm_log("\r\n__elsv_alarm_data_t");
    alarm_log("alarm_id:%s", alarm_data->alarm_id);
    alarm_log("alarm_color:%d", alarm_data->alarm_color);
    alarm_log("alarm_time:%s", alarm_data->alarm_time);

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
    alarm_log("\r\nalarm_data_for_eland");
    alarm_log("moment_second:%ld", alarm_data->alarm_data_for_eland.moment_second);
    alarm_log("color:%d", alarm_data->alarm_color);
    alarm_log("snooze_count:%d", alarm_data->snooze_count);
    alarm_log("\r\nalarm_mcu_data_t");
    alarm_log("color:%d", alarm_data->alarm_data_for_mcu.alarm_color);
    alarm_log("snooze_count:%d", alarm_data->alarm_data_for_mcu.snooze_enabled);
    alarm_log("moment_time:%02d-%02d-%02d %d %02d-%02d-%02d",
              alarm_data->alarm_data_for_mcu.moment_time.year,
              alarm_data->alarm_data_for_mcu.moment_time.month,
              alarm_data->alarm_data_for_mcu.moment_time.date,
              alarm_data->alarm_data_for_mcu.moment_time.weekday,
              alarm_data->alarm_data_for_mcu.moment_time.hr,
              alarm_data->alarm_data_for_mcu.moment_time.min,
              alarm_data->alarm_data_for_mcu.moment_time.sec);
}

__elsv_alarm_data_t *get_nearest_alarm()
{
    return alarm_list.alarm_nearest;
}

uint8_t get_next_alarm_serial(uint8_t now_serial)
{
    now_serial++;
    if (now_serial >= get_schedules_number())
        now_serial = 0;
    return now_serial;
}
uint8_t get_previous_alarm_serial(uint8_t now_serial)
{
    uint8_t schedules_number;
    schedules_number = get_schedules_number();
    if (schedules_number)
    {
        if ((now_serial >= schedules_number) || (now_serial == 0))
            now_serial = schedules_number - 1;
        else
            now_serial--;
    }
    return now_serial;
}

uint8_t get_display_na_serial(void)
{
    return alarm_list.na_display_serial;
}
void set_display_na_serial(uint8_t serial)
{
    if (serial >= get_schedules_number())
        serial = 0;
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_lock_mutex(&alarm_list.AlarmSerialMutex);
    alarm_list.na_display_serial = serial;
    if (alarm_list.AlarmSerialMutex != NULL)
        mico_rtos_unlock_mutex(&alarm_list.AlarmSerialMutex);

    eland_push_uart_send_queue(ALARM_SEND_0B);
}
_alarm_mcu_data_t *get_alarm_mcu_data(void)
{
    _alarm_mcu_data_t *temp = NULL;
    if (alarm_list.alarm_nearest)
        temp = &(alarm_list.alarm_nearest)->alarm_data_for_mcu;
    return temp;
}
_alarm_schedules_t *get_schedule_data(void)
{
    uint8_t serial;
    serial = get_display_na_serial();
    return &alarm_list.schedules[serial];
}

uint8_t get_alarm_number(void)
{
    return alarm_list.alarm_number;
}
uint8_t get_schedules_number(void)
{
    return alarm_list.schedules_num;
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
        i = 0;
        do
        {
            if ((alarm->alarm_data_for_eland.moment_second < utc_time) ||
                today_is_alarm_off_day(&date_time, i, alarm))
            {
                i++;
                alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
                continue;
            }
            break;
        } while (1);
        break;
    case 2:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        i = 0;
        do
        {
            if ((((week_day + i - 1) % 7) == 0) ||
                (((week_day + i - 1) % 7) == 6) ||
                today_is_holiday(&date_time, i) ||
                today_is_alarm_off_day(&date_time, i, alarm) ||
                (alarm->alarm_data_for_eland.moment_second < utc_time))
            {
                i++;
                alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
                continue;
            }
            break;
        } while (1);
        alarm_log("utc_time:%ld", alarm->alarm_data_for_eland.moment_second);
        break;
    case 3:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        i = 0;
        do
        {
            if (((((week_day + i - 1) % 7) != 0) && (((week_day + i - 1) % 7) != 6)) ||
                today_is_alarm_off_day(&date_time, i, alarm) ||
                (alarm->alarm_data_for_eland.moment_second < utc_time))
            {
                i++;
                alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
                continue;
            }
            break;
        } while (1);
        break;
    case 4:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        i = 0;
        do
        {
            if ((alarm->alarm_on_days_of_week[(week_day + i - 1) % 7] == '0') ||
                today_is_alarm_off_day(&date_time, i, alarm) ||
                (alarm->alarm_data_for_eland.moment_second < utc_time))
            {
                i++;
                alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
                continue;
            }
            else
                break;
        } while (1);
        break;
    case 5: //do not check alarm_off_dates
        alarm_log("utc_time:%ld", utc_time);
        for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
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
    /*if alarm > one day then next alaarm disappeared*/
    // if ((utc_time < alarm->alarm_data_for_eland.moment_second) &&
    //     ((alarm->alarm_data_for_eland.moment_second - utc_time) < SECOND_ONE_DAY))
    alarm->alarm_data_for_mcu.next_alarm = 1;
    // else
    //     alarm->alarm_data_for_mcu.next_alarm = 0;
    // alarm_log("%04d-%02d-%02d %02d:%02d:%02d",
    //           alarm->alarm_data_for_mcu.moment_time.year + 2000,
    //           alarm->alarm_data_for_mcu.moment_time.month,
    //           alarm->alarm_data_for_mcu.moment_time.date,
    //           alarm->alarm_data_for_mcu.moment_time.hr,
    //           alarm->alarm_data_for_mcu.moment_time.min,
    //           alarm->alarm_data_for_mcu.moment_time.sec);
    // alarm_log("alarm_id:%s", alarm->alarm_id);
}

void alarm_off_history_record_time(alarm_off_history_record_t type, iso8601_time_t *iso8601_time)
{
    __elsv_alarm_data_t *nearestAlarm;
    if (get_alarm_history_data_state() == WAIT_UPLOAD)
        return;
    mico_rtos_lock_mutex(&off_history.off_Mutex);
    switch (type)
    {
    case ALARM_ON:
        memset(&off_history.HistoryData, 0, sizeof(AlarmOffHistoryData_t));
        nearestAlarm = get_nearest_alarm();
        require_quiet(nearestAlarm, exit);
        memcpy(off_history.HistoryData.alarm_id, nearestAlarm->alarm_id, ALARM_ID_LEN);
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
        nearestAlarm = get_nearest_alarm();
        require_quiet(nearestAlarm, exit);
        memcpy(off_history.HistoryData.alarm_id, nearestAlarm->alarm_id, ALARM_ID_LEN);
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
exit:
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
    json_object *AlarmOffDaysJson = NULL;
    __elsv_alarm_data_t *alarm_now;
    const char *generate_data = NULL;
    uint32_t generate_data_len = 0;
    uint8_t i;
    char stringbuf[20];

    Json = json_object_new_object();
    alarm_now = get_nearest_alarm();
    require_quiet(alarm_now, exit);
    require_action_string(Json, exit, err = kNoMemoryErr, "create Json object error!");
    AlarmJson = json_object_new_object();
    require_action_string(AlarmJson, exit, err = kNoMemoryErr, "create AlarmJson object error!");
    AlarmOnDaysJson = json_object_new_array();
    require_action_string(AlarmJson, exit, err = kNoMemoryErr, "create AlarmOnJson object error!");
    AlarmOffDaysJson = json_object_new_array();
    require_action_string(AlarmJson, exit, err = kNoMemoryErr, "create AlarmOffJson object error!");

    alarm_log("Begin add AlarmJson object");
    json_object_object_add(AlarmJson, "alarm_id", json_object_new_string(alarm_now->alarm_id));
    json_object_object_add(AlarmJson, "alarm_color", json_object_new_int(alarm_now->alarm_color));
    json_object_object_add(AlarmJson, "alarm_time", json_object_new_string(alarm_now->alarm_time));
    for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
    {
        if (alarm_now->alarm_off_dates[i] == 0)
            break;
        memset(stringbuf, 0, 20);
        sprintf(stringbuf, "%d-%02d-%02d",
                alarm_now->alarm_off_dates[i] / SIMULATE_DAYS_OF_YEAR + 2000,
                alarm_now->alarm_off_dates[i] % SIMULATE_DAYS_OF_YEAR / SIMULATE_DAYS_OF_MONTH,
                alarm_now->alarm_off_dates[i] % SIMULATE_DAYS_OF_YEAR % SIMULATE_DAYS_OF_MONTH);
        json_object_array_add(AlarmOffDaysJson, json_object_new_string(stringbuf));
    }
    json_object_object_add(AlarmJson, "alarm_off_dates", AlarmOffDaysJson);

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
    for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
    {
        if (alarm_now->alarm_on_dates[i] == 0)
            break;
        memset(stringbuf, 0, 20);
        sprintf(stringbuf, "%d-%02d-%02d",
                alarm_now->alarm_on_dates[i] / SIMULATE_DAYS_OF_YEAR + 2000,
                alarm_now->alarm_on_dates[i] % SIMULATE_DAYS_OF_YEAR / SIMULATE_DAYS_OF_MONTH,
                alarm_now->alarm_on_dates[i] % SIMULATE_DAYS_OF_YEAR % SIMULATE_DAYS_OF_MONTH);
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
/**
 ****************************************************************************
 * @Function : static bool today_is_alarm_off_day(DATE_TIME_t *date, uint8_t offset, __elsv_alarm_data_t *alarm)
 * @File     : eland_alarm.c
 * @Program  : date:today
 *             offset: =0 today  =1 tomorrow.....
 * @Created  : 2018-4-27 by seblee
 * @Brief    :  
 * @Version  : V1.0
**/
static bool today_is_alarm_off_day(DATE_TIME_t *date, uint8_t offset, __elsv_alarm_data_t *alarm)
{
    uint16_t Cache = 0;
    uint8_t i;
    Cache = (date->iYear % 100) * SIMULATE_DAYS_OF_YEAR +
            date->iMon * SIMULATE_DAYS_OF_MONTH +
            date->iDay +
            offset;
    for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
    {
        if (alarm->alarm_off_dates[i] == 0)
            break;
        if (Cache == alarm->alarm_off_dates[i])
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

static void combing_alarm(_eland_alarm_list_t *list, __elsv_alarm_data_t **alarm_nearest, _alarm_list_state_t state)
{
    /**refresh point**/
    alarm_log("list_refreshed");
    uint8_t skip_flag_push = 0;

    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    if (*alarm_nearest)
    {
        free(*alarm_nearest);
        *alarm_nearest = NULL;
    }
    if (state == ALARM_SKIP)
    {
        if ((list->alarm_lib + list->alarm_now_serial)->skip_flag)
            (list->alarm_lib + list->alarm_now_serial)->skip_flag = 0;
        else
            (list->alarm_lib + list->alarm_now_serial)->skip_flag = 1;
        (list->alarm_lib + list->alarm_now_serial)->alarm_data_for_mcu.skip_flag = (list->alarm_lib + list->alarm_now_serial)->skip_flag;
        skip_flag_push = 1;
    }
    *alarm_nearest = Alarm_ergonic_list(list);
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    if (*alarm_nearest)
    {
        eland_push_http_queue(DOWNLOAD_SCAN);
        TCP_Push_MSG_queue(TCP_SD00_Sem);
        alarm_log("alarm_nearest time:%s,second:%ld,alarm_id:%s,skip_flag:%d", (*alarm_nearest)->alarm_time, (*alarm_nearest)->alarm_data_for_eland.moment_second, (*alarm_nearest)->alarm_id, (*alarm_nearest)->skip_flag);
    }
    else
        alarm_log("NO alarm_nearest ");
    set_alarm_state(ALARM_IDEL);
    set_display_na_serial(SCHEDULE_MAX);
    if (skip_flag_push)
        TCP_Push_MSG_queue(TCP_HT02_Sem);
}

OSStatus alarm_sound_oid(void)
{
    OSStatus err = kNoErr;
    uint8_t i;
    char uri_str[100] = {0};
    mscp_result_t result = MSCP_RST_ERROR;
    mscp_status_t audio_status;
    uint8_t oid_volume = 0;
    /******other mode exit********/
#ifdef MICO_DISABLE_STDIO
    require_string(get_eland_mode() == ELAND_NC, exit, "mode err");
#endif
    set_alarm_stream_state(STREAM_PLAY);
    for (i = 0; i < 32; i++)
        audio_service_volume_down(&result, 1);
    oid_volume = get_notification_volume();
    alarm_log("oid_volume:%d", oid_volume);
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

    audio_service_get_audio_status(&result, &audio_status);
    if (audio_status != MSCP_STATUS_IDLE)
    {
        alarm_log("stop playing");
        audio_service_stream_stop(&result, alarm_stream.stream_id);
    }
    if ((get_alarm_stream_state() != STREAM_STOP) && (err != kNoErr))
        err = eland_play_oid_error_sound();

    set_alarm_stream_state(STREAM_IDEL);
    alarm_log("play stopped err:%d", err);

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
/***alarm compare***/
static bool eland_alarm_is_same(__elsv_alarm_data_t *alarm1, __elsv_alarm_data_t *alarm2)
{
    if (strncmp(alarm1->alarm_id, alarm2->alarm_id, ALARM_ID_LEN))
    {
        alarm_log("alarm_id:%s", alarm1->alarm_id);
        alarm_log("alarm_id:%s", alarm2->alarm_id);
        return false;
    }
    if (alarm1->alarm_color != alarm2->alarm_color)
    {
        alarm_log("alarm_color:%d", alarm1->alarm_color);
        alarm_log("alarm_color:%d", alarm2->alarm_color);
        return false;
    }
    if (strncmp(alarm1->alarm_time, alarm2->alarm_time, ALARM_TIME_LEN))
    {
        alarm_log("alarm_time:%s", alarm1->alarm_time);
        alarm_log("alarm_time:%s", alarm2->alarm_time);
        return false;
    }
    if (memcmp(alarm1->alarm_off_dates, alarm2->alarm_off_dates, ALARM_ON_OFF_DATES_COUNT * sizeof(uint8_t)))
    {
        alarm_log("alarm_off_dates ");
        alarm_log("alarm_off_dates ");
        return false;
    }
    if (alarm1->snooze_enabled != alarm2->snooze_enabled)
    {
        alarm_log("snooze_enabled:%d", alarm1->snooze_enabled);
        alarm_log("snooze_enabled:%d", alarm2->snooze_enabled);
        return false;
    }
    if (alarm1->snooze_count != alarm2->snooze_count)
    {
        alarm_log("snooze_count:%d", alarm1->snooze_count);
        alarm_log("snooze_count:%d", alarm2->snooze_count);
        return false;
    }
    if (alarm1->snooze_interval_min != alarm2->snooze_interval_min)
    {
        alarm_log("snooze_interval_min:%d", alarm1->snooze_interval_min);
        alarm_log("snooze_interval_min:%d", alarm2->snooze_interval_min);
        return false;
    }
    if (alarm1->alarm_pattern != alarm2->alarm_pattern)
    {
        alarm_log("alarm_pattern:%d", alarm1->alarm_pattern);
        alarm_log("alarm_pattern:%d", alarm2->alarm_pattern);
        return false;
    }
    if (alarm1->alarm_sound_id != alarm2->alarm_sound_id)
    {
        alarm_log("alarm_sound_id:%ld", alarm1->alarm_sound_id);
        alarm_log("alarm_sound_id:%ld", alarm2->alarm_sound_id);
        return false;
    }
    if (strncmp(alarm1->voice_alarm_id, alarm2->voice_alarm_id, VOICE_ALARM_ID_LEN))
    {
        alarm_log("voice_alarm_id:%s", alarm1->voice_alarm_id);
        alarm_log("voice_alarm_id:%s", alarm2->voice_alarm_id);
        return false;
    }
    if (strncmp(alarm1->alarm_off_voice_alarm_id, alarm2->alarm_off_voice_alarm_id, VOICE_ALARM_ID_LEN))
    {
        alarm_log("alarm_off_voice_alarm_id:%s", alarm1->alarm_off_voice_alarm_id);
        alarm_log("alarm_off_voice_alarm_id:%s", alarm2->alarm_off_voice_alarm_id);
        return false;
    }
    if (alarm1->alarm_volume != alarm2->alarm_volume)
    {
        alarm_log("alarm_volume:%d", alarm1->alarm_volume);
        alarm_log("alarm_volume:%d", alarm2->alarm_volume);
        return false;
    }
    if (alarm1->volume_stepup_enabled != alarm2->volume_stepup_enabled)
    {
        alarm_log("volume_stepup_enabled:%d", alarm1->volume_stepup_enabled);
        alarm_log("volume_stepup_enabled:%d", alarm2->volume_stepup_enabled);
        return false;
    }
    if (alarm1->alarm_continue_min != alarm2->alarm_continue_min)
    {
        alarm_log("alarm_continue_min:%d", alarm1->alarm_continue_min);
        alarm_log("alarm_continue_min:%d", alarm2->alarm_continue_min);
        return false;
    }
    if (alarm1->alarm_repeat != alarm2->alarm_repeat)
    {
        alarm_log("alarm_repeat:%d", alarm1->alarm_repeat);
        alarm_log("alarm_repeat:%d", alarm2->alarm_repeat);
        return false;
    }
    if (memcmp(alarm1->alarm_on_dates, alarm2->alarm_on_dates, ALARM_ON_OFF_DATES_COUNT * sizeof(uint8_t)))
    {
        alarm_log("alarm_on_dates");
        alarm_log("alarm_on_dates");
        return false;
    }
    if (strncmp(alarm1->alarm_on_days_of_week, alarm2->alarm_on_days_of_week, ALARM_ON_DAYS_OF_WEEK_LEN))
    {
        alarm_log("alarm_on_days_of_week:%s", alarm1->alarm_on_days_of_week);
        alarm_log("alarm_on_days_of_week:%s", alarm2->alarm_on_days_of_week);
        return false;
    }
    if (alarm1->skip_flag != alarm2->skip_flag)
    {
        alarm_log("skip_flag:%d", alarm1->skip_flag);
        alarm_log("skip_flag:%d", alarm2->skip_flag);
        return false;
    }
    return true;
}
