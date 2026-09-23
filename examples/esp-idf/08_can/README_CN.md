# 08_can — CAN / TWAI 验证

[English](README.md)

用 BSP `bsp_can_init()` 打开片上 TWAI + 板上 **SIT1050T**。默认 **500 kbit/s**、经典 CAN（不要开 CAN FD）。

默认 `EXAMPLE_CAN_SELF_TEST=1`：不要求对端 ACK，并打开控制器 **loopback**。单板能看到自己的回读；两板 / USB-CAN 还能看到对端。

发送 ID 按 Wi-Fi MAC 最后一字节自动分配，同一份固件可烧两块板：

| MAC 最后一字节 | 发送 ID |
|----------------|---------|
| `< 0x50`（例如 `…:39:3c`） | `0x123` |
| `≥ 0x50`（例如 `…:39:66`） | `0x321` |

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

## 硬件

| 信号 | 位置 |
|------|------|
| CAN_TX | GPIO53 |
| CAN_RX | GPIO54 |
| CAN_H / CAN_L / GND | 端子 `CAN_H` / `CAN_L` / `CAN_GND` |

板上已有 120 Ω 终端。测的是 **CAN_H / CAN_L / GND**，不是 USB 调试口，也不能用 USB-RS485 或 USB-TTL。**不要**把 GPIO53/54 或 `CAN_TX`/`CAN_RX` 短接。

## 编译烧录

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\08_can
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

监视器 115200。两块板就开两个监视器（两个 COM 口）。

任选下面一种测法即可。方法一够快速自检；方法二或三才算总线 / 收发器正式通过。

---

## 方法一：单板自环回（不接线）

不接 CAN 端子。烧录后看 USB 日志。

### 怎样算成功

启动后应有：

```
I (xxx) BSP: CAN TX=53 RX=54 bitrate=500000 self_test=1 loopback=1
I (xxx) bsp_can: mac xx:xx:xx:xx:xx:xx tx_id=0x123 period … self_test=1
I (xxx) bsp_can: tx id=0x123 n=1
I (xxx) bsp_can: rx self id=0x123 n=1
```

**通过条件（同时满足）：**

1. `self_test=1 loopback=1`
2. 周期性出现 `tx id=0x123` 或 `tx id=0x321`（不要 `tx failed` / `bus-off`）
3. 每次 `tx` 后立刻有同 ID、同 `n` 的 `rx self`

只证明控制器自环回和固件在跑，**不证明** SIT1050 和 CAN 端子。要验 PHY，用方法二或三。

---

## 方法二：两块同样的板子互测（推荐，无需 USB-CAN）

### 接线（先断电）

```
板 A                         板 B
----                         ----
CAN_H  -------------------  CAN_H
CAN_L  -------------------  CAN_L
GND    -------------------  GND
```

- 只接这三根。H 接 H、L 接 L，必须共地。
- 两块板都有 120 Ω，一对板正好两端各一只终端。
- 两块都用 USB 供电，并各接一根 USB 看日志。
- 接反则两边都只有 `tx` / `rx self`，没有 `rx peer`。

### 烧同一份固件

两块都烧本工程。MAC 不同则发送 ID 不同，日志一眼能分清谁发的。

```powershell
idf.py --preview -p COM72 flash
idf.py --preview -p COM3 flash
idf.py --preview -p COM72 monitor
idf.py --preview -p COM3 monitor
```

（COM 口按设备管理器改。）

### 怎样算成功

板 A（`tx_id=0x123`，例如 MAC `…:39:3c`）：

```
I (xxx) bsp_can: tx id=0x123 n=1
I (xxx) bsp_can: rx self id=0x123 n=1
I (xxx) bsp_can: rx peer id=0x321 n=1
```

板 B（`tx_id=0x321`，例如 MAC `…:39:66`）：

```
I (xxx) bsp_can: tx id=0x321 n=1
I (xxx) bsp_can: rx self id=0x321 n=1
I (xxx) bsp_can: rx peer id=0x123 n=1
```

