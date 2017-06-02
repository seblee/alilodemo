#include "mico.h"
#include "media_play.h"
#include "check_pin.h"
#include "http_trans.h"

#define media_button_log(M, ...) custom_log("MEDIA_BUTTON", M, ##__VA_ARGS__)

#define RECODER_PIN        MICO_GPIO_6
#define CALL_PIN           MICO_GPIO_18
#define PAUSE_RESUME_PIN   MICO_GPIO_30
#define LOCAL_PIN          MICO_GPIO_NONE
#define LEFT_PIN           MICO_GPIO_NONE
#define RIGHT_PIN          MICO_GPIO_NONE

extern void http_trans_state_stop(void);

void media_recoder_button_check_high_callback(void)
{
  media_button_log("recoder stop");
  media_recode_type_recoder();
  media_play_cmd_decode();
}

void media_recoder_button_check_low_callback(void)
{
  media_button_log("recoder start");
  http_trans_state_stop();
  media_recode_type_recoder();
  media_play_cmd_encode();
}

void media_call_button_check_high_callback(void)
{
  media_button_log("call stop");
  if( media_recode_buffer_full() == false ){
    media_recode_type_call();
    media_play_cmd_decode();
  }
}


void media_call_button_check_low_callback(void)
{
  media_button_log("call start");
  http_trans_state_stop();
  media_recode_type_call();
  media_play_cmd_encode();
}

void media_pause_resume_button_check_low_callback(void)
{
  media_button_log("pause/resume");
  media_play_cmd_pause_resum();
}

void media_local_button_check_low_callback(void)
{
  media_button_log("local");
}

void media_left_button_check_low_callback(void)
{
  media_button_log("left button");
}

void media_right_button_check_low_callback(void)
{
  media_button_log("right button");
  
  http_trans_state_stop();
  //media_recode_type_recoder();
  media_play_cmd_asr_mode();
}

void media_button_init(void)
{
  check_pin_init_t recoder_pin;
  check_pin_init_t call_pin;
  check_pin_init_t pause_resume_pin;
//  check_pin_init_t local_pin;
  check_pin_init_t left_pin;
  check_pin_init_t right_pin;
  
  recoder_pin.gpio = RECODER_PIN;
  recoder_pin.long_pressed_timeout = 100;
  recoder_pin.check_pin_high_func = media_recoder_button_check_high_callback;
  recoder_pin.check_pin_low_func = media_recoder_button_check_low_callback;
  check_pin_init(IOCHECK_PIN_1, recoder_pin);
  
  call_pin.gpio = CALL_PIN;
  call_pin.long_pressed_timeout = 100;
  call_pin.check_pin_high_func = media_call_button_check_high_callback;
  call_pin.check_pin_low_func = media_call_button_check_low_callback;
  check_pin_init(IOCHECK_PIN_2, call_pin);
  
  pause_resume_pin.gpio = PAUSE_RESUME_PIN;
  pause_resume_pin.long_pressed_timeout = 100;
  pause_resume_pin.check_pin_high_func = NULL;
  pause_resume_pin.check_pin_low_func = media_pause_resume_button_check_low_callback;
  check_pin_init(IOCHECK_PIN_3, pause_resume_pin);
  
//  local_pin.gpio = LOCAL_PIN;
//  local_pin.long_pressed_timeout = 100;
//  local_pin.check_pin_high_func = NULL;
//  local_pin.check_pin_low_func = media_local_button_check_low_callback;  
//  check_pin_init(IOCHECK_PIN_4, local_pin);

  left_pin.gpio = LEFT_PIN;
  left_pin.long_pressed_timeout = 100;
  left_pin.check_pin_high_func = NULL;
  left_pin.check_pin_low_func = media_left_button_check_low_callback;  
  check_pin_init(IOCHECK_PIN_5, left_pin);

  right_pin.gpio = RIGHT_PIN;
  right_pin.long_pressed_timeout = 100;
  right_pin.check_pin_high_func = NULL;
  right_pin.check_pin_low_func = media_right_button_check_low_callback;  
  check_pin_init(IOCHECK_PIN_6, right_pin);
}


