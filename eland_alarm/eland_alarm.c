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

/* Private functions ---------------------------------------------------------*/
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
    _alarm_MCU_data_t *alarm_mcu_data = NULL;
    int ye, mo, da, ho, mi, se;
    uint8_t i;

    if (elsv_alarm_data == NULL)
        return;
    alarm_eland_data = &(elsv_alarm_data->alarm_data_for_eland);
    alarm_mcu_data = &(elsv_alarm_data->alarm_data_for_eland);
    sscanf(&(elsv_alarm_data->alarm_time), "%2d:%2d:%2d", &ho, &mi, &se);

    alarm_eland_data->alarm_moment = (uint32_t)ho * 3600 + (uint32_t)mi * 60 + (uint32_t)se;
    alarm_mcu_data->moment_time.hr = ho;
    alarm_mcu_data->moment_time.min = mi;
    alarm_mcu_data->moment_time.sec = se;

    alarm_eland_data->color = elsv_alarm_data->alarm_colro;
    alarm_mcu_data->color = elsv_alarm_data->alarm_colro;

    if (snooze_enabled)
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
            if (elsv_alarm_data->alarm_on_days_of_week[i] = '1')
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
void elsv_alarm_data_init_MCU(__elsv_alarm_data_t *elsv_alarm_data, _alarm_MCU_data_t *alarm_mcu_data)
{
    if ((elsv_alarm_data == NULL) || (alarm_mcu_data == NULL))
        return;
    memset(elsv_alarm_data, 0, sizeof(__elsv_alarm_data_t));
    memcpy(alarm_mcu_data, elsv_alarm_data->alarm_data_for_mcu, sizeof(_alarm_MCU_data_t));
    strcpy(elsv_alarm_data->alarm_id, ALARM_ID_OF_SIMPLE_CLOCK, strlen(ALARM_ID_OF_SIMPLE_CLOCK));
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
__elsv_alarm_data_t *Alarm_ergonic_list(_eland_alarm_list_t *list)
{
    uint8_t i;
    __elsv_alarm_data_t *elsv_alarm_data = NULL;
    uint32_t current_seconds = 0;

    list->list_refreshed = false;
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

void Alarm_Manager(uint32_t arg)
{
    bool alarm_stop_flag = false;
    __elsv_alarm_data_t *alarm_nearest;
    uint32_t current_seconds = 0;
    while (1)
    {
        if ((alarm_list.list_refreshed) || (alarm_stop_flag == true))
        {
            alarm_stop_flag = false;
            /**refresh point**/
            alarm_nearest = Alarm_ergonic_list(&alarm_list);
        }
        if (alarm_nearest)
        {
            current_seconds = GET_current_second();
            if (current_seconds > alarm_nearest->alarm_data_for_eland.moment_second)
            {
                /**operation output alarm**/
            }
        }
        /* sleep 1s */
        mico_rtos_thread_sleep(1);
    }
exit:

    mico_rtos_delete_thread(NULL);
}
