#include "flash_kh25.h"
#define flash_kh25_log(M, ...) custom_log("flash_kh25", M, ##__VA_ARGS__)

uint8_t *elandSPIBuffer;
#define flash_kh25_check_string "flash_kh25_check_string\r\n"
// Define SPI pins
Spi_t Spi_eland = {
    .ui_CS = Eland_CS,
    .ui_SCLK = Eland_SCLK,
    .ui_MOSI = Eland_MOSI,
    .ui_MISO = Eland_MISO,
    .spiMode = Mode0_0,
    .spiType = SPIMaster,
};

OSStatus flash_kh25_init(void)
{
    OSStatus err = kGeneralErr;
    int i;
    v_SPIInitSimulate(&Spi_eland); //初始化IO
    mico_thread_sleep(1);
    flash_kh25_log("check kh25");
    elandSPIBuffer = malloc(4);
    memset(elandSPIBuffer, 0, 4);
    *elandSPIBuffer = (uint8_t)ElandFlash_READ_JEDEC_ID;
    spiReadWirteOneData(&Spi_eland, elandSPIBuffer, 4);
    require_string((*(elandSPIBuffer + 1) == 0xc2), exit, "flash_kh25 check err");
    flash_kh25_log("flash_kh25 = %s", (*(elandSPIBuffer + 3) == 0x14) ? "KH25L8006E" : "KH25L1606E");
    *elandSPIBuffer = (uint8_t)ElandFlash_READ_JEDEC_ID;
    spiReadWirteOneData(&Spi_eland, elandSPIBuffer, 4);

    free(elandSPIBuffer);

    elandSPIBuffer = malloc(280);
    memset(elandSPIBuffer, 0, 280);
    sprintf((char *)(elandSPIBuffer + 4), "%s", flash_kh25_check_string);
    // for (i = 8; i < strlen(flash_kh25_check_string); i++)
    //     *(elandSPIBuffer + 4 + i) = 0xff;
    flash_kh25_write(317, elandSPIBuffer, strlen(flash_kh25_check_string));
    memset(elandSPIBuffer, 0, 280);
    flash_kh25_read(317, elandSPIBuffer, strlen(flash_kh25_check_string));

    printf("read out:%s", elandSPIBuffer + 4);
    free(elandSPIBuffer);

    return kNoErr;
exit:
    free(elandSPIBuffer);
    return err;
}

void flash_kh25_read(uint32_t address, uint8_t *spireadbuffer, uint32_t length)
{
    *spireadbuffer = (uint8_t)ElandFlash_READ;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length + 4);
}
void flash_kh25_write(uint32_t address, uint8_t *spireadbuffer, uint32_t length)
{
    //the write enable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_ENABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    //the page program
    *spireadbuffer = (uint8_t)ElandFlash_WRITE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length + 4);
    //the write disable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_DISABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
}
void flash_kh25_sector_erase(uint32_t address)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    //the write enable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_ENABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    //the sector erase
    *spireadbuffer = (uint8_t)ElandFlash_SECTOR_ERASE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    //the write disable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_DISABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
}
