/*
 * oled_hal.c
 *
 *  Created on: 2019年2月9日
 *      Author: XIAOSENLUO
 */

#include "oled_hal.h"
#include "stddef.h"
#include "stdio.h"

#ifndef pgm_read_byte
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
 #define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
 #define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
 #define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
 #define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))
#endif

#define OLED_HEX_CHARS	 "0123456789ABCDEF"

//uint8_t* oled_buffer = ssd1306_getBuffer();

extern int16_t
  _width,         ///< 当前旋转修改后的显示宽度
  _height,        ///< 当前旋转修改后的显示高度
  cursorX,       ///< 开始打印文本的 x 位置
  cursorY;       ///< 开始打印文本的 y 位置
__IO uint8_t
  textsize = 1;       ///< 文本期望放大倍数
__IO uint8_t
  textcolor = WHITE,
  textbgcolor = WHITE;
extern uint8_t
	rotation;       ///< 显示旋转 (0~3)
extern bool
	_cp437,
  wrap;           ///< 如果设置，则在显示右边缘" wrap"文本

//GFXfont *gfxFont = NULL;

__IO oledFont* oledfont = &SongFont_ASCII_5x8;

int16_t oled_getHeight(void){
	return _height;
}

int16_t oled_getWidth(void){
	return _width;
}

void oled_setRotation(uint8_t r){
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

uint8_t oled_getRotation(void){
	return rotation;
}

void oled_setCursor(int16_t x, int16_t y){
	cursorX = x;
	cursorY = y;
}
// 获取当前光标位置（考虑旋转安全最大值，使用：width() 获取 x，height() 获取 y）
int16_t oled_getCursorX(void){
	return cursorX;
}
int16_t oled_getCursorY(void){
	return cursorY;
}



void oled_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color){
	ssd1306_drawFastHLine(x, y, w, color);
	ssd1306_drawFastHLine(x, y+h-1, w, color);
	ssd1306_drawFastVLine(x, y, h, color);
	ssd1306_drawFastVLine(x+w-1, y, h, color);
}

