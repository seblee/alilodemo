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
/* Private define ------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
_eland_alarm_list_t alarm_list;

/* Private function prototypes -----------------------------------------------*/
static uint32_t GET_current_second(void);
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list);
static void set_alarm_state(_alarm_list_state_t state);
static _alarm_list_state_t get_alarm_state(void);
void Alarm_Manager(uint32_t arg);
/* Private functions ---------------------------------------------------------*/
OSStatus Start_Alarm_service(void)
{
    OSStatus err;
    err = mico_rtos_init_mutex(&alarm_list.AlarmListMutex);
    require_noerr(err, exit);

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Alarm_Manager", Alarm_Manager, 0x500, 0);
    require_noerr(err, exit);
exit:
    return err;
}
void Alarm_Manager(uint32_t arg)
{
    __elsv_alarm_data_t *alarm_nearest;
    uint32_t current_seconds = 0;
    uint8_t i;

    for (i = 0; i < alarm_list.alarm_number; i++)
        elsv_alarm_data_sort_out(alarm_list.alarm_lib + i);
    alarm_nearest = Alarm_ergonic_list(&alarm_list);

    while (1)
    {
        if ((alarm_list.list_refreshed) || (alarm_list.alarm_stoped == true))
        {
            /**refresh point**/
            alarm_nearest = Alarm_ergonic_list(&alarm_list);
        }
        if (get_alarm_state() == ALARM_IDEL)
        {
            if (alarm_nearest)
            {
                current_seconds = GET_current_second();
                if (current_seconds >= alarm_nearest->alarm_data_for_eland.moment_second)
                {
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
    return alarm_list.state;
}
static void set_alarm_state(_alarm_list_state_t state)
{
    alarm_list.state = state;
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
    set_alarm_state(ALARM_IDEL);
    mico_rtos_unlock_mutex(&alarm_list.AlarmListMutex);
}
void elsv_alarm_data_init_MCU(__elsv_alarm_data_t *elsv_alarm_data, _alarm_mcu_data_t *alarm_mcu_data)
{
    if ((elsv_alarm_data == NULL) || (alarm_mcu_data == NULL))
        return;
    memset(elsv_alarm_data, 0, sizeof(__elsv_alarm_data_t));
    memcpy(alarm_mcu_data, &elsv_alarm_data->alarm_data_for_mcu, sizeof(_alarm_mcu_data_t));
    strcpy(elsv_alarm_data->alarm_id, ALARM_ID_OF_SIMPLE_CLOCK);
    elsv_alarm_data->alarm_color = 0;
    sprintf(elsv_alarm_data->alarm_time, "%02d:%02d:%02d", alarm_mcu_data->moment_time.hr, alarm_mcu_data->moment_time.min, alarm_mcu_data->moment_time.sec);
    elsv_alarm_data->snooze_enabled = 0;
    elsv_alarm_data->alarm_pattern = 1;
    elsv_alarm_data->alarm_sound_id = 1;
    elsv_alarm_data->alarm_volume = 80;
    elsv_alarm_data->alarm_continue_min = 5;
    elsv_alarm_data->alarm_repeat = 1;

    elsv_alarm_data->alarm_data_for_eland.moment_second = (uint32_t)alarm_mcu_data->moment_time.hr * 3600 +
                                                          (uint32_t)alarm_mcu_data->moment_time.min * 60 +
                                                          (uint32_t)alarm_mcu_data->moment_time.sec;
    elsv_alarm_data->alarm_data_for_eland.color = alarm_mcu_data->color;
    elsv_alarm_data->alarm_data_for_eland.snooze_count = 0;
    elsv_alarm_data->alarm_data_for_eland.alarm_on_days_of_week = 0x7f;
}
static __elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list)
{
    uint8_t i;
    __elsv_alarm_data_t *elsv_alarm_data = NULL;
    uint32_t current_seconds = 0;

    list->list_refreshed = false;
    list->alarm_stoped = false;
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
    return elsv_alarm_data;
}
void alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData)
{
    uint8_t i;
    mico_rtos_lock_mutex(&alarm_list.AlarmListMutex);
    set_alarm_state(ALARM_ADD);
    if (AlarmList->alarm_number == 0)
    {
        AlarmList->alarm_lib = calloc(sizeof(__elsv_alarm_data_t), sizeof(uint8_t));
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
                memcpy((AlarmList->alarm_lib + i), inData, sizeof(__elsv_alarm_data_t));
                AlarmList->list_refreshed = true;
                goto exit;
            }
        }
        /**a new alarm**/
        if (i == AlarmList->alarm_number)
        {
            realloc(AlarmList->alarm_lib, (AlarmList->alarm_number + 1) * sizeof(__elsv_alarm_data_t));
            memcpy((AlarmList->alarm_lib + i), inData, sizeof(__elsv_alarm_data_t));
            AlarmList->list_refreshed = true;
            AlarmList->alarm_number++;
        }
    }
exit:
    set_alarm_state(ALARM_IDEL);
    mico_rtos_unlock_mutex(&alarm_list.AlarmListMutex);
}

void alarm_operation(__elsv_alarm_data_t *alarm)
{
    _alarm_list_state_t current_state;

    do
    {
        current_state = get_alarm_state();
        switch (current_state)
        {
        case ALARM_IDEL:
        case ALARM_ING:
            Play_Alarm();
            break;
        case ALARM_ADD:
        case ALARM_MINUS:
        case ALARM_SORT:
        case ALARM_STOP:
            Stop_Alarm();
            alarm_list.alarm_stoped == true;
            break;
        case ALARM_SNOOZ_STOP:
            Stop_Alarm();
            break;
        default:
            break;
        }
    } while (alarm_list.alarm_stoped == false);
}

static void Play_Alarm(__elsv_alarm_data_t *alarm)
{
    mscp_result_t result;
    mscp_status_t audio_status;
    static bool isVoice = true;
    audio_service_get_audio_status(&result, &audio_status);
    if (audio_status == MSCP_STATUS_IDLE)
    {
        if ((alarm->alarm_pattern == 1) || (alarm->alarm_pattern == 4))
            audio_service_sound_remind_start(&result, 12);
        else if (alarm->alarm_pattern == 2)
        {
            /***paly voice***/
        }
        else if (alarm->alarm_pattern == 3)
        {
            if (isVoice)
            {
                /***paly voice***/
            }
            else
                audio_service_sound_remind_start(&result, 12);
        }
    }
    if ((audio_status == MSCP_STATUS_STREAM_PLAYING) || (audio_status == MSCP_STATUS_SOUND_REMINDING))
        return;
}
