# 15_mp4_player — play MP4 from SD (beginner guide)

[中文](README_CN.md)

This app plays an **MP4** from the TF card on the 800×480 LCD with speaker audio. When the file ends it **loops**.

It is **not** a general movie player, and it is **not** `14_avi_player` (that one plays AVI).

Only this kind of clip works:

| Item | Required |
|------|----------|
| Container | `.mp4` |
| Video | **MJPEG**, **800×480**, pixel format **`yuvj420p` (4:2:0)** |
| Audio | **AAC** (PCM also works), 44100 Hz stereo recommended |
| Frame rate | About 20–24 fps |

**Do not use H.264 / H.265.** Phone videos and most internet MP4s are H.264 and will fail.  
**Do not use `yuvj422p`.** That produces mosaic / color blocks.

---

## Install the tools (read this first)

**ESP32-S31 can only use the ESP-IDF master branch today.** Stable releases (5.4, 5.5, and so on) do not include this chip. If you install a release, `esp32s31` will not appear in the target list and none of these examples will build. Do not use a numbered release for this board.

Either order works:

1. Install Espressif **EIM** (ESP-IDF Installation Manager) first, install IDF **master** with it, then install VS Code and the ESP-IDF extension.
2. Install VS Code and the ESP-IDF extension first, then open EIM from the extension and install IDF **master**.

### Step 1 — install EIM, then IDF master

1. Open <https://dl.espressif.com/dl/eim/index.html>
2. In mainland China, use the **Download** button (Espressif download server). Do not download the installer from GitHub; that download often fails.
3. Run the installer. It should finish without a red error.
4. In EIM, install the **master** branch. Do not pick a numbered stable release.
5. Open **ESP-IDF PowerShell** from the Start menu. Its name should mention master.

This step succeeded when:

- The PowerShell window opens and the path or prompt contains `master`.
- Both commands below finish:

```powershell
idf.py --version
idf.py --preview --list-targets
```

- `idf.py --version` prints a version, and the path contains `master` (for example `...\master\esp-idf`).
- `idf.py --preview --list-targets` includes **`esp32s31`**.

If `esp32s31` is missing, this is not master. Go back to EIM and install **master**. Do not keep using a stable release.

### Step 2 — install VS Code and the ESP-IDF extension

1. Install VS Code: <https://code.visualstudio.com/>
2. In Extensions, search for **Espressif IDF** (publisher: Espressif Systems) and install it.
3. Press `Ctrl+Shift+P` and run **ESP-IDF: Open ESP-IDF Installation Manager**.
4. If master is already installed, point the extension at that directory. If not, install **master** here.

This step succeeded when:

- The status bar shows ESP-IDF and the path contains `master`.
- The chip target can be set to **esp32s31**.
- Opening this example folder and building ends with **Project build complete**.

PowerShell alone is enough. You do not have to use VS Code. Both paths flash the same firmware.

### Step 3 — flash and confirm the serial log

1. Plug in the board. In Device Manager, under Ports, find **USB-SERIAL CH340** and note the port, for example `COM72`. Replace `COMx` in the commands below.
2. Open ESP-IDF PowerShell **in this example folder**. Do not build from the parent `bsp_verify` folder.
3. Run:

