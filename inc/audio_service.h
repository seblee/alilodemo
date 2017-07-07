#ifndef _AUDIO_SERVICE_H_
#define _AUDIO_SERVICE_H_

#include "mscp_type.h"
#include "mico.h"

#define AUDIO_SERVICE_DEBUG_ENABLE          (1)

#define AUDIO_STREAM_PLAY_HEAD_LEN          (8) // 1Byte(id) + 1Byte(type) + 4Byte(total len) + 2Byte(len)

#define AUDIO_MIC_RECORD_AMR_FILE_LEN_MAX   (60*1024)
#define AUDIO_MIC_RECORD_LEN_MAX            (2*1024*1024)

#define AUDIO_SERVICE_MD5_STR_LEN           (33) //33 bytes

typedef enum
{
  MSCP_BRANCH_CFM             = 0x00,
  MSCP_BRANCH_SYS             = 0x01,
  MSCP_BRANCH_AUDIO           = 0x02,
  MSCP_BRANCH_NOTIFY          = 0x03,

  MSCP_BRANCH_NUM
} mscp_branch_t;

typedef enum {
  MSCP_SYS_SET_RUN_MODE             = 0x01,
  MSCP_SYS_GET_SYSTEM_INFO          = 0x02,
  MSCP_SYS_FILE_DOWNLOAD_START      = 0x03,
  MSCP_SYS_FILE_DOWNLOAD_PROCESSING = 0x04,
  MSCP_SYS_FILE_DOWNLOAD_CANCEL     = 0x05,
  MSCP_SYS_FILE_DELETE              = 0x06,
  MSCP_SYS_GET_SYETEM_STATE         = 0x07,
  MSCP_SYS_NUM
} mscp_sys_leaf_t;

typedef enum {
  MSCP_AUDIO_COMMON_ERROR               = 0x00,
  MSCP_AUDIO_SD_PLAY_START              = 0x01,
  MSCP_AUDIO_SD_PLAY_NEXT               = 0x02,
  MSCP_AUDIO_SD_PLAY_PRE                = 0x03,
  MSCP_AUDIO_STREAM_PLAY_START          = 0x04,
  MSCP_AUDIO_STREAM_PLAY_STOP           = 0x05,
  MSCP_AUDIO_MIC_START                  = 0x06,
  MSCP_AUDIO_MIC_STOP                   = 0x07,
  MSCP_AUDIO_MIC_RESULT_RESULT_GET      = 0x08,
  MSCP_AUDIO_SOUND_REMIND               = 0x09,
  MSCP_AUDIO_SOUND_REMIND_FINISH        = 0x0A,
  MSCP_AUDIO_SD_PLAY_PAUSE              = 0x0B,
  MSCP_AUDIO_MUTE                       = 0x0C,
  MSCP_AUDIO_UNMUTE                     = 0x0D,
  MSCP_AUDIO_ENTER_ASR                  = 0x0E,
  MSCP_AUDIO_GET_ASR_RESULT             = 0x0F,
  MSCP_AUDIO_GET_AUDIO_STATUS           = 0x10,
  MSCP_AUDIO_SET_VOLUME_UP              = 0x11,
  MSCP_AUDIO_SET_VOLUME_DOWN            = 0x12,
  MSCP_AUDIO_STREAM_PLAY_PAUSE          = 0x13,
  MSCP_AUDIO_SWITCH_MENU_DIR            = 0x14,
  MSCP_AUDIO_SWITCH_DOWNLOAD_DIR        = 0x15,
  MSCP_AUDIO_SD_PLAY_CYCLE              = 0x16,
  MSCP_AUDIO_SD_PLAY_STOP               = 0x17,
  MSCP_AUDIO_STREAM_PLAY_CONTINUE       = 0x18,
  MSCP_AUDIO_SD_GET_PLAYING_FILENAME    = 0x19,
  MSCP_AUDIO_NUM
} mscp_audio_leaf_t;

typedef enum {
  MSCP_NOTIFICATION_GET_EVENT       = 0x00,
  MSCP_NOTIFICATION_NUM
} mscp_notification_leaf_t;

typedef enum
{
  MSCP_STATUS_ERR                   = 0x00,
  MSCP_STATUS_IDLE                  = 0x01,
  MSCP_STATUS_SD_PLAYING            = 0x02,
  MSCP_STATUS_RECODING              = 0x03,
  MSCP_STATUS_STREAM_PLAYING        = 0x04,
  MSCP_STATUS_SOUND_REMINDING       = 0x05,
  MSCP_STATUS_NUM
} mscp_status_t;

