/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-06-13 09:34:44
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-06-13 10:57:45
 * @brief   :
 ****************************************************************************
**/
#ifndef NETCLOCK_NETCLOCKCONFIG_H_
#define NETCLOCK_NETCLOCKCONFIG_H_

/* Private include -----------------------------------------------------------*/
#include "mico.h"

/* Private define ------------------------------------------------------------*/
#define Timezone_offset_sec_Min ((int32_t)-43200) //时区offset最小值
#define Timezone_offset_sec_Max ((int32_t)50400)  //时区offset最大值
#define Timezone_offset_elsv ((int32_t)32400)     //elsv time zone
#define DEFAULT_TIMEZONE ((int32_t)32400)
#define DEFAULT_AREACODE ((int16_t)43)

#define DEVICE_MAC_LEN (17) //MAC地址长度

#define ELAND_AP_SSID ("LinkTime_LT10")     //配置模式ssid
#define ELAND_AP_KEY ("")                   //配置模式key
#define ELAND_AP_LOCAL_IP ("192.168.0.1")   //本體IP
#define ELAND_AP_DNS_SERVER ("192.168.0.1") //本體 DNS
#define ELAND_AP_NET_MASK ("255.255.255.0") //本體IP

#define ELAND_TEST_SSID ("TP-LINK_eland") //eland test ap
#define ELAND_TEST_KEY ("eland1234")      //eland test ap

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
#define hash_Len 33             //URL长度

#define DEFAULT_BRIGHTNESS_NORMAL 80
#define DEFAULT_BRIGHTNESS_NIGHT 20
#define DEFAULT_LED_NORMAL 80
#define DEFAULT_LED_NIGHT 20
#define DEFAULT_VOLUME_NORMAL 50
#define DEFAULT_VOLUME_NIGHT 50
#define DEFAULT_NIGHT_MODE_ENABLE 0

#define DEFAULT_NIGHT_BEGIN ("22:00:00")
#define DEFAULT_NIGHT_END ("06:00:00")

/* Private typedef -----------------------------------------------------------*/
typedef struct _TIME_FORMAT_
{
    char hour[2];
    char colon1;
    char minute[2];
    char colon2;
    char second[2];
} __time_format_t;

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
    char primary_dns[ip_address_Len];            //DNS server ip address
    int8_t time_display_format;                  //12小时显示还是24小时显示的代码  1:12時間表示(AM/PM表示) 2 : 24時間表示
    int8_t brightness_normal;                    //通常时的液晶显示亮度。背光亮度
    int8_t brightness_night;                     //夜间模式时的液晶显示亮度、背光亮度
    int8_t led_normal;                           //通常时的led显示亮度。
    int8_t led_night;                            //通常时的led显示亮度。
    int8_t notification_volume_normal;           //通常时的通知音量。
    int8_t notification_volume_night;            //夜间时的led显示亮度。
    int8_t night_mode_enabled;                   //指定時刻间、是否调节背光亮度
    char night_mode_begin_time[Time_Format_Len]; //设定背光的亮度调节的开始时刻
    char night_mode_end_time[Time_Format_Len];   //设定背光的亮度调节的結束时刻
    int16_t area_code;                           //天气预报的地域代码

    /*******************/
    uint8_t flash_check;
    /*****************/
    char Wifissid[ElandSsid_Len]; //wifi 賬號
    char WifiKey[ElandKey_Len];   //wifi 密碼

    char tcpIP_host[ip_address_Len];
    uint16_t tcpIP_port;

    char OTA_version[firmware_version_len];
    char OTA_url[URL_Len];
    char OTA_hash[hash_Len];

    mico_mutex_t des_mutex;
} ELAND_DES_S;

typedef struct _ELAND_DEVICE //设备状态结构
{
    bool IsActivate;   //是否已激活设备 0
    bool IsAlreadySet; //是否已經寫入設備碼  01

    uint32_t eland_id;                     //Eland唯一识别的ID 4
    char serial_number[serial_number_len]; //Eland的串口番号。8
    char eland_name[eland_name_Len + 1];   //Eland名称，用户输入 20
    int32_t timezone_offset_sec;           //Eland的时区的UTC的offset秒  172

    /*APP/tcpIP通信時獲取*/
    char user_id[user_id_len];            //用户唯一识别ID 176
    int8_t dhcp_enabled;                  //IP地址自动取号  213
    char ip_address[ip_address_Len];      //IP地址 214
    char subnet_mask[ip_address_Len];     //的IPv4子网掩码。 230
    char default_gateway[ip_address_Len]; //的IPv4默认网关。 246
    char primary_dns[ip_address_Len];     //DNS server ip address  262
    int16_t area_code;                    //天气预报的地域代码 278

    int8_t reserve[1024];
} _ELAND_DEVICE_t;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

#endif /* NETCLOCK_NETCLOCKCONFIG_H_ */
