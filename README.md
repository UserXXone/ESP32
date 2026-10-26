# ESP32-S3 Cyber-Security Toolkit

A single-device, battery-of-tools security gadget built from just:
- an **ESP32-S3 WROOM** dev board, and
- a **0.96" SSD1306 OLED** (I2C, 128x64)

No other hardware. Navigation uses the board's built-in **BOOT button**
(GPIO0, present on every ESP32-S3 dev board); typed input (WiFi password,
TOTP secrets) goes through the **USB-serial console** you already use to
flash it.

> **Use responsibly.** Modes 1–5 and 10 (WiFi scan, deauth/evil-twin/BLE
> detectors, packet monitor, probe sniffer) are passive/receive-only —
> they never transmit disruptive frames. Modes 6, 8 and 9 (**port
> scanner, deauth attack, beacon spam**) are active: they probe hosts or
> transmit real 802.11 frames. Only ever point them at a network and
> devices **you own or are explicitly authorized to test** (pentest
> engagement, CTF, your own lab gear). Doing this against networks you
> don't control is illegal in most jurisdictions.

## Wiring

| OLED pin | ESP32-S3 pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

If your board already uses GPIO8/9 for something else, change
`OLED_SDA_PIN` / `OLED_SCL_PIN` in `include/config.h` — nothing else
needs to change.

## Build & flash (PlatformIO)

```sh
pip install -U platformio       # once
pio run                         # build
pio run -t upload               # flash (board in download mode if needed)
pio device monitor               # open the serial console (115200 baud)
```

The project targets the `esp32-s3-devkitc-1` board definition, which
fits any generic ESP32-S3 WROOM dev board (DevKitC-1, "N16R8" clones,
etc.) as long as it has at least 4MB flash — the partition table used
(`huge_app.csv`) needs the extra room for WiFi + BLE + mbedtls.

### Arduino IDE alternative

1. Install the `esp32` board package (Espressif) and these libraries via
   Library Manager: **Adafruit SSD1306**, **Adafruit GFX Library**,
   **NimBLE-Arduino**, **ESP32Ping**.
2. Board: "ESP32S3 Dev Module". Partition Scheme: **Huge APP (3MB No
   OTA/1MB SPIFFS)**.
3. Copy `src/*.cpp` and `include/*.h` into one Arduino sketch folder (or
   open `src/main.cpp` after renaming it `*.ino` — Arduino IDE compiles
   every `.cpp`/`.h` in the sketch folder together either way).

## Controls

One button, three press lengths:

| Press | Action |
|---|---|
| Short (<0.6s) | Cycle menu / scroll a list |
| Long (0.6–2.5s) | Select / enter mode / mode-specific action (rescan, lock, etc.) |
| Very long (≥2.5s) | Always back to the main menu |

## The seven modes

1. **WiFi Scanner** — lists nearby APs by signal strength: SSID, channel,
   RSSI, encryption. `!` marks weak encryption (open/WEP/WPA1).
   Long press rescans.

2. **Deauth Detector** — passively counts 802.11 deauth/disassoc
   management frames per second while hopping all channels. Alarms past
   a threshold and shows the offending transmitter's MAC + channel.
   Long press locks onto the current channel (stops hopping) to dig in;
   short press resets the counters.

3. **Evil Twin Detector** — rescans periodically and tracks which
   BSSIDs + encryption types answer to each SSID over time. Flags an
   SSID as suspicious if it suddenly shows up with mixed encryption types
   or an unexpected extra transmitter — the classic "rogue AP cloning a
   known network's name" pattern. (Heuristic: legitimate mesh/multi-AP
   networks using one SSID + matching encryption are *not* flagged.)

4. **BLE Scanner** — lists nearby BLE advertisers (address, RSSI, name).
   `*` flags Apple "Find My" offline-finding broadcasts (seen from
   AirTags and similar accessories while separated from their owner —
   a heuristic, not a certainty). Also alarms on a burst of new/rotating
   addresses in a short window, the signature of BLE MAC-spam / fake
   pairing-request flood attacks.

5. **Packet Monitor** — bar graph of 802.11 traffic volume per channel,
   with a rolling statistical baseline; alarms when a cycle's total
   traffic is a statistical outlier vs. recent history (generic "the air
   just got busy" signal).

6. **Port Scanner** *(active, own network only)* — connects to the WiFi
   network you configured via serial (`wifi set <ssid> <password>`),
   ping-sweeps the local /24 to find live hosts, then probes each one
   against ~19 commonly-interesting TCP ports and lists what's open.

7. **TOTP Generator** — RFC 6238 six-digit codes for accounts you add via
   serial (`totp add <label> <base32secret>`), same algorithm as Google
   Authenticator / Authy. Needs one WiFi connection on first use to fetch
   accurate time over NTP, then runs offline for the rest of the session.

8. **Deauth Attack** *(active, authorized testing only)* — ESP32Marauder-
   style: scan, pick a target AP (SHORT=next, LONG=lock target), then
   broadcasts deauth frames to all its clients on that channel until you
   stop it (SHORT).

9. **Beacon Spam** *(active, authorized testing only)* — broadcasts fake
   beacon frames for a rotating list of built-in SSIDs across all
   channels, each from a random MAC. SHORT pauses/resumes.

10. **Probe Sniffer** *(passive)* — logs which nearby device MACs are
    probing for which remembered SSIDs; classic WiFi recon technique.

## Serial console

Open the serial monitor (115200 baud) and type:

```
help
wifi set <ssid> <password>
wifi show
wifi clear
totp add <label> <base32secret>
totp del <label>
totp list
```

Credentials and secrets are stored in NVS (flash), not in source code.

## Project layout

```
platformio.ini          build config, library versions, partition table
include/config.h        pin map + every tunable threshold in one place
include/ui.h / src/ui.cpp          OLED helpers + single-button input
include/storage.h / src/storage.cpp  NVS-backed WiFi creds + TOTP accounts
include/serial_console.h / .cpp    the `wifi` / `totp` serial commands
include/<mode>.h / src/<mode>.cpp  one pair of files per mode (see above)
src/main.cpp            menu + mode state machine wiring it all together
```

Each mode module exposes the same three functions — `enter()`, `loop(ButtonEvent)`,
`exit()` — called by `main.cpp`'s state machine, so adding an 8th mode is
just: write `include/foo.h` + `src/foo.cpp` with that shape, add one line
to the `menuItems[]` table in `main.cpp`.
