#include "image_process.h"
#include "oled_ssd1306.h"
#include "oled_spi_.h"
#include <string.h>
#include <math.h>

// ==================== 参数 ====================

// 连续丢线达到该帧数后，清空滤波器并判定为无效
// 数值越小，丢线后恢复越快；数值越大，短时遮挡时越稳
#define LOST_FRAME_THRESHOLD    3

// ==================== 全局变量 ====================

// OLED显示缓冲区 (128x64像素 = 1024字节，按页组织：128宽 x 8页高)
static uint8 oled_display_buffer[DISPLAY_W * 8];

// 二值化图像数组 (行优先: [y][x], y=0-63, x=0-127)
uint8 binary_image[DISPLAY_H][DISPLAY_W];

// 边界图像数组 (行优先: [y][x], y=0-63, x=0-127)
uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// 当前显示模式
static DisplayMode current_display_mode = DISPLAY_BINARY;

// 边界线数组 (下半部分32行: y=32~63)
// 值255表示无效/未检测到边界
uint8 left_boundary_line[BOUNDARY_LINE_ROWS];
uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// 拟合结果全局变量
LineFitResult left_line_fit;
LineFitResult right_line_fit;

// 拟合直线平滑滤波
FitFilter left_fit_filter;
FitFilter right_fit_filter;

// 丢线计数
static uint8 left_lost_count = 0;
static uint8 right_lost_count = 0;

// ==================== 工具函数 ====================

// 清空拟合滤波器
static void clear_fit_filter(FitFilter *filter)
{
    memset(filter->k_buf, 0, sizeof(filter->k_buf));
    memset(filter->b_buf, 0, sizeof(filter->b_buf));
    filter->write_idx = 0;
    filter->count = 0;
    filter->k_sum = 0.0f;
    filter->b_sum = 0.0f;
    filter->k_smooth = 0.0f;
    filter->b_smooth = 0.0f;
}

// 计算点到拟合直线的距离
// 拟合模型: x = k*y + b
static float calc_point_deviation(uint8 x, uint8 y, float k, float b)
{
    // 点(x,y)到直线 x = k*y + b 的距离:
    // |x - (k*y + b)| / sqrt(k^2 + 1)
    float numerator = fabsf((float)x - (k * (float)y + b));
    float denominator = sqrtf(k * k + 1.0f);
    return numerator / denominator;
}

// 最小二乘法直线拟合 (x = k*y + b)
static void least_squares_fit(uint8 *y_points, uint8 *x_points, uint8 n, float *k, float *b)
{
    if (n < 2) {
        *k = 0.0f;
        *b = 0.0f;
        return;
    }

    float sum_y = 0.0f, sum_x = 0.0f, sum_yx = 0.0f, sum_yy = 0.0f;
    float f_n = (float)n;
    uint8 i;

    for (i = 0; i < n; i++) {
        float y = (float)y_points[i];
        float x = (float)x_points[i];
        sum_y  += y;
        sum_x  += x;
        sum_yx += y * x;
        sum_yy += y * y;
    }

    float denominator = f_n * sum_yy - sum_y * sum_y;
    if (fabsf(denominator) < 0.001f) {
        // y变化太小，退化为常数线 x = b
        *k = 0.0f;
        *b = sum_x / f_n;
        return;
    }

    *k = (f_n * sum_yx - sum_y * sum_x) / denominator;
    *b = (sum_x - (*k) * sum_y) / f_n;
}

// ==================== 初始化 ====================

// 初始化图像处理模块
void image_process_init(void)
{
    memset(binary_image, 0, sizeof(binary_image));
    memset(boundary_image, 0, sizeof(boundary_image));
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));
    memset(left_boundary_line, 255, sizeof(left_boundary_line));
    memset(right_boundary_line, 255, sizeof(right_boundary_line));

    memset(&left_line_fit, 0, sizeof(left_line_fit));
    memset(&right_line_fit, 0, sizeof(right_line_fit));

    clear_fit_filter(&left_fit_filter);
    clear_fit_filter(&right_fit_filter);

    left_lost_count = 0;
    right_lost_count = 0;
}

