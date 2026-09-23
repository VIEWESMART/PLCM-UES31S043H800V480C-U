# BSP verification apps

[中文](README_CN.md)

These projects use `bsp_ues31s043h800v480c_u` only. Each folder has `README.md` (English) and `README_CN.md` (Chinese) with the install steps and the log lines that mean success.

**ESP32-S31 can only use ESP-IDF master.** Stable releases such as 5.4 or 5.5 do not include this chip. Install Espressif EIM first (<https://dl.espressif.com/dl/eim/index.html>; in mainland China use **Download**, not GitHub), then install the **master** branch. VS Code plus the Espressif IDF extension can come before or after EIM. The full beginner steps, including how to tell the install worked, are at the top of every example README.

Toolchain check:

```powershell
idf.py --version
idf.py --preview --list-targets
```

`idf.py --version` must show a path that contains `master`. The target list must include `esp32s31`.

Flash from the example folder (`COMx` is the CH340 port):

```powershell
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Flash succeeded when the log shows **100%**, **Hash of data verified**, **Hard resetting via RTS pin...**, then `Project name:` plus that folder name. Press `Ctrl+]` to leave the monitor.

| Folder | What it checks | Guide |
|--------|----------------|-------|
| `01_display_touch` | LCD, GT911, backlight | [EN](01_display_touch/README.md) / [中文](01_display_touch/README_CN.md) |
| `02_audio_speaker` | ES8389 speaker | [EN](02_audio_speaker/README.md) / [中文](02_audio_speaker/README_CN.md) |
| `03_audio_mic` | Microphone recording to SD | [EN](03_audio_mic/README.md) / [中文](03_audio_mic/README_CN.md) |
| `04_sdcard` | TF / SDMMC | [EN](04_sdcard/README.md) / [中文](04_sdcard/README_CN.md) |
| `05_adc_buttons` | SW3–SW6 | [EN](05_adc_buttons/README.md) / [中文](05_adc_buttons/README_CN.md) |
| `06_uart1` | UART1 GPIO33/34 loopback | [EN](06_uart1/README.md) / [中文](06_uart1/README_CN.md) |
| `07_rs485` | RS485 GPIO35/36 | [EN](07_rs485/README.md) / [中文](07_rs485/README_CN.md) |
| `08_can` | CAN GPIO53/54 | [EN](08_can/README.md) / [中文](08_can/README_CN.md) |
| `09_wifi_sta` | Wi-Fi 6 station | [EN](09_wifi_sta/README.md) / [中文](09_wifi_sta/README_CN.md) |
| `10_ble_gatt` | BLE HID remote | [EN](10_ble_gatt/README.md) / [中文](10_ble_gatt/README_CN.md) |
| `11_bt_spp` | BLE UART echo | [EN](11_bt_spp/README.md) / [中文](11_bt_spp/README_CN.md) |
| `12_usb_hid` | USB HID mouse from touch | [EN](12_usb_hid/README.md) / [中文](12_usb_hid/README_CN.md) |
| `13_led_buzzer` | WS2812 + buzzer | [EN](13_led_buzzer/README.md) / [中文](13_led_buzzer/README_CN.md) |
| `14_avi_player` | SD AVI + JPEG + speaker | [EN](14_avi_player/README.md) / [中文](14_avi_player/README_CN.md) |
| `15_mp4_player` | SD MP4/MJPEG + AAC + speaker | [EN](15_mp4_player/README.md) / [中文](15_mp4_player/README_CN.md) |
| `16_sd_music` | SD MP3 + LVGL player | [EN](16_sd_music/README.md) / [中文](16_sd_music/README_CN.md) |
| `17_bt_audio` | Classic A2DP / HFP + LVGL | [EN](17_bt_audio/README.md) / [中文](17_bt_audio/README_CN.md) |
| `18_lvgl` | LVGL widgets + touch | [EN](18_lvgl/README.md) / [中文](18_lvgl/README_CN.md) |

Before flashing `09_wifi_sta`, edit `EXAMPLE_WIFI_SSID` and `EXAMPLE_WIFI_PASSWORD` in `main/main.c` to your own 2.4 GHz access point.

`17_bt_audio` needs this set before `idf.py`, and must not use `idf.py bmgr`:

```powershell
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.defaults.esp32s31.classic"
```

Clear that variable before building a BLE example. BLE (`10`, `11`) and Classic Bluetooth (`17`) must not be combined in one firmware. The phone name for `17` is `S31-BT-AUDIO`.
