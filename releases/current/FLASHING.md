# M5StickS3 Smart Level 固件保存与重新烧录说明

## 1. 最推荐的保存方式

请优先保存整个项目文件夹：

```text
C:\Users\zgb67\Documents\Codex\2026-05-24\vibecoding-m5sticks3
```

这个文件夹里包含：

- `platformio.ini`：PlatformIO 工程配置。
- `src/main.cpp`：固件源码。
- `README.md`：项目简介。
- `USER_GUIDE.md`：完整使用说明。
- `releases/current`：当前版本烧录产物备份。

只要这个文件夹还在，以后就可以用 VS Code + PlatformIO 重新编译和烧录。

## 2. 以后如何重新烧录

1. 打开 VS Code。
2. 选择 `File` -> `Open Folder...`。
3. 打开项目文件夹：

```text
C:\Users\zgb67\Documents\Codex\2026-05-24\vibecoding-m5sticks3
```

4. 插上 M5StickS3。
5. 点击左侧 PlatformIO 图标。
6. 选择：

```text
m5stack-sticks3 -> General -> Upload
```

看到 `[SUCCESS]` 就表示烧录成功。

## 3. 以后如何重新编译

如果你改了代码，先执行：

```text
m5stack-sticks3 -> General -> Build
```

编译通过后再执行：

```text
m5stack-sticks3 -> General -> Upload
```

## 4. 当前已备份的烧录文件

本目录保存了当前 v1.0.0 编译成功后的主要二进制文件：

- `bootloader.bin`
- `partitions.bin`
- `boot_app0.bin`
- `firmware.bin`
- `m5sticks3-smart-level-v1.0.0-merged.bin`

如果以后只是用 PlatformIO 烧录，其实不需要手动使用这些 `.bin` 文件；PlatformIO 会自动处理。

这些文件主要用于备份当前成功版本，或者将来用 ESP32 Flash Download Tool / esptool 进行手动烧录。

其中 `m5sticks3-smart-level-v1.0.0-merged.bin` 是已经合并好的完整固件包，适合用于 M5Burner 自定义固件上传或 ESP32 手动烧录工具。

## 5. 手动烧录地址参考

如果以后不用 PlatformIO，而是使用 ESP32 Flash Download Tool 或 esptool，常见地址如下：

| 文件 | 地址 |
| --- | --- |
| `bootloader.bin` | `0x0000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `firmware.bin` | `0x10000` |

通常仍然建议使用 PlatformIO 的 `Upload`，因为它会自动选择串口、烧录参数和芯片配置。

## 6. 建议额外备份到哪里

建议把整个项目文件夹复制一份到：

- 一个固定的代码目录。
- U 盘或移动硬盘。
- OneDrive / 坚果云 / 百度网盘等云盘。
- GitHub 私有仓库。

如果只备份 `.bin` 文件，将来可以恢复当前固件，但不方便修改功能。

如果备份整个项目文件夹，将来既能重新烧录，也能继续开发。

## 7. 当前版本

- 固件名称：M5StickS3 Smart Level
- 版本：v1.0.0
- 构建环境：PlatformIO
- 目标环境：`m5stack-sticks3`
