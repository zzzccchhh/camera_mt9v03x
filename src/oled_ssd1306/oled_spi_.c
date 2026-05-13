/*
 * oled_spi.c
 *
 *  Created on: 2019年2月9日
 *      Author: XIAOSENLUO
 *  移植到CH32V307VCT6 - 软件SPI (GPIO模拟)
 */

#include "oled_spi_.h"
#include "zf_common_headfile.h"

/* 软件SPI引脚初始化 */
static void SoftSPI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    /* 使能GPIO时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);

    /* SCL (SCK) - PE11 */
    GPIO_InitStruct.GPIO_Pin = OLED_SCL_GPIO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(OLED_SCL_GPIO_PORT, &GPIO_InitStruct);

    /* SDA (MOSI) - PE13 */
    GPIO_InitStruct.GPIO_Pin = OLED_SDA_GPIO_PIN;
    GPIO_Init(OLED_SDA_GPIO_PORT, &GPIO_InitStruct);

    /* RST - PE15 */
    GPIO_InitStruct.GPIO_Pin = OLED_RST_GPIO_PIN;
    GPIO_Init(OLED_RST_GPIO_PORT, &GPIO_InitStruct);

    /* DC - PD9 */
    GPIO_InitStruct.GPIO_Pin = OLED_DC_GPIO_PIN;
    GPIO_Init(OLED_DC_GPIO_PORT, &GPIO_InitStruct);

    /* CS - PD11 */
    GPIO_InitStruct.GPIO_Pin = OLED_CS_GPIO_PIN;
    GPIO_Init(OLED_CS_GPIO_PORT, &GPIO_InitStruct);

    /* 初始化引脚电平 */
    GPIO_SetBits(OLED_SCL_GPIO_PORT, OLED_SCL_GPIO_PIN);  // SCL高
    GPIO_SetBits(OLED_SDA_GPIO_PORT, OLED_SDA_GPIO_PIN);  // SDA高
    GPIO_SetBits(OLED_RST_GPIO_PORT, OLED_RST_GPIO_PIN);  // RST高
    GPIO_SetBits(OLED_DC_GPIO_PORT, OLED_DC_GPIO_PIN);    // DC高
    GPIO_SetBits(OLED_CS_GPIO_PORT, OLED_CS_GPIO_PIN);    // CS高
}

/* 软件SPI发送一个字节 */
static void SoftSPI_SendByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        /* SCL低开始传输 */
        GPIO_ResetBits(OLED_SCL_GPIO_PORT, OLED_SCL_GPIO_PIN);

        /* 若是高位，先设置SDA再拉高SCL；否则先清零SDA再拉高SCL */
        if (data & 0x80)
        {
            GPIO_SetBits(OLED_SDA_GPIO_PORT, OLED_SDA_GPIO_PIN);
        }
        else
        {
            GPIO_ResetBits(OLED_SDA_GPIO_PORT, OLED_SDA_GPIO_PIN);
        }

        /* 拉高SCL */
        GPIO_SetBits(OLED_SCL_GPIO_PORT, OLED_SCL_GPIO_PIN);

        /* 左移准备下一位 */
        data <<= 1;
    }

    /* 传输完成后SCL保持高 */
    GPIO_SetBits(OLED_SCL_GPIO_PORT, OLED_SCL_GPIO_PIN);
}

void SPIInit(void)
{
    SoftSPI_Init();
}

void SPIWrite(uint8_t chr)
{
#if (SPI_TRANSMIT == 0)
    /* 软件SPI发送 */
    SSD1306_SELETE();
    SoftSPI_SendByte(chr);
    SSD1306_NOSELETE();
#elif(SPI_TRANSMIT == 1)
    /* DMA模式暂不支持 */
#endif
}

void SPIWriteBuffer(uint8_t *buffer, uint16_t Size)
{
#if (SPI_TRANSMIT == 0)
    SSD1306_SELETE();
    while (Size--)
    {
        SoftSPI_SendByte(*buffer);
        buffer++;
    }
    SSD1306_NOSELETE();
#elif(SPI_TRANSMIT == 1)
    /* DMA模式暂不支持 */
#endif
}