// ==================== 二值化与边界提取 ====================

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
                boundary_image[y][x] = 0;
                continue;
            }

            uint8 is_boundary = 0;

            if (y > 0 && binary_image[y - 1][x] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && binary_image[y + 1][x] == 0) is_boundary = 1;
            if (x > 0 && binary_image[y][x - 1] == 0) is_boundary = 1;
            if (x < DISPLAY_W - 1 && binary_image[y][x + 1] == 0) is_boundary = 1;

            if (y > 0 && x > 0 && binary_image[y - 1][x - 1] == 0) is_boundary = 1;
            if (y > 0 && x < DISPLAY_W - 1 && binary_image[y - 1][x + 1] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && x > 0 && binary_image[y + 1][x - 1] == 0) is_boundary = 1;
            if (y < DISPLAY_H - 1 && x < DISPLAY_W - 1 && binary_image[y + 1][x + 1] == 0) is_boundary = 1;

            boundary_image[y][x] = is_boundary;
        }
    }
}

// 从boundary_image提取边界线（下半部分32行: y=32~63）
// 融入【防串线交叉过滤机制】
void extract_boundary_line(void)
{
    int x, y;

    // 设定一个下半部分赛道的最小可能像素宽度（128总宽下，近景赛道宽度通常>30，设15为绝对死线）
    const uint8 MIN_TRACK_WIDTH = 15;

    // 1. 重置边界线数组，使用 255 作为无效标志
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        left_boundary_line[y] = 255;
        right_boundary_line[y] = 255;
    }

    // 2. 逐行扫描
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        int actual_y = BOUNDARY_LINE_START_ROW + y;
        uint8 lx = 255;
        uint8 rx = 255;

        // 左边界：从左往右找第一个边界点
        for (x = 0; x < DISPLAY_W; x++) {
            if (boundary_image[actual_y][x] == 1) {
                lx = (uint8)x;
                break;
            }
        }

        // 右边界：从右往左找第一个边界点
        for (x = DISPLAY_W - 1; x >= 0; x--) {
            if (boundary_image[actual_y][x] == 1) {
                rx = (uint8)x;
                break;
            }
        }

        // ==================== 核心：防串线过滤 ====================
        if (lx != 255 && rx != 255) {
            // 如果左线跑到了右线右边，或者两线距离近得不合常理
            if (lx >= rx || (rx - lx) < MIN_TRACK_WIDTH) {

                // 判定谁是内鬼：如果相撞位置在图像右半边，说明左线丢了，左线串到了右线上
                if (lx > (DISPLAY_W / 2)) {
                    lx = 255; // 强行抹除左线
                }
                // 如果相撞位置在图像左半边，说明右线丢了，右线串到了左线上
                else {
                    rx = 255; // 强行抹除右线
                }
            }

            // 边缘硬限制补充：防止死死贴边（x=0或127）影响拟合
            if (lx <= 1)           lx = 255;
            if (rx >= DISPLAY_W-2) rx = 255;
        }

        // 最终存入全局数组
        left_boundary_line[y] = lx;
        right_boundary_line[y] = rx;
    }

    // 3. 差分噪声过滤（注意：已将原本的 127 修改为 255）
    for (y = 1; y < BOUNDARY_LINE_ROWS; y++) {
        if (left_boundary_line[y] != 255 && left_boundary_line[y - 1] != 255) {
            int16 delta = abs((int16)left_boundary_line[y] - (int16)left_boundary_line[y - 1]);
            if (delta > STEP_THRESHOLD) {
                left_boundary_line[y] = left_boundary_line[y - 1];
            }
        }

        if (right_boundary_line[y] != 255 && right_boundary_line[y - 1] != 255) {
            int16 delta = abs((int16)right_boundary_line[y] - (int16)right_boundary_line[y - 1]);
            if (delta > STEP_THRESHOLD) {
                right_boundary_line[y] = right_boundary_line[y - 1];
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

    if (window_size < 1) return;
    if (window_size > 5) window_size = 5;

    memcpy(temp_left, left_boundary_line, sizeof(temp_left));
    memcpy(temp_right, right_boundary_line, sizeof(temp_right));

    // 左边界中值滤波
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];
        uint8 count = 0;

        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_left[idx] != 255) {
                    values[count++] = temp_left[idx];
                }
            }
        }

        if (count > 0) {
            for (j = 0; j < count - 1; j++) {
                uint8 k;
                for (k = 0; k < count - j - 1; k++) {
                    if (values[k] > values[k + 1]) {
                        uint8 tmp = values[k];
                        values[k] = values[k + 1];
                        values[k + 1] = tmp;
                    }
                }
            }
            left_boundary_line[i] = values[count / 2];
        }
    }

    // 右边界中值滤波
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];
        uint8 count = 0;

        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_right[idx] != 255) {
                    values[count++] = temp_right[idx];
                }
            }
        }

        if (count > 0) {
            for (j = 0; j < count - 1; j++) {
                uint8 k;
                for (k = 0; k < count - j - 1; k++) {
                    if (values[k] > values[k + 1]) {
                        uint8 tmp = values[k];
                        values[k] = values[k + 1];
                        values[k + 1] = tmp;
                    }
                }
            }
            right_boundary_line[i] = values[count / 2];
        }
    }
}

