/**
 ****************************************************************************
 * @Warning :Without permission from the author,Not for commercial use
 * @File    :undefined
 * @Author  :seblee
 * @date    :2018-03-22 09:15:02
 * @version :V 1.0.0
 *************************************************
 * @Last Modified by  :seblee
 * @Last Modified time:2018-03-22 09:26:03
 * @brief   :
 ****************************************************************************
**/

/* Private include -----------------------------------------------------------*/
#include "mico.h"
#include "eland_spi.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define CONFIG_SPI_DEBUG
#ifdef CONFIG_SPI_DEBUG
#define spi_log(M, ...) custom_log("SPI", M, ##__VA_ARGS__)
#else
#define client_log(...)
#endif /* ! CONFIG_CLIENT_DEBUG */
/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/*
Brief: SPI protocol initiate
Input: p_Spi, which spi use
Output: None
Return: None
Author: Andy Lai
*/
void v_SPIInitSimulate(Spi_t *p_Spi)
{
    //assert(p_Spi != NULL);

    if (p_Spi->spiType == SPIMaster)
    {
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_CS, OUTPUT_OPEN_DRAIN_PULL_UP);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_SCLK, OUTPUT_OPEN_DRAIN_PULL_UP);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_MOSI, OUTPUT_OPEN_DRAIN_PULL_UP);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_MISO, INPUT_PULL_UP);
    }
    else
    {
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_CS, INPUT_HIGH_IMPEDANCE);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_SCLK, INPUT_HIGH_IMPEDANCE);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_MOSI, INPUT_HIGH_IMPEDANCE);
        MicoGpioInitialize((mico_gpio_t)p_Spi->ui_MISO, OUTPUT_OPEN_DRAIN_PULL_UP);
    }
    v_CSIsEnableSimulate(p_Spi, 0);

    switch (p_Spi->spiMode)
    {
    case Mode0_0:
    case Mode0_1:
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;
    case Mode1_0:
    case Mode1_1:
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;
    }
}

/*
Brief: CS low level signal enable and high level signal disable
Input: (1)p_Spi, which spi use
       (2)i_IsEnable, Chip select(Slave select) enable flag
Output: None
Return: None
Author: Andy Lai
*/
void v_CSIsEnableSimulate(Spi_t *p_Spi, int i_IsEnable)
{
    //    assert(p_Spi != NULL);

    if (i_IsEnable)
    {
        MicoGpioOutputLow(p_Spi->ui_CS);
    }
    else
    {
        MicoGpioOutputHigh(p_Spi->ui_CS);
    }
}

