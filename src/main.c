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

// OLED显示缓冲 (128x64像素 = 1024字节, 按页组织: 128宽 x 8页高)
static uint8 oled_display_buffer[128 * 8];

// 二值图像数组 (行优先: [y][x], y=0-63, x=0-127)
static uint8 binary_image[64][128];

// 边界图像数组 (行优先: [y][x], y=0-63, x=0-127)
static uint8 boundary_image[64][128];

// 显示模式枚举
typedef enum {
    DISPLAY_BINARY,    // 显示原始二值图
    DISPLAY_BOUNDARY   // 显示边界图
} DisplayMode;
static DisplayMode current_display_mode = DISPLAY_BINARY;

// 八邻域边界检测：目标像素为1但周围有0则为边界
void extract_boundary(const uint8 binary_img[64][128], uint8 bound_img[64][128])
{
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 128; x++) {
            if (binary_img[y][x] == 0) {
                bound_img[y][x] = 0;  // 背景保留为0
                continue;
            }
            // 八邻域检查
            uint8 is_boundary = 0;
            if (y > 0 && binary_img[y-1][x] == 0) is_boundary = 1;
            if (y < 63 && binary_img[y+1][x] == 0) is_boundary = 1;
            if (x > 0 && binary_img[y][x-1] == 0) is_boundary = 1;
            if (x < 127 && binary_img[y][x+1] == 0) is_boundary = 1;
            if (y > 0 && x > 0 && binary_img[y-1][x-1] == 0) is_boundary = 1;
            if (y > 0 && x < 127 && binary_img[y-1][x+1] == 0) is_boundary = 1;
            if (y < 63 && x > 0 && binary_img[y+1][x-1] == 0) is_boundary = 1;
            if (y < 63 && x < 127 && binary_img[y+1][x+1] == 0) is_boundary = 1;

            bound_img[y][x] = is_boundary;
        }
    }
}

// 缩放显示：将MT9V03X图像缩放到OLED并二值化,根据模式显示二值图或边界图
void oled_display_binary_scaled_mode(DisplayMode mode)
{
    uint16 x, y;
    uint8 (*img)[128] = (mode == DISPLAY_BINARY) ? binary_image : boundary_image;

    // 清空显示缓冲
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));

    // 从选择的图像数组构建显示缓冲
    for (y = 0; y < 64; y++)
    {
        uint8 page = y / 8;
        uint8 row_bit = y % 8;

        for (x = 0; x < 128; x++)
        {
            if (img[y][x]) {
                oled_display_buffer[page * 128 + x] |= (1 << row_bit);
            }
        }
    }

    // 复制到SSD1306显示缓冲
    memcpy(ssd1306_getBuffer(), oled_display_buffer, sizeof(oled_display_buffer));

    // 整屏刷新
    ssd1306_updateScreen();
}

// 切换显示模式
static void toggle_display_mode(void)
{
    current_display_mode = (current_display_mode == DISPLAY_BINARY) ? DISPLAY_BOUNDARY : DISPLAY_BINARY;
}

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
            toggle_display_mode();
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
            // 二值化处理
            uint16 x, y;
            uint16 src_x, src_y;
            for (y = 0; y < 64; y++) {
                src_y = y * MT9V03X_H / 64;
                for (x = 0; x < 128; x++) {
                    src_x = x * MT9V03X_W / 128;
                    binary_image[y][x] = mt9v03x_image[0][src_y * MT9V03X_W + src_x] > cached_threshold ? 1 : 0;
                }
            }
            // 边界检测
            extract_boundary(binary_image, boundary_image);
            // 根据当前模式显示二值图或边界图
            oled_display_binary_scaled_mode(current_display_mode);
        #endif
        }
    }
}
