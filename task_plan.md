# Plan: 八邻域边界扫描 + PE8切换显示

## Context
在已有的 `binary_image[64][128]` 二值化数组基础上，添加八邻域边界扫描功能，将边界检测结果存到新数组，并通过PE8按键切换显示原始二值图和边界图。

## 变更内容

### 关键文件
- `d:\CH32\camera\ch32v307\src\main.c`

### 实现步骤

#### 1. 新增边界图像数组 (第30行后)
```c
// 边界图像数组 (行优先: [y][x], y=0-63, x=0-127)
static uint8 boundary_image[64][128];
```

#### 2. 新增八邻域边界扫描函数
```c
// 八邻域边界检测：目标像素为1但周围有0则为边界
void extract_boundary(const uint8 binary_img[64][128], uint8 bound_img[64][128])
{
    int x, y;
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 128; x++) {
            if (binary_img[y][x] == 0) {
                bound_img[y][x] = 0;  // 背景保留为0
                continue;
            }
            // 八邻域检查：上、下、左、右、四个对角
            uint8 is_boundary = 0;
            // 上
            if (y > 0 && binary_img[y-1][x] == 0) is_boundary = 1;
            // 下
            if (y < 63 && binary_img[y+1][x] == 0) is_boundary = 1;
            // 左
            if (x > 0 && binary_img[y][x-1] == 0) is_boundary = 1;
            // 右
            if (x < 127 && binary_img[y][x+1] == 0) is_boundary = 1;
            // 左上
            if (y > 0 && x > 0 && binary_img[y-1][x-1] == 0) is_boundary = 1;
            // 右上
            if (y > 0 && x < 127 && binary_img[y-1][x+1] == 0) is_boundary = 1;
            // 左下
            if (y < 63 && x > 0 && binary_img[y+1][x-1] == 0) is_boundary = 1;
            // 右下
            if (y < 63 && x < 127 && binary_img[y+1][x+1] == 0) is_boundary = 1;

            bound_img[y][x] = is_boundary;
        }
    }
}
```

#### 3. 新增显示模式枚举和切换变量
```c
typedef enum {
    DISPLAY_BINARY,    // 显示原始二值图
    DISPLAY_BOUNDARY   // 显示边界图
} DisplayMode;

static DisplayMode current_display_mode = DISPLAY_BINARY;
```

#### 4. 修改 `oled_display_binary_scaled()` 函数参数
将 `const uint8 *image` 参数改为使用 `binary_image` 内部数组，新增 `display_mode` 参数控制显示源：
```c
void oled_display_binary_scaled_mode(uint8 display_mode)
{
    // 根据display_mode选择从binary_image或boundary_image构建显示缓冲
    ...
}
```

#### 5. PE8按键初始化和检测
在 `main()` 中初始化PE8为输入，循环检测按键状态进行模式切换。

### 修改位置索引
- 第30行：`boundary_image` 数组声明
- 第32行后：边界扫描函数
- 第106行附近：while循环内添加PE8检测和模式切换

## 验证方法
1. 编译无错误
2. PE8按下时能在原始二值图和边界图之间切换
3. 边界图正确标识出前景物体的边缘