/*
Brief: Use SPI to write a byte data
Input: (1)p_Spi, which spi use
       (2)uc_Bt, write byte data
Output: None
Return: None
Author: Andy Lai
*/
static void v_SPIWriteByte(Spi_t *p_Spi, unsigned char uc_Bt)
{
    int i = 0;
    //    assert(p_Spi != NULL);
    switch (p_Spi->spiMode)
    {
    case Mode0_0: /* Clock Polarity is 0 and Clock Phase is 0 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            if (uc_Bt & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCL; //è‡³å°‘2ns

            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            SPIDelaytCH; //è‡³å°‘5ns
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode0_1: /* Clock Polarity is 0 and Clock Phase is 1 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            if (uc_Bt & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL;
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode1_0: /* Clock Polarity is 1 and Clock Phase is 0 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            if (uc_Bt & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    case Mode1_1: /* Clock Polarity is 1 and Clock Phase is 1 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            if (uc_Bt & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCL;
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            SPIDelaytCH;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    default:
        break;
    }
}

/*
Brief: Use SPI protocol to write data
Input: (1)p_Spi, which spi use
       (2)puc_Data, write data string
       (3)i_DataLength, write data length
Output: None
Return: None
Author: Andy Lai
*/
void v_SPIWriteSimulate(Spi_t *p_Spi, unsigned char *puc_Data, int i_DataLength)
{
    int i = 0;

    //    assert(p_Spi != NULL);
    //    assert(puc_Data != NULL);
    //    assert(i_DataLength > 0);

    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(8);

    // Write data
    for (i = 0; i < i_DataLength; i++)
    {
        v_SPIWriteByte(p_Spi, puc_Data[i]);
    }

    SPIDelay(8);
    v_CSIsEnableSimulate(p_Spi, 0);
}

/*
Brief: Read a byte data from SPI
Input: p_Spi, which spi use
Output: None
Return: Read data
Author: Andy Lai
*/
static unsigned char uc_SPIReadByte(Spi_t *p_Spi)
{
    int i = 0;
    unsigned char uc_ReadData = 0;

    //    assert(p_Spi != NULL);

    switch (p_Spi->spiMode)
    {
    case Mode0_0: /* Clock Polarity is 0 and Clock Phase is 0 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        for (i = 0; i < 8; i++)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL; //æœ€å¤§8/6ns
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            uc_ReadData = uc_ReadData << 1;
            uc_ReadData |= MicoGpioInputGet(p_Spi->ui_MISO);
            SPIDelaytCH; //æœ€å¤§8/6ns
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode0_1: /* Clock Polarity is 0 and Clock Phase is 1 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        for (i = 0; i < 8; i++)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            uc_ReadData = uc_ReadData << 1;
            uc_ReadData |= MicoGpioInputGet(p_Spi->ui_MISO);
            SPIDelaytCL;
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode1_0: /* Clock Polarity is 1 and Clock Phase is 0 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 0; i < 8; i++)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            uc_ReadData = uc_ReadData << 1;
            uc_ReadData |= MicoGpioInputGet(p_Spi->ui_MISO);
            SPIDelaytCL;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    case Mode1_1: /* Clock Polarity is 1 and Clock Phase is 1 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 0; i < 8; i++)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL;
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            uc_ReadData = uc_ReadData << 1;
            uc_ReadData |= MicoGpioInputGet(p_Spi->ui_MISO);
            SPIDelaytCH;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    default:
        break;
    }
    return uc_ReadData;
}

/*
Brief: Use SPI to read data
Input: (1)p_Spi, which SPI use;    
       (2)i_DataLength, the length of data that need to read
Output: puc_Data, need to get data
Return: None
Author: Andy Lai
*/
void v_SPIReadSimulate(Spi_t *p_Spi, unsigned char *puc_Data, int i_DataLength)
{
    int i = 0;

    //    assert(p_Spi != NULL);
    //    assert(i_DataLength > 0);

    v_CSIsEnableSimulate(p_Spi, 1);
    SPIDelay(8);

    // Read data
    for (i = 0; i < i_DataLength; i++)
    {
        puc_Data[i] = uc_SPIReadByte(p_Spi);
    }

    SPIDelay(8);
    v_CSIsEnableSimulate(p_Spi, 0);
}
/*
Brief: Use SPI to read data
Input: (1)p_Spi, which SPI use;    
       (2)inbuff, the data of master to slaver
Output: outbuff,the data of slaver to master
Return: None
Author: seblee
*/
static void spiReadWirteOneByte(Spi_t *p_Spi, unsigned char *tranferbuff)
{
    int i = 0;
    //    assert(p_Spi != NULL);
    switch (p_Spi->spiMode)
    {
    case Mode0_0: /* Clock Polarity is 0 and Clock Phase is 0 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);

        for (i = 0; i < 8; i++)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            if (*tranferbuff & 0x80)
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCL; //最少2ns
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            *tranferbuff <<= 1;
            if (MicoGpioInputGet(p_Spi->ui_MISO))
                *tranferbuff |= 1;
            SPIDelaytCH; //最少5ns
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode0_1: /* Clock Polarity is 0 and Clock Phase is 1 */
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            if (*tranferbuff & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL;
        }
        MicoGpioOutputLow(p_Spi->ui_SCLK);
        break;

    case Mode1_0: /* Clock Polarity is 1 and Clock Phase is 0 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            if (*tranferbuff & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCH;
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            SPIDelaytCL;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    case Mode1_1: /* Clock Polarity is 1 and Clock Phase is 1 */
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        for (i = 7; i >= 0; i--)
        {
            MicoGpioOutputLow(p_Spi->ui_SCLK);
            if (*tranferbuff & (1 << i))
            {
                MicoGpioOutputHigh(p_Spi->ui_MOSI);
            }
            else
            {
                MicoGpioOutputLow(p_Spi->ui_MOSI);
            }
            SPIDelaytCL;
            MicoGpioOutputHigh(p_Spi->ui_SCLK);
            SPIDelaytCH;
        }
        MicoGpioOutputHigh(p_Spi->ui_SCLK);
        break;

    default:
        break;
    }
}
/*
Brief: Use SPI to read data
Input: (1)p_Spi, which SPI use;    
       (2)inbuff, the data of master to slaver
Output: outbuff,the data of slaver to master
Return: None
Author: seblee
*/
void spiReadWirteOneData(Spi_t *p_Spi, uint8_t *tranferbuff, uint16_t datalength)
{
    uint16_t i;
    for (i = 0; i < datalength; i++)
    {
        spiReadWirteOneByte(p_Spi, tranferbuff + i);
    }
}
