# 16_sd_music — SD 卡音乐播放器（小白教程）

[English](README.md)

本示例从 TF 卡播放 **MP3**，屏幕上有音乐播放器界面：上一曲 / 暂停 / 下一曲、音量、浏览目录。声音从板上喇叭出来。当前目录里的歌播完会 **自动下一首**（目录内循环）。

它 **不是** 视频播放器（那个是 `14` / `15`），也 **不是** 蓝牙音箱（那个是 `17_bt_audio`）。

---

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

- 一块 **UES31S043H800V480C-U** 开发板。
- 一根 USB 线：接 **CH340 调试口**（供电 + 烧录 + 看日志）。
- 一张 **FAT32** TF 卡，插在板子卡槽里。
- 卡上有 `.mp3`：可以放在 **根目录**，也可以放在 `music` 等子文件夹里。
- 电脑已装 ESP-IDF preview（芯片 `esp32s31`）。

喇叭在板子上。默认音量 50，可在界面里拖动滑条或点 `−` / `+`。

上电后会列出 `/sdcard`。根目录没有 MP3、但有 `music` 文件夹时，会自动进入 `music` 并播放第一首。

---

## 第一步：确认 TF 卡是 FAT32

1. 卡插到电脑读卡器。
2. 「此电脑」里找到盘符，右键 → **属性**。
3. 文件系统必须是 **FAT32**。若是 exFAT / NTFS：先备份，再格式化成 FAT32。

---

## 第二步：准备 MP3

1. 任意一首常见 MP3（128–320 kbps 都行）。不要用 wma / flac / 网易云加密 ncm。
2. 复制到 TF 卡：
   - 根目录，或
   - `music` 文件夹（推荐把歌集中放这里）。
3. 右键盘符 → **弹出**，再插回板子卡槽，推到位。

中文文件名可以显示。文件名里的空格、括号也能播。

---

## 第三步：接线上电

1. TF 卡已在板子上。
2. USB 只插 **CH340 调试口**。
3. 设备管理器里看 **USB-SERIAL CH340** 口，例如 `COM72`。下面的 `COMx` 改成你的口。

---

## 第四步：烧录并打开串口日志

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\16_sd_music
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口波特率 115200。成功时类似：

```
I (xxx) BSP: SD mounted at /sdcard
I (xxx) bsp_mp3: scan /sdcard count=...
I (xxx) bsp_mp3: play file:///sdcard/...mp3
I (xxx) music_ui: LVGL music UI ready
I (xxx) bsp_mp3: audio OK, listen to speaker
```

下面几行 **可以忽略**：

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
```

---

## 第五步：用屏幕操作

屏幕是横屏 800×480：

| 位置 | 做什么 |
|------|--------|
| 左侧列表 | 点文件夹进入；点 `..` 返回上一级；点 `.mp3` 播放 |
| 顶栏 | 当前目录路径 |
| 右侧曲名 | 正在播放的文件名 |
| `|<` / `>` | 上一曲 / 下一曲（当前目录内的 MP3） |
| 中间播放键 | 暂停 / 继续 |
| 右侧竖滑条 | 音量 0–100，上大下小 |

1. 把耳朵靠近板上喇叭，应能听出歌曲。
2. 点暂停，声音应停下；再点应继续。
3. 点左侧 `music`（如果有）应能进去看到里面的歌。

---

## 怎样算成功（请对照打勾）

下面 **都满足** 才算通过：

1. 屏幕出现深蓝色「音乐播放器」界面，左侧有文件/文件夹列表。
2. 喇叭能听出音乐。
3. 暂停、上一曲/下一曲、音量滑条有反应。
4. 能点进 `music` 等目录，并能点 `..` 返回。

---

## 常见问题（按现象查）

| 你看到的现象 | 多半原因 | 怎么处理 |
|--------------|----------|----------|
| 反复 `insert SD card` | 卡没插好，或不是 FAT32 | 重新插紧；电脑上看文件系统 |
| 列表是空的 | 这个目录没有文件夹也没有 `.mp3` | 把 MP3 拷进根目录或 `music` |
| 列表里有 `output.mp4` 但不显示 | 本示例只列出文件夹和 `.mp3` | 去测 `15_mp4_player` |
| 日志在播但听不到 | 音量被拖到很小，或耳朵离得远 | 把滑条拖到 50 以上；把板子拿近 |
| `player error` | 文件损坏或不是真 MP3 | 换一首普通 MP3 |
| 中文歌名显示方块 | 少见，字体未编进固件 | 重启；仍不行把文件改成英文名 |
| COM 口 busy / 找不到 | 上一个 monitor 没关，或 CH340 掉了 | 关掉旧窗口，拔插调试 USB |

---

## 客户验收一句话

> TF 卡放好普通 **MP3**（根目录或 `music` 文件夹），烧录后屏幕能浏览目录、能暂停/切歌/调音量，喇叭能听出音乐，即通过。