```powershell
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

The first build downloads components from `components.espressif.com` and can take a while. The monitor baud rate is 115200; `idf.py monitor` already uses it.

Every example shows the same three signs when flashing worked:

- Progress reaches **100%**, then **Hash of data verified**.
- Then **Hard resetting via RTS pin...** (the board resets into the new firmware).
- The monitor prints `Project name:` followed by this example's folder name.

After that, check the example-specific log and the board, in the next section. Press `Ctrl+]` to leave the monitor.

If a later section gives a command just for this example, use that command.

Common failures:

| What you see | Why | What to do |
| --- | --- | --- |
| `esp32s31` is not in the target list | Not master, or `--preview` was omitted | Reinstall master with EIM and add `--preview` |
| The port will not open | Wrong COM port, or a monitor is still open | Close the monitor and recheck Device Manager |
| The port disappears after you close the monitor | Normal for some CH340 cables | Unplug and replug USB |
| Component download fails | The PC cannot reach the component registry | Check the network and run the same command again |

Steps and the log lines that mean success for this example start in the next section.

## What you need

- One **UES31S043H800V480C-U** board.
- One USB cable on the **CH340 debug port** (power + flash + log).
- A **FAT32** TF card in the slot.
- `output.mp4` in the card **root**. If missing, the app tries `output_hq.mp4`, then any other root `.mp4`.
- ESP-IDF preview (target `esp32s31`).
- (Optional) [ffmpeg](https://ffmpeg.org/). Skip if a converted clip is already on the card.

You do **not** need a second USB cable, a phone, the ADC keys, or touch.

Files inside folders such as `mjpeg` or `music` are **not** picked up. Put the clip on the first level of the drive.

---

## Step 1 — confirm the card is FAT32

1. Insert the card in a PC reader.
2. This PC → right-click the drive → **Properties**.
3. File system must be **FAT32**. If it is exFAT / NTFS, back up and format as FAT32.

---

## Step 2 — prepare `output.mp4`

### Case A — the card already has a clip

If you already tested the original MP4 example and the root has `output.mp4` or `output_hq.mp4`, **skip encoding** and go to step 4.

This board’s card often already has `output_hq.mp4`. This app accepts that name.

### Case B — convert a video

1. Install ffmpeg, open a terminal, `cd` to the video folder.
2. Run (change `your.mp4` to the real name):

```bat
ffmpeg -i your.mp4 -vf "fps=24,scale=800:480:flags=lanczos" -c:v mjpeg -q:v 2 -pix_fmt yuvj420p -c:a aac -ar 44100 -ac 2 -b:a 128k output.mp4
```

A smaller `-q:v` is sharper and a larger file. `2` has fewer JPEG blocks than `3`/`6`. Do not use `yuvj422p`; the hardware JPEG decoder turns that into mosaic.

3. Optional check (you want `mjpeg`, `yuvj420p`, `aac`):

```bat
ffprobe -hide_banner output.mp4
```

4. **Copy** `output.mp4` to the TF card root (not inside a folder).
5. Right-click the drive → **Eject**, then insert the card into the board until it seats.

A smaller `-q:v` number is sharper and a larger file. Prefer `3`; `6` still plays but looks softer.

---

## Step 3 — wire and power

1. The TF card is already in the board.
2. Plug USB only into the **CH340 debug port**.
3. Device Manager → Ports → note **USB-SERIAL CH340**, for example `COM72`. Replace `COMx` below.

---

## Step 4 — flash and open the serial log

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\15_mp4_player
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Serial baud rate is 115200. When playback starts:

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

You can **ignore**:

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
W (xxx) i2s_common: dma frame num is out of dma buffer size
```

`Loop playback` means the file ended and started again. That is not a crash.

---

## Step 5 — watch and listen

1. **LCD**: the first half-second may stay black (backlight waits for the first decoded frame so it does not flash). Then video should play smoothly, not mosaic, not stuck black.
2. **Speaker**: you should hear audio.
3. This app has **no pause key and no progress bar**. SW3–SW6 and touch do nothing here.

---

## Pass criteria (tick all four)

1. The log shows `play /sdcard/...mp4`.
2. The log shows `video OK JPEG frame`.
3. **The picture is moving** and looks like video, without large mosaic blocks.
4. **The speaker plays sound** (unless the clip has no audio track).

Backlight only, or an SD file list with a frozen screen, is not a pass.

---

## FAQ

| What you see | Likely cause | What to do |
|--------------|--------------|------------|
| Repeating `insert SD card` | Card not seated, or not FAT32 | Re-insert; check the filesystem on the PC |
| Repeating `no .mp4 on SD` | No `.mp4` in the root | Put the clip on the first level of the drive |
| `Only MJPEG video is supported` | The clip is H.264 | Re-encode with the ffmpeg line in step 2 |
| Mosaic / blurry | `-q:v` too high, or `yuvj422p` | Re-encode with `-q:v 2 -pix_fmt yuvj420p` and replace the root `output.mp4` |
| Picture but no sound | Audio is not AAC/PCM | Add `-c:a aac -ar 44100 -ac 2` |
| `output.avi` listed but not played | This app only accepts `.mp4` | Use `14_avi_player`, or convert to MP4 |
| COM port busy / not found | Old monitor still open, or CH340 dropped | Close it, re-plug debug USB |

---

## Customer accept, one line

> Put an **MJPEG (yuvj420p) + AAC** `output.mp4` in the TF card root, flash this app, and pass if the LCD shows video and the speaker plays sound.
