/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:28
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-10 16:12:58
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_ALARM_H_
#define __ELAND_ALARM_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "netclock_uart.h"
#include "eland_sound.h"
/* Private define ------------------------------------------------------------*/
#define ALARM_ID_LEN 36  //闹钟唯一识别的ID。
#define ALARM_TIME_LEN 8 //時刻是"HH:mm:ss"的形式。
#define ALARM_DATES_LEN 11
#define ALARM_ON_OFF_DATES_COUNT 50
#define VOICE_ALARM_ID_LEN 37       //语音闹钟的ID
#define ALARM_ON_DAYS_OF_WEEK_LEN 8 //鬧鐘播放的星期幾
#define ALARM_ID_OF_DEFAULT_CLOCK "simple_clock"

#define SECOND_ONE_DAY 86400
#define SECOND_ONE_HOUR 3600
#define SECOND_ONE_MINUTE 60

#define SCHEDULE_MAX 50
/* Private typedef -----------------------------------------------------------*/

typedef enum
{
    ALARM_IDEL,
    ALARM_ADD,
    ALARM_MINUS,
    ALARM_SORT,
    ALARM_ING,
    ALARM_SNOOZ_STOP,
    ALARM_STOP,
    ALARM_SKIP,
} _alarm_list_state_t;
typedef enum
{
    AUDIO_NONE,
    AUDIO_PALY,
    AUDIO_STOP,
    AUDIO_STOP_PLAY,
} _alarm_play_tyep_t;

typedef struct
{
    mico_rtc_time_t moment_time;
    int8_t alarm_color;
    int8_t snooze_enabled;
    int8_t next_alarm;
    int8_t skip_flag;
    _ELAND_MODE_t mode;
    _alarm_list_state_t alarm_state;
} _alarm_mcu_data_t;

typedef struct
{
    uint32_t moment_second;
} _alarm_eland_data_t;

typedef struct _ELSV_ALARM_DATA //闹钟情報结构体
{
    char alarm_id[ALARM_ID_LEN + 1];                       //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    int8_t alarm_color;                                    //鬧鐘顏色
    char alarm_time[ALARM_TIME_LEN + 1];                   //闹钟播放时刻
    uint16_t alarm_off_dates[ALARM_ON_OFF_DATES_COUNT];    //不播放闹钟的日期的排列
    int8_t snooze_enabled;                                 //继续响铃 有效标志  0：无效   1：有效
    int8_t snooze_count;                                   //继续响铃的次数
    int8_t snooze_interval_min;                            //继续响铃的间隔。単位「分」
    int8_t alarm_pattern;                                  //闹钟的播放样式。 1 : 只有闹钟音 2 : 只有语音 3 : digital音和语音（交互播放）4:default alarm
    int32_t alarm_sound_id;                                //鬧鈴音ID
    char voice_alarm_id[VOICE_ALARM_ID_LEN];               //语音闹钟的ID
    char alarm_off_voice_alarm_id[VOICE_ALARM_ID_LEN];     //Alarm off 後语音闹钟的ID
    int8_t alarm_volume;                                   //the volume of voice when alarming
    int8_t volume_stepup_enabled;                          //音量升高功能是否有效的标志
    int8_t alarm_continue_min;                             //鬧鐘自動停止的時間
    int8_t alarm_repeat;                                   //鬧鐘反復條件
    uint16_t alarm_on_dates[ALARM_ON_OFF_DATES_COUNT];     //鬧鐘日期排列
    char alarm_on_days_of_week[ALARM_ON_DAYS_OF_WEEK_LEN]; //鬧鐘播放的星期幾
    int8_t skip_flag;
    _alarm_eland_data_t alarm_data_for_eland;
    _alarm_mcu_data_t alarm_data_for_mcu;
} __elsv_alarm_data_t;

typedef struct _ELSV_HOLIDAY //闹钟情報结构体
{
    uint8_t number;
    uint16_t *list;
    mico_mutex_t holidaymutex;
} _elsv_holiday_t;

typedef struct
{
    _alarm_list_state_t state;
    bool alarm_stoped;
    mico_mutex_t AlarmStateMutex;
} _alarm_state_t;
typedef enum
{
    STREAM_IDEL,
    STREAM_PLAY,
    STREAM_STOP,
    STREAM_DONE,
} _stream_state_t;

typedef struct
{
    mico_mutex_t stream_Mutex;
    uint8_t stream_id;
    char alarm_id[ALARM_ID_LEN + 1];
    uint8_t sound_type;
    uint16_t stream_count;
    _stream_state_t state;
} _alarm_stream_t;
typedef enum
{
    ALARM_ON = (uint8_t)0,
    ALARM_OFF_SNOOZE = (uint8_t)1,
    ALARM_OFF_ALARMOFF = (uint8_t)2,
    ALARM_OFF_AUTOOFF = (uint8_t)3,
    ALARM_OFF_SKIP = (uint8_t)4,
    ALARM_SNOOZE = (uint8_t)5,
} alarm_off_history_record_t;

