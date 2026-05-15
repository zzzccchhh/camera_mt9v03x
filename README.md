# CH32V307 摄像头项目

基于 CH32V307VCT6 微控制器的 MT9V03X 摄像头图像采集与显示项目。支持 OLED 本地显示和 PC 端显示两种模式。

## 硬件连接

### Debug UART (图像传输)
| 功能 | 引脚 | 描述 |
|------|------|------|
| TX | B10 | 发送图像数据到 PC |
| RX | B11 | 接收调试命令 |

### 摄像头配置 UART5
| 功能 | 引脚 | 说明 |
|------|------|------|
| TX | C12 | 摄像头接收配置 |
| RX | D2 | 芯片接收摄像头响应 |

### DVP 摄像头接口
| 信号 | 引脚 | 说明 |
|------|------|------|
| D0 | A9 | 数据位0 |
| D1 | A10 | 数据位1 |
| D2 | C8 | 数据位2 |
| D3 | C9 | 数据位3 |
| D4 | C11 | 数据位4 |
| D5 | B6 | 数据位5 |
| D6 | B8 | 数据位6 |
| D7 | B9 | 数据位7 |
| PCLK | A6 | 像素时钟 |
| VSYNC | A5 | 垂直同步 |
| HSYNC | A4 | 行同步 |

### 其他
| 功能 | 引脚 | 说明 |
|------|------|------|
| LED1 | A15 | 运行指示灯 |
| PE8 | E8 | 显示模式切换按键 |
| SCL | D2 | IIC 时钟（与 UART5_RX 共用） |
| SDA | C12 | IIC 数据（与 UART5_TX 共用） |

### OLED SSD1306 (4线软件SPI)
| OLED引脚 | GPIO   | 管脚  | 说明   |
|----------|--------|-------|--------|
| SCL      | GPIOE  | PE11  | SPI时钟 |
| SDA      | GPIOE  | PE13  | SPI数据  |
| RST      | GPIOE  | PE15  | 复位    |
| DC       | GPIOD  | PD9   | 数据/命令 |
| CS       | GPIOD  | PD11  | 片选    |

OLED驱动位于 `src/oled_ssd1306/`

## 编译烧录

使用 VSCode + EIDE 插件。

## 显示模式

通过 `CAM_DEBUG_VIEW` 宏切换：

- `CAM_DEBUG_VIEW = 0`：OLED 显示模式
- `CAM_DEBUG_VIEW = 1`：PC 显示模式（通过 UART3）

### OLED 显示模式 (CAM_DEBUG_VIEW = 0)

使用 SSD1306 OLED (128x64) 显示二值化图像或边界图。按 PE8 按键切换显示模式。

**图像处理流程：**

1. **二值化** (`image_binarize`)：将原始图像缩放到 128x64 分辨率并二值化
2. **边界检测** (`extract_boundary`)：使用八邻域算法检测前景边界
3. **显示** (`image_display`)：根据当前模式显示二值图或边界图

**Otsu 自动阈值：**
使用 Otsu 大津法自动计算最优阈值。根据图像灰度分布最大化类间方差，找到最佳分割点。

- 每 10 帧计算一次 Otsu 阈值，避免频繁计算开销
- 阈值变化带有平滑处理（70% 收敛），避免显示闪烁
- 算法使用 uint64 避免中间计算溢出

**边界检测算法 (八邻域)：**
目标像素为前景（1）且周围 8 邻域中存在背景（0）时，标记为边界点。

```
  p[-1,-1] p[0,-1] p[1,-1]
  p[-1, 0]    P    p[1, 0]
  p[-1, 1] p[0, 1] p[1, 1]
```

**PE8 按键切换：**
按下 PE8（低电平）在原始二值图和边界图之间切换显示。

### PC 显示模式 (CAM_DEBUG_VIEW = 1)

通过 UART3 发送图像到 PC，使用 seekfree_assistant 逐飞助手显示。

1. 下载逐飞助手
2. 选择对应串口，波特率 921600
3. 点击连接，图像自动显示

## 模块说明

| 文件 | 功能 |
|------|------|
| `src/main.c` | 主程序入口 |
| `src/otsu.c/h` | Otsu 自动阈值算法 |
| `src/image_process.c/h` | 图像二值化、边界检测、显示控制 |
| `src/oled_ssd1306/` | OLED SSD1306 驱动 |

### image_process 模块接口

```c
// 初始化
void image_process_init(void);

// 二值化（缩放到128x64）
void image_binarize(const uint8 *camera_img, uint16 img_w, uint16 img_h, uint8 threshold);

// 八邻域边界检测
void extract_boundary(void);

// 显示模式切换
DisplayMode image_get_display_mode(void);
void image_toggle_display_mode(void);

// 显示到OLED
void image_display(void);
```

## 目录结构

```
ch32v307/
├── src/              # 主程序
│   ├── main.c        # 主程序入口
│   ├── otsu.c/h      # Otsu自动阈值算法
│   ├── image_process.c/h  # 图像处理模块（二值化、边界检测）
│   └── oled_ssd1306/ # OLED SSD1306驱动
├── sdk/              # WCH SDK
├── zf_common/        # 通用组件
├── zf_driver/        # 底层驱动
├── zf_device/        # 设备驱动
├── zf_components/    # 应用组件
└── tools/            # 烧录配置
```