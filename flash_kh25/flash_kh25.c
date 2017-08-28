#include "flash_kh25.h"
#define flash_kh25_log(M, ...) custom_log("flash_kh25", M, ##__VA_ARGS__)

uint8_t *elandSPIBuffer;

// Define SPI pins
Spi_t Spi_eland = {
    .ui_CS = Eland_CS,
    .ui_SCLK = Eland_SCLK,
    .ui_MOSI = Eland_MOSI,
    .ui_MISO = Eland_MISO,
    .spiMode = Mode0_0,
    .spiType = SPIMaster,
};

void flash_kh25_init(void)
{
    v_SPIInitSimulate(&Spi_eland); //初始化IO
    flash_kh25_log("check kh25");
    elandSPIBuffer = malloc(4);
    memset(elandSPIBuffer, 0, 4);
    *elandSPIBuffer = KH_CMD_RDID;
    spiReadWirteOneData
}
