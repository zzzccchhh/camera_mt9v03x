/*
 * oled_ssd1306.c
 *
 *  Created on: 2019年2月9日
 *      Author: XIAOSENLUO
 */

#include "oled_ssd1306.h"
#include "oled_spi_.h"
#include "string.h"
#include "math.h"

#define HEIGHT 									SSD1306_LCDHEIGHT
#define WIDTH										SSD1306_LCDWIDTH

#ifndef SSD1306_Delay
#define SSD1306_Delay(time)												delay_ms(time)
#endif
#define SSD1306_Swap(a,b)													(((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))
#ifndef SSD1306_Min
#define SSD1306_Min(a,b) 													(((a) < (b)) ? (a) : (b))
#endif
#define SSD1306_Write(chr)												SPIWrite((chr))
#define SSD1306_WriteBuffer(buffer, Size)					SPIWriteBuffer((buffer), (Size))


static void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint8_t color);
static void drawFastHLineInternal(int16_t x, int16_t __y, int16_t __w, uint8_t color);
static void ssd1306_reset(void);
static void ssd1306_pinInit(void);
static void ssd1306_data(uint8_t* dataBuffer, uint16_t Size);


int16_t
  _width = WIDTH,         ///< 当前旋转修改后的显示宽度
  _height = HEIGHT,        ///< 当前旋转修改后的显示高度
  cursorX = 0,       ///< 开始打印文本的 x 位置
  cursorY = 0;       ///< 开始打印文本的 y 位置
uint8_t
  rotation = 0;       ///< 显示旋转 (0~3)
bool
	_cp437 = false,
  wrap = true;           ///< 如果设置，则在显示右边缘自动换行文本

static volatile uint8_t vccState;
volatile bool isChange = false;
uint8_t ssd1306_buffer[SSD1306_LCDWIDTH * ((SSD1306_LCDHEIGHT + 7) / 8)];

/************************************** static function **************************************************************/