typedef struct _AlarmOffHistoryData //闹钟履历结构体
{
    char alarm_id[ALARM_ID_LEN + 1];             //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    iso8601_time_t alarm_on_datetime;            //闹钟播放时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    iso8601_time_t alarm_off_datetime;           //闹钟停止时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    alarm_off_history_record_t alarm_off_reason; //闹钟停止的理由。1、鬧鐘繼續響鈴操作 2 鬧鐘停止操作 3、鬧鐘自動停止
    iso8601_time_t snooze_datetime;              //闹钟繼續響鈴时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
} AlarmOffHistoryData_t;
typedef struct
{
    mico_mutex_t off_Mutex;
    AlarmOffHistoryData_t HistoryData;
} _alarm_off_history_t;

typedef struct ALARM_SCHEDULES //闹钟显示列表
{
    uint32_t utc_second;
    int8_t alarm_color;
    int8_t snooze_enabled;
} _alarm_schedules_t;

typedef struct
{
    bool list_refreshed;
    _alarm_state_t state;
    /*******************/
    uint8_t na_display_serial;
    uint8_t alarm_na_serial;
    mico_mutex_t AlarmSerialMutex;
    /**********************/
    mico_mutex_t AlarmlibMutex;
    _alarm_schedules_t schedules[SCHEDULE_MAX];
    uint8_t schedules_num;
    /***********/
    __elsv_alarm_data_t *alarm_lib;
    mico_utc_time_t add_utc;
    uint8_t alarm_number;
    uint8_t alarm_now_serial;
    mico_mutex_t AlarmNearMutex;
    __elsv_alarm_data_t *alarm_nearest;
    /************************/
} _eland_alarm_list_t;

typedef enum
{
    DOWNLOAD_IDEL,
    DOWNLOAD_SCAN,
    DOWNLOAD_OID,
    DOWNLOAD_OTA,
    DOWNLOAD_WEATHER,
    GO_INTO_AP_MODE,
    GO_OUT,
} _download_type_t;
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern _eland_alarm_list_t alarm_list;
extern _elsv_holiday_t holiday_list;
extern _alarm_stream_t alarm_stream;
extern mico_semaphore_t alarm_update;
extern mico_semaphore_t alarm_skip_sem;
extern mico_queue_t http_queue;
extern mico_mutex_t time_Mutex;

extern _alarm_off_history_t off_history;
extern mico_queue_t history_queue;
/* Private function prototypes -----------------------------------------------*/
OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData);
OSStatus alarm_list_minus(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData);
OSStatus alarm_list_clear(_eland_alarm_list_t *AlarmList);
void elsv_alarm_data_sort_out(__elsv_alarm_data_t *elsv_alarm_data);
bool eland_alarm_is_same(__elsv_alarm_data_t *alarm1, __elsv_alarm_data_t *alarm2);
OSStatus Start_Alarm_service(void);
OSStatus elsv_alarm_data_init_MCU(_alarm_mcu_data_t *alarm_mcu_data);
void alarm_print(__elsv_alarm_data_t *alarm_data);
_alarm_mcu_data_t *get_alarm_mcu_data(void);
_alarm_schedules_t *get_schedule_data(void);
__elsv_alarm_data_t *get_nearest_alarm();
uint8_t get_display_na_serial(void);
uint8_t get_schedules_number(void);
void set_display_na_serial(uint8_t serial);
uint8_t get_next_alarm_serial(uint8_t now_serial);
uint8_t get_previous_alarm_serial(uint8_t now_serial);
_alarm_list_state_t get_alarm_state(void);
void set_alarm_state(_alarm_list_state_t state);
uint8_t get_alarm_number(void);

void alarm_off_history_record_time(alarm_off_history_record_t type, iso8601_time_t *iso8601_time);

OSStatus alarm_off_history_json_data_build(AlarmOffHistoryData_t *HistoryData, char *json_buff);
OSStatus alarm_sound_scan(void);
OSStatus alarm_sound_oid(void);
OSStatus weather_sound_scan(void);
OSStatus check_default_sound(void);
uint8_t eland_oid_status(bool style, uint8_t value);

OSStatus Alarm_build_JSON(char *json_str);
OSStatus eland_push_http_queue(_download_type_t msg);

int get_g_alldays(int year, int month, int day);
void get_d_withdays(int *year, int *mon, int *day, int days);
void UCT_Convert_Date(uint32_t *utc, mico_rtc_time_t *time);
void set_alarm_stream_state(_stream_state_t state);
_stream_state_t get_alarm_stream_state(void);

uint32_t GET_current_second(void);

void eland_alarm_control(uint16_t Count, uint16_t Count_Trg,
                         uint16_t Restain, uint16_t Restain_Trg,
                         _ELAND_MODE_t eland_mode);
void eland_volume_set(uint8_t value);
/* Private functions ---------------------------------------------------------*/

#endif