typedef enum {
  AUDIO_MIC_RESULT_TYPE_STREAM,
  AUDIO_MIC_RESULT_TYPE_FILE,
  AUDIO_MIC_RESULT_TYPE_NUM
} audio_mic_result_type_e;

typedef enum {
  AUDIO_MIC_RESULT_FORMAT_SPEEX,
  AUDIO_MIC_RESULT_FORMAT_AMR,
  AUDIO_MIC_RESULT_FORMAT_NUM
} audio_mic_record_format_e;

typedef enum {
  AUDIO_STREAM_TYPE_MP3,
  AUDIO_STREAM_TYPE_AMR,
  AUDIO_STREAM_TYPE_NUM
} audio_stream_type_e;

typedef enum {
  AUDIO_MENU_DIR_STATUS_INVALID,
  AUDIO_MENU_DIR_STATUS_TINGERGE,
  AUDIO_MENU_DIR_STATUS_JIANGGUSHI,
  AUDIO_MENU_DIR_STATUS_HAOXIGUAN,
  AUDIO_MENU_DIR_STATUS_NUM
} audio_menu_dir_status_e;

typedef enum {
  AUDIO_DOWNLOAD_DIR_STATUS_HAVE_FILES,
  AUDIO_DOWNLOAD_DIR_STATUS_NO_FILES,
  AUDIO_DOWNLOAD_DIR_STATUS_NUM
} audio_download_dir_status_e;

typedef enum {
  ROBOT_SET_CYCLE_MODE_ALL_CYCLE,
  ROBOT_SET_CYCLE_MODE_SINGLE_CYCLE,
  ROBOT_SET_CYCLE_MODE_RANDOM_CYCLE,
  ROBOT_SET_CYCLE_MODE_MAX,
  ROBOT_SET_CYCLE_MODE_NONE
} audio_set_cycle_mode_e;

//system management
typedef enum {
  AUDIO_SYS_SET_RUN_MODE_REBOOT,
  AUDIO_SYS_SET_RUN_MODE_WORK,
  AUDIO_SYS_SET_RUN_MODE_SLEEP,
  AUDIO_SYS_SET_RUN_MODE_DEEP_SLEEP,
  AUDIO_SYS_SET_RUN_MODE_BIG_VOLUME,
  AUDIO_SYS_SET_RUN_MODE_UPFRADE,
  AUDIO_SYS_SET_RUN_MODE_NUM
} audio_sys_set_run_mode_e;

typedef enum {
  AUDIO_SYS_FILE_DOWNLOAD_TYPE_SOUND_REMINDING, //mx1200 sound reminding
  AUDIO_SYS_FILE_DOWNLOAD_TYPE_SD,              //mx1200 sd card
  AUDIO_SYS_FILE_DOWNLOAD_TYPE_FIRMWARE,        //mx1200 firmware
  AUDIO_SYS_FILE_DOWNLOAD_TYPE_NUM
} audio_sys_file_download_type_e;

typedef enum {
    AUDIO_SYS_FILE_DOWNLOAD_RESULT_SUCCESS,
    AUDIO_SYS_FILE_DOWNLOAD_RESULT_FAILED,
    AUDIO_SYS_FILE_DOWNLOAD_RESULT_FILE_NOT_CREATE,
    AUDIO_SYS_FILE_DOWNLOAD_RESULT_FILE_ALREADY_EXISTS,
    AUDIO_SYS_FILE_DOWNLOAD_RESULT_NUM
} audio_sys_file_download_result_e;

typedef enum {
    AUDIO_SYS_USB_STATE_NOT_INSERTED,
    AUDIO_SYS_USB_STATE_CHARGE_ONLY,
    AUDIO_SYS_USB_STATE_U_DISK,
    AUDIO_SYS_USB_STATE_MAX,
    AUDIO_SYS_USB_STATE_NONE
} audio_sys_usb_state_e;

typedef enum {
  AUDIO_SYS_SD_STATE_NOT_INSERTED,
  ROBOT_SYS_SD_STATE_INSERTED,
  ROBOT_SYS_SD_STATE_MAX,
  ROBOT_SYS_SD_STATE_NONE
} audio_sys_sd_state_e;

typedef enum {
  ROBOT_SYS_VOLUME_STATE_COMMON_MODE,
  ROBOT_SYS_VOLUME_STATE_BIG_MODE,
  ROBOT_SYS_VOLUME_STATE_MAX,
  ROBOT_SYS_VOLUME_STATE_NONE
} audio_sys_volume_state_e;

