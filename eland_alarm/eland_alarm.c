/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:34
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-07-10 17:59:49
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
#include "error_bin.h"
#include "default_bin.h"
/* Private define ------------------------------------------------------------*/
#define CONFIG_ALARM_DEBUG
#ifdef CONFIG_ALARM_DEBUG
#define alarm_log(M, ...) custom_log("alarm", M, ##__VA_ARGS__)
#else
#define alarm_log(...)
#endif /* ! CONFIG_ALARM_DEBUG */
/*********/
#define ALARM_DATA_MAX_LEN 50
#define ALARM_DATA_SIZE (uint16_t)(sizeof(__elsv_alarm_data_t) * ALARM_DATA_MAX_LEN)
/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
mico_semaphore_t alarm_update = NULL;
mico_semaphore_t alarm_skip_sem = NULL;
mico_queue_t http_queue = NULL;
mico_queue_t download_result = NULL;
mico_mutex_t time_Mutex = NULL;

_eland_alarm_list_t alarm_list;
_elsv_holiday_t holiday_list;
_alarm_stream_t alarm_stream;
mico_thread_t play_voice_thread = NULL;

_alarm_off_history_t off_history;
mico_queue_t history_queue = NULL;

mico_timer_t timer_1s;
mico_utc_time_t eland_current_utc = 0;

mico_mutex_t volume_Mutex = NULL;
/* Private function prototypes -----------------------------------------------*/

static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list);
void Alarm_Manager(uint32_t arg);
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, uint8_t CMD);

static void play_voice(mico_thread_arg_t arg);
static void alarm_operation(__elsv_alarm_data_t *alarm);
static void get_alarm_utc_second(__elsv_alarm_data_t *alarm, mico_utc_time_t *utc_time);
static bool today_is_holiday(DATE_TIME_t *date, uint8_t offset);
static bool today_is_alarm_off_day(DATE_TIME_t *date, uint8_t offset, __elsv_alarm_data_t *alarm);
static void combing_alarm(_eland_alarm_list_t *list, __elsv_alarm_data_t **alarm_nearest, _alarm_list_state_t state);
static OSStatus set_alarm_history_send_sem(void);
static void timer_1s_handle(void *arg);
/* Private functions ---------------------------------------------------------*/
OSStatus Start_Alarm_service(void)
{
    OSStatus err;
    /*Register user function for MiCO nitification: WiFi status changed*/
    err = mico_rtos_init_mutex(&time_Mutex);
    require_noerr(err, exit);
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
    err = mico_rtos_init_queue(&download_result, "download_result", sizeof(_download_type_t), 1); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);
    err = mico_rtos_init_queue(&history_queue, "history_queue", sizeof(AlarmOffHistoryData_t *), 5); //只容纳一个成员 传递的只是地址
    require_noerr(err, exit);

    err = mico_rtos_init_semaphore(&alarm_skip_sem, 1);
    require_noerr(err, exit);
    err = mico_rtos_init_mutex(&off_history.off_Mutex);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager", Alarm_Manager, 0x800, 0);
    require_noerr(err, exit);

    /*configuration usart timer*/
    err = mico_init_timer(&timer_1s, 1000, timer_1s_handle, NULL); //100ms 讀取一次
    require_noerr(err, exit);
    /*start key read timer*/
    err = mico_start_timer(&timer_1s); //開始定時器
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    _alarm_list_state_t alarm_state;
    mico_utc_time_t utc_time = 0;
    uint8_t count_temp = 0;
    uint8_t sound_other = 0, weather_0_e_f = 0, weather_c_d = 0;
    uint16_t time_count = 0;
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
        err = mico_rtos_get_semaphore(&alarm_update, 50);
        alarm_state = get_alarm_state();
        if ((alarm_state != ALARM_IDEL) || (err == kNoErr))
        {
            combing_alarm(&alarm_list, &(alarm_list.alarm_nearest), alarm_state);
            sound_other = 0;
            weather_0_e_f = 0;
            weather_c_d = 0;
            time_count = 0;
            if (alarm_list.alarm_nearest)
                TCP_Push_MSG_queue(TCP_SD00_Sem);
        }
        else if ((alarm_state == ALARM_IDEL) &&
                 (alarm_list.alarm_nearest) &&
                 (count_temp++ > 10))
        {
            count_temp = 0;
            utc_time = GET_current_second();
            if (time_count < 1000)
                time_count++;
            /**************check alarm sound file*********************/
            if ((alarm_list.alarm_nearest->skip_flag == 0) &&
                (sound_other == 0) &&
                (time_count >= 3))
            {
                alarm_log("##### DOWNLOAD_SCAN ######");
                err = eland_push_http_queue(DOWNLOAD_SCAN);
                if (err == kNoErr)
                    sound_other = 1;
            }

            /**************check weather 0 e f *********************/
            if ((alarm_list.alarm_nearest->alarm_data_for_eland.moment_second < (300 + utc_time)) &&
                (alarm_list.alarm_nearest->skip_flag == 0) &&
                (weather_0_e_f == 0) &&
                (time_count >= 4))
            {
                alarm_log("##### DOWNLOAD_0_E_F ######");
                err = eland_push_http_queue(DOWNLOAD_0_E_F);
                if (err == kNoErr)
                    weather_0_e_f = 1;
            }

            /**************check weather c d *********************/
            if ((alarm_list.alarm_nearest->alarm_data_for_eland.moment_second < (120 + utc_time)) &&
                (alarm_list.alarm_nearest->skip_flag == 0) &&
                (weather_c_d == 0) &&
                (time_count >= 4))
            {
                alarm_log("##### DOWNLOAD_c_d ######");
                err = eland_push_http_queue(DOWNLOAD_C_D);
                if (err == kNoErr)
                    weather_c_d = 1;
            }

            if ((utc_time >= alarm_list.alarm_nearest->alarm_data_for_eland.moment_second) &&
                (time_count >= 5))
            {
                alarm_log("##### start alarm ######");
                /**operation output alarm**/
                alarm_operation(alarm_list.alarm_nearest);
            }
        }
        /* check current time per second */
    }
    mico_rtos_delete_thread(NULL);
}
static void timer_1s_handle(void *arg)
{
    Eland_Status_type_t state;
    static mico_utc_time_t eland_utc_cache = 0;
    mico_rtos_lock_mutex(&time_Mutex);
    mico_time_get_utc_time(&eland_current_utc);
    mico_rtos_unlock_mutex(&time_Mutex);

    state = get_eland_mode_state() & 0xff;
    //   alarm_log("##### timer_1s_handle state:%d ######", state);

    if ((eland_current_utc % 60) == (netclock_des_g->eland_id % 60) &&
        (eland_utc_cache != eland_current_utc))
    {
        eland_utc_cache = eland_current_utc;
        if (state >= CONNECTED_NET)
            TCP_Push_MSG_queue(TCP_HC00_Sem);
        else /*read mcu rtc time*/
            eland_push_uart_send_queue(TIME_READ_04);
    }
}
_alarm_list_state_t get_alarm_state(void)
{
    return alarm_list.state.state;
}
void set_alarm_state(_alarm_list_state_t state)
{
    mico_rtos_lock_mutex(&alarm_list.state.AlarmStateMutex);
    alarm_list.state.state = state;
    mico_rtos_unlock_mutex(&alarm_list.state.AlarmStateMutex);
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
        memcpy(alarm_stream.alarm_id, alarm->alarm_off_voice_alarm_id, strlen(alarm->alarm_off_voice_alarm_id));
        break;
    case SOUND_FILE_DEFAULT:
        memcpy(alarm_stream.alarm_id, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK));
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

