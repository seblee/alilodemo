/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-02-04 16:29:31
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-04-10 16:13:26
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
#define SOUND_STREAM_DEFAULT_LENGTH 2000

#define SOUND_FILE_VID (uint8_t)0X01
#define SOUND_FILE_SID (uint8_t)0X02
#define SOUND_FILE_OFID (uint8_t)0X03
#define SOUND_FILE_DEFAULT (uint8_t)0X04
#define SOUND_FILE_WEATHER_0 (uint8_t)0X05
#define SOUND_FILE_WEATHER_E (uint8_t)0X06
#define SOUND_FILE_WEATHER_F (uint8_t)0X07
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
    _sound_file_type_t *lib;
    uint16_t file_number;
    uint32_t sector_start;
    uint32_t sector_end;
} _sound_file_lib_t;

typedef enum _FILE_OPERATION_MODE_
{
    FILE_READ,   /**read file**/
    FILE_WRITE,  /**write file**/
    FILE_REMOVE, /**remove file**/
} _file_operation_t;
typedef struct __SOUND_READ_WRITE_TYPE_
{
    char alarm_ID[ALARM_ID_LEN + 1];
    uint8_t sound_type;
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

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
extern _http_w_r_struct_t HTTP_W_R_struct;
extern _sound_file_lib_t sound_file_list;
/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/
OSStatus start_eland_flash_service(void);
OSStatus sound_file_sort(_sound_file_lib_t *sound_list);
OSStatus sound_file_read_write(_sound_file_lib_t *sound_list, _sound_read_write_type_t *alarm_w_r_temp);
OSStatus SOUND_CHECK_DEFAULT_FILE(void);
OSStatus SOUND_FILE_CLEAR(void);
void file_download(void);
int32_t get_flash_capacity(void);
OSStatus eland_sound_file_arrange(_sound_file_lib_t *sound_list);
/*************************/

#endif
