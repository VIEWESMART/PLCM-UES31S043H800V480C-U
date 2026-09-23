# 05_adc_buttons — onboard keys SW3 to SW6

[中文](README_CN.md)

Detects the silkscreen keys SW3, SW4, SW5, and SW6. Do not press BOOT and do not use the touch panel.

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

## What this example does

The four keys share one ADC pin (GPIO42). Idle voltage is about 0. A press raises the voltage, and the firmware decides which key it was.

## How to tell it worked

After reset, tag `bsp_btn` prints:

```
press KEY array SW3-SW6 (not BOOT / not touch), hold 1s each
adc raw=0 mv=0 (idle/N-sat)
```

`raw=0 mv=0 (idle/N-sat)` means no key is down.

Press SW3, then SW4, then SW5, then SW6. Hold each for about one second. The name in the log must match the key you pressed:

| Key | Log line | Voltage while held |
| --- | --- | --- |
| SW3 | `SW3 event=BUTTON_PRESS_DOWN` | 100–600 mV, about 270 mV on a known board |
| SW4 | `SW4 event=BUTTON_PRESS_DOWN` | 600–1080 mV, about 760 mV |
| SW5 | `SW5 event=BUTTON_PRESS_DOWN` | 1080–1605 mV, about 1240 mV |
| SW6 | `SW6 event=BUTTON_PRESS_DOWN` | 1605–2200 mV, about 1650 mV |

Release adds `BUTTON_PRESS_UP`. A short press also adds `BUTTON_SINGLE_CLICK`. After release the log returns to `adc raw=0 mv=0 (idle/N-sat)`.

`IoT Button Version: 4.2.1` means the button component loaded. `ADC calibration is not supported` is **expected** on ESP32-S31. The millivolt value is still computed.

## What failure looks like

- Pressing SW3 prints SW4 (or another key): the voltage does not match the window. Note the `mv=` value on that line.
- Every press stays at `raw=0`: BOOT or the touch panel was pressed, or the key did not make contact.
