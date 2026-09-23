# 14_avi_player — SD 卡播 AVI（小白教程）

[English](README.md)

本示例把开发板当成一台 **AVI 播放器**：从 TF 卡读视频，画面显示在 800×480 屏幕上，声音从板上喇叭出来。播完会 **自动从头再播**。

它 **不是** 随便播网上电影的播放器，也 **不是** 下一个示例 `15_mp4_player`（那个播 MP4）。

只能播这一种片子：

| 项目 | 必须是 |
|------|--------|
| 容器 | `.avi` |
| 画面 | **MJPEG**（Motion JPEG），建议 800×480、约 24 帧 |
| 声音 | **PCM**（`pcm_s16le`），建议 44100 Hz、双声道、16 bit |

网上随手下的 AVI、手机拍的视频、H.264 电影，**直接拷进去通常花屏或没声**。请按下面步骤用 ffmpeg 转一次，或使用已经转好的 `output.avi`。

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
- 一根 USB 线：接板子上的 **CH340 调试口**（供电 + 烧录 + 看日志）。
- 一张 **TF / microSD 卡**，电脑能认到，格式必须是 **FAT32**（不是 exFAT、不是 NTFS）。
- 电脑已装好 ESP-IDF preview（本仓库目标芯片 `esp32s31`）。
- （可选）电脑已装 [ffmpeg](https://ffmpeg.org/)。卡里 **已经有** `output.avi` 就可以不装。

**不需要** 第二根 USB 线、不需要手机、不需要按键、不需要触摸。

喇叭在板子上，音量固件里固定约 55。环境吵就把耳朵靠近喇叭。

---

## 第一步：确认 TF 卡是 FAT32

1. 把卡插到电脑读卡器（或带卡槽的笔记本）。
2. 打开「此电脑」，找到新出现的盘符（例如 `E:`）。
3. 在盘符上 **右键 → 属性**。
4. 看「文件系统」：
   - 写着 **FAT32**：可以继续。
   - 写着 **exFAT / NTFS**：先备份卡里的文件，再右键 → 格式化 → 文件系统选 **FAT32** → 开始。格式化会清空卡。

卡容量大于 32 GB 时，Windows 自带格式化可能不给选 FAT32。可用第三方工具，或换一张 8～32 GB 的卡。

---

## 第二步：准备 `output.avi`

打开卡盘符后，**第一层**（不要点进任何文件夹）里要有一个 `.avi`。

优先用这个名字：**`output.avi`**。  
如果没有这个名字，板子会改播根目录里找到的 **第一个** `.avi`。

`mjpeg`、`music` 这种文件夹里的片子 **不会被自动找到**。必须放在根目录。

### 情况 A：卡里已经有 `output.avi`

以前测过原厂 AVI 示例、或别人已经拷好片子，**跳过「转码」**，只确认文件在根目录，然后做第四步。

### 情况 B：没有片子，需要自己转

1. 电脑安装 ffmpeg，装好后打开 **命令提示符** 或 PowerShell。
2. 先 `cd` 到你的视频所在目录。
3. 整行复制执行（把 `你的视频.mp4` 改成真实文件名）：

```bat
ffmpeg -i 你的视频.mp4 -vf "fps=24,scale=800:480:flags=lanczos" -c:v mjpeg -q:v 6 -c:a pcm_s16le -ar 44100 -ac 2 -pix_fmt yuvj420p output.avi
```

4. 同一目录会出现 `output.avi`。
5. 把 `output.avi` **复制** 到 TF 卡盘符的根目录（和打开卡后直接看到的那一层）。
6. 等复制进度走完。
7. 在盘符上右键 → **弹出**（或点任务栏「安全删除硬件」）。不要直接拔卡，否则文件可能没写完。
8. 把卡插入开发板侧面的 TF 卡槽，金手指朝向丝印/卡槽指示的方向，推到底，有轻微卡到位的感觉。

命令在做什么（可以不求甚解）：把画面缩成 800×480、每秒 24 帧、画面压成 JPEG 序列、声音变成电脑喇叭也能懂的 PCM。

---

## 第三步：接线上电

1. TF 卡已经插在板子上。
2. USB 线只插 **CH340 调试口**，另一头插电脑。
3. 打开「设备管理器 → 端口」，记住 **USB-SERIAL CH340** 的口，例如 `COM72`。下面命令里的 `COMx` 改成你的口。
4. 屏幕暂时可能是黑的或停在上一份固件的画面，等烧完再看。

---

## 第四步：烧录并打开串口日志

在 PowerShell 里执行（路径按本仓库）：

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\14_avi_player
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

串口波特率 115200。烧完复位后，应陆续看到类似内容（时间戳会变）：

```
I (xxx) BSP-DISPLAY: RGB LCD 800x480 RGB888 ready (2 FB)
I (xxx) BSP: SD mounted at /sdcard
I (xxx) bsp_avi: SD files:
I (xxx) bsp_avi:   output.avi
I (xxx) bsp_avi: play /sdcard/output.avi
I (xxx) avifile: Find a video stream
I (xxx) avifile: Find a audio stream
I (xxx) bsp_avi: AVI audio 44100 Hz 16 bit 2 ch
I (xxx) bsp_avi: video OK 800x480 JPEG
I (xxx) bsp_avi: audio OK, listen to speaker
```

下面几行 **可以忽略**，不是失败：

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
W (xxx) i2s_common: dma frame num is out of dma buffer size, limited to 1023
```

GPIO47 是喇叭功放脚，和别的功能共用提示，实测能出声即可。  
`i2s_channel_disable` 是打开音频时钟时的正常提示。

---

## 第五步：看画面、听声音

1. **看屏幕**：应出现视频，画面在动。不是一直黑、不是彩条花屏、不是定住一张图不动（片头黑场几秒可以等）。
2. **听喇叭**：应有声音。太吵就把板子拿近耳朵。片源本身是静音视频，则只要日志有 `audio OK` 或确认片子没有音轨。
3. **等播完**：日志出现 `AVI ended, loop /sdcard/output.avi`，然后自动再来一遍。这是正常循环，不是死机。

本示例 **没有播放暂停键、没有进度条**。不要去按 SW3–SW6 或摸屏幕，那些键在本固件里不起作用。

---

## 怎样算成功（请对照打勾）

下面 **4 条都满足** 才算本示例通过：

1. 串口有 `SD mounted at /sdcard`，文件列表里能看到 `output.avi`（或其它根目录 `.avi`）。
2. 串口有 `play /sdcard/...avi` 和 `video OK`。
3. **屏幕上的画面在动**，能看出是视频。
4. **喇叭有声音**（或片子无音轨，且日志已说明没有音频流）。

只亮背光、或只列出 SD 文件但画面不动，不算通过。

---

## 常见问题（按现象查）

| 你看到的现象 | 多半原因 | 怎么处理 |
|--------------|----------|----------|
| 反复打印 `insert SD card` | 卡没插好，或不是 FAT32 | 重新插紧；在电脑上看文件系统 |
| 反复打印 `no .avi on SD` | 根目录没有 `.avi`，或文件放进了文件夹 | 把 `output.avi` 放到打开卡盘符后的第一层 |
| 列表里有 `output_hq.mp4` 但不播 | 本示例只认 `.avi` | 用 ffmpeg 转成 `output.avi`，或去测 `15_mp4_player` |
| 有 `play` 但 `JPEG decode failed` / 花屏 | 不是 MJPEG，或分辨率太大 | 按第二步的 ffmpeg 命令重转，`scale=800:480` |
| 有画面没声音 | 音轨不是 PCM，或音量太小 | ffmpeg 必须带 `-c:a pcm_s16le`；耳朵靠近喇叭 |
| `JPEG frame too large` | 单帧 JPEG 超过缓冲 | 降低分辨率或 `-q:v` 用更大数字（画质更差、文件更小） |
| 烧录提示 COM 口 busy / 找不到 | 上一个 `idf.py monitor` 没关，或 CH340 掉了 | 关掉旧监视窗口，拔插调试 USB，等设备管理器再出现口 |
| 想播 MP4 / MP3 | 换对应示例 | MP4 用 `15_mp4_player`，MP3 用 `16_sd_music` |

---

## 客户验收一句话

> TF 卡根目录放好 **MJPEG+PCM** 的 `output.avi`，烧录本示例后屏幕播视频、喇叭有声音，即通过。
