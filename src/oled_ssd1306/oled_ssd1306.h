/*
 * oled_ssd1306.h
 *
 *  Created on: 2019年2月9日
 *      Author: XIAOSENLUO
 */

#ifndef OLED_SSD1306_H_
#define OLED_SSD1306_H_

#include "stdbool.h"
#include "oled_spi_.h"

#define SSD1306_128_64 ///< 已弃用：旧的方式指定 128x64 屏幕
//#define SSD1306_128_32   ///< 已弃用：旧的方式指定 128x32 屏幕
//#define SSD1306_96_16  ///< 已弃用：旧的方式指定 96x16 屏幕

#define HEIGHT 									SSD1306_LCDHEIGHT
#define WIDTH										SSD1306_LCDWIDTH

#define BLACK                          0 ///< 绘制"灭"像素
#define WHITE                          1 ///< 绘制"亮"像素
#define INVERSE                        2 ///< 反转像素

#define SSD1306_I2C_ADDRESS   0x3C   // I2C地址 0x3C 或 0x3D

/*=========================================================================
    SSD1306 显示屏驱动
    -----------------------------------------------------------------------
    此驱动可用于多种显示屏 (128x64, 128x32 等).
    请在下方选择合适的显示屏以创建适当大小的帧缓冲等.

    SSD1306_128_64  128x64 像素显示屏

    SSD1306_128_32  128x32 像素显示屏

    SSD1306_96_16   96x16  像素显示屏

    -----------------------------------------------------------------------*/
   #define SSD1306_128_64
//   #define SSD1306_128_32
//   #define SSD1306_96_16
/*=========================================================================*/

#if defined SSD1306_128_64 && defined SSD1306_128_32
  #error "SSD1306.h 中同时只能指定一个 SSD1306 显示屏"
#endif
#if !defined SSD1306_128_64 && !defined SSD1306_128_32 && !defined SSD1306_96_16
  #error "SSD1306.h 中必须至少指定一个 SSD1306 显示屏"
#endif

#if defined SSD1306_128_64
  #define SSD1306_LCDWIDTH                  128
  #define SSD1306_LCDHEIGHT                 64
#endif
#if defined SSD1306_128_32
  #define SSD1306_LCDWIDTH                  128
  #define SSD1306_LCDHEIGHT                 32
#endif
#if defined SSD1306_96_16
  #define SSD1306_LCDWIDTH                  96
  #define SSD1306_LCDHEIGHT                 16
#endif

#define SSD1306_MEMORYMODE          0x20 ///< 显存模式 参见数据手册
#define SSD1306_COLUMNADDR          0x21 ///< 列地址 参见数据手册
#define SSD1306_PAGEADDR            0x22 ///< 页地址 参见数据手册
#define SSD1306_SETCONTRAST         0x81 ///< 设置对比度 参见数据手册
#define SSD1306_CHARGEPUMP          0x8D ///< 电荷泵 参见数据手册
#define SSD1306_SEGREMAP            0xA0 ///< 段重映射 参见数据手册
#define SSD1306_DISPLAYALLON_RESUME 0xA4 ///< 显示全部恢复 参见数据手册
#define SSD1306_DISPLAYALLON        0xA5 ///< 显示全部开启（未使用）
#define SSD1306_NORMALDISPLAY       0xA6 ///< 正常显示 参见数据手册
#define SSD1306_INVERTDISPLAY       0xA7 ///< 反转显示 参见数据手册
#define SSD1306_SETMULTIPLEX        0xA8 ///< 设置多路复用 参见数据手册
#define SSD1306_DISPLAYOFF          0xAE ///< 关闭显示 参见数据手册
#define SSD1306_DISPLAYON           0xAF ///< 开启显示 参见数据手册
#define SSD1306_COMSCANINC          0xC0 ///< COM扫描递增（未使用）
#define SSD1306_COMSCANDEC          0xC8 ///< COM扫描递减 参见数据手册
#define SSD1306_SETDISPLAYOFFSET    0xD3 ///< 设置显示偏移 参见数据手册
#define SSD1306_SETDISPLAYCLOCKDIV  0xD5 ///< 设置显示时钟分频 参见数据手册
#define SSD1306_SETPRECHARGE        0xD9 ///< 设置预充电 参见数据手册
#define SSD1306_SETCOMPINS          0xDA ///< 设置COM引脚 参见数据手册
#define SSD1306_SETVCOMDETECT       0xDB ///< 设置VCOM检测 参见数据手册

