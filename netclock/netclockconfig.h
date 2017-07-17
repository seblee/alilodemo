/*
 * netclockconfig.h
 *
 *  Created on: 2017å¹´7æœˆ7æ—¥
 *      Author: ceeu
 */

#ifndef NETCLOCK_NETCLOCKCONFIG_H_
#define NETCLOCK_NETCLOCKCONFIG_H_
#include "mico.h"

#define Eland_ID ("0123456789ABCDEF")    //Eland唯一识别的ID
#define Eland_Firmware_Version ("01.00") //Eland固件版本号
#define Serial_Number ("01000001")       //Eland的串口番号

#define Timezone_offset_sec_Min ((int32_t)-43200) //时区offset最小值
#define Timezone_offset_sec_Max ((int32_t)50400)  //时区offset最大值
#define DEVICE_MAC_LEN (12)                       //MAC地址长度

#define ELAND_AP_SSID ("Eland")   //配置模式ssid
#define ELAND_AP_KEY ("12345678") //配置模式key

#define ElandID_Len 16     //Eland唯一识别的ID
#define ElandName_Len 16   //Eland名称，用户输入
#define ElandSerial_len 20 //Eland的串口番号。
#define ElandFW_V_Len 10   //Eland固件版本号
#define ElandMAC_Len 13    //MAC地址
#define ElandSsid_Len 32   //wifi 用户名长度
#define ElandKey_Len 64    //wifi 密码长度
#define ElandIPMax_Len 16  //Eland IP地址
#define Time_Len 9         //"HH:mm:ss"的形式
#define URL_Len 32         //URL长度
#define DateTime_Len 20    //闹钟播报的时间 "yyyy-MM-dd HH:mm:ss"的形式。

typedef struct _ElandAlarmData //闹钟情報结构体
{
    int32_t AlarmID;                     //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    char AlarmDateTime[DateTime_Len];    //闹钟播报的时间 "yyyy-MM-dd HH:mm:ss"的形式。
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
    int32_t AlarmID;           //闹钟唯一识别的ID ELSV是闹钟设定时自动取号
    char AlarmOnDateTime[25];  //闹钟播放时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    char AlarmOffDateTime[25]; //闹钟停止时间。"yyyy-MM-dd HH:mm:ss"的形式。 （ex : "2017-06-21 15:30:00"）
    int32_t AlarmOffReason;    //闹钟停止的理由。 1 : 用户操作 2 : 自動停止
} AlarmOffHistoryData;

typedef struct _ELAND_DES_S //设备状态结构
{
    bool IsActivate;       //是否已激活设备
    bool IsHava_superuser; //是否拥有超级用户
    bool IsRecovery;       //是否需要回收授权

    char ElandID[ElandID_Len];                 //Eland唯一识别的ID
    int32_t UserID;                            //用户唯一识别ID，用户登录时获取
    char ElandName[ElandName_Len];             //Eland名称，用户输入
    int32_t ElandZoneOffset;                   //Eland的时区的UTC的offset秒 日本标准时为UTC + 09 : 00  「32400」
    char ElandSerial[ElandSerial_len];         //Eland的串口番号。
    char ElandFirmwareVersion[ElandFW_V_Len];  //Eland固件版本号
    char ElandMAC[ElandMAC_Len];               //MAC地址
    char Wifissid[ElandSsid_Len];              //MAC地址
    char WifiKey[ElandKey_Len];                //MAC地址
    int32_t ElandDHCPEnable;                   //Eland IP地址自动取号是否有效的标志 0 : 無効 1 : 有効
    char ElandIPstr[ElandIPMax_Len];           //Eland IP地址
    char ElandSubnetMask[ElandIPMax_Len];      //Eland的IPv4子网掩码。
    char ElandDefaultGateway[ElandIPMax_Len];  //Eland的IPv4默认网关。
    int32_t ElandBackLightOffEnable;           //指定時刻内背光是否熄灭的标志  0 : 無効 1 : 有効
    char ElandBackLightOffBeginTime[Time_Len]; //设定背光开始熄灭的时刻 "HH:mm:ss"的形式。（ex: "05:00:00"）
    char ElandBackLightOffEndTime[Time_Len];   //设定背光开始熄灭的时刻 "HH:mm:ss"的形式。（ex: "05:00:00"）
    char ElandFirmwareUpdateUrl[URL_Len];      //固件升级下载的URL
    ElandAlarmData ElandNextAlarmData;         //下次播报予定的闹钟情報

} ELAND_DES_S;

#endif /* NETCLOCK_NETCLOCKCONFIG_H_ */