static void drawFastVLineInternal(int16_t x, int16_t __y, int16_t __h, uint8_t color){
  // 如果在屏幕左侧或右侧之外，什么都不做
  if(x < 0 || x >= WIDTH) { return; }

  // 确保不尝试绘制到0以下
  if(__y < 0) {
    // __y为负数，这将从__h中减去足够的量以弥补__y为0的情况
    __h += __y;
    __y = 0;

  }

  // 确保不超过显示高度
  if( (__y + __h) > HEIGHT) {
    __h = (HEIGHT - __y);
  }

  // 如果高度现在为负，退出
  if(__h <= 0) {
    return;
  }

  // 此显示器坐标不需要int，使用本地字节寄存器可以更快地处理
  register uint8_t y = __y;
  register uint8_t h = __h;


  // 设置指针以便快速遍历缓冲区
  register uint8_t *pBuf = ssd1306_buffer;
  // 调整当前行的缓冲区指针
  pBuf += ((y/8) * SSD1306_LCDWIDTH);
  // 并偏移x列
  pBuf += x;

  // 如有必要，进行第一次部分字节处理——这需要一些掩码操作
  register uint8_t mod = (y&7);
  if(mod) {
    // 屏蔽我们想要设置的高n位
    mod = 8-mod;

    // 注意——查找表使fill*函数的性能提高了近10%
    // register uint8_t mask = ~(0xFF >> (mod));
    static uint8_t premask[8] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };
    register uint8_t mask = premask[mod];

    // 如果我们不会到达字节末尾，调整掩码
    if( h < mod) {
      mask &= (0XFF >> (mod-h));
    }

  switch (color)
    {
    case WHITE:   *pBuf |=  mask;  break;
    case BLACK:   *pBuf &= ~mask;  break;
    case INVERSE: *pBuf ^=  mask;  break;
    }

    // 如果完成了，快速退出！
    if(h<mod) { return; }

    h -= mod;

    pBuf += SSD1306_LCDWIDTH;
  }


  // 当我们可以时写入完整的字节——实际上是同时处理8行
  if(h >= 8) {
    if (color == INVERSE)  {          // 代码的单独副本，这样我们不会在黑/白写入版本中每次循环增加额外的比较而影响性能
      do  {
      *pBuf=~(*pBuf);

        // 将缓冲区向前调整8行数据
        pBuf += SSD1306_LCDWIDTH;

        // 调整h和y（肯定有更快的方法，但我现在这样做仍然会有很大帮助）
        h -= 8;
      } while(h >= 8);
      }
    else {
      // 存储一个本地值来使用
      register uint8_t val = (color == WHITE) ? 255 : 0;

      do  {
        // 写入我们的值
      *pBuf = val;

        // 将缓冲区向前调整8行数据
        pBuf += SSD1306_LCDWIDTH;

        // 调整h和y（肯定有更快的方法，但我现在这样做仍然会有很大帮助）
        h -= 8;
      } while(h >= 8);
      }
    }

  // 现在进行最后的部分字节处理（如有必要）
  if(h) {
    mod = h & 7;
    // 这次我们要屏蔽字节的低位，而我们之前处理的是高位
    // register uint8_t mask = (1 << mod) - 1;
    // 注意——查找表使fill*函数的性能提高了近10%
    static uint8_t postmask[8] = {0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };
    register uint8_t mask = postmask[mod];
    switch (color)
    {
      case WHITE:   *pBuf |=  mask;  break;
      case BLACK:   *pBuf &= ~mask;  break;
      case INVERSE: *pBuf ^=  mask;  break;
    }
  }

}
static void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint8_t color){
  if((y >= 0) && (y < HEIGHT)){ // Y坐标在范围内？
    if(x < 0){ // 裁剪左侧
      w += x;
      x  = 0;
    }
    if((x + w) > WIDTH){ // 裁剪右侧
      w = (WIDTH - x);
    }

    if(w > 0){ // 仅在宽度为正时继续
    	register uint8_t *pBuf = ssd1306_buffer;
      // 调整当前行的缓冲区指针
      pBuf += ((y/8) * SSD1306_LCDWIDTH);
      // 并偏移x列
      pBuf += x;
      register  uint8_t mask = 1 << (y&7);
      switch(color){
       case WHITE:               while(w--) { *pBuf++ |= mask; }; break;
       case BLACK: mask = ~mask; while(w--) { *pBuf++ &= mask; }; break;
       case INVERSE:             while(w--) { *pBuf++ ^= mask; }; break;
      }
    }
  }
}
static void ssd1306_reset(void){
	SSD1306_RST_SET();
	SSD1306_Delay(1);
	SSD1306_RST_RESET();
	SSD1306_Delay(10);
	SSD1306_RST_SET();
}
static void ssd1306_pinInit(void){
	GPIO_InitTypeDef GPIO_InitStruct;

	/* 使能GPIO时钟 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE | RCC_APB2Periph_GPIOD, ENABLE);

	/* RST - PE15 */
	GPIO_InitStruct.GPIO_Pin = SSD1306_RST_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SSD1306_RST_PORT, &GPIO_InitStruct);

	/* DC - PD9 */
	GPIO_InitStruct.GPIO_Pin = SSD1306_DC_PIN;
	GPIO_Init(SSD1306_DC_PORT, &GPIO_InitStruct);

	/* CS - PD11 */
	GPIO_InitStruct.GPIO_Pin = SSD1306_CS_PIN;
	GPIO_Init(SSD1306_CS_PORT, &GPIO_InitStruct);
}

static void ssd1306_data(uint8_t* dataBuffer, uint16_t Size){
	SSD1306_MODE_DATA();
	SSD1306_SELETE();
	SSD1306_WriteBuffer(dataBuffer, Size);
	SSD1306_NOSELETE();
}

