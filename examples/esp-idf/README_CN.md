# BSP 验证示例

[English](README.md)

这些工程只用 `bsp_ues31s043h800v480c_u`。每个文件夹都有中文 `README_CN.md` 和英文 `README.md`，开头是安装步骤，后面写了成功时串口和板子上应看到的现象。

**目前 ESP32-S31 只能使用 ESP-IDF master。** 稳定版（5.4、5.5 等）里没有这颗芯片，选错后目标列表不会出现 `esp32s31`。请先安装乐鑫 EIM：<https://dl.espressif.com/dl/eim/index.html>。中国大陆点 **Download**（下载服务器），不要从 GitHub 下安装包。在 EIM 里安装 **master**。VS Code 和 ESP-IDF 插件可以先装，也可以后装，两种顺序都可以。每个示例 README 的开头有完整的小白步骤，包括怎样判断安装成功。

工具链是否装对，看这两条：

```powershell
idf.py --version
idf.py --preview --list-targets
```

`idf.py --version` 的路径里要有 `master`。目标列表里要有 `esp32s31`。

在**该示例自己的文件夹**里烧录（`COMx` 换成设备管理器里 CH340 的端口）：

```powershell
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

烧录成功的共同标志：进度 **100%**，接着 **Hash of data verified**，然后 **Hard resetting via RTS pin...**，监视器里出现 `Project name:` 加本示例文件夹名。按 `Ctrl+]` 退出监视器。关掉监视器后端口有时会消失，拔插 USB 即可。

| 目录 | 验证内容 | 说明 |
|------|----------|------|
| `01_display_touch` | 屏幕、GT911、背光 | [中文](01_display_touch/README_CN.md) / [EN](01_display_touch/README.md) |
| `02_audio_speaker` | 喇叭 | [中文](02_audio_speaker/README_CN.md) / [EN](02_audio_speaker/README.md) |
| `03_audio_mic` | 麦克风录音到 SD | [中文](03_audio_mic/README_CN.md) / [EN](03_audio_mic/README.md) |
| `04_sdcard` | TF 卡 | [中文](04_sdcard/README_CN.md) / [EN](04_sdcard/README.md) |
| `05_adc_buttons` | 按键 SW3–SW6 | [中文](05_adc_buttons/README_CN.md) / [EN](05_adc_buttons/README.md) |
| `06_uart1` | UART1 GPIO33/34 回环 | [中文](06_uart1/README_CN.md) / [EN](06_uart1/README.md) |
| `07_rs485` | RS485 GPIO35/36 | [中文](07_rs485/README_CN.md) / [EN](07_rs485/README.md) |
| `08_can` | CAN GPIO53/54 | [中文](08_can/README_CN.md) / [EN](08_can/README.md) |
| `09_wifi_sta` | Wi-Fi 6 站点 | [中文](09_wifi_sta/README_CN.md) / [EN](09_wifi_sta/README.md) |
| `10_ble_gatt` | BLE HID 遥控器 | [中文](10_ble_gatt/README_CN.md) / [EN](10_ble_gatt/README.md) |
| `11_bt_spp` | BLE 串口回显 | [中文](11_bt_spp/README_CN.md) / [EN](11_bt_spp/README.md) |
| `12_usb_hid` | 触摸当 USB 鼠标 | [中文](12_usb_hid/README_CN.md) / [EN](12_usb_hid/README.md) |
| `13_led_buzzer` | WS2812 + 蜂鸣器 | [中文](13_led_buzzer/README_CN.md) / [EN](13_led_buzzer/README.md) |
| `14_avi_player` | SD 卡 AVI | [中文](14_avi_player/README_CN.md) / [EN](14_avi_player/README.md) |
| `15_mp4_player` | SD 卡 MP4 | [中文](15_mp4_player/README_CN.md) / [EN](15_mp4_player/README.md) |
| `16_sd_music` | SD 卡 MP3 + 界面 | [中文](16_sd_music/README_CN.md) / [EN](16_sd_music/README.md) |
| `17_bt_audio` | 经典蓝牙音乐 / 通话 | [中文](17_bt_audio/README_CN.md) / [EN](17_bt_audio/README.md) |
| `18_lvgl` | LVGL 控件 + 触摸 | [中文](18_lvgl/README_CN.md) / [EN](18_lvgl/README.md) |

烧录 `09_wifi_sta` 之前，先改 `main/main.c` 里的 `EXAMPLE_WIFI_SSID` 和 `EXAMPLE_WIFI_PASSWORD`，改成你自己的 2.4 GHz 路由器。源码里原来的账号不是给你用的。

`17_bt_audio` 烧录前要设置下面这行，并且不要执行 `idf.py bmgr`：

```powershell
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.defaults.esp32s31.classic"
```

接着去编译 BLE 示例（`10`、`11`）之前，把这个变量清掉。BLE 和经典蓝牙（`17`）不能做进同一份固件。`17` 的手机搜索名是 `S31-BT-AUDIO`。