typedef enum {
  ROBOT_SYS_FILE_DOWNLOAD_STATE_NO_FILE,
  ROBOT_SYS_FILE_DOWNLOAD_STATE_DOWNLOADING,
  ROBOT_SYS_FILE_DOWNLOAD_STATE_MAX,
  ROBOT_SYS_FILE_DOWNLOAD_STATE_NONE
} audio_sys_file_download_state_e;

typedef enum {
  ROBOT_SYS_MUTE_STATE_IN,
  ROBOT_SYS_MUTE_STATE_NOT_IN,
  ROBOT_SYS_MUTE_STATE_MAX,
  ROBOT_SYS_MUTE_STATE_NONE
} audio_sys_mute_state_e;

typedef enum {
  ROBOT_SYS_OTA_STATE_NO_OTA,
  ROBOT_SYS_OTA_STATE_OTA_PROCESS,
  ROBOT_SYS_OTA_STATE_OTA_FINISH,
  ROBOT_SYS_OTA_STATE_MAX,
  ROBOT_SYS_OTA_STATE_NONE
} audio_sys_ota_state_e;

typedef enum {
  ROBOT_SYS_CYCLE_STATE_ALL_CYCLE,
  ROBOT_SYS_CYCLE_STATE_SINGLE_CYCLE,
  ROBOT_SYS_CYCLE_STATE_RANDOM_CYCLE,
  ROBOT_SYS_CYCLE_STATE_MAX,
  ROBOT_SYS_CYCLE_STATE_NONE
} audio_sys_cycle_state_e;

typedef struct _AUDIO_STREAM_PALY_S{
    uint8_t                 stream_id;          //The ID of stream
    audio_stream_type_e     type;               //The type of stream
    uint32_t                total_len;          //the len of total stream
    uint16_t                stream_len;         //The len of stream
    const uint8_t           *pdata;             //The payload of stream
}AUDIO_STREAM_PALY_S;

typedef struct _AUDIO_MIC_RECORD_START_S{
  uint8_t                   record_id;          //The ID of this record
  audio_mic_result_type_e   type;               //The type of this record
  audio_mic_record_format_e format;             //The format of this record
} AUDIO_MIC_RECORD_START_S;

typedef struct _AUDIO_MIC_RECORD_RESULT_S{
  uint8_t                   record_id;
  audio_mic_result_type_e   type;
  audio_mic_record_format_e format;
  uint8_t                   total_len[4];
  uint8_t                   block_index[2];
  uint8_t                   cur_len[2];
  uint8_t                   data[1];
} AUDIO_MIC_RECORD_RESULT_S;

typedef struct AUDIO_MIC_RECORD_USER_PARAM_S{
  uint8_t                   record_id; /*in*/
  audio_mic_result_type_e   type;
  audio_mic_record_format_e format;
  uint32_t                  total_len;
  uint16_t                  block_index;
  uint16_t                  cur_len;
  uint8_t                   *user_data; /*out*/
  uint16_t                  user_data_len; /*in*/
} AUDIO_MIC_RECORD_USER_PARAM_S;

typedef struct _AUDIO_PLAYING_FILENAME_S{
  uint8_t                   filename_len;       //filename len
  uint8_t                   file_name[255];
} AUDIO_PLAYING_FILENAME_S;

typedef struct _ADUIO_SYSTEM_FW_INFO_S{
  uint8_t mx1200_ver_major;
  uint8_t mx1200_ver_minor;
  uint8_t mx1200_ver_revision;
  uint8_t sound_remind_ver_major;
  uint8_t sound_remind_ver_minor;
  uint8_t sound_remind_ver_revision;
  uint8_t hardware_ver_major;
  uint8_t hardware_ver_minor;
  uint8_t mx1200_fw_name_len;
  uint8_t mx1200_fw_name[64];
  uint8_t sound_remind_fw_name_len;
  uint8_t sound_remind_fw_name[64];
} ADUIO_SYSTEM_FW_INFO_S;

typedef struct _ADUIO_SYSTEM_FILE_DOWNLOAD_START_INFO_S{
    audio_sys_file_download_type_e file_type;
    uint8_t file_id;
    bool is_file_check;
    uint8_t *md5_string;
    uint8_t file_name_len; //file_name_len == (strlen(file_name) + 1)
    uint8_t *file_name;
} ADUIO_SYSTEM_FILE_DOWNLOAD_START_INFO_S;

typedef struct _ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_S{
    uint8_t file_id;
    uint32_t file_total_len;
    uint16_t packet_serial_num;
    uint16_t packet_length;
    uint8_t *packet_data;
} ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_REQUEST_S;

typedef struct _ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_RESPONSE_S{
    uint8_t file_id;
    audio_sys_file_download_result_e download_reslut;
    uint8_t serial_num[2];
} ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_RESPONSE_S;

