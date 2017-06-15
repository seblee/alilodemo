#ifndef _HAL_ALILO_RABBIT_H
#define _HAL_ALILO_RABBIT_H

#include "audio_service.h"
#include "http_file_download_thread.h"

#define HAL_TEST_EN 1

#define KEY_RELEASE false
#define KEY_PRESS   true

typedef struct _PLAYER_OPTION_S
{
  audio_stream_type_e type;         //The type of stream, MP3 OR AMR
  uint8_t stream_player_id;                //The ID of stream
  HTTP_FILE_DOWNLOAD_STATE_E file_download_status_e;

} PLAYER_OPTION_S;


extern mico_semaphore_t recordKeyPress_Sem;
extern bool recordKeyStatus;
extern uint8_t mic_record_id;
extern uint8_t audio_play_id;

extern OSStatus hal_alilo_rabbit_init( void );
extern int32_t  hal_getVoiceData     ( uint8_t* voice_buf, uint32_t voice_buf_len );
extern bool     hal_player_test      (const char *data, uint32_t data_len, uint32_t file_total_len);

#endif
