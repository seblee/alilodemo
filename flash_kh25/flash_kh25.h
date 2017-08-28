#include "mico.h"
#include "eland_spi.h"

// Define 4 SPI pins
#define Eland_CS MICO_GPIO_23
#define Eland_SCLK MICO_GPIO_12
#define Eland_MOSI MICO_GPIO_14
#define Eland_MISO MICO_GPIO_4

#define KH_CMD_WREN (uint8_t)0X06
#define KH_CMD_WRDI (uint8_t)0X04
#define KH_CMD_RDSR (uint8_t)0X05
#define KH_CMD_WRSR (uint8_t)0X01
#define KH_CMD_READ (uint8_t)0X03
#define KH_CMD_FAST_READ (uint8_t)0X0B
#define KH_CMD_DREAD (uint8_t)0X3B
#define KH_CMD_SE (uint8_t)0X20
#define KH_CMD_BE (uint8_t)0XD8
#define KH_CMD_CE (uint8_t)0XC7
#define KH_CMD_pp (uint8_t)0X02
#define KH_CMD_DP (uint8_t)0XB9
#define KH_CMD_RDP (uint8_t)0XAB
#define KH_CMD_RES (uint8_t)0XAB
#define KH_CMD_RDID (uint8_t)0X9F
#define KH_CMD_REMS (uint8_t)0X90
#define KH_CMD_ENSO (uint8_t)0XB1
#define KH_CMD_EXSO (uint8_t)0XC1
#define KH_CMD_RESCUR (uint8_t)0X2B
#define KH_CMD_WRSCUR (uint8_t)0X2F
