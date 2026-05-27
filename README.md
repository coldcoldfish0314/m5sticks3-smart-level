# M5StickS3 Smart Level

一个基于 M5StickS3 的小型智能工具固件。它使用内置 IMU 读取姿态，用屏幕显示气泡水平仪、数字角度和目标角度导航，并支持外接 VL53L0X ToF 模块进行测距。

## 功能

- 气泡水平仪：小球随 pitch / roll 移动，接近水平时变绿。
- 数字角度：大字显示 pitch 和 roll。
- 目标角度：可把 pitch 目标设为 -45 到 45 度，并显示调节方向。
- 当前姿态校准：把当前摆放状态设为 0 度，方便复制斜面或安装角度。
- 蜂鸣辅助：越接近目标越容易听出状态，可静音。
- 测距模式：支持外接 VL53L0X ToF 模块，显示 mm、cm、m 读数。
- 更精致的仪表盘 UI：深色背景、圆形水平仪、双轴微型刻度条。

## 操作

- `A`：校准当前姿态为 0 度。
- `B`：切换视图，气泡 / 数字 / 目标角度。
- `B`：切换视图，气泡 / 数字 / 目标角度 / 测距。
- 目标角度视图中长按 `A`：目标角度 -1 度。
- 目标角度视图中长按 `B`：目标角度 +1 度。
- 测距视图中短按 `A`：重新检测 ToF 模块。
- 同时长按 `A + B`：打开 / 关闭蜂鸣。

## 测距硬件

M5StickS3 本体没有内置测距传感器。测距模式需要外接 VL53L0X ToF 模块，默认接线：

```text
GND -> GND
5V  -> 5V
SDA -> G9
SCL -> G10
```

## 编译和烧录

本项目使用 PlatformIO：

```powershell
pio run
pio run --target upload
pio device monitor
```

如果使用 Arduino IDE，也可以新建一个 M5StickS3 工程，把 `src/main.cpp` 的内容复制为 `.ino`，并安装 `M5Unified` 和 `M5GFX` 库。

## 设计备注

`platformio.ini` 采用 M5Stack 官方文档推荐的 StickS3 ESP32-S3 DevKitC 配置思路：`esp32-s3-devkitc-1`、8MB 分区、`qio_opi` 内存类型，以及 M5Unified / Adafruit VL53L0X 驱动库。