mico_utc_time_t GET_current_second(void)
{
    mico_utc_time_t utc_time;
    mico_rtos_lock_mutex(&time_Mutex);
    utc_time = eland_current_utc;
    mico_rtos_unlock_mutex(&time_Mutex);
    return utc_time;
}
/**elsv alarm data init**/
void elsv_alarm_data_sort_out(__elsv_alarm_data_t *elsv_alarm_data)
{
    _alarm_mcu_data_t *alarm_mcu_data = NULL;
    mico_utc_time_t utc_time;
    int ho, mi, se;
    if (elsv_alarm_data == NULL)
        return;
    alarm_mcu_data = &(elsv_alarm_data->alarm_data_for_mcu);
    sscanf((const char *)(elsv_alarm_data->alarm_time), "%02d:%02d:%02d", &ho, &mi, &se);

    alarm_mcu_data->moment_time.hr = ho;
    alarm_mcu_data->moment_time.min = mi;
    alarm_mcu_data->moment_time.sec = se;
    if (elsv_alarm_data->alarm_repeat <= 0)
    {
        alarm_log("alarm_time:%s", elsv_alarm_data->alarm_time);
        utc_time = GET_current_second();
        get_alarm_utc_second(elsv_alarm_data, &utc_time);
    }

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
    strncpy(alarm_data_cache.alarm_id, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK));
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
    err = alarm_list_add(&alarm_list, &alarm_data_cache);
    if (err == kNoErr)
    {
        alarm_list.list_refreshed = true;
        set_alarm_state(ALARM_ADD);
    }
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
    alarm_log("alarm_number = %d", list->alarm_number);
    list->alarm_now_serial = 0;
    if (list->alarm_number == 0)
    {
        elsv_alarm_data = NULL;
        goto exit;
    }
    if (list->add_utc == 0)
        utc_time = GET_current_second();
    else
    {
        utc_time = list->add_utc;
        list->add_utc = 0;
    }
    for (i = 0; i < list->alarm_number; i++)
    {
        if ((list->alarm_lib + i)->alarm_repeat > 0)
            get_alarm_utc_second(list->alarm_lib + i, &utc_time);
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

    for (i = 0; i < list->alarm_number; i++)
    {
        alarm_log("%d,moment_second:%ld,utc:%ld", i, (list->alarm_lib + i)->alarm_data_for_eland.moment_second, utc_time);
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
        if ((list->alarm_lib + i)->alarm_repeat == -1)
        {
            alarm_log("alarm -1:%s,second:%ld,alarm_id:%s,skip_flag:%d", (list->alarm_lib + i)->alarm_time, (list->alarm_lib + i)->alarm_data_for_eland.moment_second, (list->alarm_lib + i)->alarm_id, (list->alarm_lib + i)->skip_flag);
            list->alarm_now_serial = i;
            elsv_alarm_data = list->alarm_lib + i;
            break;
        }
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
    uint8_t i, temp_point = 0, count = 0;
    _sound_download_para_t sound_para_temp[6];
    _download_type_t result_msg = DOWNLOAD_SCAN;

    alarm_log("alarm_number:%d", alarm_list.alarm_number);
    if (alarm_list.alarm_number == 0)
        return err;
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    memset(sound_para_temp, 0, 6 * sizeof(_sound_download_para_t));
    for (i = alarm_list.alarm_now_serial; i < (alarm_list.alarm_now_serial + 3); i++)
    {
        temp_point = i % alarm_list.alarm_number;
        if (((alarm_list.alarm_lib + temp_point)->alarm_pattern == 1) ||
            ((alarm_list.alarm_lib + temp_point)->alarm_pattern == 3))
        {
            // alarm_log("alarm:%d,SID:%ld", temp_point, (alarm_list.alarm_lib + temp_point)->alarm_sound_id);

            memcpy(sound_para_temp[count].alarm_ID, &((alarm_list.alarm_lib + temp_point)->alarm_sound_id), sizeof((alarm_list.alarm_lib + temp_point)->alarm_sound_id));
            sound_para_temp[count].sound_type = SOUND_FILE_SID;
            count++;
        }
        if (((alarm_list.alarm_lib + temp_point)->alarm_pattern == 2) ||
            ((alarm_list.alarm_lib + temp_point)->alarm_pattern == 3))
        {
            // alarm_log("alarm:%d,VID:%s", temp_point, (alarm_list.alarm_lib + temp_point)->voice_alarm_id);
            if (strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "00000000") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "aaaaaaaa") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "bbbbbbbb") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "cccccccc") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "dddddddd") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "eeeeeeee") ||
                strstr((alarm_list.alarm_lib + temp_point)->voice_alarm_id, "ffffffff"))
            {
            }
            else
            {
                memcpy(sound_para_temp[count].alarm_ID, (alarm_list.alarm_lib + temp_point)->voice_alarm_id, strlen((alarm_list.alarm_lib + temp_point)->voice_alarm_id));
                sound_para_temp[count].sound_type = SOUND_FILE_VID;
                count++;
            }
        }
        if (strlen((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id) > 20)
        {
            // alarm_log("alarm:%d,OFID:%s", temp_point, (alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id);
            if (strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "00000000") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "aaaaaaaa") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "bbbbbbbb") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "cccccccc") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "dddddddd") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "eeeeeeee") ||
                strstr((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, "ffffffff"))
            {
            }
            else
            {
                memcpy(sound_para_temp[count].alarm_ID, (alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id, strlen((alarm_list.alarm_lib + temp_point)->alarm_off_voice_alarm_id));
                sound_para_temp[count].sound_type = SOUND_FILE_OFID;
                count++;
            }
        }
    }
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < count; i++)
    {
        err = alarm_sound_download(sound_para_temp[i]);
        if (err != kNoErr)
            result_msg = DOWNLOAD_IDEL;
    }
    err = mico_rtos_push_to_queue(&download_result, &result_msg, 10);
    return err;
}