void oled_drawCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color){
  int16_t f = 1 - r;
   int16_t ddF_x = 1;
   int16_t ddF_y = -2 * r;
   int16_t x = 0;
   int16_t y = r;


   ssd1306_drawPixel(x0  , y0+r, color);
   ssd1306_drawPixel(x0  , y0-r, color);
   ssd1306_drawPixel(x0+r, y0  , color);
   ssd1306_drawPixel(x0-r, y0  , color);

   while (x<y) {
       if (f >= 0) {
           y--;
           ddF_y += 2;
           f += ddF_y;
       }
       x++;
       ddF_x += 2;
       f += ddF_x;

       ssd1306_drawPixel(x0 + x, y0 + y, color);
       ssd1306_drawPixel(x0 - x, y0 + y, color);
       ssd1306_drawPixel(x0 + x, y0 - y, color);
       ssd1306_drawPixel(x0 - x, y0 - y, color);
       ssd1306_drawPixel(x0 + y, y0 + x, color);
       ssd1306_drawPixel(x0 - y, y0 + x, color);
       ssd1306_drawPixel(x0 + y, y0 - x, color);
       ssd1306_drawPixel(x0 - y, y0 - x, color);
   }
}
void oled_drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint8_t color){
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
      if (f >= 0) {
          y--;
          ddF_y += 2;
          f     += ddF_y;
      }
      x++;
      ddF_x += 2;
      f     += ddF_x;
      if (cornername & 0x4) {
      	ssd1306_drawPixel(x0 + x, y0 + y, color);
      	ssd1306_drawPixel(x0 + y, y0 + x, color);
      }
      if (cornername & 0x2) {
      	ssd1306_drawPixel(x0 + x, y0 - y, color);
      	ssd1306_drawPixel(x0 + y, y0 - x, color);
      }
      if (cornername & 0x8) {
      	ssd1306_drawPixel(x0 - y, y0 + x, color);
      	ssd1306_drawPixel(x0 - x, y0 + y, color);
      }
      if (cornername & 0x1) {
      	ssd1306_drawPixel(x0 - y, y0 - x, color);
      	ssd1306_drawPixel(x0 - x, y0 - y, color);
      }
  }
}
void oled_fillCircle(int16_t x0, int16_t y0, int16_t r, uint8_t color){
	ssd1306_drawFastVLine(x0, y0-r, 2*r+1, color);
  oled_fillCircleHelper(x0, y0, r, 3, 0, color);
}
void oled_fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint8_t color){
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
      if (f >= 0) {
          y--;
          ddF_y += 2;
          f     += ddF_y;
      }
      x++;
      ddF_x += 2;
      f     += ddF_x;

      if (cornername & 0x1) {
      	ssd1306_drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      	ssd1306_drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
      }
      if (cornername & 0x2) {
      	ssd1306_drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      	ssd1306_drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
      }
  }
}
void oled_drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color){
	ssd1306_drawLine(x0, y0, x1, y1, color);
	ssd1306_drawLine(x1, y1, x2, y2, color);
	ssd1306_drawLine(x2, y2, x0, y0, color);
}
void oled_fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color){
  int16_t a, b, y, last;

  // 按Y坐标排序（y2 >= y1 >= y0）
  if (y0 > y1) {
      _swap_int16_t(y0, y1); _swap_int16_t(x0, x1);
  }
  if (y1 > y2) {
      _swap_int16_t(y2, y1); _swap_int16_t(x2, x1);
  }
  if (y0 > y1) {
      _swap_int16_t(y0, y1); _swap_int16_t(x0, x1);
  }

  if(y0 == y2) { // 处理所有点共线的特殊情况
      a = b = x0;
      if(x1 < a)      a = x1;
      else if(x1 > b) b = x1;
      if(x2 < a)      a = x2;
      else if(x2 > b) b = x2;
      ssd1306_drawFastHLine(a, y0, b-a+1, color);
      return;
  }

  int16_t
  dx01 = x1 - x0,
  dy01 = y1 - y0,
  dx02 = x2 - x0,
  dy02 = y2 - y0,
  dx12 = x2 - x1,
  dy12 = y2 - y1;
  int32_t
  sa   = 0,
  sb   = 0;

  // 对于三角形的上半部分，找到线段0-1和0-2的扫描线交叉点
  // 如果y1=y2（平底三角形），扫描线y1在这里包含（第二个循环将跳过，避免/0错误），
  // 否则扫描线y1在这里跳过，在第二个循环中处理...如果y0=y1（平顶三角形）也能避免/0错误
  if(y1 == y2) last = y1;   // 包含y1扫描线
  else         last = y1-1; // 跳过它

  for(y=y0; y<=last; y++) {
      a   = x0 + sa / dy01;
      b   = x0 + sb / dy02;
      sa += dx01;
      sb += dx02;
      /* longhand:
      a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
      b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
      */
      if(a > b) _swap_int16_t(a,b);
      ssd1306_drawFastHLine(a, y, b-a+1, color);
  }

  // 对于三角形的下半部分，找到线段0-2和1-2的扫描线交叉点。如果y1=y2，此循环跳过。
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
      a   = x1 + sa / dy12;
      b   = x0 + sb / dy02;
      sa += dx12;
      sb += dx02;
      /* longhand:
      a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
      b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
      */
      if(a > b) _swap_int16_t(a,b);
      ssd1306_drawFastHLine(a, y, b-a+1, color);
  }
}
void oled_drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color){
  ssd1306_drawFastHLine(x+radius  , y    , w-2*radius, color); // 顶部
  ssd1306_drawFastHLine(x+radius  , y+h-1, w-2*radius, color); // 底部
  ssd1306_drawFastVLine(x    , y+radius  , h-2*radius, color); // 左
  ssd1306_drawFastVLine(x+w-1, y+radius  , h-2*radius, color); // 右
  // 绘制四个角
  oled_drawCircleHelper(x+radius    , y+radius    , radius, 1, color);
  oled_drawCircleHelper(x+w-radius-1, y+radius    , radius, 2, color);
  oled_drawCircleHelper(x+w-radius-1, y+h-radius-1, radius, 4, color);
  oled_drawCircleHelper(x+radius    , y+h-radius-1, radius, 8, color);
}
void oled_fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint8_t color){
  oled_fillRect(x+radius, y, w-2*radius, h, color);
  // 绘制四个角
  oled_fillCircleHelper(x+w-radius-1, y+radius, radius, 1, h-2*radius-1, color);
  oled_fillCircleHelper(x+radius    , y+radius, radius, 2, h-2*radius-1, color);
}

