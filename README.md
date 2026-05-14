# CH32V307 摄像头项目

基于 CH32V307VCT6 微控制器的 MT9V03X 摄像头图像采集与传输项目。通过 UART3 将摄像头图像发送至 PC 端显示软件（seekfree_assistant）。

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

### OLED 显示模式 (CAM_DEBUG_VIEW = 0)

使用 SSD1306 OLED (128x64) 显示二值化图像。支持两种阈值方式：

**Otsu 自动阈值模式：**
使用 Otsu 大津法自动计算最优阈值。根据图像灰度分布最大化类间方差，找到最佳分割点。

- 每 10 帧计算一次 Otsu 阈值，避免频繁计算开销
- 阈值变化带有平滑处理（70% 收敛），避免显示闪烁
- 算法使用 uint64 避免中间计算溢出

Otsu 算法位于 `src/otsu.c`

### PC 显示模式 (CAM_DEBUG_VIEW = 1)

通过 UART3 发送图像到 PC，使用 seekfree_assistant 逐飞助手显示。

1. 下载逐飞助手
2. 选择对应串口，波特率 921600
3. 点击连接，图像自动显示

## 目录结构

```
ch32v307/
├── src/              # 主程序
│   ├── main.c        # 主程序入口
│   ├── otsu.c/h      # Otsu自动阈值算法
│   └── oled_ssd1306/ # OLED SSD1306驱动
├── sdk/              # WCH SDK
├── zf_common/        # 通用组件
├── zf_driver/        # 底层驱动
├── zf_device/        # 设备驱动
├── zf_components/    # 应用组件
└── tools/            # 烧录配置
```