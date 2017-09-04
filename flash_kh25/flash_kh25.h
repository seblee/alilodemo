#include "mico.h"
#include "eland_spi.h"
// Define 4 SPI pins
#define Eland_CS MICO_GPIO_23
#define Eland_SCLK MICO_GPIO_12
#define Eland_MOSI MICO_GPIO_22
#define Eland_MISO MICO_GPIO_14

#define KH25L8006_PAGE_SIZE 256

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
    ElandFlash_DUMMY_BYTE = 0xA5,
} ElandFlash_command_t;
void start_spi_test_service(void);
void KH25_TEST_Thread(mico_thread_arg_t arts);
OSStatus flash_kh25_init(void);
//void flash_kh25_read(uint8_t *spireadbuffer, uint32_t address, uint32_t length);
//void flash_kh25_write(uint8_t *spireadbuffer, uint32_t address, uint32_t length);
void flash_kh25_sector_erase(uint32_t address);
void flash_kh25_block_erase(uint32_t address);
void flash_kh25_chip_erase(void);

#define flash_kh25_check_string "flash_kh25_check_string"
#define flash_kh25_test_address 0x801c
#define flash_kh25_test_string "\
-----BEGIN CERTIFICATE-----\r\n\
MIIDWTCCAkGgAwIBAgIUVtsDRUEJ8Hb8OqE35C4GKr9MQCQwDQYJKoZIhvcNAQEL\r\n\
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\r\n\
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTE3MDMyMjAzNDg0\r\n\
M1oXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\r\n\
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKpKGTwnVu+40zto4nHN\r\n\
XlH8yWJswrv3YS4FeYYBKTleHzOx7exjdXMd/L019cmUExPinKKc9wlPIjg5YlYT\r\n\
0qTgNIqoN0KSwr2lNxfrl7BhlGwNjv1n78JhNgq3JSHXXwbjx4TpSvDVi1Xk2gOQ\r\n\
8WQAJyf7PwcArHtVWJf3S/nbxsdywT9GxK7otF6i59fWe2gvCrMUgJ0sN48sm9d3\r\n\
kvQba+P5urWGOZoIi7VkGucQxogvIp7pu8tWtKwfF3qfLERGhhTMHEfnU6NlJaVn\r\n\
I5gRUQzo2ZD25ly2nsSyaYVaPjzCykDAk/VR4HlmXhl9LlP5nJLZeFR7wz369qAY\r\n\
ml8CAwEAAaNgMF4wHwYDVR0jBBgwFoAUnx5icteUC/r3yxIPHFJE9+4SH5wwHQYD\r\n\
VR0OBBYEFJQkP39VqmzrH4aKLQex8ERFzvTQMAwGA1UdEwEB/wQCMAAwDgYDVR0P\r\n\
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQAgqp2O3YhDcROr5pAto/rpAhtP\r\n\
SQIsst8W2SMaeimt9w9j8VEhUvWVDaVEjYCDdqYM6eZqzPXSXSkOsX0hT6Sj+VKt\r\n\
Sqn7POVZYaeJOopSZFid5NQPUcZnDW8HSNUo58Ow3n3WLgNrucNwrCVMf8hsUHNc\r\n\
5XIEZ+7X04JsXQkQCqHIP5jJOOsi6TFsB03eiyYeFGAYG/UX9TL73+3QGt9ABUn/\r\n\
PhLtql6vN87LOApQQ8P4SudFXUQ5h1pkYK4dLhhrP1cdIT7F/w8RgEILgMsb2/nI\r\n\
Nmgha2YDDMMJrZivg982f6gmv4hI/k/FDv+f294H+JOk3coIF2b5CDSWoqJr\r\n\
-----END CERTIFICATE-----\r\n\
"
