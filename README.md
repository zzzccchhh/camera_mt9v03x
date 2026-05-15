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

### 显示模式切换

按 PE8 按键循环切换 5 种显示模式：

```
二值图 → 边界图 → 边界线 → 拟合直线 → 二值图 ...
```

### OLED 显示模式 (CAM_DEBUG_VIEW = 0)

使用 SSD1306 OLED (128x64) 显示处理后的图像。

通过 DEBUG_UART 输出道路中心偏差值 `Dev`，格式：
```
Dev=<偏差值>\r\n
```

- 正值：偏右
- 负值：偏左
- 0：边线无效或位于中心

偏差计算位置由 `DEVIATION_CALC_ROW` 宏定义（默认 row=8 对应 y=40）。

### PC 显示模式 (CAM_DEBUG_VIEW = 1)

通过 UART3 发送图像到 PC，使用 seekfree_assistant 逐飞助手显示。

1. 下载逐飞助手
2. 选择对应串口，波特率 921600
3. 点击连接，图像自动显示

## 图像处理流程

1. **Otsu 自动阈值**：每 10 帧计算一次最优阈值，带 70% 平滑收敛
2. **二值化** (`image_binarize`)：将原始图像缩放到 128x64 分辨率并二值化
3. **边界检测** (`extract_boundary`)：使用八邻域算法检测前景边界
4. **边界线提取** (`extract_boundary_line`)：下半部分 32 行，每行取左右边界点
5. **差分滤波** (`extract_boundary_line`)：去除跳变大于 4 的噪声点
6. **中值滤波** (`median_filter_boundary_line`)：滑动窗口滤波
7. **二次拟合** (`pre_fit_boundary_lines`)：最小二乘法拟合，剔除离群点，后再次拟合
8. **滑动平均滤波** (`fit_filter_boundary_lines`)：递推平均平滑

### 直线拟合模型

```
x = k * y + b
```

其中 x 为列坐标，y 为行坐标（下半部分 0~31 对应实际行 32~63）。

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `STEP_THRESHOLD` | 4 | 相邻边界点最大跳变 |
| `DEVIATION_THRESHOLD` | 4 | 离群点判定阈值 |
| `FIT_FILTER_WINDOW` | 3 | 滑动平均窗口 |
| `FIT_VALUE_DEVIATION` | 30 | 拒绝更新的偏差阈值 |
| `LOST_FRAME_THRESHOLD` | 3 | 丢线判定帧数 |
| `DEVIATION_CALC_ROW` | 8 | 偏差计算行（对应 y=40） |

## 边界线与拟合

- **边界线数组**：`left_boundary_line[32]`、`right_boundary_line[32]`
- **拟合结果**：`left_line_fit`、`right_line_fit`（含 k、b、valid_count）
- **平滑滤波**：`left_fit_filter`、`right_fit_filter`（含 k_smooth、b_smooth）

OLED 模式下通过 DEBUG_UART 输出道路中心偏差值 `Dev`。
PC 模式下通过 DEBUG_UART 输出拟合参数（Lk/Lb/Rk/Rb）及有效点计数（Lc/Rc）。

## 模块说明

| 文件 | 功能 |
|------|------|
| `src/main.c` | 主程序入口 |
| `src/otsu.c/h` | Otsu 自动阈值算法 |
| `src/image_process.c/h` | 图像处理（二值化、边界检测、直线拟合） |
| `src/oled_ssd1306/` | OLED SSD1306 驱动 |

## 目录结构

```
ch32v307/
├── src/              # 主程序
│   ├── main.c        # 主程序入口
│   ├── otsu.c/h      # Otsu自动阈值算法
│   ├── image_process.c/h  # 图像处理（二值化、边界、拟合）
│   └── oled_ssd1306/ # OLED SSD1306驱动
├── sdk/              # WCH SDK
├── zf_common/        # 通用组件
├── zf_driver/        # 底层驱动
├── zf_device/        # 设备驱动
├── zf_components/    # 应用组件
└── tools/            # 烧录配置
```