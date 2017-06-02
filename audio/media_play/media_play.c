#include "mico.h"
#include "ring_buf.h"
#include "media_play.h"
#include "http_trans.h"
#include "stream_crc.h"
#include "media_button.h"
//#include "baichuan.h"
//#include "vbs_driver.h"

#define media_play_log(M, ...) custom_log("MEDIA_PLAY", M, ##__VA_ARGS__)
#define SSAP_SEND_DATA_LENGTH       (512*2)

static uint8_t mkey = 0;
#define get_media_cmd() mkey
#define set_media_cmd(v) mkey = v
#define SPIM_STREAM_CONTROL_GET_STATUS() MicoGpioInputGet( (mico_gpio_t)Meida_SPI_CONTROL )

static mico_mutex_t media_mutex;
struct ring_buffer *ring_buf = NULL;
static void *media_buf = NULL;
static media_context_t *media_context = NULL;
static mico_semaphore_t g_sem_audio_sync = NULL;

uint16_t asr_cmd_id = ASR_CMD_ID_NONE;

const mico_spi_device_t mico_spi_media =
{
    .port        = Media_SPI_PORT,
    .chip_select = Media_SPI_CS,
    .speed       = 24000000,
    .mode        = (SPI_CLOCK_RISING_EDGE | SPI_CLOCK_IDLE_LOW | SPI_NO_DMA | SPI_MSB_FIRST),
    .bits        = 8
};

void media_play_get_audio_wait_forever( void )
{
    if( g_sem_audio_sync != NULL ){
        mico_rtos_get_semaphore(&g_sem_audio_sync, MICO_WAIT_FOREVER );
    }
}

void media_play_data_clean(void)
{
  ring_buf_clean(ring_buf);
}

uint8_t *ring_buf_get_buf_point()
{
  return ring_buf->buffer;
}

uint32_t media_play_put_data(void *buf, uint32_t len)
{
  uint32_t buf_len;

  buf_len = ring_buf_len(ring_buf);
  if( (buf_len + len) >=  MEDIA_RING_BUF_SIZE){
    return 0;
  }
  
  return ring_buf_put(ring_buf, buf, len);
}

uint32_t media_play_get_data(void *buf, uint32_t len)
{
  return ring_buf_get(ring_buf, buf, len);
}

static int media_command_send_recv( uint8_t *send_data, uint8_t *revc_data, uint32_t len )
{
  int err;

  mico_spi_message_segment_t message ={
    send_data, revc_data, len 
  };
  
  err = MicoSpiTransfer(&mico_spi_media, &message, 1);

  return err;
}

static bool media_play_data_is_end(void)
{
  if( media_context->media_source == MEDIA_SOURCE_NET ){
    return http_trans_is_end();
  }else if( media_context->media_source == MEDIA_SOURCE_TF ){
    
  }
  return false;
}

static void http_trans_upload(void)
{
//  media_play_log("http upload size: %d", ring_buf_len(ring_buf));
//  http_trans_set_url(HTTP_TYPE_POST, BC_HOST);
  http_trans_state_start();
}

