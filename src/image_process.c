/*
 * Image Processing Module
 * Binary thresholding and boundary extraction for MT9V03X camera
 */

#include "image_process.h"
#include "oled_ssd1306.h"
#include "oled_spi_.h"

// OLED display buffer (128x64 pixels = 1024 bytes, organized by pages: 128 wide x 8 pages high)
static uint8 oled_display_buffer[DISPLAY_W * 8];

// Binary image array (row-major: [y][x], y=0-63, x=0-127)
uint8 binary_image[DISPLAY_H][DISPLAY_W];

// Boundary image array (row-major: [y][x], y=0-63, x=0-127)
uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// Current display mode
static DisplayMode current_display_mode = DISPLAY_BINARY;

// Boundary line arrays (bottom 32 rows: y=32~63)
// Value 127 means invalid/no boundary detected
uint8 left_boundary_line[BOUNDARY_LINE_ROWS];
uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// Initialize image processing module
void image_process_init(void)
{
    memset(binary_image, 0, sizeof(binary_image));
    memset(boundary_image, 0, sizeof(boundary_image));
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));
    memset(left_boundary_line, 127, sizeof(left_boundary_line));
    memset(right_boundary_line, 127, sizeof(right_boundary_line));
}

// Binarize camera image to 128x64 display resolution
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

// Extract boundary using 8-neighborhood algorithm
void extract_boundary(void)
{
    int x, y;
    for (y = 0; y < DISPLAY_H; y++) {
        for (x = 0; x < DISPLAY_W; x++) {
            if (binary_image[y][x] == 0) {
                boundary_image[y][x] = 0;  // Background stays 0
                continue;
            }
            // 8-neighborhood check
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

// Extract boundary lines from boundary_image (bottom 32 rows: y=32~63)
// Applies row uniqueness constraint (leftmost/rightmost per row)
void extract_boundary_line(void)
{
    int x, y;

    // Reset boundary line arrays
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        left_boundary_line[y] = 127;
        right_boundary_line[y] = 127;
    }

    // Scan bottom 32 rows (y=32 to y=63)
    for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
        int actual_y = BOUNDARY_LINE_START_ROW + y;

        // Find left boundary (rightmost point in row - inner side)
        for (x = 0; x < DISPLAY_W; x++) {
            if (boundary_image[actual_y][x] == 1) {
                left_boundary_line[y] = x;
                break;  // Found leftmost boundary point
            }
        }

        // Find right boundary (leftmost point in row - inner side)
        for (x = DISPLAY_W - 1; x >= 0; x--) {
            if (boundary_image[actual_y][x] == 1) {
                right_boundary_line[y] = x;
                break;  // Found rightmost boundary point
            }
        }
    }

    // Step 2: Differential noise filtering (remove isolated points)
    // Left boundary differential filter
    for (y = 1; y < BOUNDARY_LINE_ROWS; y++) {
        if (left_boundary_line[y] != 127 && left_boundary_line[y-1] != 127) {
            int16 delta = abs((int16)left_boundary_line[y] - (int16)left_boundary_line[y-1]);
            if (delta > STEP_THRESHOLD) {
                left_boundary_line[y] = left_boundary_line[y-1];  // Replace with previous point
            }
        }
    }

    // Right boundary differential filter
    for (y = 1; y < BOUNDARY_LINE_ROWS; y++) {
        if (right_boundary_line[y] != 127 && right_boundary_line[y-1] != 127) {
            int16 delta = abs((int16)right_boundary_line[y] - (int16)right_boundary_line[y-1]);
            if (delta > STEP_THRESHOLD) {
                right_boundary_line[y] = right_boundary_line[y-1];  // Replace with previous point
            }
        }
    }
}

// Apply sliding window median filter to boundary lines
void median_filter_boundary_line(uint8 window_size)
{
    uint8 temp_left[BOUNDARY_LINE_ROWS];
    uint8 temp_right[BOUNDARY_LINE_ROWS];
    int half_window = window_size / 2;
    int i, j;

    // Copy original arrays
    memcpy(temp_left, left_boundary_line, sizeof(temp_left));
    memcpy(temp_right, right_boundary_line, sizeof(temp_right));

    // Median filter for left boundary
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];  // Max window size 5
        uint8 count = 0;

        // Collect window elements
        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_left[idx] != 127) {  // Skip invalid values
                    values[count++] = temp_left[idx];
                }
            }
        }

        // Sort and find median
        if (count > 0) {
            // Simple bubble sort for small array
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
            left_boundary_line[i] = values[count / 2];  // Median
        }
    }

    // Median filter for right boundary
    for (i = 0; i < BOUNDARY_LINE_ROWS; i++) {
        uint8 values[5];  // Max window size 5
        uint8 count = 0;

        // Collect window elements
        for (j = -half_window; j <= half_window; j++) {
            int idx = i + j;
            if (idx >= 0 && idx < BOUNDARY_LINE_ROWS) {
                if (temp_right[idx] != 127) {  // Skip invalid values
                    values[count++] = temp_right[idx];
                }
            }
        }

        // Sort and find median
        if (count > 0) {
            // Simple bubble sort for small array
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
            right_boundary_line[i] = values[count / 2];  // Median
        }
    }
}

// Get boundary x-coordinate for a given row (0-31 internally, maps to y=32~63)
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

// Get current display mode
DisplayMode image_get_display_mode(void)
{
    return current_display_mode;
}

// Toggle display mode (cycles: BINARY -> BOUNDARY -> BOUNDARY_LINE -> BINARY)
void image_toggle_display_mode(void)
{
    switch (current_display_mode) {
        case DISPLAY_BINARY:       current_display_mode = DISPLAY_BOUNDARY;      break;
        case DISPLAY_BOUNDARY:      current_display_mode = DISPLAY_BOUNDARY_LINE; break;
        case DISPLAY_BOUNDARY_LINE: current_display_mode = DISPLAY_BINARY;       break;
    }
}

// Display current mode image to OLED
void image_display(void)
{
    uint16 x, y;

    // Clear display buffer
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));

    if (current_display_mode == DISPLAY_BOUNDARY_LINE) {
        // Draw boundary lines for bottom 32 rows (y=32~63)
        for (y = 0; y < BOUNDARY_LINE_ROWS; y++) {
            int actual_y = BOUNDARY_LINE_START_ROW + y;
            uint8 page = actual_y / 8;
            uint8 row_bit = actual_y % 8;

            // Draw left boundary point
            if (left_boundary_line[y] != 127) {
                x = left_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }

            // Draw right boundary point
            if (right_boundary_line[y] != 127) {
                x = right_boundary_line[y];
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }
        }
    } else {
        // Draw binary or boundary image
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

    // Copy to SSD1306 display buffer
    memcpy(ssd1306_getBuffer(), oled_display_buffer, sizeof(oled_display_buffer));

    // Full screen refresh
    ssd1306_updateScreen();
}