/************************************** static function **************************************************************/

/************************************** public function **************************************************************/
void ssd1306_Init(uint8_t _vccState){
	ssd1306_pinInit();
	SPIInit();
	ssd1306_clearScreen();

	vccState = _vccState;
	vccState = SSD1306_SWITCHCAPVCC;
	/*	RESET SSD1306 MCU	*/
	ssd1306_reset();
	/*	Init sequence	*/
#if defined SSD1306_128_32
 // 128x32 OLED模块初始化序列
 ssd1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
 ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
 ssd1306_command(0x80);                                  // 建议的比率 0x80
 ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
 ssd1306_command(0x1F);
 ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
 ssd1306_command(0x0);                                   // 无偏移
 ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // 行 #0
 ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
 if (_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x10); }
 else
   { ssd1306_command(0x14); }
 ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
 ssd1306_command(0x00);                                  // 0x0 像 ks0108 一样工作
 ssd1306_command(SSD1306_SEGREMAP | 0x1);
 ssd1306_command(SSD1306_COMSCANDEC);
 ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
 ssd1306_command(0x02);
 ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
 ssd1306_command(0x8F);
 ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
 if (_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x22); }
 else
   { ssd1306_command(0xF1); }
 ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
 ssd1306_command(0x40);
 ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
 ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6
#endif

#if defined SSD1306_128_64
 // 128x64 OLED模块初始化序列
 ssd1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
 ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
 ssd1306_command(0x80);                                  // 建议的比率 0x80
 ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
 ssd1306_command(0x3F);
 ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
 ssd1306_command(0x0);                                   // 无偏移
 ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // 行 #0
 ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
 if(_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x10); }
 else
   { ssd1306_command(0x14); }
 ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
 ssd1306_command(0x00);                                  // 0x0 像 ks0108 一样工作
 ssd1306_command(SSD1306_SEGREMAP | 0x1);
 ssd1306_command(SSD1306_COMSCANDEC);
 ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
 ssd1306_command(0x12);
 ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
 if (_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x9F); }
 else
   { ssd1306_command(0xCF); }
 ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
 if (_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x22); }
 else
   { ssd1306_command(0xF1); }
 ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
 ssd1306_command(0x40);
 ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
 ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6
#endif

#if defined SSD1306_96_16
 // 96x16 OLED模块初始化序列
 ssd1306_command(SSD1306_DISPLAYOFF);                    // 0xAE
 ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);            // 0xD5
 ssd1306_command(0x80);                                  // 建议的比率 0x80
 ssd1306_command(SSD1306_SETMULTIPLEX);                  // 0xA8
 ssd1306_command(0x0F);
 ssd1306_command(SSD1306_SETDISPLAYOFFSET);              // 0xD3
 ssd1306_command(0x00);                                   // 无偏移
 ssd1306_command(SSD1306_SETSTARTLINE | 0x0);            // 行 #0
 ssd1306_command(SSD1306_CHARGEPUMP);                    // 0x8D
 if(_vccState == SSD1306_EXTERNALVCC){
	 ssd1306_command(0x10);
 }
 else{
	 ssd1306_command(0x14);
 }
 ssd1306_command(SSD1306_MEMORYMODE);                    // 0x20
 ssd1306_command(0x00);                                  // 0x0 像 ks0108 一样工作
 ssd1306_command(SSD1306_SEGREMAP | 0x1);
 ssd1306_command(SSD1306_COMSCANDEC);
 ssd1306_command(SSD1306_SETCOMPINS);                    // 0xDA
 ssd1306_command(0x2);	//ada x12
 ssd1306_command(SSD1306_SETCONTRAST);                   // 0x81
 if(_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x10); }
 else
   { ssd1306_command(0xAF); }
 ssd1306_command(SSD1306_SETPRECHARGE);                  // 0xd9
 if (_vccState == SSD1306_EXTERNALVCC)
   { ssd1306_command(0x22); }
 else
   { ssd1306_command(0xF1); }
 ssd1306_command(SSD1306_SETVCOMDETECT);                 // 0xDB
 ssd1306_command(0x40);
 ssd1306_command(SSD1306_DISPLAYALLON_RESUME);           // 0xA4
 ssd1306_command(SSD1306_NORMALDISPLAY);                 // 0xA6
