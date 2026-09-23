# 14_avi_player — play AVI from SD (beginner guide)

[中文](README_CN.md)

This app turns the board into an **AVI player**. It reads a video from the TF card, shows it on the 800×480 LCD, and plays sound on the onboard speaker. When the file ends it **loops from the start**.

It is **not** a general movie player, and it is **not** `15_mp4_player` (that one plays MP4).

Only this kind of clip works:

| Item | Required |
|------|----------|
| Container | `.avi` |
| Video | **MJPEG** (Motion JPEG), 800×480 at about 24 fps recommended |
| Audio | **PCM** (`pcm_s16le`), 44100 Hz, stereo, 16-bit recommended |

A random AVI from the internet, a phone recording, or an H.264 movie **usually fails** if you copy it as-is. Convert it with ffmpeg below, or use a ready-made `output.avi`.

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
- A **TF / microSD** card the PC can see. The filesystem must be **FAT32** (not exFAT, not NTFS).
- ESP-IDF preview installed (target `esp32s31`).
- (Optional) [ffmpeg](https://ffmpeg.org/) on the PC. Skip this if the card already has `output.avi`.

You do **not** need a second USB cable, a phone, the ADC keys, or the touch panel.

The speaker is on the board. Firmware volume is about 55. Hold the board near your ear if the room is noisy.

---

## Step 1 — confirm the card is FAT32

1. Insert the card in a PC card reader (or a laptop slot).
2. Open This PC and find the new drive letter (for example `E:`).
3. Right-click the drive → **Properties**.
4. Check File system:
   - **FAT32**: continue.
   - **exFAT / NTFS**: back up the files, then Right-click → Format → File system **FAT32** → Start. Format erases the card.

Windows may hide FAT32 for cards larger than 32 GB. Use another tool, or use an 8–32 GB card.

---

## Step 2 — prepare `output.avi`

After you open the card drive, the **first level** (do not enter any folder) must contain an `.avi`.

Prefer this name: **`output.avi`**.  
If that name is missing, the board plays the **first** `.avi` it finds in the root.

Files inside folders such as `mjpeg` or `music` are **not** picked up automatically. Put the file in the root.

### Case A — the card already has `output.avi`

If you already tested the original AVI example, or someone copied a converted clip, **skip encoding**. Confirm the file is in the root, then go to step 4.

### Case B — you must convert a video

1. Install ffmpeg, then open Command Prompt or PowerShell.
2. `cd` to the folder that holds your video.
3. Run this line (change `your.mp4` to the real name):

```bat
ffmpeg -i your.mp4 -vf "fps=24,scale=800:480:flags=lanczos" -c:v mjpeg -q:v 6 -c:a pcm_s16le -ar 44100 -ac 2 -pix_fmt yuvj420p output.avi
```

4. `output.avi` appears in the same folder.
5. **Copy** it to the TF card root (the first level you see when you open the drive).
6. Wait until the copy finishes.
7. Right-click the drive → **Eject** (or use Safely Remove Hardware). Do not pull the card out early.
8. Insert the card into the board TF slot. Gold contacts face the way the silk / slot shows. Push until it seats.

What the command does (you can skip the details): scale to 800×480, 24 fps, JPEG video frames, PCM audio.

---

## Step 3 — wire and power

1. The TF card is already in the board.
2. Plug USB only into the **CH340 debug port**, other end to the PC.
3. Open Device Manager → Ports and note the **USB-SERIAL CH340** port, for example `COM72`. Replace `COMx` below with that port.
4. The LCD may still show the previous firmware until flash finishes.

---

## Step 4 — flash and open the serial log

In PowerShell (paths match this repo):

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\14_avi_player
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Serial baud rate is 115200. After reset you should see something like this (timestamps change):

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

You can **ignore** these lines; they are not a fail:

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
W (xxx) i2s_common: dma frame num is out of dma buffer size, limited to 1023
```

GPIO47 is the speaker PA pin sharing a warning; sound still works.  
`i2s_channel_disable` appears when the audio clock is opened. That is expected.

---

## Step 5 — watch and listen

1. **LCD**: video should move. Not a black screen, not colored garbage, not one frozen still (a short black intro is OK).
2. **Speaker**: you should hear audio. Hold the board near your ear if needed. If the clip is silent, you only need `audio OK` or a clip with no audio track.
3. **End of file**: the log prints `AVI ended, loop /sdcard/output.avi` and playback restarts. That is a loop, not a crash.

This app has **no pause key and no progress bar**. SW3–SW6 and the touch panel do nothing here.

---

## Pass criteria (tick all four)

All **four** must be true:

1. The log shows `SD mounted at /sdcard` and the file list includes `output.avi` (or another root `.avi`).
2. The log shows `play /sdcard/...avi` and `video OK`.
3. **The picture on the LCD is moving** and looks like video.
4. **The speaker plays sound** (or the clip has no audio track and the log says so).

Backlight only, or an SD file list with a frozen screen, is not a pass.

---

## FAQ

| What you see | Likely cause | What to do |
|--------------|--------------|------------|
| Repeating `insert SD card` | Card not seated, or not FAT32 | Re-insert; check the filesystem on the PC |
| Repeating `no .avi on SD` | No `.avi` in the root, or it is inside a folder | Put `output.avi` on the first level of the drive |
| `output_hq.mp4` listed but not played | This app only accepts `.avi` | Convert with ffmpeg, or use `15_mp4_player` |
| `play` then `JPEG decode failed` / garbage | Not MJPEG, or the frame is too large | Re-encode with the ffmpeg line in step 2, `scale=800:480` |
| Picture but no sound | Audio is not PCM, or volume is low | ffmpeg must include `-c:a pcm_s16le`; hold the board near your ear |
| `JPEG frame too large` | One JPEG frame is bigger than the buffer | Lower resolution or raise `-q:v` (worse quality, smaller frames) |
| COM port busy / not found | An old `idf.py monitor` is still open, or CH340 dropped | Close the old monitor, re-plug debug USB, wait for the port |
| Want MP4 / MP3 | Wrong example | MP4 → `15_mp4_player`; MP3 → `16_sd_music` |

---

## Customer accept, one line

> Put an **MJPEG+PCM** `output.avi` in the TF card root, flash this app, and pass if the LCD shows video and the speaker plays sound.