**通过条件（同时满足）：**

1. 两边都持续 `tx`，没有反复 `bus-off`
2. 两边都有自己的 `rx self`（自环回仍在）
3. **两边都出现对方的 `rx peer`**，且 `n` 递增

第 3 条成立，即 SIT1050 + CAN_H/L 双向通了。这是没有分析仪时的正式通过标准。

两块 MAC 若碰巧落在同一侧（都 `< 0x50` 或都 `≥ 0x50`），两边会发同一个 ID。此时仍看 `rx peer`：对端的 `n` 和本机当前 `tx … n=` 对不上，同样算成功。

### 方法二翻车对照

| 现象 | 多半原因 | 怎么处理 |
|------|----------|----------|
| 只有 `tx` + `rx self`，没有 `rx peer` | H/L 接反，或没共地，或只烧了一块 | 对调一端 H/L；加 GND；确认两块都是本固件 |
| 只有一块有 `rx peer` | 看错 COM，或另一块没跑起来 | 两个监视器对上两块板 |
| `tx failed` / `bus-off` | 把 `EXAMPLE_CAN_SELF_TEST` 改成了 `0` 且对端未就绪 | 保持为 `1`，或等两边都起来后再复位 |

---

## 方法三：USB-CAN 工具

一块板 + USB-CAN 适配器。电脑开两个窗口：USB 看板子日志，上位机看总线帧。

CAN **不是串口**。不要用串口助手 115200 去开 USB-CAN，也不要用测 485 的 USB-RS485。

### 接线（先断电）

```
开发板                USB-CAN
------                -------
CAN_H  ------------->  CAN_H（或 CANH / H）
CAN_L  ------------->  CAN_L（或 CANL / L）
GND    ------------->  GND
```

板上已有 120 Ω。适配器若有 120 Ω 拨码，一对设备、线短时建议打开。

### 上位机

| 项目 | 填什么 |
|------|--------|
| 波特率 | **500 kbit/s** |
| 模式 | 经典 CAN / CAN 2.0，标准帧。**不要开 CAN FD** |
| 工作方式 | 正常发送（不要只用 listen-only） |

板子发送 ID 看启动日志里的 `tx_id=`（`0x123` = 十进制 291，`0x321` = 801）。

#### 测试 A：电脑能不能收到板子

上位机周期性出现该 ID、DLC=4、数据递增（`00 00 00 01`、`00 00 00 02`…）。看到即 **板子 → 总线 → USB-CAN** 通了。

#### 测试 B：板子能不能收到电脑

上位机发一帧（ID 不要和板子自己的 `tx_id` 相同，例如板子是 `0x123` 就发 `0x321`），DLC 任意。USB 日志应出现：

```
I (xxx) bsp_can: rx peer id=0x321 n=…
```

**通过条件：** A、B 都过。

### 方法三翻车对照

| 现象 | 多半原因 | 怎么处理 |
|------|----------|----------|
| 板子有 `tx`，上位机空白 | H/L 接反，或不是 500K | 对调 H/L；改成 500 kbit/s |
| 上位机有帧但 ID 不是 123/321 | 软件用十进制 | `0x123`=291，`0x321`=801 |
| 能看到板子帧，发送后没有 `rx peer` | 只听模式，或发成扩展帧 / CAN FD | 正常模式；标准帧；经典 CAN |
| 用 USB-RS485 / USB-TTL 接 CAN_H/L | 协议不对 | 必须用 USB-**CAN** |

---

## 可选：改成标准 ACK

测通后把 `main/main.c` 里 `EXAMPLE_CAN_SELF_TEST` 改成 `0`，重新编译烧录。此时不再 loopback，日志里没有 `rx self`；总线上必须有另一个节点（另一块板或 USB-CAN **正常模式**）给 ACK，否则可能 `tx failed` / `bus-off`。给客户出厂自检建议保持 `1`。
