# 10_ble_gatt — BLE HID 遥控器（控制手机音量）

[English](README.md)

这份示例把开发板变成一只 **可以配对的蓝牙遥控器**。配对成功后，用板上的 4 个 ADC 按键就能调手机音量、静音、播放。

它不是“只能被搜到名字”的广播，也不是 nRF Connect 那种调试连接。必须用手机 **系统蓝牙设置** 配对，才会变成系统认可的 HID 遥控器。

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

- 一块已烧录本示例的开发板，USB 接到电脑（用来供电、看日志）。
- 一部打开蓝牙的手机（安卓或 iPhone 都可以）。
- 手机最好先打开任意音乐 App，或至少能看到系统音量条，方便确认音量有没有变。

**不需要** 另接耳机到开发板。音量改的是 **手机自己的喇叭/耳机**，不是板上喇叭。

## 按哪些键（请认准丝印）

板上有多个按键。本示例只用 **SW3、SW4、SW5、SW6** 这一排 ADC 键。

| 丝印 | 作用 | 成功时手机上会发生什么 |
|------|------|------------------------|
| SW3 | 音量减 | 系统音量条下降一格（或连续下降） |
| SW4 | 音量加 | 系统音量条上升一格（或连续上升） |
| SW5 | 静音 | 音量变成静音 / 取消静音（看手机） |
| SW6 | 播放 | 正在播的音乐会播放或暂停（没开音乐时可能没反应） |

**不要按这些：**

- **BOOT**：那是下载/启动键，不是音量键。
- **屏幕 / 触摸**：本示例不用触摸。
- 复位键：会让板子重新广播，手机可能要再连一次。

第一次按某一颗键时，请 **按住大约 1 秒**，松手后再试短按。这是 ADC 按键去抖，不是坏了。

---

## 第一步：烧录并确认板子已经在广播

电脑装好 ESP-IDF（本仓库用 preview / `esp32s31`）。USB 线插上后，在设备管理器里看串口号（例如 `COM72`），下面命令里的 `COMx` 改成你的口。

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\10_ble_gatt
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口波特率 115200。烧完复位后，日志里应陆续出现类似内容：

```
I (xxx) bsp_ble_hid: SW3 vol-  SW4 vol+  SW5 mute  SW6 play
I (xxx) bsp_ble_hid: HID GAP mode=1
I (xxx) bsp_ble_hid: HID start, advertising S31-BSP-HID
```

看到 **`advertising S31-BSP-HID`**，说明板子已经在对外广播，手机现在可以搜到它。  
如果还停留在旧名字 `S31-BSP-BLE`，说明没烧到本固件，请重新 `flash`。

本固件只开 **BLE**。不要和 `11_bt_spp`、`17_bt_audio`（经典蓝牙）混烧到同一块板。

---

## 第二步：手机忘掉旧设备（很重要）

如果你以前测过旧版示例，手机里可能还记着 **`S31-BSP-BLE`**。那个版本 **只能被搜到，不能配对**，留着会干扰。

1. 打开手机 **设置 → 蓝牙**（有的安卓在 **设置 → 已连接的设备**）。
2. 找到 `S31-BSP-BLE` 或以前连过的同名设备。
3. 点旁边的齿轮 / `i` / “忽略此设备” / “取消配对” / “忘掉此设备”。
4. 等列表里没有旧名字再继续。

iPhone：设置 → 蓝牙 → 点设备右边的 `i` → **忽略此设备**。

---

## 第三步：用系统蓝牙配对（不要用 App）

### 安卓（大同小异）

1. 打开 **设置 → 蓝牙**，打开蓝牙开关。
2. 下拉刷新或等几秒，在“可用设备”里找 **`S31-BSP-HID`**。
3. 点这个名字。不要点 nRF Connect、LightBlue 等蓝牙调试 App 里的“Connect”。
4. 如果弹出 **配对码 / 是否配对**：点 **配对** 或 **同意**。板子会自动确认，你不用在开发板上输入数字。
5. 配对成功后，设备会跑到“已配对 / 已连接”列表，名字仍是 `S31-BSP-HID`。有的手机显示成“输入设备 / 遥控器 / HID”。

