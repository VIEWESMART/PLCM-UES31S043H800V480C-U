# 17_bt_audio（BSP 验证：经典蓝牙音乐）

[English](README.md)

本工程用 `bsp_ues31s043h800v480c_u` 初始化 **ES8389 + ST7262 + GT911**，上层仍是乐鑫 GMF `bt_audio`（A2DP Sink + HFP + AVRCP + LVGL）。

**不要**再执行 `idf.py bmgr`，也不要选 `s31_es8389` / `esp_board_manager`。

默认蓝牙名：`S31-BT-AUDIO`。这是经典蓝牙，不要和 BLE 固件混烧。

## 安装开发环境（第一次必读）

**目前 ESP32-S31 只能使用 ESP-IDF 的 master 版本。** 稳定版（5.4、5.5 等）里没有这颗芯片。选错版本后，目标列表里不会出现 `esp32s31`，后面所有示例都编译不了。请不要安装 release / v5.x 来做这块板。

安装顺序两种都可以，结果一样：

1. 先安装乐鑫的 **EIM**（ESP-IDF Installation Manager），用它安装 IDF **master**，再安装 VS Code 和 ESP-IDF 插件。
2. 先安装 VS Code 和 ESP-IDF 插件，再在插件里打开 EIM，安装 IDF **master**。

### 第一步：安装 EIM，并安装 IDF master

1. 打开乐鑫下载页：<https://dl.espressif.com/dl/eim/index.html>
2. 中国大陆请点页面上的 **Download**（乐鑫下载服务器）。不要从 GitHub 下安装包，国内经常下不完。
3. 运行安装包，按提示装完。结束时不应出现红色失败。
4. 在 EIM 里选择安装 **master**，不要选带版本号的稳定版。
5. 安装完成后，从开始菜单打开 **ESP-IDF PowerShell**（名字里应带 master）。

怎样算这一步成功：

- PowerShell 能打开，提示或路径里能看到 `master`。
- 依次输入下面两条命令，都能跑完：

```powershell
idf.py --version
idf.py --preview --list-targets
```

- `idf.py --version` 打出版本号，路径里包含 `master`（例如 `...\master\esp-idf`）。
- `idf.py --preview --list-targets` 的列表里有 **`esp32s31`**。

列表里没有 `esp32s31`，就是没装到 master。回到 EIM 改选 **master** 再装一次，不要继续用稳定版。

### 第二步：安装 VS Code 和 ESP-IDF 插件

1. 安装 VS Code：<https://code.visualstudio.com/>
2. 左侧扩展商店搜索 **Espressif IDF**（发布者是 Espressif Systems），点安装。
3. 按 `Ctrl+Shift+P`，运行 **ESP-IDF: Open ESP-IDF Installation Manager**。
4. 已经用 EIM 装过 master：把插件指向那个 master 目录。还没装：就在这里安装 **master**。

怎样算这一步成功：

- 窗口左下角状态栏出现 ESP-IDF，路径里带 `master`。
- 命令面板里能把芯片目标选成 **esp32s31**。
- 用 VS Code 打开本示例文件夹，点编译（Build），最后一行是 **Project build complete**。

也可以只用 PowerShell，不打开 VS Code。两种方式烧进去的程序一样。

### 第三步：烧录，并确认串口已经跑起来

1. USB 线插上板子。打开「设备管理器 → 端口（COM 和 LPT）」，找到 **USB-SERIAL CH340**，记下端口，例如 `COM72`。下面命令里的 `COMx` 改成你的端口。
2. 在 **本示例文件夹** 里打开 ESP-IDF PowerShell。不要在上一级 `bsp_verify` 目录里编译。
3. 执行：

```powershell
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

第一次会从网络下载组件，需要能访问 `components.espressif.com`，时间会比较长。串口波特率是 115200，`idf.py monitor` 已经按这个速率打开，不用再改。

所有示例烧录成功时，日志都先出现这三样：

- 进度到 **100%**，然后出现 **Hash of data verified**。
- 接着出现 **Hard resetting via RTS pin...**（板子被复位，开始跑新程序）。
- 监视器里出现 `Project name:`，后面是本示例的文件夹名。

然后再看本示例自己的日志和板子上的现象，见下一节。按 `Ctrl+]` 退出监视器。

若下文已经写了本示例专用的烧录命令，以那个为准。

常见失败：

| 你看到的 | 原因 | 怎么处理 |
| --- | --- | --- |
| 目标列表没有 `esp32s31` | 装的不是 master，或命令忘了 `--preview` | 用 EIM 重装 master，命令加上 `--preview` |
| 端口打不开、烧录失败 | 端口号错了，或监视器还开着 | 关掉监视器，设备管理器里重新确认 COM 口 |
| 关掉监视器后端口消失 | CH340 的常见现象 | 拔插 USB，等端口重新出现 |
| 下载组件失败 | 网络访问不了组件仓库 | 检查网络后在同一目录再执行一次 |

本示例自己的操作和「怎样算成功」，从下一节开始。

本示例是经典蓝牙。烧录前必须先设置 `SDKCONFIG_DEFAULTS`，命令以下文「编译 / 烧录」为准。不要只执行上面两条通用命令，也不要执行 `idf.py bmgr`。

## 编译 / 烧录

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
cd E:\idf-debuging\02_s31_examples\bsp_verify\17_bt_audio
$env:SDKCONFIG_DEFAULTS = "sdkconfig.defaults;sdkconfig.defaults.esp32s31.classic"
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

## 怎么测

1. 开机后屏上应有 LVGL 播放 / 拨号界面。
2. 手机蓝牙搜索并连接 `S31-BT-AUDIO`（后面可能带 MAC 后缀）。
3. 放歌，喇叭应出声；可用屏幕按钮或串口 `play` / `pause` / `vol_set 70`。
4. 通话走板载 MIC（ES8389 ADC）。

串口里还应看到：

- `RGB LCD 800x480 RGB888 ready (2 FB, pclk 18 MHz, bounce 20)`：屏幕已按 2 块帧缓冲启动。
- 手机连上之后：`Connection state changed: Connected`。
- 手机播放音乐时，喇叭出声。搜不到名字时，先在手机里忽略旧配对，再搜 `S31-BT-AUDIO`。这是经典蓝牙，不是 BLE。

本示例不强制插 TF 卡。AVI / MP4 / MP3 请用 `14_avi_player`、`15_mp4_player`、`16_sd_music`。