void oled_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color){
  for(int16_t i = x; i < x + w; i++){
      ssd1306_drawFastVLine(i, y, h, color);
  }
}
// 从Flash绘制PROGMEM驻留的1位图像到指定(x,y)位置，
// 使用指定的前景色（未设置的位是透明的）。
/*水平扫描，从上往下， 左为高位*/
void oled_drawBitmapFromFlash(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint8_t color){

	int16_t byteWidth = (w + 7) / 8; // 位图扫描线填充=整字节
  uint8_t byte = 0;
  for(int16_t j=0; j<h; j++, y++) {
      for(int16_t i=0; i<w; i++){
          if(i & 7) byte <<= 1;
          else      byte   = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
          if(byte & 0x80) ssd1306_drawPixel(x+i, y, color);
      }
  }
}
// 从RAM绘制RAM驻留的1位图像到指定(x,y)位置，
// 使用指定的前景色（未设置的位是透明的）。
void oled_drawBitmapFromRAM(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint8_t color){
  int16_t byteWidth = (w + 7) / 8; // 位图扫描线填充=整字节
  uint8_t byte = 0;
  for(int16_t j=0; j<h; j++, y++) {
      for(int16_t i=0; i<w; i++ ) {
          if(i & 7) byte <<= 1;
          else      byte   = bitmap[j * byteWidth + i / 8];
          if(byte & 0x80) ssd1306_drawPixel(x+i, y, color);
      }
  }
}
// 从GIMP导出的PROGMEM驻留XBitMap文件(*.xbm)绘制，
// 使用方法：从GIMP导出到*.xbm，重命名为*.c并在编辑器中打开。
// C数组可直接用于此函数。
// 此函数没有RAM驻留版本；如果要在RAM中生成位图，
// 请使用drawBitmap()定义的格式并调用该函数。
/*水平扫描，从上往下， 左为高位*/
void oled_drawXBitmapFromFlash(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint8_t color){
  int16_t byteWidth = (w + 7) / 8; // 位图扫描线填充=整字节
  uint8_t byte = 0;

  for(int16_t j=0; j<h; j++, y++) {
      for(int16_t i=0; i<w; i++ ) {
          if(i & 7) byte >>= 1;
          else      byte   = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
          // 与drawBitmap()几乎相同，只是这里的位顺序反转了
          // （从左到右= LSB到MSB）：
          if(byte & 0x01) ssd1306_drawPixel(x+i, y, color);
      }
  }
}
//void oled_drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h){
//
//}
//void oled_drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap,int16_t w, int16_t h){
//
//}
//void oled_drawGrayscaleBitmap(int16_t x, int16_t y,const uint8_t bitmap[], const uint8_t mask[],int16_t w, int16_t h){
//
//}
//void oled_drawGrayscaleBitmap(int16_t x, int16_t y,uint8_t *bitmap, uint8_t *mask, int16_t w, int16_t h){
//
//}

