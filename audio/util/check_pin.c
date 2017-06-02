/**
******************************************************************************
* @file    keys.c
* @author  Eshen Wang
* @version V1.0.0
* @date    1-May-2015
* @brief   user keys operation. 
  operation
******************************************************************************
* @attention
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDATAG CUSTOMERS
* WITH CODATAG INFORMATION REGARDATAG THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODATAG INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
******************************************************************************
*/ 

#include "mico_platform.h"
#include "check_pin.h"

#define keys_log(M, ...) custom_log("USER_KEYS", M, ##__VA_ARGS__)
#define keys_log_trace() custom_log_trace("USER_KEYS")

/*-------------------------------- VARIABLES ---------------------------------*/
#define PIN_NUM 8
/*------------------------------ USER INTERFACES -----------------------------*/

typedef struct _check_context_t{
  mico_gpio_t gpio;
  int timeout;
  int check_pin_state;
  mico_timer_t _user_button_timer;
  check_pin_low_cb check_pin_low_func;
  check_pin_high_cb check_pin_high_func;
} check_context_t;

static check_context_t context[PIN_NUM];
static int count = 0;

//typedef void (*_button_irq_handler)( void* arg );

static void check_irq_handler( void* arg )
{
  check_context_t *_context = arg;
  
  if ( MicoGpioInputGet( _context->gpio ) == 0 ) {
    MicoGpioEnableIRQ( _context->gpio, IRQ_TRIGGER_RISING_EDGE, check_irq_handler, _context );
//    keys_log("0");
  } else {
    MicoGpioEnableIRQ( _context->gpio, IRQ_TRIGGER_FALLING_EDGE, check_irq_handler, _context );
//    keys_log("1");
  }
  if( count < 2){
    mico_stop_timer(&_context->_user_button_timer);
    mico_start_timer(&_context->_user_button_timer);
  }
  count ++;
}

void (*check_irq_handler_array[PIN_NUM])() = {check_irq_handler, check_irq_handler, check_irq_handler, check_irq_handler, check_irq_handler, check_irq_handler, check_irq_handler, check_irq_handler};


static void check_timeout_handler( void* arg )
{
  check_context_t *_context = arg;
  count = 0;
  
  if ( MicoGpioInputGet( _context->gpio ) == 0 ) {
    if( _context->check_pin_low_func != NULL )
      (_context->check_pin_low_func)();
  }else{
    if( _context->check_pin_high_func != NULL )
      (_context->check_pin_high_func)();
  }
  mico_stop_timer(&_context->_user_button_timer);  
}

void (*check_timeout_handler_array[PIN_NUM])() = {check_timeout_handler, check_timeout_handler, check_timeout_handler, check_timeout_handler, check_timeout_handler, check_timeout_handler, check_timeout_handler, check_timeout_handler};

void check_pin_init( int index, check_pin_init_t init)
{
  context[index].gpio = init.gpio;
  context[index].timeout = init.long_pressed_timeout;
  context[index].check_pin_low_func = init.check_pin_low_func;
  context[index].check_pin_high_func = init.check_pin_high_func;

  MicoGpioInitialize( init.gpio, INPUT_PULL_UP );
  mico_init_timer( &context[index]._user_button_timer, init.long_pressed_timeout, check_timeout_handler_array[index], &context[index] );
  MicoGpioEnableIRQ( init.gpio, IRQ_TRIGGER_FALLING_EDGE, check_irq_handler_array[index], &context[index] );
}




