#ifndef __ELAND_SOUND_H_
#define __ELAND_SOUND_H_

#include "flash_kh25.h"
#include "eland_alarm.h"
#define ALARM_ID_LEN 36
#define ALARM_SIZE_LEN 4
#define ALARM_FILE_FLAG_LEN 9
#define ALARM_FILE_FLAG_STRING "ALARMFILE"

#define SOUND_STREAM_DEFAULT_LENGTH 2000

extern mico_queue_t eland_sound_R_W_queue;    //flash sound 讀寫命令队列
extern mico_queue_t eland_sound_reback_queue; //flash sound 讀寫完成返回队列

typedef struct __SOUND_FILE_TYPE_
{
    char flag[ALARM_FILE_FLAG_LEN + 1];
    char alarm_ID[ALARM_ID_LEN + 1];
    uint32_t file_len;
    uint32_t file_address;
} _sound_file_type_t;

typedef struct __SOUND_READ_WRITE_TYPE_
{
    char alarm_ID[ALARM_ID_LEN + 1];
    bool is_read;
    uint32_t total_len;
    uint32_t file_address;
    uint32_t len;
    uint32_t pos;
    uint8_t *sound_data;
} _sound_read_write_type_t;

typedef enum {
    ERRNONE,
    FILE_NOT_FIND,
} SOUND_FILE_ERR_TYPE;
typedef struct __SOUND_CALLBACK_TYPE_
{
    char alarm_ID[ALARM_ID_LEN + 1];
    SOUND_FILE_ERR_TYPE read_write_err;
} _sound_callback_type_t;

typedef enum {
    FILE_SCAN_START,     //沒有文件
    FIND_A_FILE,         //有文件
    GET_FILE_SCAN_START, //開始掃描文件起始
    GET_FILE_START,      //找到文件起始
    GET_FILE_END,        //找到文件結尾
    FILE_SCAN_END,       //文件掃描結束
} SOUND_FILE_SCAN_STATUS;

/*************************/
OSStatus start_eland_flash_service(void);

#endif
