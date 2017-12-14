/*
 * netclockconfig.h
 *
 *  Created on: 2017å¹´7æœˆ7æ—¥
 *      Author: ceeu
 */

#ifndef NETCLOCK_NETCLOCKCONFIG_H_
#define NETCLOCK_NETCLOCKCONFIG_H_
#include "mico.h"

#define Eland_Firmware_Version ("01.00") //Eland固件版本号

#define Timezone_offset_sec_Min ((int32_t)-43200) //时区offset最小值
#define Timezone_offset_sec_Max ((int32_t)50400)  //时区offset最大值
#define DEVICE_MAC_LEN (17)                       //MAC地址长度

#define ELAND_AP_SSID ("Eland")   //配置模式ssid
#define ELAND_AP_KEY ("12345678") //配置模式key

#define user_id_len 37          //Eland name
#define eland_name_Len 151      //Eland名称，用户输入
#define serial_number_len 12    //Eland的串口番号。
#define firmware_version_len 6  //Eland固件版本号
#define mac_address_Len 18      //MAC地址
#define ip_address_Len maxIpLen //Eland IP地址 必須為16，防止內存操作有問題
#define ElandSsid_Len 32        //wifi 用户名长度
#define ElandKey_Len 64         //wifi 密码长度
#define Time_Format_Len 9       //"HH:mm:ss"的形式
#define DateTime_Len 19         //闹钟播报的时间 "yyyy-MM-dd HH:mm:ss"的形式。
#define Date_formate_len 11     //日期为"yyyy-MM-dd"的形式
#define URL_Len 128             //URL长度

#define alarm_id_len 37   //闹钟唯一识别的ID。
#define alarm_color_len 8 //闹钟颜色的RGB字符串。"#RRGGBB"的形式。

typedef struct _TIME_FORMAT_
{
    char hour[2];
    char colon1;
    char minute[2];
    char colon2;
    char second[2];
} TIME_FORMAT;

typedef struct _ElandAlarmData //闹钟情報结构体
{
    int32_t AlarmID;                     //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    iso8601_time_t AlarmDateTime;        //闹钟播报的时间 "yyyy-MM-dd HH:mm:ss"的形式。
    int32_t SnoozeEnabled;               //继续响铃 有效标志  0：无效   1：有效
    int32_t SnoozeCount;                 //继续响铃的次数
    int32_t SnoozeIntervalMin;           //继续响铃的间隔。単位「分」
    int32_t AlarmPattern;                //闹钟的播放样式。 1 : 只有闹钟音 2 : 只有语音 3 : digital音和语音（交互播放） 4 : 闹钟OFF后的语音
    char AlarmSoundDownloadURL[URL_Len]; //从ELSV下载闹钟音的URL
    char AlarmVoiceDownloadURL[URL_Len]; //从ELSV下载语音的URL
    int32_t AlarmVolume;                 //闹钟的音量
    int32_t VolumeStepupEnabled;         //音量升高功能是否有效
    int32_t AlarmContinueMin;            //闹钟自动停止为止的时间。 単位は「分」
} ElandAlarmData;
typedef struct _AlarmOffHistoryData //闹钟履历结构体
{
    int32_t AlarmID;                 //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    iso8601_time_t AlarmOnDateTime;  //闹钟播放时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    iso8601_time_t AlarmOffDateTime; //闹钟停止时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    int32_t AlarmOffReason;          //闹钟停止的理由。 1 : 用户操作 2 : 自動停止
} AlarmOffHistoryData;

typedef struct _ELAND_DES_S //设备状态结构
{
    int32_t eland_id;          //Eland唯一识别的ID
    char user_id[user_id_len]; //用户唯一识别ID，用户登录时获取

    char serial_number[serial_number_len]; //Eland的串口番号。
    char eland_name[eland_name_Len];       //Eland名称，用户输入

    int32_t timezone_offset_sec;                 //Eland的时区的UTC的offset秒 日本标准时为UTC + 09 : 00  「32400」
    char firmware_version[firmware_version_len]; //Eland固件版本号
    char mac_address[mac_address_Len];           //MAC地址
    int8_t dhcp_enabled;                         //Eland IP地址自动取号是否有效的标志 0 : 無効 1 : 有効
    char ip_address[ip_address_Len];             //Eland IP地址
    char subnet_mask[ip_address_Len];            //Eland的IPv4子网掩码。
    char default_gateway[ip_address_Len];        //Eland的IPv4默认网关。
    char dnsServer[ip_address_Len];              //DNS server ip address
    int8_t time_display_format;                  //12小时显示还是24小时显示的代码  1:12時間表示(AM/PM表示) 2 : 24時間表示
    int8_t brightness_normal;                    //通常时的液晶显示亮度。背光亮度
    int8_t brightness_night;                     //夜间模式时的液晶显示亮度、背光亮度
    int8_t night_mode_enabled;                   //指定時刻间、是否调节背光亮度
    char night_mode_begin_time[Time_Format_Len]; //设定背光的亮度调节的开始时刻
    char night_mode_end_time[Time_Format_Len];   //设定背光的亮度调节的結束时刻
    char Wifissid[ElandSsid_Len];                //wifi 賬號
    char WifiKey[ElandKey_Len];                  //wifi 密碼

    char tcpIP_host[ip_address_Len];
    uint16_t tcpIP_port;
} ELAND_DES_S;

typedef struct _ELAND_DEVICE //设备状态结构
{
    bool IsActivate;   //是否已激活设备
    bool IsAlreadySet; //是否已經寫入設備碼 eg.

    int32_t eland_id;                      //Eland唯一识别的ID
    char serial_number[serial_number_len]; //Eland的串口番号。

    /*APP/tcpIP通信時獲取*/
    char user_id[user_id_len];            //用户唯一识别ID
    int8_t dhcp_enabled;                  //Eland IP地址自动取号是否有效的标志 0 : 無効 1 : 有効
    char ip_address[ip_address_Len];      //Eland IP地址
    char subnet_mask[ip_address_Len];     //Eland 的IPv4子网掩码。
    char default_gateway[ip_address_Len]; //Eland 的IPv4默认网关。
    char dnsServer[ip_address_Len];       //DNS server ip address
} _ELAND_DEVICE_t;
#endif /* NETCLOCK_NETCLOCKCONFIG_H_ */
