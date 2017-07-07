#ifndef _AUDIO_INTERFACE_H_
#define _AUDIO_INTERFACE_H_

#include "mico.h"

#define AUDIO_SPI_DEBUG_ENABLE      (0)

#define AUDIO_SPI_PORT              (MICO_AUDIO_AIROBOT_SPI)
#define AUDIO_SPI_CS                (MICO_AUDIO_AIROBOT_SPI_CHIP_SELECT)

#define AUDIO_SPI_CONTROL           (MICO_AUDIO_AIROBOT_SPI_FLOW_CTRL) //FLOW CONTROL

#define SPI_LAYER_FRAME_HEAD        ('S')
#define SPI_LAYER_FRAME_LEN         (5)

#define AUDIO_MAX_SPI_MESSAGE_LEN   (2048)
#define SPI_RECV_TEMP_LEN           (50)

#define SPI_RECV_TIMEOUT            (40)

#define SPI_TRANSFER_MAX_TETRY      (3)

//user api
OSStatus audio_interface_start( void );
OSStatus audio_interface_stop( void );
OSStatus audio_interface_transfer( const uint8_t *send_buff, uint32_t send_len, uint8_t *recv_buff, uint32_t recv_len, uint32_t *real_recv_len );

#endif