### iPhone

1. 打开 **设置 → 蓝牙**。
2. 在“其他设备”里点 **`S31-BSP-HID`**。
3. 弹出配对请求就点 **配对**。
4. 成功后它会出现在“我的设备”里，状态为 **已连接**。

配对时电脑串口应出现：

```
I (xxx) ESP_HID_GAP: BLE GAP AUTH SUCCESS
I (xxx) bsp_ble_hid: HID connected, press SW3-SW6
```

这两行是“配对 + 当成遥控器连上了”的标志。  
只有手机里出现了名字、但串口没有 `HID connected`，还不能算成功。

---

## 第四步：按键控制手机

1. 保持蓝牙连着，不要把手机拿到太远。
2. 打开任意音乐（网易云、QQ 音乐、系统自带播放器都可以），或下拉控制中心看音量条。
3. 按住 **SW4 约 1 秒**（音量加），再按 **SW3**（音量减）。
4. 看手机音量条是否跟着动。
5. 再试 SW5（静音）、SW6（播放/暂停）。SW6 要在有媒体播放时才明显。

按键时串口应出现：

```
I (xxx) bsp_ble_hid: SW4 down key=233 connected=1
I (xxx) bsp_ble_hid: SW4 up key=233 connected=1
```

`connected=1` 表示板子认为已经连上手机。如果是 `connected=0`，音量不会变，请回到第三步重新配对。

---

## 怎样算成功（请对照打勾）

下面 **4 条都满足** 才算本示例通过：

1. **烧录后** 串口有 `HID start, advertising S31-BSP-HID`。
2. **手机系统蓝牙** 能搜到并配对 `S31-BSP-HID`，状态变成已连接（不是只看到名字却点不开）。
3. **串口** 出现 `BLE GAP AUTH SUCCESS` 和 `HID connected`。
4. **按 SW3 / SW4**，手机系统音量条会下降 / 上升。

SW5、SW6 作为加分项：静音和播放取决于手机当前界面，个别机型可能只认音量、不认播放，**音量加减有效即可判定硬件和协议通过**。

---

## 常见问题（按现象查）

| 你看到的现象 | 多半原因 | 怎么处理 |
|--------------|----------|----------|
| 手机只能搜到 `S31-BSP-BLE`，点配对没反应或立刻失败 | 还是旧固件，或没忘掉旧设备 | 重新烧本工程；在手机里忘掉 `S31-BSP-BLE` |
| 搜不到任何名字 | 板子没起来，或离太远 | 看串口有没有 `advertising`；USB 供电；靠近再扫 |
| nRF Connect 能连，但按键不能调音量 | 调试 App 的连接 **不是** HID 配对 | 断开 App，改用 **设置 → 蓝牙** 配对 |
| 配对框一闪就断开 | 旧绑定残留，或同时被两个手机连 | 两边都忘掉设备，只留一部手机配对；板子按复位后再配 |
| 已连接，但按键没反应 | 按错键，或第一次太短，或 `connected=0` | 认准 SW3–SW6；按住约 1 秒；看串口 `connected=` |
| 音量键有效，SW6 没反应 | 手机没在播媒体 | 先打开音乐再按 |
| 想连电脑当音箱听歌 | 这是遥控器，不是音箱 | 听音乐请用 `17_bt_audio`（和本固件不能共存） |
| 烧完后经典蓝牙示例也搜不到 | BLE 和经典蓝牙不能同一份固件 | 测 SPP / 蓝牙音乐时再烧对应工程 |

---

## 客户验收一句话

> 用手机自带的蓝牙设置搜到 **S31-BSP-HID** 并配对成功，按板上 **SW4 音量变大、SW3 音量变小**，即通过。
