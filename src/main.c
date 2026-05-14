/*
 * ch32v307 eide demo
 * MT9V03X PC Monochrome Camera Demo
 * Copyright (c) 2022 Taoyukai & SEEKFREE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zf_common_headfile.h"
#include "oled_ssd1306.h"
#include "oled_spi_.h"
#include "otsu.h"

#define LED1                    (A15)

// ==================== 模式选择 ====================
// 1: 发送图像到PC显示模式
// 0: OLED显示二值化图像模式
#define CAM_DEBUG_VIEW          (0)

// Otsu自动阈值缓存（带时间平滑）
static uint8 cached_threshold = 127;
static uint8 frame_counter = 0;
static uint8 prev_threshold = 127;
#define THRESHOLD_SMOOTH (0.7f)

// 缩放显示：将MT9V03X图像(188x120)缩放到OLED(128x64)并二值化
void oled_display_binary_scaled(const uint8 *image, uint16 img_w, uint16 img_h, uint8 threshold)
{
    uint16 x, y;
    uint16 src_x, src_y;
    uint8 pixel;

    for (y = 0; y < 64; y++)
    {
        for (x = 0; x < 128; x++)
        {
            // 计算源图像坐标（等比缩放）
            src_x = x * img_w / 128;
            src_y = y * img_h / 64;

            // 读取灰度像素并二值化
            pixel = image[src_y * img_w + src_x] > threshold ? 1 : 0;

            // 使用ssd1306驱动绘制像素
            ssd1306_drawPixel(x, y, pixel);
        }
    }

    // 刷新显示
    ssd1306_updateScreen();
}

int main(void)
{
    clock_init(SYSTEM_CLOCK_120M);
    debug_init();

    gpio_init(LED1, GPO, GPIO_HIGH, GPO_PUSH_PULL);

    // 摄像头初始化,失败时LED闪烁提示
    while (mt9v03x_init())
    {
        gpio_toggle_level(LED1);
        system_delay_ms(500);
    }

#if (0 == CAM_DEBUG_VIEW)
    // OLED初始化（使用ssd1306驱动）
    ssd1306_Init(SSD1306_SWITCHCAPVCC);
    ssd1306_clearScreen();
#endif

#if (1 == CAM_DEBUG_VIEW)
    // 图像模式,发送图像给电脑软件
    // 注意: 此模式下不要加 printf 的打印语句,会扰乱图像协议
    seekfree_assistant_interface_init(SEEKFREE_ASSISTANT_DEBUG_UART);
    seekfree_assistant_camera_information_config(
        SEEKFREE_ASSISTANT_MT9V03X,
        mt9v03x_image[0],
        MT9V03X_W,
        MT9V03X_H);
#endif

    while (1)
    {
        if (mt9v03x_finish_flag)
        {
            mt9v03x_finish_flag = 0;

        #if (1 == CAM_DEBUG_VIEW)
            seekfree_assistant_camera_send();
        #else
            // 每10帧计算一次Otsu自动阈值（带平滑）
            if (++frame_counter >= 10) {
                uint8 new_threshold = otsu_threshold(mt9v03x_image[0], MT9V03X_W, MT9V03X_H);
                // 平滑处理：避免阈值突变
                if (new_threshold > prev_threshold) {
                    cached_threshold = prev_threshold + (uint8)((float)(new_threshold - prev_threshold) * THRESHOLD_SMOOTH);
                } else {
                    cached_threshold = prev_threshold - (uint8)((float)(prev_threshold - new_threshold) * THRESHOLD_SMOOTH);
                }
                prev_threshold = cached_threshold;
                frame_counter = 0;
            }
            // 显示二值化图像到OLED（使用Otsu自动阈值）
            oled_display_binary_scaled(mt9v03x_image[0], MT9V03X_W, MT9V03X_H, cached_threshold);
        #endif
        }
    }
}
