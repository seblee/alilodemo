#ifndef __MEDIA_CONTROL_H
#define __MEDIA_CONTROL_H

//#include "platform.h"

#define MEDIA_USE_RING_BUF
#define MEDIA_RING_BUF_SIZE 8192//65536 //8192 2^13 //16384 2^14 //65536 2^16  131072 2^17

/**
 * Audio Decoder Type Set
 */
typedef enum _DecoderType
{
    UNKOWN_DECODER               = 0,       /**< Unknown decoder                     */
    PURE_SOFTWARE_DECODERS       = 1,       /**< Pure software decoders as follows   */
    WAV_DECODER,                            /**< WAV  decoder */
    FLAC_DECODER,                           /**< FLAC decoder */
    AAC_DECODER,                            /**< AAC  decoder */
    AIF_DECODER,                            /**< AIFF and AIFC decoder */
    WITH_HARDWARE_DECODERS       = 128,     /**< Decoders with hardware as follows   */
    MP3_DECODER,                            /**< MP3  decoder */
    WMA_DECODER,                            /**< WAM  decoder */
    SBC_DECODER,                            /**< SBC  decoder */
} DecoderType;

#define Meida_SPI_CONTROL MICO_GPIO_5
#define Media_SPI_PORT    MICO_SPI_1
#define Media_SPI_CS      MICO_GPIO_12

#define SPEEX_DECODE  64
#define  SPEEX_CODEC_PARAM_MODE         (1)     //0: Narrow band, 1: Wide band
#define  SPEEX_CODEC_PARAM_MODE_WB

#if SPEEX_CODEC_PARAM_MODE == 1
#define SPEEX_SEND_BUF 32 
#elif SPEEX_CODEC_PARAM_MODE == 0
#define SPEEX_SEND_BUF 20
#endif 

/**
 * @brief SPI Stream Audio Player Protocol Command Set
 *        1. For Reduced Instruction Set, see the context SSAPCmdContext when SSAP_SYNC_WORD_LENGTH equal 1.
 *        2. For Complex Instruction Set, see the context SSAPCmdContext when SSAP_SYNC_WORD_LENGTH equal 4.
 *        
 * @Note  For those commands with few content (less than 2 bytes) or without content, the content data
 *        just fill in the SSPPCmdContext.Content field.
 *        For those commands with large content (more than 2 bytes), such as command SSPP_CMD_DATA, first
 *        fill in the SSPPCmdContext.Content field with the length of content data, and then send a content
 *        data packet with the format as follow:
 *        SYNC_WORD + content_data + CRC
 */
typedef enum _SSPP_CMD_E {
    SSAP_CMD_UNKOWN = 0,
    SSAP_CMD_START,
    SSAP_CMD_PAUSE,
    SSAP_CMD_RESUME,
    SSAP_CMD_STOP,
    SSAP_CMD_MUTE,
    SSAP_CMD_UNMUTE,
    SSAP_CMD_DATA,
    SSAP_CMD_WRITE_DATA = SSAP_CMD_DATA,
    SSAP_CMD_VOL_SET,
    SSAP_CMD_VOL_ADD,
    SSAP_CMD_VOL_SUB,
    SSAP_CMD_INFO_GET,
    SSAP_CMD_TIME_REST,
    SSAP_CMD_TIME_GET,
    SSAP_CMD_VERSION_GET,
    SSAP_CMD_ENCODE_MODE,
    SSAP_CMD_DECODE_MODE,
    SSAP_CMD_ASR_MODE,
    SSAP_CMD_READ_DATA,
    SSAP_CMD_READ_ASR_CMD_DATA,
    SSAP_CMD_LIN_SWITCH,
    SSAP_CMD_MIC_SWITCH,
} SSPP_CMD_E;