/*纵向取点， 上边为高位，下边为低位*/
/* 按列从左往右排列， 列内排序：上在下前 */
void oled_drawChar(int16_t x, int16_t y, char c, uint8_t color, uint16_t bg, uint8_t size){
	uint8_t _fontW = (*(const uint8_t*)&(oledfont->width));
	uint8_t _fontH = (*(const uint8_t*)&(oledfont->height));
  if(oledfont) { // 内置经典字体

      if((x >= _width)            || // 裁剪右侧
         (y >= _height)           || // 裁剪底部
         ((x + oledfont->width * size - 1) < 0) || // 裁剪左侧
         ((y + oledfont->height * size - 1) < 0))   // 裁剪顶部
          return;

      if(!_cp437 && (c >= 176)) c++; // 处理经典字符集行为
      register uint8_t line;
      for(int8_t i=0; i<_fontW; i++){
      	for(int8_t k = 0; k < ((_fontH + 7) / 8); k++){
          line = pgm_read_byte(&(oledfont->data[(c - 32)*(_fontW*((_fontH + 7)/8))+i*((_fontH + 7)/8) + k]));
          for(int8_t j=0; j<8; j++){
              if((line<<j) & 0x80){
                  if(size == 1){
                    if(j+k*8 < _fontH) ssd1306_drawPixel(x+i, y+j+k*8, color);
                  }else{
                  	if(j+k*8 < _fontH) oled_fillRect(x+i*size, y+(j+k*8)*size, size, size, color);
                  }
              }else if(bg != color){
                  if(size == 1){
                  	if(j+k*8 < _fontH) ssd1306_drawPixel(x+i, y+j+k*8, bg);
                  }else{
                  	if(j+k*8 < _fontH) oled_fillRect(x+i*size, y+(j+k*8)*size, size, size, bg);
                  }
              }
          }
      	}
      }
      if(bg != color) { // 如果不透明，绘制最后一列的垂直线
          if(size == 1) ssd1306_drawFastVLine(x+oledfont->width, y, oledfont->height, bg);
          else          oled_fillRect(x+oledfont->width*size, y, size, oledfont->height*size, bg);
      }
  }
//  else { // Custom font
//
//      // Character is assumed previously filtered by write() to eliminate
//      // newlines, returns, non-printable characters, etc.  Calling
//      // drawChar() directly with 'bad' characters of font may cause mayhem!
//
//      c -= (uint8_t)pgm_read_byte(&gfxFont->first);
//      GFXglyph *glyph  = &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
//      uint8_t  *bitmap = (uint8_t *)pgm_read_pointer(&gfxFont->bitmap);
//
//      uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
//      uint8_t  w  = pgm_read_byte(&glyph->width),
//               h  = pgm_read_byte(&glyph->height);
//      int8_t   xo = pgm_read_byte(&glyph->xOffset),
//               yo = pgm_read_byte(&glyph->yOffset);
//      uint8_t  xx, yy, bits = 0, bit = 0;
//      int16_t  xo16 = 0, yo16 = 0;
//
//      if(size > 1) {
//          xo16 = xo;
//          yo16 = yo;
//      }
//
//      // Todo: Add character clipping here
//
//      // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
//      // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
//      // has typically been used with the 'classic' font to overwrite old
//      // screen contents with new data.  This ONLY works because the
//      // characters are a uniform size; it's not a sensible thing to do with
//      // proportionally-spaced fonts with glyphs of varying sizes (and that
//      // may overlap).  To replace previously-drawn text when using a custom
//      // font, use the getTextBounds() function to determine the smallest
//      // rectangle encompassing a string, erase the area with fillRect(),
//      // then draw new text.  This WILL infortunately 'blink' the text, but
//      // is unavoidable.  Drawing 'background' pixels will NOT fix this,
//      // only creates a new set of problems.  Have an idea to work around
//      // this (a canvas object type for MCUs that can afford the RAM and
//      // displays supporting setAddrWindow() and pushColors()), but haven't
//      // implemented this yet.
//      for(yy=0; yy<h; yy++) {
//          for(xx=0; xx<w; xx++) {
//              if(!(bit++ & 7)) {
//                  bits = pgm_read_byte(&bitmap[bo++]);
//              }
//              if(bits & 0x80) {
//                  if(size == 1) {
//                  	ssd1306_drawPixel(x+xo+xx, y+yo+yy, color);
//                  } else {
//                      oled_fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size,
//                        size, size, color);
//                  }
//              }
//              bits <<= 1;
//          }
//      }
//  } // End classic vs custom font
}

void oled_drawText(int16_t x, int16_t y, char* text){

	ssd1306_setCursor(x, y);
	uint8_t c = text[0];
	uint8_t _w = 7, _h = 8;
  if(oledfont) { // 内置经典字体
  	while(c != '\0'){
      if(c == '\n') {                        // 换行？
          cursorX  = 0;                     // x重置为零，
          cursorY += textsize * _h;          // y前进一行
      } else if(c != '\r') {                 // 忽略回车
          if(wrap && ((cursorX + textsize * _w) > _width)) { // 超出右侧？
              cursorX  = 0;                 // x重置为零，
              cursorY += textsize * _h;      // y前进一行
          }
          oled_drawChar(cursorX, cursorY, c, textcolor, textbgcolor, textsize);
          cursorX += textsize * _w;          // x前进一个字符
      }
      c = *(++text);
  	}
  }
//  else { // Custom font
//  	while(c != '\0'){
//      if(c == '\n'){
//          cursorX  = 0;
//          cursorY += (int16_t)textsize *
//                      (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
//      } else if(c != '\r') {
//          uint8_t first = pgm_read_byte(&gfxFont->first);
//          if((c >= first) && (c <= (uint8_t)pgm_read_byte(&gfxFont->last))) {
//              GFXglyph *glyph = &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c - first]);
//              uint8_t   w     = pgm_read_byte(&glyph->width),
//                        h     = pgm_read_byte(&glyph->height);
//              if((w > 0) && (h > 0)) { // Is there an associated bitmap?
//                  int16_t xo = (int8_t)pgm_read_byte(&glyph->xOffset); // sic
//                  if(wrap && ((cursorX + textsize * (xo + w)) > _width)){
//                      cursorX  = 0;
//                      cursorY += (int16_t)textsize *
//                        (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
//                  }
//                  oled_drawChar(cursorX, cursorY, c, textcolor, textbgcolor, textsize);
//              }
//              cursorX += (uint8_t)pgm_read_byte(&glyph->xAdvance) * (int16_t)textsize;
//          }
//      }
//      c = *(++text);
//  	}
//  }
}

