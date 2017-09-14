#include "flash_kh25.h"
#define flash_kh25_log(M, ...) custom_log("flash_kh25", M, ##__VA_ARGS__)

uint8_t *elandSPIBuffer = NULL;
static mico_semaphore_t kh25_InitCompleteSem = NULL;
// Define SPI pins
Spi_t Spi_eland = {
    .ui_CS = Eland_CS,
    .ui_SCLK = Eland_SCLK,
    .ui_MOSI = Eland_MOSI,
    .ui_MISO = Eland_MISO,
    .spiMode = Mode0_0,
    .spiType = SPIMaster,
};

/****************************/
static void flash_kh25_wait_for_WIP(uint32_t time)
{
    uint32_t count = 0;
    uint8_t cache = 0;
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    //the write  enable
    cache = (uint8_t)ElandFlash_READ_STATUS_REGISTER;
    spiReadWirteOneData(&Spi_eland, &cache, 1);
    do
    {
        spiReadWirteOneData(&Spi_eland, &cache, 1);
        if (cache & 1)
        {
            //flash_kh25_log("%02X", cache);
            SPIDelay(10);
            count++;
            continue;
        }
        else
            break;
    } while (count < time);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
}
/*************************/
OSStatus
start_spi_test_service(void)
{
    OSStatus err = kGeneralErr;
    err = mico_rtos_init_semaphore(&kh25_InitCompleteSem, 1);
    require_noerr_string(err, exit, "kh25_InitCompleteSem init err");

    err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "KH25_TEST_Thread", KH25_TEST_Thread, 0x1000, 0);
    require_noerr_string(err, exit, "KH25_TEST_Thread err");

    err = mico_rtos_get_semaphore(&kh25_InitCompleteSem, MICO_WAIT_FOREVER);
    require_noerr_string(err, exit, "kh25_InitCompleteSem get err");

    err = mico_rtos_deinit_semaphore(&kh25_InitCompleteSem);
    require_noerr_string(err, exit, "kh25_InitCompleteSem deinit err");

exit:
    return err;
}
void KH25_TEST_Thread(mico_thread_arg_t arts)
{
    flash_kh25_init();
    mico_rtos_set_semaphore(&kh25_InitCompleteSem); //wait until get semaphore
    mico_rtos_delete_thread(NULL);
}

