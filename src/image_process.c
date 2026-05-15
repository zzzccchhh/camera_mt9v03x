/*
 * 图像处理模块
 * MT9V03X摄像头二值化与边界提取
 */

#include "image_process.h"
#include "oled_ssd1306.h"
#include "oled_spi_.h"

// OLED显示缓冲区 (128x64像素 = 1024字节，按页组织：128宽 x 8页高)
static uint8 oled_display_buffer[DISPLAY_W * 8];

// 二值化图像数组 (行优先: [y][x], y=0-63, x=0-127)
uint8 binary_image[DISPLAY_H][DISPLAY_W];

// 边界图像数组 (行优先: [y][x], y=0-63, x=0-127)
uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// 当前显示模式
static DisplayMode current_display_mode = DISPLAY_BINARY;

// 边界线数组 (下半部分32行: y=32~63)
// 值127表示无效/未检测到边界
uint8 left_boundary_line[BOUNDARY_LINE_ROWS];
uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// 初始化图像处理模块
void image_process_init(void)
{
    memset(binary_image, 0, sizeof(binary_image));
    memset(boundary_image, 0, sizeof(boundary_image));
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));
    memset(left_boundary_line, 127, sizeof(left_boundary_line));
    memset(right_boundary_line, 127, sizeof(right_boundary_line));
}

// 将摄像头图像二值化到128x64显示分辨率
void image_binarize(const uint8 *camera_img, uint16 img_w, uint16 img_h, uint8 threshold)
{
    uint16 x, y;
    uint16 src_x, src_y;

    for (y = 0; y < DISPLAY_H; y++) {
        src_y = y * img_h / DISPLAY_H;
        for (x = 0; x < DISPLAY_W; x++) {
            src_x = x * img_w / DISPLAY_W;
            binary_image[y][x] = camera_img[src_y * img_w + src_x] > threshold ? 1 : 0;
        }
    }
}

// 使用八邻域算法提取边界
void extract_boundary(void)
{
    int x, y;
    for (y = 0; y < DISPLAY_H; y++) {
        for (x = 0; x < DISPLAY_W; x++) {
            if (binary_image[y][x] == 0) {
                boundary_image[y][x] = 0;  // 背景保持为0
                continue;
            }
            // 八邻域检查
            uint8 is_boundary = 0;
            if (y > 0 && binary_image[y-1][x] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && binary_image[y+1][x] == 0) is_boundary = 1;
            if (x > 0 && binary_image[y][x-1] == 0) is_boundary = 1;
            if (x < DISPLAY_W - 1 && binary_image[y][x+1] == 0) is_boundary = 1;
            if (y > 0 && x > 0 && binary_image[y-1][x-1] == 0) is_boundary = 1;
            if (y > 0 && x < DISPLAY_W - 1 && binary_image[y-1][x+1] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && x > 0 && binary_image[y+1][x-1] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && x < DISPLAY_W - 1 && binary_image[y+1][x+1] == 0) is_boundary = 1;

            boundary_image[y][x] = is_boundary;
        }
    }
}

// 从boundary_image提取边界线（下半部分32行: y=32~63）
// 应用行唯一性约束（每行取最左/最右边界点）
void extract_boundary_line(void)
{
    int x, y;

    // 重置边界线数组
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        left_boundary_line[y] = 127;
        right_boundary_line[y] = 127;
    }

    // 扫描下半部分32行 (y=32至y=63)
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        int actual_y = BOUNDARY_LINE_START_ROW + y;

        // 寻找左边界（行中最右侧的点，即靠内侧）
        for (x = 0; x < DISPLAY_W; x++) {
            if (boundary_image[actual_y][x] == 1) {
                left_boundary_line[y] = x;
                break;  // 找到最左边界点后跳出
            }
        }

        // 寻找右边界（行中最左侧的点，即靠内侧）
        for (x = DISPLAY_W - 1; x >= 0; x--) {
            if (boundary_image[actual_y][x] == 1) {
                right_boundary_line[y] = x;
                break;  // 找到最右边界点后跳出
            }
        }
    }

    // 步骤2：差分噪声过滤（去除孤立点）
    // 左边界差分过滤
    for (y = 1; y < BOUNDARY_LINE_ROWS; y++) {
        if (left_boundary_line[y] != 127 && left_boundary_line[y-1] != 127) {
            int16 delta = abs((int16)left_boundary_line[y] - (int16)left_boundary_line[y-1]);
            if (delta > STEP_THRESHOLD) {
                left_boundary_line[y] = left_boundary_line[y-1];  // 用前驱点替代
            }
        }
    }

    // 右边界差分过滤
    for (y = 1; y < BOUNDARY_LINE_ROWS; y++) {
        if (right_boundary_line[y] != 127 && right_boundary_line[y-1] != 127) {
            int16 delta = abs((int16)right_boundary_line[y] - (int16)right_boundary_line[y-1]);
            if (delta > STEP_THRESHOLD) {
                right_boundary_line[y] = right_boundary_line[y-1];  // 用前驱点替代
            }
        }
    }
}