// ==================== 显示相关 ====================

// 获取指定行的边界x坐标
uint8 get_left_boundary_x(uint8 row)
{
    if (row < BOUNDARY_LINE_ROWS) {
        return left_boundary_line[row];
    }
    return 255;
}

uint8 get_right_boundary_x(uint8 row)
{
    if (row < BOUNDARY_LINE_ROWS) {
        return right_boundary_line[row];
    }
    return 255;
}

// 获取当前显示模式
DisplayMode image_get_display_mode(void)
{
    return current_display_mode;
}

// 切换显示模式（循环：二值图 -> 边界图 -> 边界线图 -> 拟合直线 -> 二值图）
void image_toggle_display_mode(void)
{
    switch (current_display_mode) {
        case DISPLAY_BINARY:        current_display_mode = DISPLAY_BOUNDARY;       break;
        case DISPLAY_BOUNDARY:      current_display_mode = DISPLAY_BOUNDARY_LINE; break;
        case DISPLAY_BOUNDARY_LINE: current_display_mode = DISPLAY_FIT_LINE;      break;
        case DISPLAY_FIT_LINE:      current_display_mode = DISPLAY_BINARY;        break;
        default:                    current_display_mode = DISPLAY_BINARY;        break;
    }
}

// 显示当前模式图像到OLED
void image_display(void)
{
    uint16 x, y;

    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));

    if (current_display_mode == DISPLAY_BOUNDARY_LINE) {
        // 绘制边界线点
        for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
            int actual_y = BOUNDARY_LINE_START_ROW + y;
            uint8 page = actual_y / 8;
            uint8 row_bit = actual_y % 8;

            if (left_boundary_line[y] != 255) {
                x = left_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }

            if (right_boundary_line[y] != 255) {
                x = right_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }
        }
    }
    else if (current_display_mode == DISPLAY_FIT_LINE) {
        // 绘制拟合直线，拟合模型：x = k*y + b
        for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
            int actual_y = BOUNDARY_LINE_START_ROW + y;
            uint8 page = actual_y / 8;
            uint8 row_bit = actual_y % 8;

            // 左拟合线
            if (left_fit_filter.count >= 2) {
                float fit_x = left_fit_filter.k_smooth * (float)y + left_fit_filter.b_smooth;
                if (fit_x >= 0.0f && fit_x < DISPLAY_W) {
                    x = (uint8)(fit_x + 0.5f);
                    oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
                }
            }

            // 右拟合线
            if (right_fit_filter.count >= 2) {
                float fit_x = right_fit_filter.k_smooth * (float)y + right_fit_filter.b_smooth;
                if (fit_x >= 0.0f && fit_x < DISPLAY_W) {
                    x = (uint8)(fit_x + 0.5f);
                    oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
                }
            }
        }
    }
    else {
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

    memcpy(ssd1306_getBuffer(), oled_display_buffer, sizeof(oled_display_buffer));
    ssd1306_updateScreen();
}

// ==================== 拟合函数 ====================