void flash_kh25_read(uint8_t *spireadbuffer, uint32_t address, uint32_t length)
{
    uint8_t cache[5];
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_READ;
    cache[1] = (uint8_t)((address >> 16) & 0xff);
    cache[2] = (uint8_t)((address >> 8) & 0xff);
    cache[3] = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, cache, 4);              //發送命令
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length); //發送數據
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
}
/******************************/
static void flash_kh25_write_enable(void)
{
    uint8_t cache;
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    //the write  enable
    cache = (uint8_t)ElandFlash_WRITE_ENABLE;
    spiReadWirteOneData(&Spi_eland, &cache, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
}
/******************************/
static void flash_kh25_write_disable(void)
{
    uint8_t cache;
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    //the write  enable
    cache = (uint8_t)ElandFlash_WRITE_DISABLE;
    spiReadWirteOneData(&Spi_eland, &cache, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
}
/******************************/
static void flash_kh25_write(uint8_t *spireadbuffer, uint32_t address, uint32_t length)
{
    uint8_t cache[5];
    flash_kh25_wait_for_WIP(KH25L8006_WIP_WAIT_TIME_MAX);
    flash_kh25_write_enable();
    //the page program
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_WRITE;
    cache[1] = (uint8_t)((address >> 16) & 0xff);
    cache[2] = (uint8_t)((address >> 8) & 0xff);
    cache[3] = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, cache, 4);              //發送命令
    spiReadWirteOneData(&Spi_eland, spireadbuffer, length); //發送數據
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_write_disable();
}
static OSStatus flash_kh25_check_device(void)
{
    uint8_t cache[6];
    memset(cache, 0, 6);
    /**************RDID***********************/
    flash_kh25_log("check kh25");
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_READ_JEDEC_ID;
    spiReadWirteOneData(&Spi_eland, cache, 4);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    require_string((cache[1] == 0xc2), exit, "flash_kh25 check err");
    flash_kh25_log("manufacturer ID = 0x%02X", cache[1]);
    flash_kh25_log("memory type     = 0x%02X", cache[2]);
    flash_kh25_log("memory density  = 0x%02X", cache[3]);
    flash_kh25_log("flash model     = %s", (cache[3] == 0x14) ? "KH25L8006E" : "KH25L1606E");
    /**************RES***********************/
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_READ_ID2;
    cache[1] = (uint8_t)ElandFlash_DUMMY_BYTE;
    cache[2] = (uint8_t)ElandFlash_DUMMY_BYTE;
    cache[3] = (uint8_t)ElandFlash_DUMMY_BYTE;
    spiReadWirteOneData(&Spi_eland, cache, 5);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_log("electronic ID   = 0x%02X", cache[4]);
    /**************REMS***********************/
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_READ_ID1;
    cache[1] = (uint8_t)ElandFlash_DUMMY_BYTE;
    cache[2] = (uint8_t)ElandFlash_DUMMY_BYTE;
    cache[3] = (uint8_t)0;
    spiReadWirteOneData(&Spi_eland, cache, 6);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_log("manufacturer ID = 0x%02X", cache[4]);
    flash_kh25_log("device ID       = 0x%02X", cache[5]);
    /**************RDSR***********************/
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    cache[0] = (uint8_t)ElandFlash_READ_SECURITY_REGISTER;
    spiReadWirteOneData(&Spi_eland, cache, 2);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_log("status register = 0x%02X", cache[2]);
    return kNoErr;
exit:
    return kGeneralErr;
}

void flash_kh25_sector_erase(uint32_t address)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    flash_kh25_wait_for_WIP(KH25L8006_WIP_WAIT_TIME_MAX); //最長等待300ms
    flash_kh25_write_enable();
    //the sector erase
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    *spireadbuffer = (uint8_t)ElandFlash_SECTOR_ERASE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 4);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_log("sector_erase");
    flash_kh25_write_disable();
    free(spireadbuffer);
}
void flash_kh25_block_erase(uint32_t address)
{
    uint8_t *spireadbuffer = NULL;
    spireadbuffer = malloc(4);
    flash_kh25_wait_for_WIP(KH25L8006_WIP_WAIT_TIME_MAX); //最長等待
    flash_kh25_write_enable();
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    //the sector erase
    *spireadbuffer = (uint8_t)ElandFlash_SECTOR_ERASE;
    *(spireadbuffer + 1) = (uint8_t)((address >> 16) & 0xff);
    *(spireadbuffer + 2) = (uint8_t)((address >> 8) & 0xff);
    *(spireadbuffer + 3) = (uint8_t)(address & 0xff);
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 4);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);
    flash_kh25_log("block_erase");
    flash_kh25_write_disable();
    free(spireadbuffer);
}
void flash_kh25_chip_erase(void)
{
    uint8_t *spireadbuffer = NULL;
    flash_kh25_log("chip_erase start");
    spireadbuffer = malloc(4);
    flash_kh25_wait_for_WIP(KH25L8006_WIP_WAIT_TIME_MAX); //最長等待
    flash_kh25_write_enable();
    v_CSIsEnableSimulate(&Spi_eland, 1);
    SPIDelay(1);
    //the sector erase
    *spireadbuffer = (uint8_t)ElandFlash_CHIP_ERASE2;
    spiReadWirteOneData(&Spi_eland, spireadbuffer, 1);
    SPIDelay(1);
    v_CSIsEnableSimulate(&Spi_eland, 0);

    flash_kh25_log("chip_erase end");
    flash_kh25_write_disable();
    flash_kh25_wait_for_WIP(KH25L8006_WIP_WAIT_TIME_MAX); //最長等待
    free(spireadbuffer);
}
/* flash 按頁寫入數據*/
void flash_kh25_write_page(uint8_t *scr, uint32_t address, uint32_t length)
{
    uint8_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
    uint32_t writeaddr, NumByteToWrite;
    uint32_t dataAlreadyWrite = 0;
    writeaddr = address;
    NumByteToWrite = length;

    Addr = writeaddr % KH25L8006_PAGE_SIZE;
    count = KH25L8006_PAGE_SIZE - Addr;
    NumOfPage = NumByteToWrite / KH25L8006_PAGE_SIZE;
    NumOfSingle = NumByteToWrite % KH25L8006_PAGE_SIZE;
    if (Addr == 0) /* writeaddr is KH25L8006_PAGE_SIZE aligned  */
    {
        if (NumOfPage == 0) /* NumByteToWrite < KH25L8006_PAGE_SIZE */
        {
            flash_kh25_write(scr, writeaddr, NumByteToWrite);
        }
        else
        {
            while (NumOfPage--)
            {
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, KH25L8006_PAGE_SIZE);
                writeaddr += KH25L8006_PAGE_SIZE;
                dataAlreadyWrite += KH25L8006_PAGE_SIZE;
            }
            flash_kh25_write(scr + dataAlreadyWrite, writeaddr, NumOfSingle);
        }
    }
    else //WriteAddr is not KH25L8006_PAGE_SIZE aligned
    {
        if (NumOfPage == 0) /* NumByteToWrite < KH25L8006_PAGE_SIZE */
        {
            if (NumOfSingle > count) /* (NumByteToWrite + WriteAddr) > KH25L8006_PAGE_SIZE */
            {
                temp = NumOfSingle - count;
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, count);
                dataAlreadyWrite += count;
                writeaddr += count;
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, temp);
            }
            else
            {
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, NumOfSingle);
            }
        }
        else
        {
            NumByteToWrite -= count;
            NumOfPage = NumByteToWrite / KH25L8006_PAGE_SIZE;
            NumOfSingle = NumByteToWrite % KH25L8006_PAGE_SIZE;
            flash_kh25_write(scr + dataAlreadyWrite, writeaddr, count);
            dataAlreadyWrite += count;
            writeaddr += count;
            while (NumOfPage--)
            {
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, KH25L8006_PAGE_SIZE);
                writeaddr += KH25L8006_PAGE_SIZE;
                dataAlreadyWrite += KH25L8006_PAGE_SIZE;
            }
            if (NumOfSingle != 0)
            {
                flash_kh25_write(scr + dataAlreadyWrite, writeaddr, NumOfSingle);
            }
        }
    }
}
OSStatus flash_kh25_init(void)
{
    OSStatus err = kGeneralErr;
    v_SPIInitSimulate(&Spi_eland); //初始化IO
    mico_thread_sleep(1);
    /*****check flash******/
    err = flash_kh25_check_device();
    require_noerr_string(err, exit, "check kh25 flash failed");

    elandSPIBuffer = malloc(strlen(flash_kh25_check_string) + 2);
    memset(elandSPIBuffer, 0, strlen(flash_kh25_check_string) + 2);
    flash_kh25_read(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
    if (strcmp(flash_kh25_check_string, (char *)(elandSPIBuffer)) == 0)
        flash_kh25_log("read out:%s", elandSPIBuffer);
    else
    {
        flash_kh25_log("first read:%s", elandSPIBuffer);
        memset(elandSPIBuffer, 0, 60);
        flash_kh25_sector_erase(0);
        sprintf((char *)(elandSPIBuffer), "%s", flash_kh25_check_string);
        flash_kh25_write_page(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
        memset(elandSPIBuffer, 0, 60);
        flash_kh25_read(elandSPIBuffer, 0, strlen(flash_kh25_check_string));
        if (strcmp(flash_kh25_check_string, (char *)(elandSPIBuffer)) == 0)
            flash_kh25_log("check again:%s", elandSPIBuffer);
        else
            err = kGeneralErr;
    }
//flash_kh25_chip_erase();
exit:
    if (elandSPIBuffer != NULL)
    {
        free(elandSPIBuffer);
        elandSPIBuffer = NULL;
    }
    return err;
}
