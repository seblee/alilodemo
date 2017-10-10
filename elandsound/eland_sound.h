#ifndef __ELAND_SOUND_H_
#define __ELAND_SOUND_H_

#include "flash_kh25.h"

#define ALARM_ID_LEN 36
#define ALARM_SIZE_LEN 4
#define ALARM_FILE_FLAG_LEN 8
#define ALARM_FILE_FLAG_STRING "ALARMFIL"

typedef struct __SOUND_FILE_TYPE_
{
    char flag[ALARM_FILE_FLAG_LEN];
    char alarm_ID[ALARM_ID_LEN];
    uint32_t file_len;
    uint32_t file_address;
} _sound_file_type_t;

typedef struct __SOUND_READ_WRITE_TYPE_
{
    char alarm_ID[ALARM_ID_LEN];
    bool is_read;
    uint32_t len;
    uint32_t pos;
    char *sound_data;
} _sound_read_write_type_t;

#endif
