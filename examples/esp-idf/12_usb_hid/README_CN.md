# 12_usb_hid — USB 触摸当鼠标

[English](README.md)

本示例把开发板当成 **USB 鼠标**。电脑通过板子上的 **USB 2.0 高速口** 识别设备。用法和普通鼠标一样：滑动只移动光标，单击点击，双击选中。滑动时不会按住左键，所以不会拖选文字。

调试口（CH340，例如 COM72）和 USB 鼠标口是 **两根线、两个口**，必须同时插着。

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

## 你需要准备什么

- 一块已烧录本示例的开发板。
- **第一根 USB 线**：接板子上的 CH340 调试口（烧录 / 看日志）。
- **第二根 USB 线**：接板子上的 **USB Type-A 设备口**（给电脑当鼠标用）。
- 一台 Windows 电脑（也可是 Linux / macOS，步骤类似）。

不要把两根线插到同一个口上。CH340 口不会变成鼠标。

---

## 第一步：烧录

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\12_usb_hid
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口应出现：

```
I (xxx) bsp_usb: USB HID mouse: slide=move, tap=click, double-tap=select
```

屏幕会亮。先不用碰屏幕。

---

## 第二步：把 USB 鼠标口插到电脑

1. 找到板子上的 **USB Type-A 母座**（不是 CH340 那个小 USB）。
2. 用一根 USB 线把它插到 **同一台电脑**。
3. Windows 右下角可能短暂弹出「正在设置设备」，**不要装驱动**，系统自带 HID。
4. 打开「设备管理器」→「鼠标和其他指针设备」，应多出一个 **HID-compliant mouse**（或「HID 兼容鼠标」）。

串口应出现：

```
I (xxx) bsp_usb: USB mounted as HID mouse
```

没这行 = 电脑还没枚举到板子。换口、换线，或再拔插一次 Type-A。

---

## 第三步：手指当鼠标

和普通鼠标一样：

1. **滑动**：按住屏幕拖动，只移动光标，**不会选中**文字或图标。
2. **单击**：手指点一下就抬起（几乎不滑动）= 鼠标左键点一下。
3. **双击**：连续点两下 = 选中单词 / 打开文件。

串口应出现：

```
I (xxx) bsp_usb: touch down x=... y=... usb=1
I (xxx) bsp_usb: tap (click)
I (xxx) bsp_usb: double-tap (select)
I (xxx) bsp_usb: touch up move=... px (cursor only)
```

`usb=1` 表示电脑已经认到鼠标。`usb=0` 时滑动只会打日志，光标不动。

---

## 怎样算成功

同时满足下面几条：

1. 日志出现 `USB mounted as HID mouse`。
2. 设备管理器能看到 HID 鼠标。
3. 手指滑动只移动光标，**不会拖选**文字。
4. 点一下 = 单击；连点两下 = 双击选中。

只亮屏、没插 Type-A，不算成功。

---

## 常见问题

**设备管理器没有新鼠标**  
CH340 口还在，但 Type-A 没插好。确认第二根线插的是板子 USB Type-A 口，不是调试口。

**日志有 `USB unmounted`**  
线松了，或电脑进休眠。重新插 Type-A。

**触摸有日志，光标不动**  
`usb=0`：电脑没认到设备。`usb=1` 仍不动：再确认设备管理器里的 HID 鼠标没有被禁用。

**屏幕有轻微闪**  
本示例只开 RGB 背光和触摸，不做 LVGL 动画，轻微闪不影响 HID 判定。
