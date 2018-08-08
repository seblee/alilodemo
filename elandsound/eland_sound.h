/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-02-04 16:29:31
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-07-04 14:46:24
 * @brief   :
 ****************************************************************************
**/
#ifndef __ELAND_SOUND_H_
#define __ELAND_SOUND_H_

/* Private include -----------------------------------------------------------*/
#include "flash_kh25.h"
#include "eland_alarm.h"

/* Private define ------------------------------------------------------------*/
#define ALARM_ID_LEN 36
#define ALARM_SIZE_LEN 4
#define ALARM_FILE_HEADER_LEN 9
#define ALARM_FILE_HEADER_STRING "ALARMFILE"
#define ALARM_FILE_END_STRING "ALARMFILEEND"
#define SOUND_STREAM_DEFAULT_LENGTH 1500

#define SOUND_FILE_VID (uint8_t)0X01
#define SOUND_FILE_SID (uint8_t)0X02
#define SOUND_FILE_OFID (uint8_t)0X03
#define SOUND_FILE_DEFAULT (uint8_t)0X04

#define SOUND_FILE_WEATHER_0 (uint8_t)0X05
#define SOUND_FILE_WEATHER_A (uint8_t)0X06
#define SOUND_FILE_WEATHER_B (uint8_t)0X07
#define SOUND_FILE_WEATHER_C (uint8_t)0X08
#define SOUND_FILE_WEATHER_D (uint8_t)0X09
#define SOUND_FILE_WEATHER_E (uint8_t)0X0a
#define SOUND_FILE_WEATHER_F (uint8_t)0X0b

#define KH25_CHECK_ADDRESS (KH25_FLASH_FILE_COUNT * KH25L8006_SECTOR_SIZE)
#define DOWNLOAD_FLAG_ADDRESS KH25_CHECK_ADDRESS + strlen(FLASH_KH25_CHECK_STRING)

#define START_ADDRESS_AREA0 0U
#define START_ADDRESS_AREA1 49152U
#define START_ADDRESS_AREA2 389120U
#define START_ADDRESS_AREA3 729088U
#define START_ADDRESS_AREA4 1069056U
#define START_ADDRESS_AREA5 1409024U
#define START_ADDRESS_AREA6 1748992U
#define START_ADDRESS_RESERVE 2088960U

#define START_SECTOR_AREA0 0
#define START_SECTOR_AREA1 12
#define START_SECTOR_AREA2 95
#define START_SECTOR_AREA3 178
#define START_SECTOR_AREA4 261
#define START_SECTOR_AREA5 344
#define START_SECTOR_AREA6 427
#define START_SECTOR_RESERVE 510

#define SECTOR_AREA0 12
#define SECTOR_AREA1 83
#define SECTOR_AREA2 83
#define SECTOR_AREA3 83
#define SECTOR_AREA4 83
#define SECTOR_AREA5 83
#define SECTOR_AREA6 83
#define SECTOR_RESERVE 2

/* Private typedef -----------------------------------------------------------*/

typedef struct __SOUND_FILE_TYPE_
{
    char flag[ALARM_FILE_HEADER_LEN + 1];
    char alarm_ID[ALARM_ID_LEN + 1];
    uint8_t sound_type;
    uint32_t file_len;
    uint32_t file_address;
} _sound_file_type_t;

typedef struct __SOUND_FILE_LIB_
{
    _sound_file_type_t lib[7];
    uint8_t exist_flag;
    uint8_t reading_point;
    uint8_t writing_point;
} _sound_file_lib_t;

typedef enum _FILE_OPERATION_MODE_
{
    FILE_READ,   /**read file**/
    FILE_WRITE,  /**write file**/
    FILE_REMOVE, /**remove file**/
} _file_operation_t;

typedef enum _FILE_WRITE_STATE
{
    WRITE_ING, /**writing file**/
    WRITE_END, /**the latest file data**/
} __file_write_state_t;
typedef struct __SOUND_READ_WRITE_TYPE_
{
    char alarm_ID[ALARM_ID_LEN + 1];
    uint8_t sound_type;
    __file_write_state_t write_state;
    _file_operation_t operation_mode;
    uint32_t total_len;
    uint32_t file_address;
    uint32_t len;
    uint32_t pos;
    uint8_t *sound_data;
} _sound_read_write_type_t;

typedef struct SOUND_OPRATION_STRUCT__
{
    _sound_read_write_type_t *alarm_w_r_queue;
    mico_mutex_t mutex;
} _http_w_r_struct_t;

typedef enum
{
    ERRNONE,
    FILE_NOT_FIND,
} SOUND_FILE_ERR_TYPE;

typedef enum
{
    FILE_SCAN_START,     //沒有文件
    FIND_A_FILE,         //有文件
    GET_FILE_SCAN_START, //開始掃描文件起始
    GET_FILE_START,      //找到文件起始
    GET_FILE_END,        //找到文件結尾
    FILE_SCAN_END,       //文件掃描結束
} SOUND_FILE_SCAN_STATUS;
typedef enum SOUND_ROM_T
{
    ROM_FLASH,   //ERROR SOUND
    ROM_ERROR,   //ERROR SOUND
    ROM_DEFAULT, //DEFAULT SOUND
} _sound_rom_t;

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern _http_w_r_struct_t HTTP_W_R_struct;
extern _sound_file_lib_t sound_file_list;
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
OSStatus start_eland_flash_service(void);
OSStatus sound_file_sort(_sound_file_lib_t *sound_list);
OSStatus alarm_sound_file_check(char *alarm_id);
OSStatus sound_file_read_write(_sound_file_lib_t *sound_list, _sound_read_write_type_t *alarm_w_r_temp);
OSStatus SOUND_CHECK_DEFAULT_FILE(void);
OSStatus SOUND_FILE_CLEAR(void);
void file_download(void);
uint8_t get_flash_capacity(void);
OSStatus eland_sound_file_arrange(_sound_file_lib_t *sound_list);
OSStatus eland_play_rom_sound(_sound_rom_t SOUND);
/*************************/

#endif
