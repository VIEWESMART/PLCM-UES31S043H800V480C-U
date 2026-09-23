# 08_can — CAN / TWAI verification

[中文](README_CN.md)

Uses BSP `bsp_can_init()` with the on-chip TWAI controller and the on-board **SIT1050T**. Default **500 kbit/s**, classic CAN (do not enable CAN FD).

Default `EXAMPLE_CAN_SELF_TEST=1`: TX does not need a peer ACK, and controller **loopback** is on. A single board can see its own echo; two boards or a USB-CAN adapter can also see peer frames.

TX ID is chosen from the last byte of the Wi-Fi MAC, so the same firmware can be flashed to both boards:

| Last MAC byte | TX ID |
|---------------|--------|
| `< 0x50` (e.g. `…:39:3c`) | `0x123` |
| `≥ 0x50` (e.g. `…:39:66`) | `0x321` |

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

## Hardware

| Signal | Location |
|--------|----------|
| CAN_TX | GPIO53 |
| CAN_RX | GPIO54 |
| CAN_H / CAN_L / GND | Terminals `CAN_H` / `CAN_L` / `CAN_GND` |

The board already has a 120 Ω terminator. Test **CAN_H / CAN_L / GND**, not the USB debug port. Do not use USB-RS485 or USB-TTL. **Do not** short GPIO53/54 or `CAN_TX`/`CAN_RX`.

## Build and flash

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\08_can
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Monitor baud is 115200. Use two monitor windows (two COM ports) for two boards.

Pick any method below. Method 1 is a quick firmware/controller check. Method 2 or 3 is the formal pass for the transceiver and bus.

---

## Method 1: Single-board loopback (no CAN wiring)

Leave the CAN terminals unconnected. Flash and watch the USB log.

### Pass criteria

After boot you should see:

```
I (xxx) BSP: CAN TX=53 RX=54 bitrate=500000 self_test=1 loopback=1
I (xxx) bsp_can: mac xx:xx:xx:xx:xx:xx tx_id=0x123 period … self_test=1
I (xxx) bsp_can: tx id=0x123 n=1
I (xxx) bsp_can: rx self id=0x123 n=1
```

**Pass if all of these are true:**

1. `self_test=1 loopback=1`
2. Periodic `tx id=0x123` or `tx id=0x321` (no `tx failed` / `bus-off`)
3. Each `tx` is followed immediately by `rx self` with the same ID and `n`

This proves the controller loopback and that the firmware is running. It does **not** prove the SIT1050 or the CAN terminals. Use method 2 or 3 for the PHY.

---

## Method 2: Two identical boards (recommended, no USB-CAN)

### Wiring (power off first)

```
Board A                      Board B
-------                      -------
CAN_H  -------------------  CAN_H
CAN_L  -------------------  CAN_L
GND    -------------------  GND
```

- These three wires only. H to H, L to L, common GND.
- Each board already has 120 Ω, so a pair is correctly terminated at both ends.
- Power both boards over USB and open a serial log on each.
- If H/L are swapped, both sides show only `tx` / `rx self` and never `rx peer`.

### Flash the same firmware

Flash this project to both boards. Different MACs pick different TX IDs, so the logs are easy to tell apart.

```powershell
idf.py --preview -p COM72 flash
idf.py --preview -p COM3 flash
idf.py --preview -p COM72 monitor
idf.py --preview -p COM3 monitor
```

(Replace COM ports as shown in Device Manager.)

### Pass criteria

Board A (`tx_id=0x123`, e.g. MAC `…:39:3c`):

```
I (xxx) bsp_can: tx id=0x123 n=1
I (xxx) bsp_can: rx self id=0x123 n=1
I (xxx) bsp_can: rx peer id=0x321 n=1
```

Board B (`tx_id=0x321`, e.g. MAC `…:39:66`):

```
I (xxx) bsp_can: tx id=0x321 n=1
I (xxx) bsp_can: rx self id=0x321 n=1
I (xxx) bsp_can: rx peer id=0x123 n=1
```

**Pass if all of these are true:**

1. Both sides keep transmitting, without repeated `bus-off`
2. Both sides show their own `rx self` (loopback still on)
3. **Both sides show `rx peer` from the other ID**, with incrementing `n`

Item 3 means SIT1050 + CAN_H/L work in both directions. That is the formal pass when you have no analyzer.

If both MACs happen to fall on the same side of `0x50`, both boards transmit the same ID. Still treat `rx peer` as success when the peer `n` does not match the local `tx … n=`.

### Method 2 troubleshooting

| Symptom | Likely cause | What to do |
|---------|--------------|------------|
| Only `tx` + `rx self`, no `rx peer` | H/L swapped, no GND, or only one board flashed | Swap H/L on one end; add GND; flash both |
| Only one board shows `rx peer` | Wrong COM window, or the other board is not running | Match each monitor to a board |
| `tx failed` / `bus-off` | `EXAMPLE_CAN_SELF_TEST` set to `0` and peer not ready | Keep it `1`, or reset both after they are up |

---

## Method 3: USB-CAN adapter

One board + a USB-CAN adapter. Use two windows on the PC: USB serial for board logs, vendor software for bus frames.

CAN is **not** UART. Do not open the adapter in a 115200 serial terminal. Do not use a USB-RS485 dongle.

### Wiring (power off first)

```
Board                  USB-CAN
-----                  -------
CAN_H  ------------->  CAN_H (or CANH / H)
CAN_L  ------------->  CAN_L (or CANL / L)
GND    ------------->  GND
```

The board already has 120 Ω. If the adapter has a 120 Ω switch, turn it on for a short two-node cable.

### Analyzer settings

| Item | Value |
|------|--------|
| Bitrate | **500 kbit/s** |
| Mode | Classic CAN / CAN 2.0, standard frame. **No CAN FD** |
| Operation | Normal TX (not listen-only) |

Read the board TX ID from the boot log `tx_id=` (`0x123` = 291 decimal, `0x321` = 801).

#### Test A: PC receives the board

The analyzer should show that ID periodically, DLC=4, data incrementing (`00 00 00 01`, `00 00 00 02`, …). That is **board → bus → USB-CAN**.

#### Test B: Board receives the PC

Send one frame whose ID is **not** the board `tx_id` (e.g. send `0x321` if the board uses `0x123`). The USB log should show:

```
I (xxx) bsp_can: rx peer id=0x321 n=…
```

**Pass if both A and B succeed.**

### Method 3 troubleshooting

| Symptom | Likely cause | What to do |
|---------|--------------|------------|
| Board has `tx`, analyzer is empty | H/L swapped, or not 500K | Swap H/L; set 500 kbit/s |
| Frames appear but ID is not 123/321 | Software shows decimal | `0x123`=291, `0x321`=801 |
| Board frames visible, no `rx peer` after TX | Listen-only, or extended / CAN FD | Normal mode; standard frame; classic CAN |
| USB-RS485 / USB-TTL on CAN_H/L | Wrong protocol | Use a USB-**CAN** adapter |

---

## Optional: standard ACK

After the bus works, set `EXAMPLE_CAN_SELF_TEST` to `0` in `main/main.c` and rebuild. Loopback turns off, so `rx self` disappears. Another node (second board or USB-CAN in **normal** mode) must ACK, or you may see `tx failed` / `bus-off`. Keep `1` for factory / customer self-check.
