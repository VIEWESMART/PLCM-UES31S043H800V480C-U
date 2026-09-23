# 12_usb_hid — USB touch as a mouse

[中文](README_CN.md)

This app makes the board a **USB mouse**. The PC enumerates the board on the **USB 2.0 High-Speed** port. Usage matches a normal mouse: slide moves the cursor, tap clicks, double-tap selects. Sliding does not hold the left button, so it will not drag-select text.

The CH340 debug UART and the USB mouse port are **two different cables**. Keep both plugged in.

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

- One board flashed with this example.
- **Cable 1**: CH340 debug USB (flash / serial log).
- **Cable 2**: board **USB Type-A device** port (HID mouse).
- A Windows PC (Linux / macOS work the same way).

Do not expect the CH340 port to become a mouse.

---

## Step 1 — flash

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\12_usb_hid
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

You should see:

```
I (xxx) bsp_usb: USB HID mouse: slide=move, tap=click, double-tap=select
```

The backlight turns on. Do not touch the panel yet.

---

## Step 2 — plug the USB-A device port

1. Find the board **USB Type-A receptacle** (not the CH340 debug USB).
2. Plug it into the **same PC**.
3. Windows may say it is setting up a device. No extra driver is needed.
4. Device Manager → Mice and other pointing devices should show a new **HID-compliant mouse**.

The serial log should print:

```
I (xxx) bsp_usb: USB mounted as HID mouse
```

No such line means the PC has not enumerated the device. Re-seat the Type-A cable.

---

## Step 3 — use a finger as the mouse

Same as a normal mouse:

1. **Slide**: drag a finger to move the cursor only. Text and icons are **not** selected.
2. **Tap**: brief tap with almost no movement = left click.
3. **Double-tap**: two taps in a row = select a word / open a file.

The log should print:

```
I (xxx) bsp_usb: touch down x=... y=... usb=1
I (xxx) bsp_usb: tap (click)
I (xxx) bsp_usb: double-tap (select)
I (xxx) bsp_usb: touch up move=... px (cursor only)
```

`usb=1` means the PC has the mouse. `usb=0` only logs touches; the cursor will not move.

---

## Pass criteria

All of the following must be true:

1. Log shows `USB mounted as HID mouse`.
2. Device Manager shows an HID mouse.
3. Sliding a finger moves the cursor and does **not** drag-select text.
4. One tap = click; two taps = double-click / select.

Backlight alone is not a pass.

---

## FAQ

**No new mouse in Device Manager**  
CH340 is still there, but Type-A is not plugged into the device port.

**Log shows `USB unmounted`**  
Cable came loose, or the host slept. Re-plug Type-A.

**Touch logs but cursor stays still**  
`usb=0`: host did not enumerate. `usb=1`: check the HID mouse is not disabled.

**Slight LCD flicker**  
This app only starts RGB + touch, not LVGL. Flicker does not fail HID.