// 对边界线应用滑动窗口中值滤波
void median_filter_boundary_line(uint8 window_size)
{
    uint8 temp_left[BOUNDARY_LINE_ROWS];
    uint8 temp_right[BOUNDARY_LINE_ROWS];
    int half_window = window_size / 2;
    int i, j;

    // 复制原始数组
    memcpy(temp_left, left_boundary_line, sizeof(temp_left));
    memcpy(temp_right, right_boundary_line, sizeof(temp_right));

    // 左边界中值滤波
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];  // 最大窗口大小5
        uint8 count = 0;

        // 收集窗口内元素
        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_left[idx] != 127) {  // 跳过无效值
                    values[count++] = temp_left[idx];
                }
            }
        }

        // 排序并取中值
        if (count > 0) {
            // 简单冒泡排序（用于小数组）
            for (j = 0; j < count - 1; j++) {
                uint8 k;
                for (k = 0; k < count - j - 1; k++) {
                    if (values[k] > values[k+1]) {
                        uint8 tmp = values[k];
                        values[k] = values[k+1];
                        values[k+1] = tmp;
                    }
                }
            }
            left_boundary_line[i] = values[count / 2];  // 中值
        }
    }

    // 右边界中值滤波
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];  // 最大窗口大小5
        uint8 count = 0;

        // 收集窗口内元素
        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_right[idx] != 127) {  // 跳过无效值
                    values[count++] = temp_right[idx];
                }
            }
        }

        // 排序并取中值
        if (count > 0) {
            // 简单冒泡排序（用于小数组）
            for (j = 0; j < count - 1; j++) {
                uint8 k;
                for (k = 0; k < count - j - 1; k++) {
                    if (values[k] > values[k+1]) {
                        uint8 tmp = values[k];
                        values[k] = values[k+1];
                        values[k+1] = tmp;
                    }
                }
            }
            right_boundary_line[i] = values[count / 2];  // 中值
        }
    }
}

// 获取指定行的边界x坐标（内部使用0-31，映射到y=32~63）
uint8 get_left_boundary_x(uint8 row)
{
    if (row < BOUNDARY_LINE_ROWS) {
        return left_boundary_line[row];
    }
    return 127;
}

uint8 get_right_boundary_x(uint8 row)
{
    if (row < BOUNDARY_LINE_ROWS) {
        return right_boundary_line[row];
    }
    return 127;
}

// 获取当前显示模式
DisplayMode image_get_display_mode(void)
{
    return current_display_mode;
}

// 切换显示模式（循环：二值图 -> 边界图 -> 边界线图 -> 二值图）
void image_toggle_display_mode(void)
{
    switch (current_display_mode) {
        case DISPLAY_BINARY:       current_display_mode = DISPLAY_BOUNDARY;      break;
        case DISPLAY_BOUNDARY:      current_display_mode = DISPLAY_BOUNDARY_LINE; break;
        case DISPLAY_BOUNDARY_LINE: current_display_mode = DISPLAY_BINARY;       break;
    }
}

// 显示当前模式图像到OLED
void image_display(void)
{
    uint16 x, y;

    // 清空显示缓冲区
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));

    if (current_display_mode == DISPLAY_BOUNDARY_LINE) {
        // 为下半部分32行绘制边界线 (y=32~63)
        for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
            int actual_y = BOUNDARY_LINE_START_ROW + y;
            uint8 page = actual_y / 8;
            uint8 row_bit = actual_y % 8;

            // 绘制左边界点
            if (left_boundary_line[y] != 127) {
                x = left_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }

            // 绘制右边界点
            if (right_boundary_line[y] != 127) {
                x = right_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }
        }
    } else {
        // 绘制二值图或边界图
        uint8 (*img)[DISPLAY_W] = (current_display_mode == DISPLAY_BINARY) ? binary_image : boundary_image;

        for (y = 0; y < DISPLAY_H; y++) {
            uint8 page = y / 8;
            uint8 row_bit = y % 8;

            for (x = 0; x < DISPLAY_W; x++) {
                if (img[y][x]) {
                    oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
                }
            }
        }
    }

    // 复制到SSD1306显示缓冲区
    memcpy(ssd1306_getBuffer(), oled_display_buffer, sizeof(oled_display_buffer));

    // 全屏刷新
    ssd1306_updateScreen();
}