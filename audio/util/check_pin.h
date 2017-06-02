/**
******************************************************************************
* @file    button.h 
* @author  Eshen Wang
* @version V1.0.0
* @date    1-May-2015
* @brief   user key operation. 
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

#ifndef __CHECK_PIN_H_
#define __CHECK_PIN_H_

#include "platform.h"
#include "platform_peripheral.h"

//--------------------------------  pin defines --------------------------------
typedef enum _check_index_e{
	IOCHECK_PIN_1 = 0,
        IOCHECK_PIN_2,
        IOCHECK_PIN_3,
        IOCHECK_PIN_4,
        IOCHECK_PIN_5,
        IOCHECK_PIN_6,
        IOCHECK_PIN_7,
} check_index_e;

typedef void (*check_pin_low_cb)(void) ;
typedef void (*check_pin_high_cb)(void) ;

typedef struct _check_pin_init_t{
	mico_gpio_t gpio;
	int long_pressed_timeout;
	check_pin_low_cb check_pin_low_func;
	check_pin_high_cb check_pin_high_func;
} check_pin_init_t;

//------------------------------ user interfaces -------------------------------
void check_pin_init( int index, check_pin_init_t init );


#endif  // __BUTTON_H_
