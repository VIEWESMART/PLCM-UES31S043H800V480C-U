# 11_bt_spp — 手机蓝牙串口（BLE UART）

[English](README.md)

本示例用 **低功耗蓝牙 BLE** 做无线串口回显。手机发出的文字，板子原样发回来，并打印到 USB 日志。

上一版用的是经典蓝牙 SPP。你的手机 `TT` 可以配对，但系统蓝牙打不开 SPP 串口（国产安卓很常见），所以会一直提示「连接失败」。现已改成手机能用的 **BLE UART**（Nordic UART，Serial Bluetooth Terminal 原生支持）。

上一个示例 `10_ble_gatt` 是遥控器调音量；本示例是收发文字。两份固件不能同时存在。

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

- 一块已烧录本示例的开发板，USB 接到电脑。
- 安卓或 iPhone 均可（这次是 BLE，苹果也能连）。
- 安卓请安装 **Serial Bluetooth Terminal**（Kai Morich）。
- iPhone 可用 **nRF Connect** 或同样支持 Nordic UART 的串口 App。

不要用系统蓝牙里的「连接」。系统蓝牙不会打开这个串口。

---

## 第一步：烧录

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\11_bt_spp
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口应出现：

```
I (xxx) bsp_ble_uart: advertising S31-BSP-UART — use Serial Bluetooth Terminal, Bluetooth LE
```

---

## 第二步：忘掉旧设备

在手机蓝牙设置里忘掉 `S31-BSP-SPP`、`S31-BSP-HID`、`S31-BSP-BLE`。那些是旧固件。

---

## 第三步：用 App 连上（安卓）

1. 打开 **Serial Bluetooth Terminal**（不要用手机自带的设置 → 蓝牙）。
2. 点左上角 **三条横线** → **Devices（设备）**。
3. 顶部选 **Bluetooth LE**（有的界面写成「低功耗 / BLE」）。**不要选 Classic。**
4. 下拉刷新，点 **`S31-BSP-UART`**。
5. 会回到黑色主界面。看右上角 **插头图标**：
   - 插头是断开的：再点一下插头，等它变成已连接。
   - 已经连上：不用再点。

到这里只是「连上了」。下面两步都在这张 **黑色聊天主界面**，不在设备列表里，也不在系统蓝牙里。

## 第四步：打开回显（Notify）

Serial Bluetooth Terminal 连上 Nordic UART 后，一般会自动打开通知。

请看黑色屏幕中间有没有一行：

`S31-BSP-UART echo ready`

- **有这行**：Notify 已经开了，直接做第五步。
- **没有这行**：点右上角 **三个点** → 先 **Disconnect**，再点插头重新连一次，再看有没有 `echo ready`。  
  电脑串口同时应出现 `notify on`。

不要去找名叫 Notify 的按钮，这个 App 没有单独的「打开通知」开关。

## 第五步：发送 hello

1. 确认你在 **黑色主界面**（能看到之前的日志，最下面有一条输入框）。
2. 如果被设备列表挡住了：点左上角返回，或再点一次三条横线外面的空白，回到主界面。
3. 点屏幕 **最底部** 的输入框（提示多半是 `Send` / `发送`）。
4. 输入英文 `hello`（不要加引号）。
5. 点输入框 **右边** 的发送键（纸飞机或箭头）。
6. 成功时：
   - 手机屏幕马上再出现一行 `hello`
   - 电脑串口出现 `rx` 和 `hello`

### iPhone（nRF Connect）

1. 打开 nRF Connect → 扫描 → 点 `S31-BSP-UART` 的 **Connect**。
2. 展开 **Nordic UART Service**。
3. 找到 **TX**（UUID 以 `...0003...` 结尾）→ 点左边 **向下的双箭头**，打开 Notify。应看到 `echo ready`。
4. 找到 **RX**（UUID 以 `...0002...` 结尾）→ 点向上箭头 **Write** → 类型选 **Text** → 输入 `hello` → Send。

---

## 怎样算成功

1. 串口有 `advertising S31-BSP-UART`。
2. App 用 **Bluetooth LE** 连上，收到 `S31-BSP-UART echo ready`。
3. 发送 `hello` 能回显，USB 有 `BLE connected` 和 `rx`。

---

## 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 系统蓝牙提示连接失败 | 系统不会打开 UART 服务 | 用 Serial Bluetooth Terminal 的 **LE** |
| App 里选了 Classic | 本固件是 BLE | 改成 **Bluetooth LE** |
| 还在搜 `S31-BSP-SPP` | 旧名字 | 搜 **`S31-BSP-UART`**，并忘掉旧设备 |
| 连上但没有回显 | 没打开 Notify | 断开重连，等看到 `echo ready` |
| 想调音量 | 那是 HID | 烧 `10_ble_gatt` |
| 想当音箱 | 那是 A2DP | 烧 `17_bt_audio` |

---

## 客户验收一句话

> 用 Serial Bluetooth Terminal 的 **Bluetooth LE** 连上 **`S31-BSP-UART`**，发出 `hello` 能原样收回，即通过。