#define SSD1306_SETLOWCOLUMN        0x00 ///< 低列地址（未使用）
#define SSD1306_SETHIGHCOLUMN       0x10 ///< 高列地址（未使用）
#define SSD1306_SETSTARTLINE        0x40 ///< 设置起始行 参见数据手册

#define SSD1306_EXTERNALVCC         0x01 ///< 外部电源
#define SSD1306_SWITCHCAPVCC        0x02 ///< 内部升压电路供电

#define SSD1306_RIGHT_HORIZONTAL_SCROLL              0x26 ///< 初始化右滚动
#define SSD1306_LEFT_HORIZONTAL_SCROLL               0x27 ///< 初始化左滚动
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29 ///< 初始化对角滚动
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A ///< 初始化对角滚动
#define SSD1306_DEACTIVATE_SCROLL                    0x2E ///< 停止滚动
#define SSD1306_ACTIVATE_SCROLL                      0x2F ///< 开始滚动
#define SSD1306_SET_VERTICAL_SCROLL_AREA             0xA3 ///< 设置滚动区域

// 为与旧工程兼容而保留的已弃用尺寸定义
#if defined SSD1306_128_64
  #define SSD1306_LCDWIDTH                  128   ///< 屏幕宽度（已弃用）
  #define SSD1306_LCDHEIGHT                 64    ///< 屏幕高度（已弃用）
#endif
#if defined SSD1306_128_32
  #define SSD1306_LCDWIDTH                  128   ///< 屏幕宽度（已弃用）
  #define SSD1306_LCDHEIGHT                 32    ///< 屏幕高度（已弃用）
#endif
#if defined SSD1306_96_16
  #define SSD1306_LCDWIDTH                  96    ///< 屏幕宽度（已弃用）
  #define SSD1306_LCDHEIGHT                 16    ///< 屏幕高度（已弃用）
#endif

typedef struct _ssd1306_pins{
#if defined(OLED_4_WIRE_SPI)

#endif
#if defined(OLED_3_WIRE_SPI)

#endif
#if defined(OLED_IIC)

#endif
}SSD1306_PinTypeDef;


void ssd1306_Init(uint8_t _vccState);

void ssd1306_updateScreen(void);
void ssd1306_updataArea(uint8_t startcol, uint8_t stopcol, uint8_t startpage, uint8_t stoppage);
void ssd1306_clearScreen(void);

void ssd1306_invertDisplay(bool _invert);
void ssd1306_dimDisplay(bool _dim);

void ssd1306_drawPixel(int16_t x, int16_t y, uint8_t color);
void ssd1306_drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
void ssd1306_drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color);
void ssd1306_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);

void ssd1306_fillScreen(uint8_t color);

void ssd1306_startScrollRight(uint8_t start, uint8_t stop);
void ssd1306_startScrollLeft(uint8_t start, uint8_t stop);
void ssd1306_startScrollDiagRight(uint8_t start, uint8_t stop);
void ssd1306_startScrollDiagLeft(uint8_t start, uint8_t stop);
void ssd1306_stopScroll(void);

void ssd1306_command(uint8_t com);

bool ssd1306_getPixel(int16_t x, int16_t y);
uint8_t* ssd1306_getBuffer(void);

int16_t ssd1306_getHeight(void);
int16_t ssd1306_getWidth(void);

// 获取当前光标位置（考虑旋转安全最大值，使用：width() 获取 x，height() 获取 y）
void ssd1306_setCursor(int16_t x, int16_t y);
int16_t ssd1306_getCursorX(void);
int16_t ssd1306_getCursorY(void);

void ssd1306_setRotation(uint8_t r);
uint8_t ssd1306_getRotation(void);

void ssd1306_setTextWrap(bool w);

void ssd1306_cp437(bool x);










#endif /* NEW_OLED_SSD1306_H_ */
