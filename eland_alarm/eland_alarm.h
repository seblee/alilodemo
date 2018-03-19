/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-01-12 09:59:28
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-15 09:58:38
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_ALARM_H_
#define __ELAND_ALARM_H_
/* Private include -----------------------------------------------------------*/
#include "mico.h"
/* Private define ------------------------------------------------------------*/
#define ALARM_ID_LEN 36        //闹钟唯一识别的ID。
#define ALARM_TIME_LEN 8       //時刻是"HH:mm:ss"的形式。
#define ALARM_OFF_DATES_LEN 11 //日期是"yyyy-MM-dd"的形式
#define ALARM_ON_DATES_COUNT 10
#define VOICE_ALARM_ID_LEN 37       //语音闹钟的ID
#define ALARM_ON_DAYS_OF_WEEK_LEN 8 //鬧鐘播放的星期幾
#define ALARM_ID_OF_SIMPLE_CLOCK "simple_clock"

#define SECOND_ONE_DAY 86400
#define SECOND_ONE_HOUR 3600
#define SECOND_ONE_MINUTE 60

#define SIMULATE_DAYS_OF_YEAR 410
#define SIMULATE_DAYS_OF_MONTH 32
/* Private typedef -----------------------------------------------------------*/
typedef struct
{
    mico_rtc_time_t moment_time; //
    int8_t color;
    int8_t snooze_count;
    int8_t alarm_repeat;
    uint8_t alarm_on_days_of_week;
} _alarm_mcu_data_t;
typedef struct __SOUND_FILE_INFO_
{
    uint32_t file_len;
    uint32_t file_address;
} _sound_file_info_t;
typedef struct
{
    uint32_t moment_second; //
    int8_t color;
    int8_t snooze_count;
    uint8_t alarm_on_days_of_week;
} _alarm_eland_data_t;

typedef struct _ELSV_ALARM_DATA //闹钟情報结构体
{
    char alarm_id[ALARM_ID_LEN + 1];                       //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    int8_t alarm_color;                                    //鬧鐘顏色
    char alarm_time[ALARM_TIME_LEN + 1];                   //闹钟播放时刻
    char alarm_off_dates[ALARM_OFF_DATES_LEN + 1];         //不播放闹钟的日期的排列 暫定一個
    int8_t snooze_enabled;                                 //继续响铃 有效标志  0：无效   1：有效
    int8_t snooze_count;                                   //继续响铃的次数
    int8_t snooze_interval_min;                            //继续响铃的间隔。単位「分」
    int8_t alarm_pattern;                                  //闹钟的播放样式。 1 : 只有闹钟音 2 : 只有语音 3 : digital音和语音（交互播放） 4 : 闹钟OFF后的语音
    int32_t alarm_sound_id;                                //鬧鈴音ID
    char voice_alarm_id[VOICE_ALARM_ID_LEN];               //语音闹钟的ID
    char alarm_off_voice_alarm_id[VOICE_ALARM_ID_LEN];     //Alarm off 後语音闹钟的ID
    int8_t alarm_volume;                                   //the volume of voice when alarming
    int8_t volume_stepup_enabled;                          //音量升高功能是否有效的标志
    int8_t alarm_continue_min;                             //鬧鐘自動停止的時間
    int8_t alarm_repeat;                                   //鬧鐘反復條件
    uint16_t alarm_on_dates[ALARM_ON_DATES_COUNT];         //鬧鐘日期排列
    char alarm_on_days_of_week[ALARM_ON_DAYS_OF_WEEK_LEN]; //鬧鐘播放的星期幾
    _alarm_eland_data_t alarm_data_for_eland;
    _alarm_mcu_data_t alarm_data_for_mcu;
} __elsv_alarm_data_t;

typedef struct _ELSV_HOLIDAY //闹钟情報结构体
{
    uint8_t number;
    uint16_t *list;
    mico_mutex_t holidaymutex;
} _elsv_holiday_t;

typedef enum {
    ALARM_IDEL,
    ALARM_ADD,
    ALARM_MINUS,
    ALARM_SORT,
    ALARM_ING,
    ALARM_SNOOZ_STOP,
    ALARM_STOP,
    ALARM_JUMP,
} _alarm_list_state_t;

