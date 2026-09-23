# 10_ble_gatt — BLE HID remote (phone volume)

[中文](README_CN.md)

This example turns the board into a **pairable Bluetooth remote**. After pairing, the four ADC keys change the phone’s volume, mute, and play/pause.

It is not advertise-only, and it is not an nRF Connect GATT session. You must pair from the phone’s **system Bluetooth settings** so the OS accepts it as a HID remote.

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

- One board flashed with this example, USB connected to a PC (power + serial log).
- A phone with Bluetooth on (Android or iPhone).
- Open any music app, or at least show the system volume bar, so you can see the level change.

You do **not** need headphones on the board. Volume changes the **phone’s** speaker/headset, not the on-board speaker.

## Which keys (read the silkscreen)

Several buttons exist on the board. This example uses only **SW3, SW4, SW5, SW6** (ADC key array).

| Silk | Action | What you should see on the phone |
|------|--------|----------------------------------|
| SW3 | Volume down | System volume bar goes down |
| SW4 | Volume up | System volume bar goes up |
| SW5 | Mute | Mute / unmute (phone-dependent) |
| SW6 | Play | Play or pause if media is active (no effect if nothing is playing) |

**Do not use:**

- **BOOT** — download/boot key, not a volume key.
- **LCD / touch** — unused in this example.
- Reset — the board advertises again; the phone may need to reconnect.

The first press of a key may need about **1 second hold**. That is ADC debounce, not a defect.

---

## Step 1: Flash and confirm advertising

Use ESP-IDF preview / target `esp32s31`. After plugging USB, check the COM port in Device Manager and replace `COMx` below.

```powershell
. C:\Espressif\tools\Microsoft.master.PowerShell_profile.ps1
$env:PYTHONUTF8 = "1"
Remove-Item Env:SDKCONFIG_DEFAULTS -ErrorAction SilentlyContinue
cd E:\idf-debuging\02_s31_examples\bsp_verify\10_ble_gatt
idf.py --preview set-target esp32s31
idf.py --preview -p COMx flash monitor
```

Serial is 115200. After reset you should see:

```
I (xxx) bsp_ble_hid: SW3 vol-  SW4 vol+  SW5 mute  SW6 play
I (xxx) bsp_ble_hid: HID GAP mode=1
I (xxx) bsp_ble_hid: HID start, advertising S31-BSP-HID
```

**`advertising S31-BSP-HID`** means the phone can discover the board.  
If the name is still `S31-BSP-BLE`, this firmware is not running — flash again.

This image is **BLE only**. Do not mix it with `11_bt_spp` or `17_bt_audio` (Classic Bluetooth) on the same board.

---

## Step 2: Forget the old device (important)

An older build may still appear as **`S31-BSP-BLE`**. That build **cannot pair**. Leave it in the phone list and pairing will fail or look confusing.

1. Open **Settings → Bluetooth** (some Android phones: **Settings → Connected devices**).
2. Find `S31-BSP-BLE` or any leftover name from this board.
3. Tap the gear / `i` / Ignore / Unpair / Forget.
4. Continue only when the old name is gone.

iPhone: Settings → Bluetooth → tap `i` next to the device → **Forget This Device**.

---

## Step 3: Pair from system Bluetooth (do not use a debug app)

### Android

1. Open **Settings → Bluetooth** and turn Bluetooth on.
2. Wait a few seconds and look under Available devices for **`S31-BSP-HID`**.
3. Tap that name. Do **not** tap Connect inside nRF Connect, LightBlue, or similar apps.
4. If a passkey / pairing dialog appears, tap **Pair** or **OK**. The board confirms automatically; you do not type a code on the board.
5. After success the device moves to Paired / Connected. Some phones show it as an input device / remote / HID.

### iPhone

1. Open **Settings → Bluetooth**.
2. Under Other Devices tap **`S31-BSP-HID`**.
3. Tap **Pair** if asked.
4. It should appear under My Devices as **Connected**.

On the PC serial log you should then see:

```
I (xxx) ESP_HID_GAP: BLE GAP AUTH SUCCESS
I (xxx) bsp_ble_hid: HID connected, press SW3-SW6
```

Those two lines mean pairing completed and the HID session is up.  
Seeing the name on the phone without `HID connected` is **not** a pass.

---

## Step 4: Control the phone

1. Keep Bluetooth connected; stay close to the board.
2. Start any music app, or open Control Center / the volume HUD.
3. Hold **SW4 for about 1 s** (volume up), then press **SW3** (volume down).
4. Watch the phone volume bar.
5. Try SW5 (mute) and SW6 (play/pause). SW6 is obvious only when media is playing.

Key presses should log:

```
I (xxx) bsp_ble_hid: SW4 down key=233 connected=1
I (xxx) bsp_ble_hid: SW4 up key=233 connected=1
```

`connected=1` means the board thinks a phone is attached. If you see `connected=0`, volume will not change — go back to Step 3.

---

## Pass criteria (all four)

1. After flash, the log shows `HID start, advertising S31-BSP-HID`.
2. The phone **system Bluetooth** can pair `S31-BSP-HID` and stay **Connected** (not discover-only).
3. The log shows `BLE GAP AUTH SUCCESS` and `HID connected`.
4. **SW3 / SW4** move the phone system volume down / up.

SW5 and SW6 are extra: mute and play depend on the phone UI. Some models honor volume only. **Volume up/down is enough to pass hardware and protocol.**

---

## Troubleshooting

| What you see | Likely cause | What to do |
|--------------|--------------|------------|
| Phone only finds `S31-BSP-BLE`; pair fails immediately | Old firmware, or old bond left on the phone | Reflash this project; forget `S31-BSP-BLE` |
| No name at all | Board not running, or too far | Confirm `advertising` in the log; USB power; scan closer |
| nRF Connect connects but keys do nothing | GATT debug connect is **not** a HID pair | Disconnect the app; pair from **Settings → Bluetooth** |
| Pair dialog flashes then drops | Stale bond, or two phones at once | Forget on both sides; reset the board; pair with one phone |
| Connected but keys do nothing | Wrong button, press too short, or `connected=0` | Use SW3–SW6; hold ~1 s; check the log |
| Volume works, SW6 does nothing | No media session | Start music first |
| Expected a speaker / music sink | This firmware is a remote, not a speaker | Use `17_bt_audio` (cannot share this image) |
| Classic BT examples disappear after this flash | BLE and Classic cannot share one firmware | Flash the Classic project when you test SPP / A2DP |

---

## One-line customer acceptance

> Pair **S31-BSP-HID** from the phone’s own Bluetooth settings. **SW4 raises volume, SW3 lowers volume.** That is a pass.
