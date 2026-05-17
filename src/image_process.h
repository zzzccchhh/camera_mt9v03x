/*
 * 图像处理模块
 * MT9V03X摄像头二值化、边界提取与直线拟合
 */

#ifndef __IMAGE_PROCESS_H__
#define __IMAGE_PROCESS_H__

#include "zf_common_headfile.h"

// ==================== 显示尺寸 ====================

// OLED显示图像尺寸
#define DISPLAY_W                  128
#define DISPLAY_H                  64

// 偏差计算使用的图像行（对应实际y坐标）
// row范围：0~31，对应 y=32~63
// 例如：DEVIATION_CALC_ROW = 8 对应 y = 8 + 32 = 40
#define DEVIATION_CALC_ROW         8

// 三个偏差计算行
#define DEVIATION_CALC_ROW_35      3   // y = 35
#define DEVIATION_CALC_ROW_40      8   // y = 40
#define DEVIATION_CALC_ROW_45      13  // y = 45
#define DEVIATION_FIT_POINTS       3   // 拟合点数

// ==================== 边界线处理参数 ====================

// 仅处理图像下半部分（y = 32 ~ 63）
#define BOUNDARY_LINE_START_ROW    32
#define BOUNDARY_LINE_ROWS         32

// 相邻边界点最大允许跳变
// 超过该值认为是噪声点
#define STEP_THRESHOLD             4

// 点到拟合直线的最大允许偏差
// 超过则认为是离群点
#define DEVIATION_THRESHOLD        4

// 最大有效拟合点数
#define MAX_VALID_POINTS           32

// ==================== 拟合滤波参数 ====================

// 拟合结果滑动平均窗口大小
#define FIT_FILTER_WINDOW          3

// 新拟合值与当前滤波值偏差过大时拒绝更新
#define FIT_VALUE_DEVIATION        30

// ==================== 图像数组 ====================

// 二值化图像数组
// 行优先：[y][x]
extern uint8 binary_image[DISPLAY_H][DISPLAY_W];

// 边界图像数组
// 行优先：[y][x]
extern uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// ==================== 边界线数组 ====================

// 左边界数组
// 下标：0~31 对应 y=32~63
// 数值：对应行的边界x坐标
// 255表示无效
extern uint8 left_boundary_line[BOUNDARY_LINE_ROWS];

// 右边界数组
extern uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// ==================== 直线拟合结构体 ====================

// 拟合模型：
// x = k * y + b
typedef struct
{
    float k;                                  // 斜率
    float b;                                  // 截距

    uint8 valid_count;                        // 有效点数量

    // 保留下来的有效点索引
    uint8 valid_indices[MAX_VALID_POINTS];

} LineFitResult;

// 左右边界拟合结果
extern LineFitResult left_line_fit;
extern LineFitResult right_line_fit;

// ==================== 拟合滤波结构体 ====================

typedef struct
{
    // k值循环缓冲区
    float k_buf[FIT_FILTER_WINDOW];

    // b值循环缓冲区
    float b_buf[FIT_FILTER_WINDOW];

    // 当前写入位置
    uint8 write_idx;

    // 当前有效数据个数
    uint8 count;

    // 当前窗口k值总和
    float k_sum;

    // 当前窗口b值总和
    float b_sum;

    // 平滑后的k值
    float k_smooth;

    // 平滑后的b值
    float b_smooth;

} FitFilter;

// 左右边界滤波器
extern FitFilter left_fit_filter;
extern FitFilter right_fit_filter;

// ==================== 显示模式 ====================

typedef enum
{
    DISPLAY_BINARY = 0,       // 显示二值图
    DISPLAY_BOUNDARY,         // 显示边界图
    DISPLAY_BOUNDARY_LINE,    // 显示边界线
    DISPLAY_FIT_LINE          // 显示拟合直线

} DisplayMode;

// ==================== 基础函数 ====================

// 初始化图像处理模块
void image_process_init(void);

// 图像二值化
// 输入灰度图，输出binary_image
void image_binarize(const uint8 *camera_img,
                    uint16 img_w,
                    uint16 img_h,
                    uint8 threshold);

// 八邻域边界提取
// 输入binary_image
// 输出boundary_image
void extract_boundary(void);

// ==================== 边界线提取 ====================

// 从boundary_image中提取左右边界线
//
// 处理内容：
// 1. 每行只保留一个边界点
// 2. 左边界：从左往右搜索
// 3. 右边界：从右往左搜索
// 4. 差分跳变滤波
void extract_boundary_line(void);

// 中值滤波
// 推荐窗口大小：3 或 5
void median_filter_boundary_line(uint8 window_size);

// 获取指定行左边界x坐标
// row范围：0~31
uint8 get_left_boundary_x(uint8 row);

// 获取指定行右边界x坐标
// row范围：0~31
uint8 get_right_boundary_x(uint8 row);

// ==================== 显示控制 ====================

// 获取当前显示模式
DisplayMode image_get_display_mode(void);

// 切换显示模式
//
// 循环顺序：
// 二值图 -> 边界图 -> 边界线 -> 拟合直线 -> 二值图
void image_toggle_display_mode(void);

// OLED显示当前模式图像
void image_display(void);

// ==================== 最小二乘拟合 ====================

// 对左右边界进行预拟合
//
// 流程：
// 1. 收集有效点
// 2. 第一次拟合
// 3. 剔除离群点
// 4. 第二次拟合
void pre_fit_boundary_lines(void);

// 对拟合结果进行滑动平均滤波
void fit_filter_boundary_lines(void);

// 获取拟合左边界在指定行的x坐标
//
// 使用模型：
// x = k*y + b
uint8 get_fit_left_x(uint8 row);

// 获取拟合右边界在指定行的x坐标
uint8 get_fit_right_x(uint8 row);

// 计算中心线目标X坐标（高鲁棒性：双边/单边/全丢场景）
uint8 calculate_track_center(uint8 row);

// 计算赛道偏差值
// 在y=35,40,45三个位置获取中心点，拟合直线x=k*y+b，返回k值作为偏差（有正负）
float calculate_deviation(void);

#endif