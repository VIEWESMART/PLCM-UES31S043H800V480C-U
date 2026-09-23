# 05_adc_buttons — 板载按键 SW3 到 SW6

[English](README.md)

检测丝印 SW3、SW4、SW5、SW6。不要按 BOOT，也不要点屏幕。

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

## 这个示例做什么

四个键共用一根 ADC（GPIO42）。没按键时电压接近 0。按下去电压升高，程序根据电压判断是哪一个键。

## 怎样算成功

复位后标签 `bsp_btn` 出现：

```
press KEY array SW3-SW6 (not BOOT / not touch), hold 1s each
adc raw=0 mv=0 (idle/N-sat)
```

`raw=0 mv=0 (idle/N-sat)` 表示当前没有键按下。

然后从左到右依次按 SW3、SW4、SW5、SW6，各按住约 1 秒再松开。每按一个键，日志应出现该键的名字，顺序不能串：

| 你按的键 | 日志里应出现 | 按下时电压大约 |
| --- | --- | --- |
| SW3 | `SW3 event=BUTTON_PRESS_DOWN` | 100–600 mV，实测约 270 mV |
| SW4 | `SW4 event=BUTTON_PRESS_DOWN` | 600–1080 mV，实测约 760 mV |
| SW5 | `SW5 event=BUTTON_PRESS_DOWN` | 1080–1605 mV，实测约 1240 mV |
| SW6 | `SW6 event=BUTTON_PRESS_DOWN` | 1605–2200 mV，实测约 1650 mV |

松开后还有 `BUTTON_PRESS_UP`，短按还会有 `BUTTON_SINGLE_CLICK`。松手后应回到 `adc raw=0 mv=0 (idle/N-sat)`。

`IoT Button Version: 4.2.1` 表示按键组件已加载。出现 `ADC calibration is not supported` **不是失败**，S31 上这行是预期提示，电压仍然会换算。

## 失败时长什么样

- 按 SW3 却打印 SW4（或其它错位）：阈值和实际电压对不上，把该行的 `mv=` 记下来。
- 怎么按都只有 `raw=0`：按到了 BOOT 或屏幕，或者按键阵列没有压到。
