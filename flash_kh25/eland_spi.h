#include "mico.h"

#define SPIDelay(n) MicoNanosendDelay(1000 * n)
#define SPIDelaytDVCH MicoNanosendDelay(5)  //至少2ns
#define SPIDelaytCHDX MicoNanosendDelay(10) //至少5ns
#define SPIDelaytCLQV MicoNanosendDelay(10) //最大8/6ns

#define SPIDelaytCH MicoNanosendDelay(100) //MIN fc8/6ns
#define SPIDelaytCL MicoNanosendDelay(100) //MIN fc8/6ns

// Define SPI communication mode
typedef enum SPIMode {
    Mode0_0, /* Clock Polarity(CPOL) is 0 and Clock Phase(CPHA) is 0 */
    Mode0_1, /* Clock Polarity(CPOL) is 0 and Clock Phase(CPHA) is 1 */
    Mode1_0, /* Clock Polarity(CPOL) is 1 and Clock Phase(CPHA) is 0 */
    Mode1_1, /* Clock Polarity(CPOL) is 1 and Clock Phase(CPHA) is 1 */
} SPIMode;

// Define SPI type
typedef enum SPIType {
    SPIMaster,
    SPISlave,
} SPIType;

// Define SPI attribute
typedef struct SpiStruct
{
    mico_gpio_t ui_CS;
    mico_gpio_t ui_SCLK;
    mico_gpio_t ui_MOSI;
    mico_gpio_t ui_MISO;
    SPIMode spiMode;
    SPIType spiType;
} Spi_t;

// Function prototypes
void v_SPIInitSimulate(Spi_t *p_Spi);                                             //IO口初始化
void v_CSIsEnableSimulate(Spi_t *p_Spi, int i_IsEnable);                          //片選使能
void v_SPIWriteSimulate(Spi_t *p_Spi, unsigned char *puc_Data, int i_DataLength); //spi數據寫入
void v_SPIReadSimulate(Spi_t *p_Spi, unsigned char *puc_Data, int i_DataLength);  //spi數據讀取
//數據讀取
void spiReadWirteOneData(Spi_t *p_Spi, unsigned char *inbuff, unsigned char *outbuff, unsigned int datalength);