#endif

  ssd1306_command(SSD1306_DISPLAYON);//--turn on oled panel

  ssd1306_clearScreen();
}
void ssd1306_updateScreen(void){
  ssd1306_command(SSD1306_COLUMNADDR);
  ssd1306_command(0);   // 列起始地址 (0 = 复位)
  ssd1306_command(SSD1306_LCDWIDTH-1); // 列结束地址 (127 = 复位)

  ssd1306_command(SSD1306_PAGEADDR);
  ssd1306_command(0); // 页起始地址 (0 = 复位)
  #if SSD1306_LCDHEIGHT == 64
    ssd1306_command(7); // 页结束地址
  #endif
  #if SSD1306_LCDHEIGHT == 32
    ssd1306_command(3); // 页结束地址
  #endif
  #if SSD1306_LCDHEIGHT == 16
    ssd1306_command(1); // 页结束地址
  #endif
  SSD1306_MODE_DATA();
  SSD1306_SELETE();
  SSD1306_WriteBuffer(ssd1306_buffer, (WIDTH * ((HEIGHT + 7) / 8)));
  SSD1306_NOSELETE();

}
/* @param:startcol:range:0~127, and startcol <= stopcol;
 *				stopcol:range:0~127
 * 				startpage:range:0~7, and startpage <= stoppage;
 * 				stoppage:range:0~7;
 * */