OSStatus weather_sound_0_e_f(void)
{
    OSStatus err = kNoErr;
    uint8_t i;
    _download_type_t result_msg = DOWNLOAD_0_E_F;
    __elsv_alarm_data_t *nearest = NULL;
    uint8_t count = 0;
    _sound_download_para_t sound_para[2];
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    nearest = get_nearest_alarm();
    require_quiet(nearest, checkout_over);
    //   alarm_log("nearest_id:%s", nearest->alarm_id);

    if ((nearest->alarm_pattern == 2) ||
        (nearest->alarm_pattern == 3))
    {
        if (strstr(nearest->voice_alarm_id, "ffffffff"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_F;
        else if (strstr(nearest->voice_alarm_id, "00000000"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_0;
        else
            goto checkout_ofid;
        alarm_log("VID:%s", nearest->voice_alarm_id);
        sprintf(nearest->voice_alarm_id + 24, "%ldww", nearest->alarm_data_for_eland.moment_second);
        memcpy(sound_para[count].alarm_ID, nearest->voice_alarm_id, strlen(nearest->voice_alarm_id));
        count++;
    }
checkout_ofid:
    if (strlen(nearest->alarm_off_voice_alarm_id) > 10)
    {
        if (strstr(nearest->alarm_off_voice_alarm_id, "ffffffff"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_F;
        else if (strstr(nearest->alarm_off_voice_alarm_id, "eeeeeeee"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_E;
        else if (strstr(nearest->alarm_off_voice_alarm_id, "00000000"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_0;
        else
            goto checkout_over;
        alarm_log("VID:%s", nearest->alarm_off_voice_alarm_id);
        sprintf(nearest->alarm_off_voice_alarm_id + 24, "%ldww", nearest->alarm_data_for_eland.moment_second);
        memcpy(sound_para[count].alarm_ID, nearest->alarm_off_voice_alarm_id, strlen(nearest->alarm_off_voice_alarm_id));
        count++;
    }
checkout_over:
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < count; i++)
    {
        err = alarm_sound_download(sound_para[i]);
        if (err != kNoErr)
            result_msg = DOWNLOAD_IDEL;
    }

    mico_rtos_push_to_queue(&download_result, &result_msg, 10);
    return err;
}

OSStatus weather_sound_c_d(void)
{
    OSStatus err = kNoErr;
    uint8_t i, count = 0;
    _download_type_t result_msg = DOWNLOAD_C_D;
    __elsv_alarm_data_t *nearest = NULL;
    _sound_download_para_t sound_para[2];
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    nearest = get_nearest_alarm();
    require_quiet(nearest, checkout_over);
    //   alarm_log("nearest_id:%s", nearest->alarm_id);

    if (strlen(nearest->alarm_off_voice_alarm_id) > 10)
    {
        if (strstr(nearest->alarm_off_voice_alarm_id, "cccccccc"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_C;
        else if (strstr(nearest->alarm_off_voice_alarm_id, "dddddddd"))
            sound_para[count].sound_type = SOUND_FILE_WEATHER_D;
        else
            goto checkout_over;
        sprintf(nearest->alarm_off_voice_alarm_id + 24, "%ldcd", nearest->alarm_data_for_eland.moment_second);
        memcpy(sound_para[count].alarm_ID, nearest->alarm_off_voice_alarm_id, strlen(nearest->alarm_off_voice_alarm_id));
        alarm_log("VID:%s", nearest->alarm_off_voice_alarm_id);
        count++;
    }
checkout_over:
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);

    for (i = 0; i < count; i++)
    {
        err = alarm_sound_download(sound_para[i]);
        if (err != kNoErr)
            result_msg = DOWNLOAD_IDEL;
    }

    err = mico_rtos_push_to_queue(&download_result, &result_msg, 10);
    return err;
}
OSStatus weather_sound_a_b(_download_type_t type)
{
    OSStatus err = kNoErr;
    uint8_t i, count = 0;
    _download_type_t result_msg = DOWNLOAD_B;
    __elsv_alarm_data_t *nearest = NULL;
    _sound_download_para_t sound_para[2];
    mico_rtos_lock_mutex(&alarm_list.AlarmlibMutex);
    nearest = get_nearest_alarm();
    require_quiet(nearest, checkout_over);
    //   alarm_log("nearest_id:%s", nearest->alarm_id);

    if (strlen(nearest->voice_alarm_id) > 10)
    {
        alarm_log("VID:%s", nearest->voice_alarm_id);
        if (type == DOWNLOAD_A)
        {
            if (strstr(nearest->voice_alarm_id, "aaaaaaaa"))
            {
                sound_para[count].sound_type = SOUND_FILE_WEATHER_A;
                memcpy(sound_para[count].alarm_ID, nearest->voice_alarm_id, strlen(nearest->alarm_off_voice_alarm_id));
                count++;
            }
        }
        else if (type == DOWNLOAD_B)
        {
            if (strstr(nearest->alarm_off_voice_alarm_id, "bbbbbbbb"))
            {
                sound_para[count].sound_type = SOUND_FILE_WEATHER_B;
                memcpy(sound_para[count].alarm_ID, nearest->alarm_off_voice_alarm_id, strlen(nearest->alarm_off_voice_alarm_id));
                count++;
            }
        }
        else
            goto checkout_over;
    }
checkout_over:
    mico_rtos_unlock_mutex(&alarm_list.AlarmlibMutex);
    for (i = 0; i < count; i++)
    {
        err = alarm_sound_download(sound_para[i]);
        if (err != kNoErr)
            result_msg = DOWNLOAD_IDEL;
    }
    err = mico_rtos_push_to_queue(&download_result, &result_msg, 10);
    return err;
}
OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    OSStatus err = kNoErr;
    uint8_t i, j = 0;
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
                memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, i * sizeof(__elsv_alarm_data_t));
                memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
                goto exit;
            }
        }
        /**a new alarm**/
        if (AlarmList->alarm_number < ALARM_DATA_MAX_LEN)
        {
            memmove(AlarmList->alarm_lib + 1, AlarmList->alarm_lib, AlarmList->alarm_number * sizeof(__elsv_alarm_data_t));
            memcpy((uint8_t *)(AlarmList->alarm_lib), inData, sizeof(__elsv_alarm_data_t));
            AlarmList->alarm_number++;
        }
        else
        {
            for (i = 0; i < AlarmList->alarm_number; i++)
            {
                if ((AlarmList->alarm_lib + i)->alarm_data_for_eland.moment_second >
                    (AlarmList->alarm_lib + j)->alarm_data_for_eland.moment_second)
                    j = i;
            }
            memcpy((uint8_t *)(AlarmList->alarm_lib + j), inData, sizeof(__elsv_alarm_data_t));
            goto exit;
        }
    }

exit:
    alarm_log("alarm_add success!");
    AlarmList->add_utc = GET_current_second();
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
            if (AlarmList->alarm_number == 0)
                memset((uint8_t *)(AlarmList->alarm_lib), 0, ALARM_DATA_SIZE);
            AlarmList->list_refreshed = true;
            set_alarm_state(ALARM_MINUS);
            break;
        }
    }

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
        set_alarm_history_send_sem();
        set_alarm_state(ALARM_STOP);
        goto exit;
    }

    if (alarm->snooze_enabled)
        snooze_count = alarm->snooze_count + 1;
    else
        snooze_count = 1;
    alarm_moment += (uint32_t)alarm->alarm_continue_min * 60;
    set_alarm_state(ALARM_ING);

    while (1)
    {
        current_state = get_alarm_state();
        utc_time = GET_current_second();

        switch (current_state)
        {
        case ALARM_ING:
            if (first_to_alarming)
            {
                snooze_count--;
                first_to_alarming = false;
                first_to_snooze = true;

                /** record_time **/
                mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)utc_time * 1000), &iso8601_time);
                alarm_off_history_record_time(ALARM_ON, &iso8601_time);
                if (utc_time >= alarm_moment)
                {
                    alarm_log("time up first_to_alarming");
                    //play with delay
                    Alarm_Play_Control(alarm, AUDIO_PALY);
                    set_alarm_state(ALARM_SNOOZ_STOP);
                    break;
                }
                /**alarm on notice**/
                TCP_Push_MSG_queue(TCP_HT00_Sem);

                eland_push_uart_send_queue(ALARM_SEND_0B);
                if (eland_oid_status(false, 0) == 0)
                {
                    for (i = 0; i < 33; i++)
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
                if (strstr(alarm->voice_alarm_id, "aaaaaaaa"))
                {
                    alarm_log("##### DOWNLOAD_A ######");
                    sprintf(alarm->voice_alarm_id + 24, "%ldbb", utc_time);
                    eland_push_http_queue(DOWNLOAD_A);
                }
            }
            if ((alarm->volume_stepup_enabled) &&
                (++volume_stepup_count > 10) &&
                (eland_oid_status(false, 0) == 0))
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
            if ((utc_time >= alarm_moment) || (eland_oid_status(false, 0) > 0))
            {
                mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)utc_time * 1000), &iso8601_time);
                if (eland_oid_status(false, 0) > 0)
                    alarm_off_history_record_time(ALARM_OFF_SNOOZE, &iso8601_time);
                else
                    alarm_off_history_record_time(ALARM_OFF_AUTOOFF, &iso8601_time);
                set_alarm_state(ALARM_SNOOZ_STOP);
                if (snooze_count == 0)
                {
                    set_alarm_history_send_sem();
                    Alarm_Play_Control(alarm, AUDIO_STOP_PLAY); //stop
                    goto exit;
                }
            }
            else
                Alarm_Play_Control(alarm, AUDIO_PALY); //play with delay
            break;
        case ALARM_ADD:
        case ALARM_MINUS:
        case ALARM_SORT:
        case ALARM_SKIP:
        case ALARM_STOP:
            mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)utc_time * 1000), &iso8601_time);
            alarm_off_history_record_time(ALARM_OFF_ALARMOFF, &iso8601_time);
            alarm_log("alarm_operation stop");
            if (eland_oid_status(false, 0) == 0)
                Alarm_Play_Control(alarm, AUDIO_STOP_PLAY); //stop
            set_alarm_history_send_sem();
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
                    if (utc_time >= alarm_moment)
                    {
                        alarm_log("time up first_to_snooze");
                        set_alarm_state(ALARM_ING);
                        break;
                    }
                    if (eland_oid_status(false, 0) == 0)
                        Alarm_Play_Control(alarm, AUDIO_STOP); //stop
                    alarm_log("time up alarm_moment:%ld", alarm_moment);
                    mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)alarm_moment * 1000), &iso8601_time);
                    alarm_off_history_record_time(ALARM_SNOOZE, &iso8601_time);
                    /**add json for tcp**/
                    set_alarm_history_send_sem();
                    alarm_log("Alarm_Play_Control: first_to_snooze");
                    eland_push_uart_send_queue(ALARM_SEND_0B);
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
    }
