#include "otsu.h"

uint8 otsu_threshold(const uint8 *image, uint16 width, uint16 height)
{
    uint32 histogram[256] = {0};
    uint32 total = (uint32)width * height;

    // 计算直方图
    for (uint32 i = 0; i < total; i++) {
        histogram[image[i]]++;
    }

    // 计算总灰度和
    uint64 sum_total = 0;
    for (uint16 i = 0; i < 256; i++) {
        sum_total += (uint64)i * histogram[i];
    }

    // Otsu算法找最大类间方差
    uint32 w0 = 0;
    uint64 sum0 = 0;
    uint64 max_variance = 0;
    uint8 threshold = 0;

    for (uint8 t = 0; t < 256; t++) {
        w0 += histogram[t];
        if (w0 == 0) continue;

        uint32 w1 = total - w0;
        if (w1 == 0) break;

        sum0 += (uint64)t * histogram[t];

        uint64 mean0 = sum0 / w0;
        uint64 mean1 = (sum_total - sum0) / w1;

        int64 diff = (int64)mean0 - (int64)mean1;
        if (diff < 0) diff = -diff;

        // 使用uint64避免溢出: w0*w1*diff^2
        uint64 variance = (uint64)w0 * (uint64)w1 * (uint64)diff * (uint64)diff;

        if (variance > max_variance) {
            max_variance = variance;
            threshold = t;
        }
    }

    return threshold;
}