void ssd1306_updataArea(uint8_t startcol, uint8_t stopcol, uint8_t startpage, uint8_t stoppage){
	ssd1306_command(SSD1306_COLUMNADDR);
	ssd1306_command(startcol);
	ssd1306_command(stopcol);

	ssd1306_command(SSD1306_PAGEADDR);
	ssd1306_command(startpage);
	ssd1306_command(stoppage);

  SSD1306_MODE_DATA();
  SSD1306_SELETE();
  for(uint8_t j = startpage; j <= stoppage; j++){
  	for(uint8_t i = startcol; i <= stopcol; i++){
  		SSD1306_Write(ssd1306_buffer[i+j*128]);
  	}
  }
  SSD1306_NOSELETE();
}
void ssd1306_clearScreen(void){
	memset(ssd1306_buffer, 0, WIDTH * ((HEIGHT + 7) / 8));
}
void ssd1306_invertDisplay(bool _invert){
	ssd1306_command((_invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY));
}
void ssd1306_dimDisplay(bool _dim){
  uint8_t contrast;
  if(_dim) {
    contrast = 0; // 调暗显示
  } else {
    contrast = ((vccState == SSD1306_EXTERNALVCC) ? 0x9F : 0xCF);
  }
  // 对比度范围太小，没有太大用处
  // 但用于调暗显示是有用的
  ssd1306_command(SSD1306_SETCONTRAST);
  ssd1306_command(contrast);
}
void ssd1306_drawPixel(int16_t x, int16_t y, uint8_t color){
  if((x >= 0) && (x < SSD1306_LCDWIDTH) && (y >= 0) && (y < SSD1306_LCDHEIGHT)){
    // 像素在范围内。如有需要，旋转坐标。
    switch(rotation) {
     case 1:
      SSD1306_Swap(x, y);
      x = SSD1306_LCDWIDTH - x - 1;
      break;
     case 2:
      x = SSD1306_LCDWIDTH  - x - 1;
      y = SSD1306_LCDHEIGHT - y - 1;
      break;
     case 3:
      SSD1306_Swap(x, y);
      y = SSD1306_LCDHEIGHT - y - 1;
      break;
    }
    switch(color) {
     case WHITE:   ssd1306_buffer[x + (y/8)*SSD1306_LCDWIDTH] |=  (1 << (y&7)); break;
     case BLACK:   ssd1306_buffer[x + (y/8)*SSD1306_LCDWIDTH] &= ~(1 << (y&7)); break;
     case INVERSE: ssd1306_buffer[x + (y/8)*SSD1306_LCDWIDTH] ^=  (1 << (y&7)); break;
    }
  }
}
void ssd1306_drawFastHLine(int16_t x, int16_t y, int16_t w, uint8_t color){
  bool bSwap = false;
  switch(rotation){
  	case 0: break;
   case 1:
    // 90度旋转，交换x和y进行旋转，然后反转x
    bSwap = true;
    SSD1306_Swap(x, y);
    x = WIDTH - x - 1;
    break;
   case 2:
    // 180度旋转，反转x和y，然后移动y以适应高度
    x  = WIDTH  - x - 1;
    y  = HEIGHT - y - 1;
    x -= (w-1);
    break;
   case 3:
    // 270度旋转，交换x和y进行旋转，
    // 然后反转y并调整y以适应w（不会变成h）
    bSwap = true;
    SSD1306_Swap(x, y);
    y  = HEIGHT - y - 1;
    y -= (w-1);
    break;
  }
  if(bSwap) drawFastVLineInternal(x, y, w, color);
  else      drawFastHLineInternal(x, y, w, color);
}
void ssd1306_drawFastVLine(int16_t x, int16_t y, int16_t h, uint8_t color){
  bool bSwap = false;
  switch(rotation){
  	case 0: break;
   case 1:
    // 90度旋转，交换x和y进行旋转，
    // 然后反转x并调整x以适应h（现在变成w）
    bSwap = true;
    SSD1306_Swap(x, y);
    x  = WIDTH - x - 1;
    x -= (h-1);
    break;
   case 2:
    // 180度旋转，反转x和y，然后移动y以适应高度
    x = WIDTH  - x - 1;
    y = HEIGHT - y - 1;
    y -= (h-1);
    break;
   case 3:
    // 270度旋转，交换x和y进行旋转，然后反转y
    bSwap = true;
    SSD1306_Swap(x, y);
    y = HEIGHT - y - 1;
    break;
  }

  if(bSwap) drawFastHLineInternal(x, y, h, color);
  else      drawFastVLineInternal(x, y, h, color);
}