exit:
    alarm_log("alarm_operation out");
    mico_rtos_thread_msleep(500);
}
/***CMD = 1 PALY   CMD = 0 STOP***/
static void Alarm_Play_Control(__elsv_alarm_data_t *alarm, _alarm_play_tyep_t CMD)
{
    uint8_t i;
    mscp_result_t result;
    mico_utc_time_t utc_time;
    mscp_status_t audio_status;
    static bool isVoice = false;
    static uint8_t CMD_bak = AUDIO_NONE;

    audio_service_get_audio_status(&result, &audio_status);
    if ((CMD == AUDIO_PALY) && (get_alarm_stream_state() != STREAM_PLAY)) //play
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
    else if (CMD == AUDIO_STOP) //stop
    {
        isVoice = false;
        alarm_log("player_flash_thread");
        if (get_alarm_stream_state() == STREAM_PLAY)
            set_alarm_stream_state(STREAM_STOP);
    }
    else if ((CMD == AUDIO_STOP_PLAY) &&
             (CMD_bak != AUDIO_STOP_PLAY)) //stop
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
            } while ((get_alarm_stream_state() != STREAM_IDEL) ||
                     (audio_status != MSCP_STATUS_IDLE));
            alarm_log("*****sudio stoped");
            utc_time = GET_current_second();
            if (strstr(alarm->alarm_off_voice_alarm_id, "bbbbbbbb"))
            {
                alarm_log("##### DOWNLOAD_B ######");
                sprintf(alarm->alarm_off_voice_alarm_id + 24, "%ldbb", utc_time);
                eland_push_http_queue(DOWNLOAD_B);
            }
            for (i = 0; i < 33; i++)
                audio_service_volume_down(&result, 1);
            for (i = 0; i < (alarm->alarm_volume * 32 / 100 + 1); i++)
                audio_service_volume_up(&result, 1);

            init_alarm_stream(alarm, SOUND_FILE_OFID);
            set_alarm_stream_state(STREAM_PLAY);
            alarm_log("start play ofid");
            mico_rtos_thread_msleep(400);
            mico_rtos_create_thread(&play_voice_thread, MICO_APPLICATION_PRIORITY, "stream_thread", play_voice, 0x500, (uint32_t)(&alarm_stream));
        }
    }
    CMD_bak = CMD;
}

