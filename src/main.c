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
#include "image_process.h"

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

int main(void)
{
    clock_init(SYSTEM_CLOCK_120M);
    debug_init();

    gpio_init(LED1, GPO, GPIO_HIGH, GPO_PUSH_PULL);
    // PE8按键初始化（用于切换显示模式，上拉输入）
    gpio_init(E8, GPI, 0, GPI_PULL_UP);

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
    // 初始化图像处理模块
    image_process_init();
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

    uint8 last_pe8_state = 1;  // PE8默认高电平（未按下）

    while (1)
    {
        // PE8按键检测（按下为低电平）
        uint8 pe8_state = gpio_get_level(E8);
        if (last_pe8_state == 1 && pe8_state == 0) {
            image_toggle_display_mode();
        }
        last_pe8_state = pe8_state;

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
            // 图像二值化
            image_binarize(mt9v03x_image[0], MT9V03X_W, MT9V03X_H, cached_threshold);
            // 边界检测
            extract_boundary();
            // 显示图像
            image_display();
        #endif
        }
    }
}