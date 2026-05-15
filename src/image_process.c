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

// Initialize image processing module
void image_process_init(void)
{
    memset(binary_image, 0, sizeof(binary_image));
    memset(boundary_image, 0, sizeof(boundary_image));
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));
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

// Get current display mode
DisplayMode image_get_display_mode(void)
{
    return current_display_mode;
}

// Toggle display mode
void image_toggle_display_mode(void)
{
    current_display_mode = (current_display_mode == DISPLAY_BINARY) ? DISPLAY_BOUNDARY : DISPLAY_BINARY;
}

// Display current mode image to OLED
void image_display(void)
{
    uint16 x, y;
    uint8 (*img)[DISPLAY_W] = (current_display_mode == DISPLAY_BINARY) ? binary_image : boundary_image;

    // Clear display buffer
    memset(oled_display_buffer, 0, sizeof(oled_display_buffer));

    // Build display buffer from selected image
    for (y = 0; y < DISPLAY_H; y++)
    {
        uint8 page = y / 8;
        uint8 row_bit = y % 8;

        for (x = 0; x < DISPLAY_W; x++)
        {
            if (img[y][x]) {
                oled_display_buffer[page * DISPLAY_W + x] |= (1 << row_bit);
            }
        }
    }

    // Copy to SSD1306 display buffer
    memcpy(ssd1306_getBuffer(), oled_display_buffer, sizeof(oled_display_buffer));

    // Full screen refresh
    ssd1306_updateScreen();
}