typedef enum __SSAP_CMD_RESPONSE_E{
    SSAP_CMD_RESP_UNKOWN = 0,
    SSAP_CMD_RESP_OKCMD,
    SSAP_CMD_RESP_ERCMD,
    SSAP_CMD_RESP_OKSEND,
    SSAP_CMD_RESP_RESEND,
    SSAP_CMD_RESP_NEXTSEND,
} SSAP_CMD_RESPONSE_E;

typedef enum {
	ASR_CMD_ID_NONE = 0,
    ASR_CMD_INIT,
	ASR_CMD_ID_1,
	ASR_CMD_ID_2,
	ASR_CMD_ID_3,
	ASR_CMD_ID_4,
	ASR_CMD_ID_5
}asr_cmd_id_t;

#define SSAP_SYNC_BYTE              'S'
#define SSAP_SYNC_BYTE_LENGTH 1
#define SSAP_CRC_VALUE_LENGTH 2
#define SPI_SEND_DATA_LEN 512*2

typedef struct _sspp_cmd_context_t{
    uint8_t  SyncWord;
    uint8_t  Command;
    uint16_t Content;
    uint16_t CrcValue;
} sspp_cmd_context_t;

typedef struct _sspp_cmd_response_context_t{
    uint8_t  SyncWord;
    uint8_t  Command;
    uint16_t Response;
    uint16_t CrcValue;
} sspp_cmd_response_context_t;

typedef enum _MEDIA_CMD_E{
  MEDIA_CMD_UNKONW,
  MEDIA_CMD_START,
  MEDIA_CMD_PAUSE_RESUM,
  MEDIA_CMD_MUTE_UNMUTE,
  MEDIA_CMD_STOP,
  MEDIA_CMD_VOL_SET,
  MEDIA_CMD_VOL_ADD,
  MEDIA_CMD_VOL_SUB,
  MEDIA_CMD_ENCODE,
  MEDIA_CMD_DECODE,
  MEDIA_CMD_ASR,
} MEDIA_CMD_E;

typedef enum _MEDIA_SOURCE_E{
  MEDIA_SOURCE_UNKONW,
  MEDIA_SOURCE_NET,
  MEDIA_SOURCE_TF,
  MEDIA_SOURCE_MEM,
} MEDIA_SOURCE_E;

typedef enum _MEDIA_MODE_E{
  MEDIA_MODE_UNKOWN,
  MEDIA_MODE_ENCODE,
  MEDIA_MODE_DECODE,
}MEDIA_MODE_E;

typedef enum _MEDIA_RECODE_TYPE_E{
  MEDIA_RECODE_TYPE_RECODER,
  MEDIA_RECODE_TYPE_CALL,
}MEDIA_RECODE_TYPE_E;

typedef struct _audia_context_t{
  uint8_t vol_value;
  uint8_t file_name[21];
  uint8_t decoder;
} audia_context_t;

typedef struct _media_context_t{
  uint8_t media_source;
  MEDIA_MODE_E media_mode;
  audia_context_t audia_context;
  MEDIA_RECODE_TYPE_E recoder_type;
  bool is_recoder_full;
} media_context_t;

void media_play_start(void);
void media_play_test( void );

uint8_t *ring_buf_get_buf_point();
uint32_t media_play_put_data(void *buf, uint32_t len);
uint32_t media_play_get_data(void *buf, uint32_t len);
void media_play_get_audio_wait_forever( void );

int media_play_set_source_decoder_type(uint8_t source, char *filename);

void media_play_cmd_start(void);
void media_play_cmd_pause_resum(void);
void media_play_cmd_mute_unmute(void);
void media_play_cmd_stop(void);
void media_play_cmd_vol_set(uint8_t vol);
void media_play_cmd_vol_add(void);
void media_play_cmd_vol_sub(void);
void media_play_cmd_encode(void);
void media_play_cmd_decode(void);
void media_play_cmd_asr_mode(void);
bool media_play_get_encode_status();
void media_recode_type_recoder(void);
void media_recode_type_call(void);
bool media_recode_buffer_full(void);
MEDIA_RECODE_TYPE_E get_media_recode_type_recoder(void);
  
#endif
