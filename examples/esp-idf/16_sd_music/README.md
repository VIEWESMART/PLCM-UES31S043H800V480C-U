# 16_sd_music — SD music player with UI (beginner guide)

[中文](README_CN.md)

This app plays **MP3** from the TF card on the onboard speaker, with an on-screen player: previous / pause / next, volume, and folder browsing. When a track ends it **plays the next MP3 in the current folder** (loops the folder).

It is **not** a video player (`14` / `15`) and **not** a Bluetooth speaker (`17_bt_audio`).

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
- At least one `.mp3` in the **root** or in a folder such as `music/`.
- ESP-IDF preview (target `esp32s31`).

Default volume is 50. Use the on-screen slider or `−` / `+`.

The app lists `/sdcard` first. If the root has no MP3 but a `music` folder exists, it opens `music` and plays the first track there.

---

## Step 1 — confirm the card is FAT32

1. Insert the card in a PC reader.
2. This PC → right-click the drive → **Properties**.
3. File system must be **FAT32**. If it is exFAT / NTFS, back up and format as FAT32.

---

## Step 2 — prepare an MP3

1. Copy a normal MP3 (128–320 kbps is fine). Do not use wma / flac / encrypted ncm.
2. Put it in the TF card **root**, or in a `music` folder.
3. Right-click the drive → **Eject**, then insert the card into the board.

Chinese file names are shown. Spaces and parentheses in names are OK.

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
cd E:\idf-debuging\02_s31_examples\bsp_verify\16_sd_music
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Serial baud rate is 115200. When playback starts:

```
I (xxx) BSP: SD mounted at /sdcard
I (xxx) bsp_mp3: scan /sdcard count=...
I (xxx) bsp_mp3: play file:///sdcard/...mp3
I (xxx) music_ui: LVGL music UI ready
I (xxx) bsp_mp3: audio OK, listen to speaker
```

You can **ignore**:

```
W (xxx) gpio: conflict found for GPIO[47]
E (xxx) i2s_common: i2s_channel_disable(...): the channel has not been enabled yet
```

---

## Step 5 — use the screen

Landscape 800×480:

| Area | Action |
|------|--------|
| Left list | Tap a folder to enter; tap `..` to go up; tap an `.mp3` to play |
| Top bar | Current directory |
| Title on the right | Current file name |
| `|<` / `>` | Previous / next MP3 in this folder |
| Center button | Pause / resume |
| Right vertical slider | Volume 0–100 |

1. Hold the board near your ear and you should hear a song.
2. Pause should silence the speaker; tap again to resume.
3. If a `music` folder exists, tapping it should list the songs inside.

---

## Pass criteria (tick all)

1. The LCD shows the dark-blue **music player** UI with a file list on the left.
2. **The speaker plays music** you can recognize.
3. Pause, previous/next, and volume respond.
4. You can open folders such as `music` and go back with `..`.

---

## FAQ

| What you see | Likely cause | What to do |
|--------------|--------------|------------|
| Repeating `insert SD card` | Card not seated, or not FAT32 | Re-insert; check the filesystem on the PC |
| Empty list | This folder has no directories and no `.mp3` | Copy MP3s to the root or `music` |
| `output.mp4` on the card but not listed | This app only lists folders and `.mp3` | Use `15_mp4_player` |
| Log plays but you hear nothing | Volume dragged down, or you are far away | Raise the slider above 50; hold the board closer |
| `player error` | Corrupt file or not a real MP3 | Use a normal MP3 |
| COM port busy / not found | Old monitor still open, or CH340 dropped | Close it, re-plug debug USB |

---

## Customer accept, one line

> Put a normal **MP3** on the TF card (root or `music/`), flash this app, and pass if the screen can browse folders and pause / skip / volume, and the speaker plays music.