typedef struct _ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_RESPONSE_USER_S{
    uint8_t file_id;
    audio_sys_file_download_result_e download_reslut;
    uint16_t packet_serial_num;
} ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_RESPONSE_USER_S;

typedef struct _ADUIO_SYSTEM_STATE_S{
    uint8_t                         battery_level;
    audio_sys_usb_state_e           usb_state;
    audio_sys_sd_state_e            sd_state;
    audio_sys_volume_state_e        volume_state;
    audio_sys_file_download_state_e file_download_state;
    audio_sys_mute_state_e          mute_state;
    audio_sys_ota_state_e           ota_state;
    audio_sys_cycle_state_e         cycle_state;
} ADUIO_SYSTEM_STATE_S;

// MSCP V4.0
OSStatus audio_service_init( void );
OSStatus audio_service_deinit( void );

//audio
uint8_t  audio_service_system_generate_stream_id(void);
OSStatus audio_service_stream_play( mscp_result_t *result, AUDIO_STREAM_PALY_S *audio_stream_s_p );
OSStatus audio_service_stream_pause( mscp_result_t *result );
OSStatus audio_service_stream_continue( mscp_result_t *result );
OSStatus audio_service_stream_stop( mscp_result_t *result, uint8_t stream_id );
OSStatus audio_service_sd_card_play( mscp_result_t *result );
OSStatus audio_service_sd_card_play_next( mscp_result_t *result );
OSStatus audio_service_sd_card_play_previous( mscp_result_t *result );
OSStatus audio_service_sd_card_pause( mscp_result_t *result );
OSStatus audio_service_sd_card_stop( mscp_result_t *result );
OSStatus audio_service_sd_card_cycle( mscp_result_t *result, audio_set_cycle_mode_e cycle_mode);
OSStatus audio_service_sd_card_get_playing_filename( mscp_result_t *result, AUDIO_PLAYING_FILENAME_S *playing_filename_s);
OSStatus audio_service_get_audio_status( mscp_result_t *result, mscp_status_t *audio_status);
OSStatus audio_service_volume_up( mscp_result_t *result, uint8_t up_data);
OSStatus audio_service_volume_down( mscp_result_t *result, uint8_t down_data);
uint8_t  audio_service_system_generate_record_id(void);
OSStatus audio_service_mic_record_start(mscp_result_t *result, AUDIO_MIC_RECORD_START_S *audio_mic_start_p);
OSStatus audio_service_mic_record_stop(mscp_result_t *result, uint8_t record_id);
OSStatus audio_service_mic_record_get_result(mscp_result_t *result, uint8_t record_id, AUDIO_MIC_RECORD_USER_PARAM_S *audio_mic_result_p);
OSStatus audio_service_mic_record_get_total_filelen(mscp_result_t *result, uint8_t record_id, uint32_t *total_len);
OSStatus audio_service_sound_remind_start( mscp_result_t *result, uint8_t sound_remind_id);
OSStatus audio_service_sound_remind_finsh( mscp_result_t *result);
OSStatus audio_service_switch_menu_dir( mscp_result_t *result, audio_menu_dir_status_e *menu_dir_status);
OSStatus audio_service_switch_download_dir( mscp_result_t *result, audio_download_dir_status_e *download_dir_status);

//notification
OSStatus audio_service_get_notification( mscp_result_t *result, char *event_content, uint32_t event_content_size);

//system
OSStatus audio_service_system_set_run_mode( mscp_result_t *result, audio_sys_set_run_mode_e run_mode);
OSStatus audio_service_system_get_fw_info( mscp_result_t *result, ADUIO_SYSTEM_FW_INFO_S *firmware_info_p);
uint8_t audio_service_system_generate_file_id(void);
OSStatus audio_service_system_file_download_start( mscp_result_t *result, ADUIO_SYSTEM_FILE_DOWNLOAD_START_INFO_S *file_download_info_p);
OSStatus audio_service_system_file_download_loading( mscp_result_t *result, ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_REQUEST_S *file_download_process_p, \
                                                     ADUIO_SYSTEM_FILE_DOWNLOAD_PROCESS_RESPONSE_USER_S *file_download_response);
OSStatus audio_service_system_file_download_cancel( mscp_result_t *result, uint8_t file_id);
OSStatus audio_service_system_file_delete( mscp_result_t *result, const char *file_name, uint32_t file_name_length);
OSStatus audio_service_system_get_system_state( mscp_result_t *result, ADUIO_SYSTEM_STATE_S *system_state_context);

#endif
