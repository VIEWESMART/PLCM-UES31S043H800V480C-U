# 11_bt_spp — Phone Bluetooth serial (BLE UART)

[中文](README_CN.md)

This example is a **BLE** wireless UART echo. Text from the phone is sent back and printed on the USB log.

Classic Bluetooth SPP was tried first. The phone `TT` can pair, but Android Settings never opens the SPP port (common on many phones), so the UI keeps saying connection failed. The firmware is now **Nordic UART over BLE**, which Serial Bluetooth Terminal supports in Bluetooth LE mode.

`10_ble_gatt` is a volume remote. This example exchanges text. The two images cannot coexist.

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

- Board flashed with this example, USB to a PC.
- Android or iPhone (BLE works on both).
- Android: **Serial Bluetooth Terminal** (Kai Morich).
- iPhone: nRF Connect or any Nordic UART app.

Do not tap Connect in system Bluetooth settings.

## Step 1: Flash

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\11_bt_spp
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Log should show:

```
I (xxx) bsp_ble_uart: advertising S31-BSP-UART — use Serial Bluetooth Terminal, Bluetooth LE
```

## Step 2: Forget old names

Forget `S31-BSP-SPP`, `S31-BSP-HID`, and `S31-BSP-BLE` in phone settings.

## Step 3: Connect from the app (Android)

1. Open **Serial Bluetooth Terminal**.
2. Menu → **Devices** → **Bluetooth LE** (not Classic).
3. Scan and tap **`S31-BSP-UART`**.
4. Tap the plug icon.
5. You should receive `S31-BSP-UART echo ready`.
6. Send `hello`; it echoes, and the USB log shows `rx`.

## Pass criteria

1. Log shows `advertising S31-BSP-UART`.
2. The app connects with **Bluetooth LE** and receives `echo ready`.
3. `hello` echoes; the log shows `BLE connected` and `rx`.

## One-line acceptance

> Connect **Serial Bluetooth Terminal → Bluetooth LE** to **`S31-BSP-UART`**. Send `hello` and get the same text back.