typedef struct
{
    _alarm_list_state_t state;
    bool alarm_stoped;
    mico_mutex_t AlarmStateMutex;
} _alarm_state_t;
typedef enum {
    STREAM_IDEL,
    STREAM_PLAY,
    STREAM_STOP,
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
typedef enum {
    ALARM_ON = (uint8_t)0,
    ALARM_OFF_SNOOZE = (uint8_t)1,
    ALARM_OFF_ALARMOFF = (uint8_t)2,
    ALARM_OFF_AUTOOFF = (uint8_t)3,
    ALARM_SNOOZE = (uint8_t)4,
} alarm_off_history_record_t;
typedef enum {
    IDEL_UPLOAD = 0,
    READY_UPLOAD,
    WAIT_UPLOAD,
    DONE_UPLOAD,
} HistoryDatastate_t;
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
    mico_semaphore_t alarm_off_sem;
    AlarmOffHistoryData_t HistoryData;
    HistoryDatastate_t state;
} _alarm_off_history_t;

typedef struct
{
    bool list_refreshed;
    bool alarm_jump_flag;
    uint8_t alarm_number;
    mico_mutex_t AlarmlibMutex;
    _alarm_state_t state;
    /*******************/
    uint8_t alarm_display_serial;
    uint8_t alarm_waiting_serial;
    mico_mutex_t AlarmSerialMutex;
    /**********************/
    /************************/
    __elsv_alarm_data_t *alarm_lib;
    __elsv_alarm_data_t *alarm_nearest;
} _eland_alarm_list_t;

typedef enum {
    DOWNLOAD_IDEL,
    DOWNLOAD_SCAN,
    DOWNLOAD_OID,
    DOWNLOAD_OTA,
} _download_type_t;
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern _eland_alarm_list_t alarm_list;
extern _elsv_holiday_t holiday_list;
extern _alarm_stream_t alarm_stream;
extern mico_semaphore_t alarm_update;
extern mico_semaphore_t alarm_jump_sem;
extern mico_queue_t download_queue;

extern _alarm_off_history_t off_history;
/* Private function prototypes -----------------------------------------------*/
OSStatus alarm_list_add(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData);
OSStatus alarm_list_minus(_eland_alarm_list_t *AlarmList, __elsv_alarm_data_t *inData);
OSStatus alarm_list_clear(_eland_alarm_list_t *AlarmList);
void elsv_alarm_data_sort_out(__elsv_alarm_data_t *elsv_alarm_data);
OSStatus Start_Alarm_service(void);
OSStatus elsv_alarm_data_init_MCU(_alarm_mcu_data_t *alarm_mcu_data);
void alarm_print(__elsv_alarm_data_t *alarm_data);
_alarm_mcu_data_t *get_alarm_mcu_data(uint8_t serial);
uint8_t get_display_alarm_serial(void);
void set_display_alarm_serial(uint8_t serial);
uint8_t get_next_alarm_serial(uint8_t now_serial);
uint8_t get_previous_alarm_serial(uint8_t now_serial);
uint8_t get_waiting_alarm_serial(void);
void set_waiting_alarm_serial(uint8_t now_serial);
_alarm_list_state_t get_alarm_state(void);
void set_alarm_state(_alarm_list_state_t state);
uint8_t get_alarm_jump_flag(void);

void alarm_off_history_record_time(alarm_off_history_record_t type, iso8601_time_t *iso8601_time);

AlarmOffHistoryData_t *get_alarm_history_data(void);
HistoryDatastate_t get_alarm_history_data_state(void);
void set_alarm_history_data_state(HistoryDatastate_t value);
OSStatus alarm_off_history_json_data_build(AlarmOffHistoryData_t *HistoryData, char *json_buff);
void alarm_sound_scan(void);
OSStatus Alarm_build_JSON(char *json_str);

void UCT_Convert_Date(uint32_t *utc, mico_rtc_time_t *time);
/* Private functions ---------------------------------------------------------*/

#endif