static void play_voice(mico_thread_arg_t arg)
{
    _alarm_stream_t *stream = (_alarm_stream_t *)arg;
    AUDIO_STREAM_PALY_S fm_stream;
    mscp_status_t audio_status;
    OSStatus err = kNoErr;
    mscp_result_t result = MSCP_RST_ERROR;
    uint32_t data_pos = 0;
    uint8_t *flashdata = NULL;
    _sound_rom_t rom_place = ROM_FLASH;
    _stream_state_t stream_state;
    uint8_t fm_test_cnt = 0;

    alarm_log("player_flash_thread");

    mico_rtos_lock_mutex(&HTTP_W_R_struct.mutex);
    err = alarm_sound_file_check(stream->alarm_id);
    if (err != kNoErr)
    {
        if (strstr(stream->alarm_id, ALARM_ID_OF_DEFAULT_CLOCK))
            rom_place = ROM_DEFAULT;
        else
            rom_place = ROM_ERROR;
    }
    if (rom_place == ROM_FLASH)
        flashdata = malloc(SOUND_STREAM_DEFAULT_LENGTH + 1);
start_play_voice:
    stream_state = get_alarm_stream_state();
    switch (stream_state)
    {
    case STREAM_STOP:
        audio_service_get_audio_status(&result, &audio_status);
        if (audio_status != MSCP_STATUS_IDLE)
            audio_service_stream_stop(&result, stream->stream_id);
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

    data_pos = 0;
    fm_stream.type = AUDIO_STREAM_TYPE_MP3;
    fm_stream.stream_id = stream->stream_id;
    if (rom_place == ROM_FLASH)
    {
        fm_stream.pdata = flashdata;
        memset(HTTP_W_R_struct.alarm_w_r_queue, 0, sizeof(_sound_read_write_type_t));
        memcpy(HTTP_W_R_struct.alarm_w_r_queue->alarm_ID, stream->alarm_id, ALARM_ID_LEN + 1);
        HTTP_W_R_struct.alarm_w_r_queue->sound_type = stream->sound_type;
        HTTP_W_R_struct.alarm_w_r_queue->operation_mode = FILE_READ;
        HTTP_W_R_struct.alarm_w_r_queue->sound_data = flashdata;
        HTTP_W_R_struct.alarm_w_r_queue->len = SOUND_STREAM_DEFAULT_LENGTH;
        alarm_log("id:%s", HTTP_W_R_struct.alarm_w_r_queue->alarm_ID);
    }
    else if (rom_place == ROM_DEFAULT)
    {
        fm_stream.pdata = (uint8_t *)default_sound;
        fm_stream.total_len = sizeof(default_sound);
    }
    else if (rom_place == ROM_ERROR)
    {
        fm_stream.pdata = (uint8_t *)error_sound;
        fm_stream.total_len = sizeof(error_sound);
    }

falsh_read_start:
    if (rom_place == ROM_FLASH)
    {
        HTTP_W_R_struct.alarm_w_r_queue->pos = data_pos;
        err = sound_file_read_write(&sound_file_list, HTTP_W_R_struct.alarm_w_r_queue);
        if (err != kNoErr)
        {
            alarm_log("sound_file_read error!!!!");
            require_noerr_action(err, exit, eland_error(EL_RAM_WRITE, EL_FLASH_READ));
        }
        if (data_pos == 0)
        {
            if (HTTP_W_R_struct.alarm_w_r_queue->total_len == 1)
                goto exit;
            fm_stream.total_len = HTTP_W_R_struct.alarm_w_r_queue->total_len;
        }
        fm_stream.stream_len = HTTP_W_R_struct.alarm_w_r_queue->len;
    }
    else if ((rom_place == ROM_ERROR) || (rom_place == ROM_DEFAULT))
    {
        if ((fm_stream.total_len - data_pos) > SOUND_STREAM_DEFAULT_LENGTH) //len
            fm_stream.stream_len = SOUND_STREAM_DEFAULT_LENGTH;
        else
            fm_stream.stream_len = fm_stream.total_len - data_pos;
        if (rom_place == ROM_DEFAULT)
            fm_stream.pdata = (uint8_t *)default_sound + data_pos;
        else if (rom_place == ROM_ERROR)
            fm_stream.pdata = (uint8_t *)error_sound + data_pos;
    }
    if ((++fm_test_cnt) >= 10)
    {
        fm_test_cnt = 0;
        alarm_log("type:%d,id:%d,total_len:%d,len:%d,pos:%ld", (int)fm_stream.type, (int)fm_stream.stream_id, (int)fm_stream.total_len, (int)fm_stream.stream_len, data_pos);
    }
audio_transfer:
    if (get_alarm_stream_state() == STREAM_PLAY)
    {
        err = audio_service_stream_play(&result, &fm_stream);
        if (err != kNoErr)
        {
            alarm_log("audio_stream_play() error!!!!");
            require_noerr_action(err, exit, eland_error(EL_RAM_WRITE, EL_AUDIO_PLAY));
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
        data_pos += fm_stream.stream_len;

        if (data_pos < fm_stream.total_len)
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
    if (flashdata)
    {
        free(flashdata);
        flashdata = NULL;
    }
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

static void get_alarm_utc_second(__elsv_alarm_data_t *alarm, mico_utc_time_t *utc_time)
{
    DATE_TIME_t date_time;
    struct tm *currentTime;
    uint8_t week_day;
    int8_t i;
    int year, month, day;

    currentTime = localtime((const time_t *)utc_time);

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
    case -1: //只在新数据导入时候计算一次
        alarm_log("alarm -1:%s,second:%ld,alarm_id:%s,skip_flag:%d", alarm->alarm_time, alarm->alarm_data_for_eland.moment_second, alarm->alarm_id, alarm->skip_flag);
        alarm->alarm_data_for_eland.moment_second = (*utc_time + 3);
        break;
    case 0: //只在新数据导入时候计算一次
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        if (alarm->alarm_data_for_eland.moment_second < *utc_time)
            alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
        break;
    case 1:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        i = 0;
        do
        {
            if ((alarm->alarm_data_for_eland.moment_second < *utc_time) ||
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
                (alarm->alarm_data_for_eland.moment_second < *utc_time))
            {
                i++;
                alarm->alarm_data_for_eland.moment_second += SECOND_ONE_DAY;
                continue;
            }
            break;
        } while (1);
        alarm_log("moment_second:%ld", alarm->alarm_data_for_eland.moment_second);
        break;
    case 3:
        alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
        i = 0;
        do
        {
            if (((((week_day + i - 1) % 7) != 0) && (((week_day + i - 1) % 7) != 6)) ||
                today_is_alarm_off_day(&date_time, i, alarm) ||
                (alarm->alarm_data_for_eland.moment_second < *utc_time))
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
                (alarm->alarm_data_for_eland.moment_second < *utc_time))
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
        alarm_log("utc_time:%ld", *utc_time);
        alarm->alarm_data_for_eland.moment_second = 0;
        for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
        {
            if (alarm->alarm_on_dates[i] == 0)
                break;
            alarm_log("date:%d-%02d-%02d", date_time.iYear, date_time.iMon, date_time.iDay);
            alarm_log("moment_second:%ld", alarm->alarm_data_for_eland.moment_second);
            get_d_withdays(&year, &month, &day, alarm->alarm_on_dates[i]);
            date_time.iYear = year;
            date_time.iMon = month;
            date_time.iDay = day;
            alarm->alarm_data_for_eland.moment_second = GetSecondTime(&date_time);
            if (alarm->alarm_data_for_eland.moment_second > *utc_time)
                break;
        }
        break;
    default:
        alarm->alarm_data_for_eland.moment_second = 0;
        break;
    }
    /*******Calculate date for mcu*********/
    UCT_Convert_Date(&(alarm->alarm_data_for_eland.moment_second), &(alarm->alarm_data_for_mcu.moment_time));
    alarm->alarm_data_for_mcu.next_alarm = 1;

    // alarm_log("alarm_id:%s", alarm->alarm_time);
    // alarm_log("%04d-%02d-%02d %02d:%02d:%02d",
    //           alarm->alarm_data_for_mcu.moment_time.year + 2000,
    //           alarm->alarm_data_for_mcu.moment_time.month,
    //           alarm->alarm_data_for_mcu.moment_time.date,
    //           alarm->alarm_data_for_mcu.moment_time.hr,
    //           alarm->alarm_data_for_mcu.moment_time.min,
    //           alarm->alarm_data_for_mcu.moment_time.sec);
}

void alarm_off_history_record_time(alarm_off_history_record_t type, iso8601_time_t *iso8601_time)
{
    __elsv_alarm_data_t *nearestAlarm;

    mico_rtos_lock_mutex(&off_history.off_Mutex);
    switch (type)
    {
    case ALARM_ON:
        memset(&off_history.HistoryData, 0, sizeof(AlarmOffHistoryData_t));
        nearestAlarm = get_nearest_alarm();
        require_quiet(nearestAlarm, exit);
        memcpy(off_history.HistoryData.alarm_id, nearestAlarm->alarm_id, ALARM_ID_LEN);
        memcpy(&off_history.HistoryData.alarm_on_datetime, iso8601_time, 19);
        break;
    case ALARM_OFF_SNOOZE:
    case ALARM_OFF_ALARMOFF:
    case ALARM_OFF_AUTOOFF:
        nearestAlarm = get_nearest_alarm();
        require_quiet(nearestAlarm, exit);
        memcpy(off_history.HistoryData.alarm_id, nearestAlarm->alarm_id, ALARM_ID_LEN);
        memcpy(&off_history.HistoryData.alarm_off_datetime, iso8601_time, 19);
        off_history.HistoryData.alarm_off_reason = type;

        break;
    case ALARM_OFF_SKIP:
        memset(&off_history.HistoryData, 0, sizeof(AlarmOffHistoryData_t));
        nearestAlarm = get_nearest_alarm();
        require_quiet(nearestAlarm, exit);
        memcpy(off_history.HistoryData.alarm_id, nearestAlarm->alarm_id, ALARM_ID_LEN);
        memcpy(&off_history.HistoryData.alarm_on_datetime, iso8601_time, 19);
        off_history.HistoryData.alarm_off_reason = type;

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
    int year, mon, day;

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
        get_d_withdays(&year, &mon, &day, alarm_now->alarm_off_dates[i]);
        sprintf(stringbuf, "%d-%02d-%02d", year, mon, day);
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
        get_d_withdays(&year, &mon, &day, alarm_now->alarm_on_dates[i]);
        sprintf(stringbuf, "%d-%02d-%02d", year, mon, day);
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
    Cache = get_g_alldays(date->iYear, date->iMon, date->iDay) + offset;

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
    Cache = get_g_alldays(date->iYear, date->iMon, date->iDay) + offset;
    // alarm_log("today_is_alarm_off_day:%d", Cache);
    for (i = 0; i < ALARM_ON_OFF_DATES_COUNT; i++)
    {
        if (alarm->alarm_off_dates[i] == 0)
            break;
        if (Cache == alarm->alarm_off_dates[i])
            return true;
    }
    return false;
}
/*获取公历年的天数*/
int year_alldays(int year)
{
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        return 366;
    else
        return 365;
}

/*获取公历年初至某整月的天数*/
int year_sumday(int year, int month)
{
    int sum = 0;
    int rui[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ping[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int ruiflag = 0;
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
        ruiflag = 1;
    for (int index = 0; index < month - 1; index++)
    {
        if (ruiflag == 1)
            sum += rui[index];
        else
            sum += ping[index];
    }
    return sum;
}
/*获取从公历1999年1月25日至当前日期的总天数*/
int get_g_alldays(int year, int month, int day)
{
    int i = 1999, days = -25;
    while (i < year)
    {
        days += year_alldays(i);
        i++;
    }
    int days2 = year_sumday(year, month);
    return days + days2 + day;
}
/*获取从公历1999年1月25日至过偏移n天后的日期*/
void get_d_withdays(int *year, int *mon, int *day, int days)
{
    int month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    *year = 1999;
    *mon = 1;
    *day = 25;
    month[1] = (*year % 4 == 0 && *year % 100 != 0) || (*year % 400 == 0) ? 29 : 28;

    while (days > month[(*mon) - 1] - *day)
    {
        days -= month[*mon - 1] - *day;
        *day = 0;
        *mon += 1;
        if (*mon == 13)
        {
            *year += 1;
            *mon = 1;
            month[1] = (*year % 4 == 0 && *year % 100 != 0) || (*year % 400 == 0) ? 29 : 28;
        }
    }
    *day += days;
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
    eland_oid_status(true, 1);
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
        err = eland_play_rom_sound(ROM_ERROR);
    eland_oid_status(true, 0);
    set_alarm_stream_state(STREAM_IDEL);
    alarm_log("play stopped err:%d", err);

    return err;
}

static OSStatus set_alarm_history_send_sem(void)
{
    OSStatus err = kNoErr;
    AlarmOffHistoryData_t *history_P = NULL;
    if (strlen(off_history.HistoryData.alarm_id) > 15)
    {
        history_P = malloc(sizeof(AlarmOffHistoryData_t));
        memcpy(history_P, &(off_history.HistoryData), sizeof(AlarmOffHistoryData_t));
        err = mico_rtos_push_to_queue(&history_queue, &history_P, 10);
        require_noerr_action(err, exit, alarm_log("[error]history_queue err"));
    }
    memset(&(off_history.HistoryData), 0, sizeof(AlarmOffHistoryData_t));

exit:
    return err;
}
OSStatus check_default_sound(void)
{
    uint8_t check_times = 0;
    OSStatus err = kNoErr;
    __elsv_alarm_data_t alarm_simple;
    _sound_download_para_t sound_para;
    memset(&sound_para, 0, sizeof(_sound_download_para_t));
check_start:
    alarm_log("check default sound");
    err = SOUND_CHECK_DEFAULT_FILE();
    if (err != kNoErr)
    {
    download_start:
        alarm_log("SOUND_FILE_CLEAR");
        SOUND_FILE_CLEAR();
        memset(&alarm_simple, 0, sizeof(__elsv_alarm_data_t));
        alarm_log("download default file");
        memcpy(sound_para.alarm_ID, ALARM_ID_OF_DEFAULT_CLOCK, strlen(ALARM_ID_OF_DEFAULT_CLOCK));
        sound_para.sound_type = SOUND_FILE_DEFAULT;

        err = alarm_sound_download(sound_para);
        require_noerr(err, exit);
        goto check_start;
    }
exit:
    if (err == kNoErr)
        alarm_log("check ok");
    else
    {
        mico_rtos_thread_sleep(5);
        goto download_start;
        if (check_times < 10)
        {
            check_times++;
        }
        else
            mico_rtos_delete_thread(NULL);
    }
    return err;
}

OSStatus eland_push_http_queue(_download_type_t msg)
{
    OSStatus err;
    _download_type_t http_msg = msg;
    _download_type_t result_msg = DOWNLOAD_IDEL;

    err = mico_rtos_push_to_queue(&http_queue, &http_msg, 10);
    if ((msg == DOWNLOAD_OID) || (msg == DOWNLOAD_OTA) || (msg == GO_INTO_AP_MODE) || (msg == GO_OUT))
        return err;

    while (!mico_rtos_is_queue_empty(&download_result))
        mico_rtos_pop_from_queue(&download_result, &result_msg, 0);
    // 等待返回结果
    err = mico_rtos_pop_from_queue(&download_result, &result_msg, 60000);
    alarm_log("##### DOWNLOAD_result:%d ######", result_msg);
    if ((err == kNoErr) && (result_msg == http_msg))
        return kNoErr;
    else
        return kGeneralErr;
}
/***alarm compare***/
bool eland_alarm_is_same(__elsv_alarm_data_t *alarm1, __elsv_alarm_data_t *alarm2)
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
    if (memcmp(alarm1->alarm_off_dates, alarm2->alarm_off_dates, ALARM_ON_OFF_DATES_COUNT * sizeof(uint16_t)))
    {
        alarm_log("alarm1 alarm2 alarm_off_dates");
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
    if (strncmp(alarm1->voice_alarm_id, alarm2->voice_alarm_id, 24))
    {
        alarm_log("voice_alarm_id:%s", alarm1->voice_alarm_id);
        alarm_log("voice_alarm_id:%s", alarm2->voice_alarm_id);
        return false;
    }
    if (strncmp(alarm1->alarm_off_voice_alarm_id, alarm2->alarm_off_voice_alarm_id, 24))
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

void eland_alarm_control(uint16_t Count, uint16_t Count_Trg,
                         uint16_t Restain, uint16_t Restain_Trg,
                         _ELAND_MODE_t eland_mode)
{
    mico_utc_time_t utc_time;
    iso8601_time_t iso8601_time;
    _alarm_list_state_t alarm_status = get_alarm_state();
    if (alarm_status == ALARM_ING)
    {
        if ((Count_Trg & KEY_Snooze) || (Count_Trg & KEY_Alarm))
        {
            utc_time = GET_current_second();
            mico_time_convert_utc_ms_to_iso8601((mico_utc_time_ms_t)((mico_utc_time_ms_t)utc_time * 1000), &iso8601_time);

            if (Count_Trg & KEY_Snooze)
            {
                set_alarm_state(ALARM_SNOOZ_STOP);
                alarm_off_history_record_time(ALARM_OFF_SNOOZE, &iso8601_time);
            }
            else if (Count_Trg & KEY_Alarm)
                set_alarm_state(ALARM_STOP);
        }
    }
    else if (alarm_status == ALARM_SNOOZ_STOP)
    {
        if (Count_Trg & KEY_Alarm)
            set_alarm_state(ALARM_STOP);
        if ((Count_Trg & KEY_Snooze) &&
            (eland_mode != ELAND_CLOCK_MON) &&
            (eland_mode != ELAND_CLOCK_ALARM) &&
            (eland_mode != ELAND_NA))
        { //sound oid
            if (get_alarm_stream_state() == STREAM_PLAY)
                set_alarm_stream_state(STREAM_STOP);
            else
                eland_push_http_queue(DOWNLOAD_OID);
        }
    }
    else
    {
        if (Restain_Trg & KEY_Alarm) //alarm skip
        {
            set_alarm_state(ALARM_SKIP);
        }
        if ((Count_Trg & KEY_Snooze) &&
            (eland_mode != ELAND_CLOCK_MON) &&
            (eland_mode != ELAND_CLOCK_ALARM) &&
            (eland_mode != ELAND_NA))
        { //sound oid
            if (get_alarm_stream_state() == STREAM_PLAY)
                set_alarm_stream_state(STREAM_STOP);
            else
                eland_push_http_queue(DOWNLOAD_OID);
        }
    }
}

uint8_t eland_oid_status(bool style, uint8_t value)
{
    static uint8_t oid_status = 0;
    if (style)
        oid_status = value;
    return oid_status;
}

// void eland_volume_set(uint8_t value)
// {
//     uint8_t i;
//     mscp_result_t result;
//     static uint8_t volume = 100;
//     if (volume_Mutex == NULL)
//     {
//         mico_rtos_init_mutex(&volume_Mutex);
//         for (i = 0; i < 33; i++)
//             audio_service_volume_down(&result, 1);
//         volume = 0;
//     }

//     mico_rtos_lock_mutex(&volume_Mutex);
//     if (volume < value)
//     {
//         for (i = volume; i < value; i++)
//             audio_service_volume_up(&result, 1);
//     }
//     else if (volume == value)
//     {
//     }
//     else
//     {
//         for (i = volume; i > value; i--)
//             audio_service_volume_down(&result, 1);
//     }
//     volume = value;
//     mico_rtos_unlock_mutex(&volume_Mutex);
// }
