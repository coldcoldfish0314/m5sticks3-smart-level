# M5StickS3 Smart Level v1.1.0 烧录说明

## 新功能

- 新增 `DISTANCE` 测距页面。
- 支持外接 VL53L0X ToF 模块。
- 水平仪 UI 升级为更精致的仪表盘视觉。
- `B` 键现在切换：`LEVEL` -> `ANGLE` -> `TARGET` -> `DISTANCE`。

## 测距接线

```text
VL53L0X GND -> StickS3 GND
VL53L0X VIN -> StickS3 5V
VL53L0X SDA -> StickS3 G9
VL53L0X SCL -> StickS3 G10
```

## PlatformIO 烧录

打开项目文件夹后执行：

```powershell
pio run
pio run --target upload
```

## M5Burner / 手动烧录文件

使用这个合并固件：

```text
m5sticks3-smart-level-v1.1.0-merged.bin
```

手动分地址烧录时：

| 文件 | 地址 |
| --- | --- |
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `firmware.bin` | `0x10000` |

