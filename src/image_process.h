/*
 * 图像处理模块
 * MT9V03X摄像头二值化与边界提取
 */

#ifndef __IMAGE_PROCESS_H__
#define __IMAGE_PROCESS_H__

#include "zf_common_headfile.h"

// OLED显示图像尺寸
#define DISPLAY_W   (128)
#define DISPLAY_H   (64)

// 边界线处理常量（下半部分32行: y=32~63）
#define BOUNDARY_LINE_START_ROW 32
#define BOUNDARY_LINE_ROWS      32
#define STEP_THRESHOLD          4   // 差分阈值，用于噪声检测（3-5像素）

// 二值化图像数组 (行优先: [y][x], y=0-63, x=0-127)
extern uint8 binary_image[DISPLAY_H][DISPLAY_W];

// 边界图像数组 (行优先: [y][x], y=0-63, x=0-127)
extern uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// 边界线数组（下半部分32行每行的左右边界x坐标）
// 值127表示无效/未检测到边界
extern uint8 left_boundary_line[BOUNDARY_LINE_ROWS];
extern uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// 显示模式枚举
typedef enum {
    DISPLAY_BINARY,       // 显示原始二值图
    DISPLAY_BOUNDARY,     // 显示边界扫描图
    DISPLAY_BOUNDARY_LINE // 显示提取的边界线
} DisplayMode;

// 初始化图像处理模块
void image_process_init(void);

// 将摄像头图像二值化到128x64显示分辨率
void image_binarize(const uint8 *camera_img, uint16 img_w, uint16 img_h, uint8 threshold);

// 使用八邻域算法提取边界
void extract_boundary(void);

// 从boundary_image提取边界线（下半部分32行, y=32~63）
// 应用：行唯一性约束、差分噪声过滤
void extract_boundary_line(void);

// 对边界线应用滑动窗口中值滤波
void median_filter_boundary_line(uint8 window_size);

// 获取指定行的边界x坐标（内部使用0-31，映射到y=32~63）
uint8 get_left_boundary_x(uint8 row);
uint8 get_right_boundary_x(uint8 row);

// 获取当前显示模式
DisplayMode image_get_display_mode(void);

// 切换显示模式（循环：二值图 -> 边界图 -> 边界线图 -> 二值图）
void image_toggle_display_mode(void);

// 显示当前模式图像到OLED
void image_display(void);

#endif