void ssd1306_drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color){
  if(x0 == x1){
      if(y0 > y1) SSD1306_Swap(y0, y1);
      ssd1306_drawFastVLine(x0, y0, y1 - y0 + 1, color);
  } else if(y0 == y1){
      if(x0 > x1) SSD1306_Swap(x0, x1);
      ssd1306_drawFastHLine(x0, y0, x1 - x0 + 1, color);
  } else {
    bool steep = abs((int)y1 - (int)y0) > abs((int)x1 - (int)x0) ? true : false;
    if (steep) {
    	SSD1306_Swap(x0, y0);
    	SSD1306_Swap(x1, y1);
    }

    if (x0 > x1) {
    	SSD1306_Swap(x0, x1);
    	SSD1306_Swap(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for (; x0<=x1; x0++) {
        if (steep) {
            ssd1306_drawPixel(y0, x0, color);
        } else {
            ssd1306_drawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
  }
}

void ssd1306_fillScreen(uint8_t color){
  for(int16_t i = 0; i < SSD1306_LCDWIDTH; i++){
      ssd1306_drawFastVLine(i, 0, SSD1306_LCDHEIGHT, color);
  }
}
/*!
    @brief  激活显示的右滚动，所有或部分区域。
    @param  start
            起始行。
    @param  stop
            结束行。
    @return None (void).
*/
/*	To scroll the whole display, run: ssd1306_startScrollRight(0x00, 0x0F)	*/
void ssd1306_startScrollRight(uint8_t start, uint8_t stop){
  ssd1306_command(SSD1306_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X0F);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}
/*!
    @brief  激活显示的左滚动，所有或部分区域。
    @param  start
            起始行。
    @param  stop
            结束行。
    @return None (void).
*/
/*	To scroll the whole display, run: ssd1306_startScrollLeft(0x00, 0x0F)	*/
void ssd1306_startScrollLeft(uint8_t start, uint8_t stop){
  ssd1306_command(SSD1306_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(7);
  ssd1306_command(stop);
  ssd1306_command(0X00);
  ssd1306_command(0XFF);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}
/*!
    @brief  激活显示的对角滚动，所有或部分区域。
    @param  start
            起始行。
    @param  stop
            结束行。
    @return None (void).
*/
void ssd1306_startScrollDiagRight(uint8_t start, uint8_t stop){
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}
/*!
    @brief  激活交替对角滚动，所有或部分区域。
    @param  start
            起始行。
    @param  stop
            结束行。
    @return None (void).
*/
void ssd1306_startScrollDiagLeft(uint8_t start, uint8_t stop){
  ssd1306_command(SSD1306_SET_VERTICAL_SCROLL_AREA);
  ssd1306_command(0X00);
  ssd1306_command(SSD1306_LCDHEIGHT);
  ssd1306_command(SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL);
  ssd1306_command(0X00);
  ssd1306_command(start);
  ssd1306_command(0X00);
  ssd1306_command(stop);
  ssd1306_command(0X01);
  ssd1306_command(SSD1306_ACTIVATE_SCROLL);
}
void ssd1306_stopScroll(void){
	ssd1306_command(SSD1306_DEACTIVATE_SCROLL);
}
void ssd1306_command(uint8_t com){
	SSD1306_SELETE();
	SSD1306_MODE_COMMAND();
	SSD1306_Write(com);
	SSD1306_NOSELETE();
}
bool ssd1306_getPixel(int16_t x, int16_t y){
  if((x >= 0) && (x < WIDTH) && (y >= 0) && (y < HEIGHT)) {
    // 像素在范围内。如有需要，旋转坐标。
    switch(rotation) {
     case 1:
      SSD1306_Swap(x, y);
      x = WIDTH - x - 1;
      break;
     case 2:
      x = WIDTH  - x - 1;
      y = HEIGHT - y - 1;
      break;
     case 3:
      SSD1306_Swap(x, y);
      y = HEIGHT - y - 1;
      break;
    }
    cursorX = x;
    cursorY = y;
    return (ssd1306_buffer[x + (y / 8) * WIDTH] & (1 << (y & 7)));
  }
  return false; // 像素超出范围
}
uint8_t* ssd1306_getBuffer(void){
	return ssd1306_buffer;
}

int16_t ssd1306_getHeight(void){
	return _height;
}

int16_t ssd1306_getWidth(void){
	return _width;
}

void ssd1306_setRotation(uint8_t r){
  rotation = (r & 3);
  switch(rotation){
    case 0:
    case 2:
      _width  = WIDTH;
      _height = HEIGHT;
      break;
    case 1:
    case 3:
      _width  = HEIGHT;
       _height = WIDTH;
      break;
  }
}

uint8_t ssd1306_getRotation(void){
	return rotation;
}

void ssd1306_setCursor(int16_t x, int16_t y){
	cursorX = x;
	cursorY = y;
}
// 获取当前光标位置（考虑旋转安全最大值，使用：width() 获取 x，height() 获取 y）
int16_t ssd1306_getCursorX(void){
	return cursorX;
}
int16_t ssd1306_getCursorY(void){
	return cursorY;
}

void ssd1306_setTextWrap(bool w){
	wrap = w;
}

void ssd1306_cp437(bool x){
	_cp437 = x;
}


/************************************** public function **************************************************************/