// 单条边界线预拟合：先拟合，剔除偏差过大点，再拟合
static void pre_fit_boundary_line(uint8 *boundary_line, LineFitResult *result)
{
    uint8 x_points[MAX_VALID_POINTS];
    uint8 y_points[MAX_VALID_POINTS];
    uint8 valid_x[MAX_VALID_POINTS];
    uint8 valid_y[MAX_VALID_POINTS];
    uint8 i;
    uint8 valid_count = 0;

    // 收集有效点
    // boundary_line[i] 表示该行的 x，i 表示 y
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        if (boundary_line[i] != 255) {
            x_points[valid_count] = boundary_line[i];
            y_points[valid_count] = i;
            valid_count++;
        }
    }

    if (valid_count < 2) {
        result->k = 0.0f;
        result->b = 0.0f;
        result->valid_count = 0;
        return;
    }

    // 第一次拟合：x = k*y + b
    float k, b;
    least_squares_fit(y_points, x_points, valid_count, &k, &b);

    // 计算偏差并过滤
    uint8 filtered_count = 0;
    for (i = 0; i < valid_count; i++) {
        float deviation = calc_point_deviation(x_points[i], y_points[i], k, b);
        if (deviation <= DEVIATION_THRESHOLD) {
            valid_x[filtered_count] = x_points[i];
            valid_y[filtered_count] = y_points[i];
            result->valid_indices[filtered_count] = i;
            filtered_count++;
        }
    }

    // 如果过滤后点数不足2个，使用全部点
    if (filtered_count < 2) {
        filtered_count = valid_count;
        for (i = 0; i < valid_count; i++) {
            valid_x[i] = x_points[i];
            valid_y[i] = y_points[i];
            result->valid_indices[i] = i;
        }
    }

    // 第二次拟合
    least_squares_fit(valid_y, valid_x, filtered_count, &k, &b);

    result->k = k;
    result->b = b;
    result->valid_count = filtered_count;
}

// 对左右边界线进行预拟合
void pre_fit_boundary_lines(void)
{
    pre_fit_boundary_line(left_boundary_line, &left_line_fit);
    pre_fit_boundary_line(right_boundary_line, &right_line_fit);
}

// 对拟合结果进行平滑滤波（递推平均，窗口FIT_FILTER_WINDOW）
void fit_filter_boundary_lines(void)
{
    // ==================== 左线滤波 ====================
    if (left_line_fit.valid_count >= 2) {
        left_lost_count = 0;

        float new_k = left_line_fit.k;
        float new_b = left_line_fit.b;

        // 新值与当前平滑值差异过大时，直接沿用旧值
        if (left_fit_filter.count > 0) {
            float k_dev = fabsf(new_k - left_fit_filter.k_smooth);
            float b_dev = fabsf(new_b - left_fit_filter.b_smooth);
            if (k_dev > FIT_VALUE_DEVIATION || b_dev > FIT_VALUE_DEVIATION) {
                new_k = left_fit_filter.k_smooth;
                new_b = left_fit_filter.b_smooth;
            }
        }

        float old_k = left_fit_filter.k_buf[left_fit_filter.write_idx];
        float old_b = left_fit_filter.b_buf[left_fit_filter.write_idx];

        left_fit_filter.k_buf[left_fit_filter.write_idx] = new_k;
        left_fit_filter.b_buf[left_fit_filter.write_idx] = new_b;

        if (left_fit_filter.count > 0) {
            left_fit_filter.k_sum = left_fit_filter.k_sum - old_k + new_k;
            left_fit_filter.b_sum = left_fit_filter.b_sum - old_b + new_b;
        } else {
            left_fit_filter.k_sum = new_k;
            left_fit_filter.b_sum = new_b;
        }

        if (left_fit_filter.count < FIT_FILTER_WINDOW) {
            left_fit_filter.count++;
        }

        left_fit_filter.write_idx = (left_fit_filter.write_idx + 1) % FIT_FILTER_WINDOW;
        left_fit_filter.k_smooth = left_fit_filter.k_sum / left_fit_filter.count;
        left_fit_filter.b_smooth = left_fit_filter.b_sum / left_fit_filter.count;
    }
    else {
        // 本帧无有效边线
        if (left_lost_count < 255) {
            left_lost_count++;
        }

        // 连续丢线达到阈值，清空滤波器
        if (left_lost_count >= LOST_FRAME_THRESHOLD) {
            clear_fit_filter(&left_fit_filter);
        }
    }

    // ==================== 右线滤波 ====================
    if (right_line_fit.valid_count >= 2) {
        right_lost_count = 0;

        float new_k = right_line_fit.k;
        float new_b = right_line_fit.b;

        if (right_fit_filter.count > 0) {
            float k_dev = fabsf(new_k - right_fit_filter.k_smooth);
            float b_dev = fabsf(new_b - right_fit_filter.b_smooth);
            if (k_dev > FIT_VALUE_DEVIATION || b_dev > FIT_VALUE_DEVIATION) {
                new_k = right_fit_filter.k_smooth;
                new_b = right_fit_filter.b_smooth;
            }
        }

        float old_k = right_fit_filter.k_buf[right_fit_filter.write_idx];
        float old_b = right_fit_filter.b_buf[right_fit_filter.write_idx];

        right_fit_filter.k_buf[right_fit_filter.write_idx] = new_k;
        right_fit_filter.b_buf[right_fit_filter.write_idx] = new_b;

        if (right_fit_filter.count > 0) {
            right_fit_filter.k_sum = right_fit_filter.k_sum - old_k + new_k;
            right_fit_filter.b_sum = right_fit_filter.b_sum - old_b + new_b;
        } else {
            right_fit_filter.k_sum = new_k;
            right_fit_filter.b_sum = new_b;
        }

        if (right_fit_filter.count < FIT_FILTER_WINDOW) {
            right_fit_filter.count++;
        }

        right_fit_filter.write_idx = (right_fit_filter.write_idx + 1) % FIT_FILTER_WINDOW;
        right_fit_filter.k_smooth = right_fit_filter.k_sum / right_fit_filter.count;
        right_fit_filter.b_smooth = right_fit_filter.b_sum / right_fit_filter.count;
    }
    else {
        if (right_lost_count < 255) {
            right_lost_count++;
        }

        if (right_lost_count >= LOST_FRAME_THRESHOLD) {
            clear_fit_filter(&right_fit_filter);
        }
    }
}

