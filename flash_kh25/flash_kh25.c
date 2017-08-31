#include "flash_kh25.h"
#define flash_kh25_log(M, ...) custom_log("flash_kh25", M, ##__VA_ARGS__)

uint8_t *elandSPIBuffer;
#define flash_kh25_check_string "flash_kh25_check_string"
// Define SPI pins
Spi_t Spi_eland = {
    .ui_CS = Eland_CS,
    .ui_SCLK = Eland_SCLK,
    .ui_MOSI = Eland_MOSI,
    .ui_MISO = Eland_MISO,
    .spiMode = Mode0_0,
    .spiType = SPIMaster,
};
/*************************/
void start_spi_test_service(void)
{
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "KH25_TEST_Thread", KH25_TEST_Thread, 0x1000, 0);

    return;
}
void KH25_TEST_Thread(mico_thread_arg_t arts)
{
    flash_kh25_init();
    while (1)
    {
        mico_rtos_thread_sleep(5);
    }
    mico_rtos_delete_thread(NULL);
}

void flash_kh25_read(uint8_t *spireadbuffer, uint32_t address, uint32_t length)
{
    *spireadbuffer = (uint8_t)ElandFlash_READ;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length + 4);
}
/******************************/
static void flash_kh25_write_enable(void)
{
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    //the write  enable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_ENABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
}
/******************************/
static void flash_kh25_write_disable(void)
{
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    //the write  enable
    *spireadbuffer = (uint8_t)ElandFlash_WRITE_DISABLE;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
}
/******************************/
static void flash_kh25_write(uint8_t *spireadbuffer, uint32_t address, uint32_t length)
{
    uint8_t cache[5];
    flash_kh25_write_enable();
    //the page program
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_WRITE;
    cache[1] = (uint8_t)((address >> 16) & 0xff);
    cache[2] = (uint8_t)((address >> 8) & 0xff);
    cache[3] = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, cache, 4);                  //發送命令
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length + 4); //發送數據
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
    flash_kh25_write_disable();
}
static void flash_kh25_check_device(void)
{
    uint8_t cache[6];
    memset(cache, 0, 6);
    cache[0] = (uint8_t)ElandFlash_READ_JEDEC_ID;
    spiReadWirteOneData(&Spi_eland, elandSPIBuffer, 4);
    require_string((cache[1] == 0xc2), exit, "flash_kh25 check err");
    flash_kh25_log("flash_kh25 = %s", (cache[3] == 0x14) ? "KH25L8006E" : "KH25L1606E");
}

void flash_kh25_sector_erase(uint32_t address)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    flash_kh25_write_enable();
    //the sector erase
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    *spireadbuffer = (uint8_t)ElandFlash_SECTOR_ERASE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 4);
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
    // mico_rtos_thread_msleep(300);
    flash_kh25_write_disable();
    free(spireadbuffer);
}
void flash_kh25_block_erase(uint32_t address)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    flash_kh25_write_enable();
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    //the sector erase
    *spireadbuffer = (uint8_t)ElandFlash_SECTOR_ERASE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 4);
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
    //mico_rtos_thread_sleep(2);
    flash_kh25_write_disable();
    free(spireadbuffer);
}
void flash_kh25_chip_erase(void)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    flash_kh25_write_enable();
    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(1);
    //the sector erase
    *spireadbuffer = (uint8_t)ElandFlash_CHIP_ERASE2;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(p_Spi, 0);
    //mico_rtos_thread_sleep(2);
    flash_kh25_write_disable();
    free(spireadbuffer);
}
/* flash 按頁寫入數據*/
void flash_kh25_write_page(uint8_t *scr, uint32_t address, uint32_t length)
{
    uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
    uint32_t writeaddr, NumByteToWrite;
    uint8_t *pBuffer;
    writeaddr = address;
    NumByteToWrite = length;
    pBuffer = scr;

    Addr = writeaddr % KH25L8006_PAGE_SIZE;
    count = KH25L8006_PAGE_SIZE - Addr;
    NumOfPage = NumByteToWrite / KH25L8006_PAGE_SIZE;
    NumOfSingle = NumByteToWrite % KH25L8006_PAGE_SIZE;
    if (Addr == 0) /* writeaddr is KH25L8006_PAGE_SIZE aligned  */
    {
        if (NumOfPage == 0) /* NumByteToWrite < KH25L8006_PAGE_SIZE */
        {
            flash_kh25_write(pBuffer, writeaddr, NumByteToWrite);
        }
        else
        {
            while (NumOfPage--)
            {
                flash_kh25_write(pBuffer, writeaddr, KH25L8006_PAGE_SIZE);
                writeaddr += KH25L8006_PAGE_SIZE;
                pBuffer += KH25L8006_PAGE_SIZE;
            }
            flash_kh25_write(pBuffer, writeaddr, NumOfSingle);
        }
    }
    else //WriteAddr is not KH25L8006_PAGE_SIZE aligned
    {
        if (NumOfPage == 0) /* NumByteToWrite < KH25L8006_PAGE_SIZE */
        {
            if (NumOfSingle > count) /* (NumByteToWrite + WriteAddr) > KH25L8006_PAGE_SIZE */
            {
                temp = NumOfSingle - count;
                flash_kh25_write(pBuffer, writeaddr, count);
                pBuffer += count;
                writeaddr += count;
                flash_kh25_write(pBuffer, writeaddr, temp);
            }
            else
            {
                flash_kh25_write(pBuffer, writeaddr, count);
            }
        }
        else
        {
            NumByteToWrite -= count;
            NumOfPage = NumByteToWrite / KH25L8006_PAGE_SIZE;
            NumOfSingle = NumByteToWrite % KH25L8006_PAGE_SIZE;
            flash_kh25_write(pBuffer, writeaddr, count);
            pBuffer += count;
            writeaddr += count;
            while (NumOfPage--)
            {
                flash_kh25_write(pBuffer, writeaddr, KH25L8006_PAGE_SIZE);
                writeaddr += KH25L8006_PAGE_SIZE;
                pBuffer += KH25L8006_PAGE_SIZE;
            }
            if (NumOfSingle != 0)
                flash_kh25_write(pBuffer, writeaddr, KH25L8006_PAGE_SIZE);
        }
    }
}
OSStatus flash_kh25_init(void)
{
    OSStatus err = kGeneralErr;
    v_SPIInitSimulate(&Spi_eland); //初始化IO
    mico_thread_sleep(1);
    flash_kh25_log("check kh25");
    elandSPIBuffer = malloc(4);
    memset(elandSPIBuffer, 0, 4);

    free(elandSPIBuffer);

    elandSPIBuffer = malloc(280);
    memset(elandSPIBuffer, 0, 280);
    flash_kh25_read(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
    if (strcmp(flash_kh25_check_string, (char *)(elandSPIBuffer + 4)) == 0)
        flash_kh25_log("read out:%s", elandSPIBuffer + 4);
    else
    {
        flash_kh25_log("check out:%s", elandSPIBuffer + 4);
        memset(elandSPIBuffer, 0, 280);
        flash_kh25_sector_erase(1);
        sprintf((char *)(elandSPIBuffer + 4), "%s", flash_kh25_check_string);
        flash_kh25_write(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
        memset(elandSPIBuffer, 0, 280);
        flash_kh25_read(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
        flash_kh25_log("check again:%s", elandSPIBuffer + 4);
    }
    free(elandSPIBuffer);
    return kNoErr;
exit:
    free(elandSPIBuffer);
    return err;
}