int media_play_set_source_decoder_type(uint8_t source, char *filename)
{
  int len = strlen(filename);
  
  if( (media_context == NULL) || (filename == NULL) || (len < 5) )
    return -1;
  
  if( (source == MEDIA_SOURCE_NET) || (source == MEDIA_SOURCE_TF) || (source == MEDIA_SOURCE_MEM) )
    media_context->media_source = (MEDIA_SOURCE_E)source;
  else
    return -1;
  
  strcpy((char *)media_context->audia_context.file_name, filename);
  media_context->audia_context.decoder = UNKOWN_DECODER;
  filename[len-3] = toupper(filename[len-3]);
  filename[len-2] = toupper(filename[len-2]);
  filename[len-1] = toupper(filename[len-1]);
  if( (filename[len-3] == 'W' && filename[len-2] == 'M' && filename[len-1] == 'A' ) ||
     (filename[len-3] == 'W' && filename[len-2] == 'M' && filename[len-1] == 'V' ) ||
       (filename[len-3] == 'A' && filename[len-2] == 'S' && filename[len-1] == 'F' ) )
  {
    media_context->audia_context.decoder = WMA_DECODER;
  }
  else if( (filename[len-3] == 'M' && filename[len-2] == 'P' && filename[len-1] == '3' ) ||
           (filename[len-3] == 'M' && filename[len-2] == 'P' && filename[len-1] == '2' ) )
  {
    media_context->audia_context.decoder = MP3_DECODER;
  }
  else if( (filename[len-3] == 'W' && filename[len-2] == 'A' && filename[len-1] == 'V' ) )
  {
    media_context->audia_context.decoder = WAV_DECODER;
  }
  else if( (filename[len-4] == 'F' && filename[len-3] == 'L' && filename[len-2] == 'A'  && filename[len-1] == 'C' ) )
  {
    media_context->audia_context.decoder = FLAC_DECODER;
  }
  else if( (filename[len-3] == 'A' && filename[len-2] == 'A' && filename[len-1] == 'C' ) )
  {
    media_context->audia_context.decoder = AAC_DECODER;
  }
  else if( (filename[len-3] == 'A' && filename[len-2] == 'I' && filename[len-1] == 'F' ) )
  {
    media_context->audia_context.decoder = AIF_DECODER;
  }else if( (filename[len-3] == 'S' && filename[len-2] == 'P' && filename[len-1] == 'X' ) ){
    media_context->audia_context.decoder = SPEEX_DECODE;
  }else{
    return -1;
  }
  
  return 0;
}

void media_play_cmd_start(void)
{
  if( strlen((char *)media_context->audia_context.file_name) <4 )
    return;
  if( media_context->media_source == MEDIA_SOURCE_NET ){
    media_play_data_clean();
    http_trans_state_start();
  }else if( media_context->media_source == MEDIA_SOURCE_TF ){
  }else if( media_context->media_source == MEDIA_SOURCE_MEM ){
  }
  set_media_cmd(MEDIA_CMD_START);
}

void media_play_cmd_pause_resum(void)
{
  set_media_cmd(MEDIA_CMD_PAUSE_RESUM);
}

void media_play_cmd_mute_unmute(void)
{
  set_media_cmd(MEDIA_CMD_MUTE_UNMUTE);
}

void media_play_cmd_stop(void)
{
  if( media_context->media_source == MEDIA_SOURCE_NET ){
    http_trans_state_stop();
    media_play_data_clean();
  }else if( media_context->media_source == MEDIA_SOURCE_TF ){
  }else if( media_context->media_source == MEDIA_SOURCE_MEM ){
  }
  
  set_media_cmd(MEDIA_CMD_STOP);
}

// vol value 0-100
void media_play_cmd_vol_set(uint8_t vol)
{
  if( vol > 100 )
    vol = 100;
  media_context->audia_context.vol_value = vol;
  set_media_cmd(MEDIA_CMD_VOL_SET);
}

void media_play_cmd_vol_add(void)
{
  set_media_cmd(MEDIA_CMD_VOL_ADD);
}

void media_play_cmd_vol_sub(void)
{
  set_media_cmd(MEDIA_CMD_VOL_SUB);
}

void media_play_cmd_encode(void)
{
  set_media_cmd(MEDIA_CMD_ENCODE);
}

void media_play_cmd_decode(void)
{
  set_media_cmd(MEDIA_CMD_DECODE);
}

void media_play_cmd_asr_mode(void)
{
  set_media_cmd(MEDIA_CMD_ASR);
}

bool media_play_get_encode_status()
{
  if( media_context->media_mode == MEDIA_MODE_DECODE ){
    return false;
  }else if( media_context->media_mode == MEDIA_MODE_ENCODE ){
    return true;
  }
  return false;
}

bool media_recode_buffer_full(void)
{
  return media_context->is_recoder_full;
}

