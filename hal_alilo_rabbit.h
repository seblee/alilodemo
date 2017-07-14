#ifndef _HAL_ALILO_RABBIT_H
#define _HAL_ALILO_RABBIT_H

#include "../alilodemo/inc/audio_service.h"
#include "../alilodemo/inc/http_file_download_thread.h"

#define HAL_TEST_EN 0

#define KEY_RELEASE false
#define KEY_PRESS   true

#define ROBOT_EVENT_KEY_URL_FILEDNLD  ROBOT_EVENT_KEY_EAR_LED_ON_OFF

typedef struct _PLAYER_OPTION_S
{
  audio_stream_type_e type;         //The type of stream, MP3 OR AMR
  uint8_t stream_player_id;                //The ID of stream
  HTTP_FILE_DOWNLOAD_STATE_E file_download_status_e;

} PLAYER_OPTION_S;


extern mico_semaphore_t recordKeyPress_Sem;
extern mico_semaphore_t urlFileDownload_Sem;
extern bool recordKeyStatus;
extern uint8_t mic_record_id;
extern uint8_t audio_play_id;
extern uint8_t flag_mic_start;

extern OSStatus hal_alilo_rabbit_init       ( void );
extern int32_t  hal_getVoiceData            ( uint8_t* voice_buf, uint32_t voice_buf_len );
extern bool     hal_player_start            (const char *data, uint32_t data_len, uint32_t file_total_len);
extern OSStatus hal_url_fileDownload_start  (char * url);

#endif
