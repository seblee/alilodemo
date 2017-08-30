#include "mico.h"
#include "eland_spi.h"
// Define 4 SPI pins
#define Eland_CS MICO_GPIO_23
#define Eland_SCLK MICO_GPIO_12
#define Eland_MOSI MICO_GPIO_22
#define Eland_MISO MICO_GPIO_14
/* Command definitions */
typedef enum {

    ElandFlash_WRITE_ENABLE = 0x06,            /* WREN write enable */
    ElandFlash_WRITE_DISABLE = 0x04,           /* WRDI write disenable */
    ElandFlash_READ_STATUS_REGISTER = 0x05,    /* RDSR read status register */
    ElandFlash_WRITE_STATUS_REGISTER = 0x01,   /* WRSR write status register*/
    ElandFlash_READ = 0x03,                    /* READ read data */
    ElandFlash_FAST_READ = 0x0B,               /* FAST READ fast read data */
    ElandFlash_DREAD = 0X3B,                   /* DREAD Double Output Mode command */
    ElandFlash_SECTOR_ERASE = 0x20,            /* SE sector erase*/
    ElandFlash_BLOCK_ERASE_MID = 0x52,         /* BE block erase */
    ElandFlash_BLOCK_ERASE_LARGE = 0xD8,       /* BE block erase */
    ElandFlash_CHIP_ERASE1 = 0x60,             /* CE   chip erase */
    ElandFlash_CHIP_ERASE2 = 0xC7,             /* CE   chip erase */
    ElandFlash_WRITE = 0x02,                   /* PP page progran */
    ElandFlash_DEEP_POWER_DOWN = 0xB9,         /* DP Deep power down */
    ElandFlash_RELEASE_DEEP_POWER_DOWN = 0xAB, /* RDP Release from deep power down */
    ElandFlash_READ_JEDEC_ID = 0x9F,           /* RDID read indentification */
    ElandFlash_READ_ID1 = 0x90,                /* REMS read electronic manufacturer & device ID*/
    ElandFlash_READ_ID2 = 0xAB,                /* RES read electronic ID */
    ElandFlash_ENTER_SECURED_OTP = 0xB1,       /* ENSO enter secured OPT */
    ElandFlash_EXIT_SECURED_OTP = 0xC1,        /* EXSO exit secured OPT */
    ElandFlash_READ_SECURITY_REGISTER = 0x2B,  /* RDSCUR  read security register */
    ElandFlash_WRITE_SECURITY_REGISTER = 0x2F, /* WRSCUR  write security register */

} ElandFlash_command_t;

OSStatus flash_kh25_init(void);
void flash_kh25_read(uint32_t address, uint8_t *spireadbuffer, uint32_t length);
void flash_kh25_write(uint32_t address, uint8_t *spireadbuffer, uint32_t length);