void oled_displayHex8(int16_t x, int16_t y, uint8_t hex8){
	char _hex8[4];
	_hex8[0] = OLED_HEX_CHARS[(hex8>>4)];
	_hex8[1] = OLED_HEX_CHARS[(hex8&0x0f)];
	_hex8[2] = 'H';
	_hex8[3] = '\0';
	oled_drawText(x, y, _hex8);
}
void oled_displayHex16(int16_t x, int16_t y, uint8_t hex16){
	char  _hex16[6];
	_hex16[0] = OLED_HEX_CHARS[(hex16 >> 12) % 0x10];
	_hex16[1] = OLED_HEX_CHARS[(hex16 >> 8) % 0x10];
	_hex16[2] = OLED_HEX_CHARS[(hex16 >> 4) % 0x10];
	_hex16[3] = OLED_HEX_CHARS[(hex16&0x0f)%0x10];
	_hex16[4] = 'H';
	_hex16[5] = '\0';
	oled_drawText(x, y, _hex16);
}
void oled_displayHex32(int16_t x, int16_t y, uint8_t hex32){
	uint8_t i, j;
	char _hex32[10];
	for(i = 28, j = 0; (i > 0) && (j < 7); i -= 4, j++) _hex32[j] = OLED_HEX_CHARS[(hex32 >> i) % 0x10];
	_hex32[7] = OLED_HEX_CHARS[(hex32&0x0f)%0x10];
	_hex32[8] = 'H';
	_hex32[9] ='\0';
	oled_drawText(x, y, _hex32);
}

void oled_displayInt(int16_t x, int16_t y, int num){
	char _num[16];
	uint8_t n;
	n=snprintf(_num, 16, "%d", num);
	if(n<16) _num[n] = '\0';
	else _num[15] = '\0';
	oled_drawText(x, y, _num);
}

void oled_displayFloat(int16_t x, int16_t y, float _float){
	char _num[16];
	uint8_t n;
	n=snprintf(_num, 16, "%.3f", _float);
	if(n<16) _num[n] = '\0';
	else _num[15] = '\0';
	oled_drawText(x, y, _num);
}

void oled_setTextSize(uint8_t s){
	textsize = (s > 0) ? s : 1;
}

void oled_setTextColor(uint8_t color, uint8_t bg){
  // 对于"透明"背景，我们将bg设置为与fg相同，而不是使用标志
  textcolor = color;
  textbgcolor = bg;
}

//void oled_setFont(const GFXfont *f){
//  if(f) {            // Font struct pointer passed in?
//      if(!gfxFont) { // And no current font struct?
//          // Switching from classic to new font behavior.
//          // Move cursor pos down 6 pixels so it's on baseline.
//          cursorY += 6;
//      }
//  } else if(gfxFont) { // NULL passed.  Current font struct defined?
//      // Switching from new to classic font behavior.
//      // Move cursor pos up 6 pixels so it's at top-left of char.
//      cursorY -= 6;
//  }
//  gfxFont = (GFXfont *)f;
//}
//void oled_getTextBounds(const char *string, int16_t x, int16_t y,int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h){
//
//}

void oled_setUserFont(oledFont* _userfont){
	if(_userfont){
		cursorY -= 7;
		oledfont = _userfont;
	}
}
uint8_t oled_getUserFontW(void){
	uint8_t _fontW = pgm_read_byte(&oledfont->width);
	return _fontW;
}
uint8_t oled_getUserFontH(void){
	uint8_t _fontH = pgm_read_byte(&oledfont->height);
	return _fontH;
}