// 获取拟合直线上指定行对应的x坐标（使用滤波后的值）
// 拟合模型: x = k*y + b
uint8 get_fit_left_x(uint8 row)
{
    if (row >= BOUNDARY_LINE_ROWS) return 255;
    if (left_lost_count >= LOST_FRAME_THRESHOLD) return 255;
    if (left_fit_filter.count < 2) return 255;

    float x = left_fit_filter.k_smooth * (float)row + left_fit_filter.b_smooth;
    if (x < 0.0f) return 0;
    if (x >= DISPLAY_W) return DISPLAY_W - 1;

    return (uint8)(x + 0.5f);
}

uint8 get_fit_right_x(uint8 row)
{
    if (row >= BOUNDARY_LINE_ROWS) return 255;
    if (right_lost_count >= LOST_FRAME_THRESHOLD) return 255;
    if (right_fit_filter.count < 2) return 255;

    float x = right_fit_filter.k_smooth * (float)row + right_fit_filter.b_smooth;
    if (x < 0.0f) return 0;
    if (x >= DISPLAY_W) return DISPLAY_W - 1;

    return (uint8)(x + 0.5f);
}

// 计算某一特定预瞄行 row 的中心线目标 X 坐标
// 高鲁棒性中心线合成逻辑：双边完好用中点，单边丢线用平移，全丢返回屏幕中心
uint8 calculate_track_center(uint8 row)
{
    uint8 fit_left  = get_fit_left_x(row);
    uint8 fit_right = get_fit_right_x(row);

    // 下半部分某行的半宽大概是 35 像素
    uint8 half_track_width = 35;

    // 情况 1：双边都有，最理想状态
    if (fit_left != 255 && fit_right != 255) {
        return (fit_left + fit_right) / 2;
    }
    // 情况 2：左线丢了，右线完好（出弯场景）
    else if (fit_left == 255 && fit_right != 255) {
        return fit_right - half_track_width;
    }
    // 情况 3：右线丢了，左线完好
    else if (fit_left != 255 && fit_right == 255) {
        return fit_left + half_track_width;
    }

    // 情况 4：全丢，返回图像正中
    return DISPLAY_W / 2;
}

// 计算赛道偏差值
// 在y=35,40,45三个位置获取中心点，拟合直线x=k*y+b
// 返回k值作为偏差（理想k=0，所以偏差就是k本身，有正负）
float calculate_deviation(void)
{
    uint8 center_x[DEVIATION_FIT_POINTS];
    uint8 y_points[DEVIATION_FIT_POINTS];
    uint8 rows[DEVIATION_FIT_POINTS] = {
        DEVIATION_CALC_ROW_35,
        DEVIATION_CALC_ROW_40,
        DEVIATION_CALC_ROW_45
    };
    uint8 i;
    float k, b;

    // 获取三个位置的中心x坐标
    for (i = 0; i < DEVIATION_FIT_POINTS; i++) {
        center_x[i] = calculate_track_center(rows[i]);
        y_points[i] = rows[i];
    }

    // 拟合直线 x = k*y + b
    least_squares_fit(y_points, center_x, DEVIATION_FIT_POINTS, &k, &b);

    // 返回k值作为偏差（理想情况下赛道中心线是垂直的，k=0）
    return k;
}