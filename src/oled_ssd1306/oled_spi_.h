/*
 * oled_spi.h
 *
 *  Created on: 2019年2月9日
 *      Author: XIAOSENLUO
 *  移植到CH32V307VCT6 - 软件SPI
 */

#ifndef OLED_SPI__H_
#define OLED_SPI__H_

#include "oled_includes_.h"

// OLED 引脚定义
#define OLED_SCL_GPIO_PORT    GPIOE
#define OLED_SCL_GPIO_PIN     GPIO_Pin_11

#define OLED_SDA_GPIO_PORT    GPIOE
#define OLED_SDA_GPIO_PIN     GPIO_Pin_13

#define OLED_RST_GPIO_PORT    GPIOE
#define OLED_RST_GPIO_PIN     GPIO_Pin_15

#define OLED_DC_GPIO_PORT     GPIOD
#define OLED_DC_GPIO_PIN      GPIO_Pin_9

#define OLED_CS_GPIO_PORT     GPIOD
#define OLED_CS_GPIO_PIN      GPIO_Pin_11

// 兼容旧名称
#define SSD1306_RST_PORT     OLED_RST_GPIO_PORT
#define SSD1306_RST_PIN      OLED_RST_GPIO_PIN
#define SSD1306_DC_PORT      OLED_DC_GPIO_PORT
#define SSD1306_DC_PIN       OLED_DC_GPIO_PIN
#define SSD1306_CS_PORT      OLED_CS_GPIO_PORT
#define SSD1306_CS_PIN       OLED_CS_GPIO_PIN
#define SSD1306_MOSI_PORT    OLED_SDA_GPIO_PORT
#define SSD1306_MOSI_PIN     OLED_SDA_GPIO_PIN
#define SSD1306_SCK_PORT     OLED_SCL_GPIO_PORT
#define SSD1306_SCK_PIN      OLED_SCL_GPIO_PIN

// 控制宏
#define SSD1306_SELETE()       GPIO_ResetBits(OLED_CS_GPIO_PORT, OLED_CS_GPIO_PIN)
#define SSD1306_NOSELETE()     GPIO_SetBits(OLED_CS_GPIO_PORT, OLED_CS_GPIO_PIN)
#define SSD1306_MODE_COMMAND() GPIO_ResetBits(OLED_DC_GPIO_PORT, OLED_DC_GPIO_PIN)
#define SSD1306_MODE_DATA()    GPIO_SetBits(OLED_DC_GPIO_PORT, OLED_DC_GPIO_PIN)
#define SSD1306_RST_SET()      GPIO_SetBits(OLED_RST_GPIO_PORT, OLED_RST_GPIO_PIN)
#define SSD1306_RST_RESET()    GPIO_ResetBits(OLED_RST_GPIO_PORT, OLED_RST_GPIO_PIN)

void SPIInit(void);
void SPIWrite(uint8_t chr);
void SPIWriteBuffer(uint8_t* buffer, uint16_t Size);

#endif /* NEW_OLED_SPI__H_ */
