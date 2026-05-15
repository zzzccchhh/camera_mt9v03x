/*
 * Image Processing Module
 * Binary thresholding and boundary extraction for MT9V03X camera
 */

#ifndef __IMAGE_PROCESS_H__
#define __IMAGE_PROCESS_H__

#include "zf_common_headfile.h"

// Image dimensions for OLED display
#define DISPLAY_W   (128)
#define DISPLAY_H   (64)

// Boundary line processing constants (for bottom 32 rows: y=32~63)
#define BOUNDARY_LINE_START_ROW 32
#define BOUNDARY_LINE_ROWS      32
#define STEP_THRESHOLD          4   // Delta-x threshold for noise detection (3-5 pixels)

// Binary image array (row-major: [y][x], y=0-63, x=0-127)
extern uint8 binary_image[DISPLAY_H][DISPLAY_W];

// Boundary image array (row-major: [y][x], y=0-63, x=0-127)
extern uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// Boundary line arrays (left/right x-coordinate for each row in bottom 32 rows)
// Value 127 means invalid/no boundary detected
extern uint8 left_boundary_line[BOUNDARY_LINE_ROWS];
extern uint8 right_boundary_line[BOUNDARY_LINE_ROWS];

// Display mode enumeration
typedef enum {
    DISPLAY_BINARY,       // Display original binary image
    DISPLAY_BOUNDARY,      // Display boundary image
    DISPLAY_BOUNDARY_LINE // Display extracted boundary lines
} DisplayMode;

// Initialize image processing module
void image_process_init(void);

// Binarize camera image to 128x64 display resolution
void image_binarize(const uint8 *camera_img, uint16 img_w, uint16 img_h, uint8 threshold);

// Extract boundary using 8-neighborhood algorithm
void extract_boundary(void);

// Extract boundary lines from boundary_image (bottom 32 rows, y=32~63)
// Applies: row uniqueness constraint, differential noise filtering
void extract_boundary_line(void);

// Apply sliding window median filter to boundary lines
void median_filter_boundary_line(uint8 window_size);

// Get boundary x-coordinate for a given row (0-31 internally, maps to y=32~63)
uint8 get_left_boundary_x(uint8 row);
uint8 get_right_boundary_x(uint8 row);

// Get current display mode
DisplayMode image_get_display_mode(void);

// Toggle display mode (cycles: BINARY -> BOUNDARY -> BOUNDARY_LINE -> BINARY)
void image_toggle_display_mode(void);

// Display current mode image to OLED
void image_display(void);

#endif