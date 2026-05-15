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

// Binary image array (row-major: [y][x], y=0-63, x=0-127)
extern uint8 binary_image[DISPLAY_H][DISPLAY_W];

// Boundary image array (row-major: [y][x], y=0-63, x=0-127)
extern uint8 boundary_image[DISPLAY_H][DISPLAY_W];

// Display mode enumeration
typedef enum {
    DISPLAY_BINARY,    // Display original binary image
    DISPLAY_BOUNDARY   // Display boundary image
} DisplayMode;

// Initialize image processing module
void image_process_init(void);

// Binarize camera image to 128x64 display resolution
void image_binarize(const uint8 *camera_img, uint16 img_w, uint16 img_h, uint8 threshold);

// Extract boundary using 8-neighborhood algorithm
void extract_boundary(void);

// Get current display mode
DisplayMode image_get_display_mode(void);

// Toggle display mode
void image_toggle_display_mode(void);

// Display current mode image to OLED
void image_display(void);

#endif