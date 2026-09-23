# 15_mp4_player — SD 卡播 MP4（小白教程）

[English](README.md)

本示例从 TF 卡播放 **MP4 视频**：画面在 800×480 屏幕上，声音从板上喇叭出来。播完会 **自动从头再播**。

它 **不是** 随便播抖音/电影下载的播放器，也 **不是** 上一个 `14_avi_player`（那个播 AVI）。

只能播这一种片子：

| 项目 | 必须是 |
|------|--------|
| 容器 | `.mp4` |
| 画面 | **MJPEG**，**800×480**，像素格式 **`yuvj420p`（4:2:0）** |
| 声音 | **AAC**（也兼容 PCM），建议 44100 Hz、双声道 |
| 帧率 | 约 20–24 帧 |

**不要用 H.264 / H.265。** 手机拍的、网上的普通 MP4 几乎都是 H.264，直接拷进去会失败。  
**不要用 `yuvj422p`。** 那种会花屏、马赛克色块。

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
- 一根 USB 线：接板子 **CH340 调试口**（供电 + 烧录 + 看日志）。
- 一张 **FAT32** TF 卡，插在板子卡槽里。
- 卡 **根目录** 有 `output.mp4`。没有这个名字时，会依次试 `output_hq.mp4`、根目录里其它 `.mp4`。
- 电脑已装 ESP-IDF preview（芯片 `esp32s31`）。
- （可选）[ffmpeg](https://ffmpeg.org/)。卡里已经有转好的片子可以不装。

**不需要** 第二根 USB、手机、按键、触摸。喇叭在板子上，音量约 50。

`mjpeg`、`music` 文件夹里的文件 **不会被自动找到**，必须放在打开卡盘符后的第一层。

---

## 第一步：确认 TF 卡是 FAT32

1. 卡插到电脑读卡器。
2. 「此电脑」里找到盘符，右键 → **属性**。
3. 文件系统必须是 **FAT32**。若是 exFAT / NTFS：先备份，再格式化成 FAT32。

---

## 第二步：准备 `output.mp4`

### 情况 A：卡里已经有片子

以前测过原厂 MP4 示例，根目录已有 `output.mp4` 或 `output_hq.mp4`，**跳过转码**，做第四步。

本板实测卡里常见文件名是 `output_hq.mp4`，本程序会认。

### 情况 B：没有片子，需要自己转

1. 安装 ffmpeg，打开命令提示符，`cd` 到视频所在目录。
2. 整行复制（把 `你的视频.mp4` 改成真实文件名）：

```bat
ffmpeg -i 你的视频.mp4 -vf "fps=24,scale=800:480:flags=lanczos" -c:v mjpeg -q:v 2 -pix_fmt yuvj420p -c:a aac -ar 44100 -ac 2 -b:a 128k output.mp4
```

`-q:v` 数字越小越清晰、文件越大。`2` 比 `3`/`6` 方块少、更清楚。不要用 `yuvj422p`，硬件 JPEG 会解成马赛克。

3. 建议自检（能看到 `mjpeg`、`yuvj420p`、`aac` 才对）：

```bat
ffprobe -hide_banner output.mp4
```

4. 把 `output.mp4` **复制到 TF 卡根目录**（不要放进文件夹）。
5. 右键盘符 → **弹出**，再插回板子卡槽，推到位。

`-q:v` 数字越小越清晰、文件越大。推荐 `3`；`6` 也能播但偏糊。

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
cd E:\idf-debuging\02_s31_examples\bsp_verify\15_mp4_player
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口波特率 115200。成功开始播放时类似：

```
I (xxx) LCD: BSP RGB LCD ready
I (xxx) BSP: SD mounted at /sdcard
I (xxx) bsp_mp4: SD files:
I (xxx) bsp_mp4:   output_hq.mp4
I (xxx) bsp_mp4: play /sdcard/output_hq.mp4 (MJPEG+AAC). Watch LCD, listen to speaker
I (xxx) bsp_mp4: Video fmt=... 800x480 @ 24 fps
I (xxx) bsp_mp4: Playing /sdcard/output_hq.mp4
I (xxx) bsp_mp4: video OK JPEG frame
I (xxx) bsp_mp4: audio OK, listen to speaker
```

下面几行 **可以忽略**：

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
W (xxx) i2s_common: dma frame num is out of dma buffer size
```

播完会打印 `Loop playback`，然后自动再来一遍，不是死机。

---

## 第五步：看画面、听声音

1. **看屏幕**：开头大约半秒可能是黑的（背光等第一帧画完再开，避免闪一下），随后视频应平稳播放，不是花屏、不是马赛克色块。
2. **听喇叭**：应有声音。太吵就把耳朵靠近。
3. 本示例 **没有暂停键、没有进度条**。不要按 SW3–SW6，也不用摸屏。

---

## 怎样算成功（请对照打勾）

下面 **4 条都满足** 才算通过：

1. 串口有 `play /sdcard/...mp4`。
2. 串口有 `video OK JPEG frame`。
3. **屏幕画面在动**，能看出是视频，没有大面积马赛克。
4. **喇叭有声音**（片子无音轨除外）。

只亮背光、或只列出 SD 文件但画面不动，不算通过。

---

## 常见问题（按现象查）

| 你看到的现象 | 多半原因 | 怎么处理 |
|--------------|----------|----------|
| 反复 `insert SD card` | 卡没插好，或不是 FAT32 | 重新插紧；电脑上看文件系统 |
| 反复 `no .mp4 on SD` | 根目录没有 `.mp4`，或放进了文件夹 | 把片子放到打开卡盘符后的第一层 |
| `Only MJPEG video is supported` | 片子是 H.264 | 按第二步 ffmpeg 重转 |
| 花屏 / 马赛克色块 / 发糊 | 片子 `-q:v` 太大，或用了 `yuvj422p` | 用 `-q:v 2 -pix_fmt yuvj420p` 重转，覆盖卡根目录 `output.mp4` |
| 有画面没声音 | 音轨不是 AAC/PCM | ffmpeg 加上 `-c:a aac -ar 44100 -ac 2` |
| 列表里有 `output.avi` 但不播 | 本示例只认 `.mp4` | 去测 `14_avi_player`，或转成 MP4 |
| COM 口 busy / 找不到 | 上一个 monitor 没关，或 CH340 掉了 | 关掉旧窗口，拔插调试 USB |

---

## 客户验收一句话

> TF 卡根目录放好 **MJPEG（yuvj420p）+ AAC** 的 `output.mp4`，烧录后屏幕播视频、喇叭有声音，即通过。
