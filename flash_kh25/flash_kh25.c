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

    mico_thread_sleep(2);
    flash_kh25_log("check kh25");
    elandSPIBuffer = malloc(4);
    memset(elandSPIBuffer, 0, 4);
    *elandSPIBuffer = KH_CMD_RDID;
    flash_kh25_log("elandSPIBuffer1 = 0x%02X 0x%02X 0x%02X 0x%02X", *elandSPIBuffer, *(elandSPIBuffer + 1),
                   *(elandSPIBuffer + 2), *(elandSPIBuffer + 3));
    spiReadWirteOneData(&Spi_eland, elandSPIBuffer, 4);
    flash_kh25_log("elandSPIBuffer2 = 0x%02X 0x%02X 0x%02X 0x%02X", *elandSPIBuffer, *(elandSPIBuffer + 1),
                   *(elandSPIBuffer + 2), *(elandSPIBuffer + 3));
    free(elandSPIBuffer);
}