void media_recode_type_recoder(void)
{
  media_context->recoder_type = MEDIA_RECODE_TYPE_RECODER;
}


MEDIA_RECODE_TYPE_E get_media_recode_type_recoder(void)
{
  return media_context->recoder_type;
}

void media_recode_type_call(void)
{
  media_context->recoder_type = MEDIA_RECODE_TYPE_CALL;
}

static void media_thread( uint32_t arg )
{
  bool PauseFlag = false;
  bool MuteFlag = false;
  bool StartFlag = false;
  bool SendDataFlag = false; //send spi decode data
  bool ReadDataFlag = false; //read memory decode data
  bool RecvDataFlag = false; //recv spi encode data
  bool AsrModeFlag = true; // ASR mode to recv spi data
  
  uint8_t media_cmd = 0;
  int get_media_data_len = 0;
  uint8_t *p;
  int buf_len = sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH + SPI_SEND_DATA_LEN + SSAP_CRC_VALUE_LENGTH;
  uint8_t *SPI_SEND_BUF = malloc(buf_len);
  uint8_t *pSync = (uint8_t *)&SPI_SEND_BUF[sizeof(sspp_cmd_context_t)];
  uint8_t *pBuffer = (uint8_t *)&SPI_SEND_BUF[sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH];
  
  sspp_cmd_context_t *command_context = (sspp_cmd_context_t *)SPI_SEND_BUF;
  sspp_cmd_response_context_t *response_context = (sspp_cmd_response_context_t *)SPI_SEND_BUF;
  
  media_context = (media_context_t *)malloc(sizeof(media_context_t));
  media_context->media_mode = MEDIA_MODE_DECODE;
  
  MicoGpioInitialize( (mico_gpio_t)Meida_SPI_CONTROL, INPUT_PULL_UP );
  MicoSpiInitialize(&mico_spi_media);
  
  media_button_init();

  while(1){
    command_context->SyncWord = SSAP_SYNC_BYTE;
    
    if( 0 != ( media_cmd = get_media_cmd() ) ){
      set_media_cmd(0);
      
      switch(media_cmd){
      case MEDIA_CMD_START:
        
        media_play_log("[CMD]: STOP");
        command_context->Command = SSAP_CMD_STOP;
        command_context->Content = 0; 
        command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
        
        while(SPIM_STREAM_CONTROL_GET_STATUS());
        //Send command to Slaver
        media_command_send_recv( (uint8_t *)command_context, NULL, sizeof(sspp_cmd_context_t) );
        
        
        media_play_log("[CMD]: START");
        StartFlag = true;
        SendDataFlag = false;
        AsrModeFlag = false;
        if( media_context->media_source == MEDIA_SOURCE_NET ){
          for(;;){
            if( (ring_buf_len(ring_buf) >= (MEDIA_RING_BUF_SIZE - 1024*4))  || (media_play_data_is_end() == true) )
              break;
            mico_thread_msleep(100);
          }
        }
        continue;
        break;
      case MEDIA_CMD_PAUSE_RESUM:
        PauseFlag = !PauseFlag;
        media_play_log("[CMD]: %s\n", PauseFlag ? "PAUSE" : "RESUME");
        command_context->Command = PauseFlag ? SSAP_CMD_PAUSE : SSAP_CMD_RESUME;
        command_context->Content = 0;
        break;
      case MEDIA_CMD_MUTE_UNMUTE:
        MuteFlag = !MuteFlag;
        media_play_log("[CMD]: %s\n", MuteFlag ? "MUTE" : "UNMUTE");
        command_context->Command = MuteFlag ? SSAP_CMD_MUTE : SSAP_CMD_UNMUTE;
        command_context->Content = 0;
        break;
      case MEDIA_CMD_STOP:
        media_play_log("[CMD]: STOP");
        command_context->Command = SSAP_CMD_STOP;
        command_context->Content = 0; 
        StartFlag = false;
        SendDataFlag = false;
        AsrModeFlag = false;
        break;
      case MEDIA_CMD_VOL_SET:
        media_play_log("[CMD]: VOL %d", media_context->audia_context.vol_value);
        command_context->Command = SSAP_CMD_VOL_SET;
        command_context->Content = media_context->audia_context.vol_value;
        break;
      case MEDIA_CMD_VOL_ADD:
        media_play_log("[CMD]: VOL+");
        command_context->Command = SSAP_CMD_VOL_ADD;
        command_context->Content = 0;
        break;
      case MEDIA_CMD_VOL_SUB:
        media_play_log("[CMD]: VOL-");
        command_context->Command = SSAP_CMD_VOL_SUB;
        command_context->Content = 0;
        break;
      case MEDIA_CMD_ENCODE:
        media_play_log("[CMD]: ENCODE");
        command_context->Command = SSAP_CMD_ENCODE_MODE;
        command_context->Content = 0;
        command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
        
        media_play_data_clean();
        StartFlag = false;
        SendDataFlag = false;
        RecvDataFlag = true;
        AsrModeFlag = false;
        media_context->media_mode = MEDIA_MODE_ENCODE;
        media_context->is_recoder_full = false;
        if( media_context->recoder_type == MEDIA_RECODE_TYPE_RECODER ){
            mico_rtos_set_semaphore(&g_sem_audio_sync);
            media_play_log("can be read audio stream");
        }
        
        break;
      case MEDIA_CMD_DECODE:
        media_play_log("[CMD]: DECODE");
        command_context->Command = SSAP_CMD_DECODE_MODE;
        command_context->Content = 0;
        command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
        
        RecvDataFlag = false;
        AsrModeFlag = false;
        media_context->media_mode = MEDIA_MODE_DECODE;
        
        if( media_context->recoder_type == MEDIA_RECODE_TYPE_CALL ){
          http_trans_upload();
        }
        break;
      case MEDIA_CMD_ASR:
        media_play_log("[CMD]: ASR MODE");
        command_context->Command = SSAP_CMD_ASR_MODE;
        command_context->Content = 0;
        SendDataFlag = false;
        AsrModeFlag = true;
//        user_asr_cmd_process(ASR_CMD_INIT);
        media_play_log("asr init");
        break;
      default:
        break;
      }
      
      if( command_context->Command != SSAP_CMD_UNKOWN ){
        command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
        
        while(SPIM_STREAM_CONTROL_GET_STATUS());
        //Send command to Slaver
        media_command_send_recv( (uint8_t *)command_context, NULL, sizeof(sspp_cmd_context_t) );
      }
    }else if( StartFlag ){
      command_context->SyncWord = SSAP_SYNC_BYTE;
      command_context->Command = SSAP_CMD_START;
      command_context->Content = 4 + strlen((char *)media_context->audia_context.file_name);
      command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
      *pSync = SSAP_SYNC_BYTE;
      //Set decoder type
      *(int32_t *)pBuffer = media_context->audia_context.decoder;
      //Set song name
      memcpy(pBuffer + 4, media_context->audia_context.file_name, strlen((char *)media_context->audia_context.file_name));
      //Calculate CRC value
      *(uint16_t*)(pBuffer + command_context->Content) = GetCRC16NBS(pBuffer, command_context->Content);
      
      while(SPIM_STREAM_CONTROL_GET_STATUS());
      
      media_command_send_recv( (uint8_t *)command_context, NULL, sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH + command_context->Content + SSAP_CRC_VALUE_LENGTH );

      for(int cnt=0; cnt<10000; cnt++){
        p = (uint8_t *)&response_context->SyncWord;
        if(0 == media_command_send_recv(NULL, p, 1)){
          if( p[0] == SSAP_SYNC_BYTE ){
            if(0 == media_command_send_recv(NULL, (uint8_t *)&response_context->Command, sizeof(sspp_cmd_response_context_t) - SSAP_SYNC_BYTE_LENGTH) ){
              if( response_context->Response == SSAP_CMD_RESP_OKSEND ){
                StartFlag = false;
                SendDataFlag = true;
                ReadDataFlag = true;
                media_play_log("send song info done");
                break;
              }
            }
          }
        }
      }
    }else if( SendDataFlag ){
      if( media_context->media_source == MEDIA_SOURCE_NET ){
        if( ring_buf_len(ring_buf) < 1024*4 ){
          mico_thread_msleep(15);
        }
      }
      if( ReadDataFlag ){
        ReadDataFlag = false;
        if( media_context->media_source == MEDIA_SOURCE_NET ){
          get_media_data_len = media_play_get_data(pBuffer, SPI_SEND_DATA_LEN);
          //media_play_log("ring buf get %d", get_media_data_len);
        }else if(media_context->media_source == MEDIA_SOURCE_TF){
          get_media_data_len = 0;
        }else if(media_context->media_source == MEDIA_SOURCE_MEM){
          get_media_data_len = media_play_get_data(pBuffer, SPEEX_SEND_BUF);
          //media_play_log("ring buf get %d", get_media_data_len);
        }else{
          SendDataFlag = false;
        }
      }      
      
      if( get_media_data_len <= 0 ){
        media_play_log("ring buf get %d", get_media_data_len);
        mico_thread_msleep(50);
        ReadDataFlag = true;
        if( (media_play_data_is_end() == true) || (media_context->media_source == MEDIA_SOURCE_MEM) ){
          set_media_cmd(MEDIA_CMD_ASR);
          SendDataFlag = false;
          AsrModeFlag = true;
//          user_asr_cmd_process(ASR_CMD_INIT);
          media_play_log("asr init");
        }
        continue;
      }else if( get_media_data_len <= 1024 ){
        mico_thread_msleep(15);
      }
      
      command_context->SyncWord = SSAP_SYNC_BYTE;
      command_context->Command = SSAP_CMD_DATA;
      command_context->Content = get_media_data_len;
      command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
      *pSync = SSAP_SYNC_BYTE;
      //Calculate CRC value
      *(uint16_t*)(pBuffer + command_context->Content) = GetCRC16NBS(pBuffer, command_context->Content);
      
      while(SPIM_STREAM_CONTROL_GET_STATUS()){
        if( 0 != ( media_cmd = get_media_cmd() ) )
          continue;
      }
      
      media_command_send_recv( (uint8_t *)SPI_SEND_BUF, NULL, sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH + command_context->Content + SSAP_CRC_VALUE_LENGTH );
      
      for(int cnt=0; cnt<10000; cnt++){
        p = (uint8_t *)&response_context->SyncWord;
        if(0 == media_command_send_recv(NULL, p, 1)){
          if( p[0] == SSAP_SYNC_BYTE ){
            if(0 == media_command_send_recv(NULL, (uint8_t *)&response_context->Command, sizeof(sspp_cmd_response_context_t) - SSAP_SYNC_BYTE_LENGTH) ){
              if( response_context->Response == SSAP_CMD_RESP_OKSEND ){
                ReadDataFlag = true;
                break;
              }else if( response_context->Response == SSAP_CMD_RESP_NEXTSEND ){
                //media_play_log("---------------------------------------data full");
                mico_thread_msleep(100);
                break;
              }else if( response_context->Response == SSAP_CMD_RESP_RESEND ){
                media_play_log("crc err");
                break;
              }
            }
          }
        }
      }
    }else if(RecvDataFlag){
      command_context->SyncWord = SSAP_SYNC_BYTE;
      command_context->Command = SSAP_CMD_READ_DATA;
      command_context->Content = 0;
      command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
      *pSync = SSAP_SYNC_BYTE;
      //Calculate CRC value
      *(uint16_t*)(pBuffer + command_context->Content) = GetCRC16NBS(pBuffer, command_context->Content);
//      while(SPIM_STREAM_CONTROL_GET_STATUS());
      media_command_send_recv( (uint8_t *)SPI_SEND_BUF, NULL, sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH + command_context->Content + SSAP_CRC_VALUE_LENGTH );
      
      for(int cnt=0; cnt<100; cnt++){
        p = (uint8_t *)&response_context->SyncWord;
        if(0 == media_command_send_recv(NULL, p, 1)){
          if( p[0] == SSAP_SYNC_BYTE ){
            if(0 == media_command_send_recv(NULL, (uint8_t *)&response_context->Command, sizeof(sspp_cmd_response_context_t)) ){
              if( response_context->Command == SSAP_CMD_READ_DATA ){
				if( response_context->Response > SSAP_SEND_DATA_LENGTH ){
                  media_play_log("!!!!!!!!!!!!!!!response %d!!!!!!!!!!!!!!!!!!!", response_context->Response);
                  goto CONTINUE;
				}
                if(0 == media_command_send_recv(NULL, (uint8_t *)(SPI_SEND_BUF+sizeof(sspp_cmd_response_context_t)+SSAP_SYNC_BYTE_LENGTH), response_context->Response + SSAP_SYNC_BYTE_LENGTH + SSAP_CRC_VALUE_LENGTH )){
                  p = (uint8_t *)&SPI_SEND_BUF[sizeof(sspp_cmd_response_context_t)];
                  if( media_play_put_data(p+1, response_context->Response) != 0 ){
                    media_play_log("p[0]=%d len=%d", p[0], response_context->Response);                 
                    mico_thread_msleep(5);                   
                  }else{
                    media_play_log("buf full");
                    media_context->is_recoder_full = true;
                    media_play_cmd_decode();
                  }
                }
              }
            }
          }
        }
      }
    }
    else if(AsrModeFlag){  // asr mode to recv spi cmd from slave
      command_context->SyncWord = SSAP_SYNC_BYTE;
      command_context->Command = SSAP_CMD_READ_ASR_CMD_DATA;
      command_context->Content = 0;
      command_context->CrcValue = GetCRC16NBS((uint8_t*)command_context, sizeof(sspp_cmd_context_t) - SSAP_CRC_VALUE_LENGTH);
      *pSync = SSAP_SYNC_BYTE;
      //Calculate CRC value
      *(uint16_t*)(pBuffer + command_context->Content) = GetCRC16NBS(pBuffer, command_context->Content);
      
      while(SPIM_STREAM_CONTROL_GET_STATUS());
      media_command_send_recv( (uint8_t *)SPI_SEND_BUF, NULL, sizeof(sspp_cmd_context_t) + SSAP_SYNC_BYTE_LENGTH + command_context->Content + SSAP_CRC_VALUE_LENGTH );
      
      for(int cnt=0; cnt<100; cnt++){
        p = (uint8_t *)&response_context->SyncWord;
        if(0 == media_command_send_recv(NULL, p, 1)){
          if( p[0] == SSAP_SYNC_BYTE ){
            if(0 == media_command_send_recv(NULL, (uint8_t *)&response_context->Command, sizeof(sspp_cmd_response_context_t)) ){
              if( response_context->Command == SSAP_CMD_READ_ASR_CMD_DATA ){
                asr_cmd_id = response_context->Response;  // ASR CMD id
                if(ASR_CMD_ID_NONE != asr_cmd_id){
                  media_play_log(">>>>>>>>> Got ASR cmd=%d.", asr_cmd_id);
//                  user_asr_cmd_process(asr_cmd_id);
                }
              }
            }
          }
        }
      }
    }else{
      mico_thread_msleep(100);
    }
    
CONTINUE:
    continue;
  }
}

void media_play_start(void)
{
    if(g_sem_audio_sync == NULL){
        mico_rtos_init_semaphore(&g_sem_audio_sync, 1 );
    }

#ifdef MEDIA_USE_RING_BUF
  ring_buf = ring_buf_init(media_buf, MEDIA_RING_BUF_SIZE, media_mutex);
  if( ring_buf == NULL ){
    media_play_log("media play start err");
    return;
  }
    
  media_play_test();
  mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "MEDIA", media_thread, 0x500, 0 );
#endif  
  http_trans_start();
}
