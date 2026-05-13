/*
 * oled_includes.h
 *
 * 移植到CH32V307VCT6
 */

#ifndef OLED_INCLUDES__H_
#define OLED_INCLUDES__H_

#include "stdint.h"
#include "ch32v30x.h"
#include "zf_common_headfile.h"

/*  软件SPI - 不用DMA  */
#define SPI_TRANSMIT 0

/* delay using zf_driver_delay */
#define SSD1306_Delay(time)                         system_delay_ms(time)

#endif /* NEW_OLED_INCLUDES__H_ */
