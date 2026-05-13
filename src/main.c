/*
 * ch32v307 eide demo
 * MT9V03X PC Monochrome Camera Demo
 * Copyright (c) 2022 Taoyukai & SEEKFREE
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zf_common_headfile.h"

#define LED1                    (A15)

// ==================== 模式选择 ====================
// 1: 发送图像到PC显示模式
// 0: 形状检测 fill 结果打印模式
#define CAM_DEBUG_VIEW          (1)

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

        #endif
        }
    }
}
