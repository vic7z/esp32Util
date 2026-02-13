# ESP32 Pocket RF Tool

WiFi and BLE scanner, security monitor, and diagnostics tool for ESP32.

![Version](https://img.shields.io/badge/version-v2.0.0-blue)
![Platform](https://img.shields.io/badge/platform-ESP32%2C%20ESP32--C3%2C%20ESP32--S2-green)

## What it does

Scans WiFi networks and BLE devices, monitors for deauth attacks and rogue APs, analyzes channel usage, and tracks signal strength — all from a pocket-sized ESP32 with a 128x64 OLED. v2.0 adds a built-in web server so you can view everything from your phone too.

## Hardware

Runs on ESP32, ESP32-S2, and ESP32-C3. Default pin config is for a Waveshare ESP32-C3-Zero with an I2C OLED and two buttons:

| Component | Pin | Notes |
|-----------|-----|-------|
| Action Button | GPIO 2 | Pull-up required |
| Back Button | GPIO 3 | Pull-up required |
| OLED SDA | GPIO 4 | I2C |
| OLED SCL | GPIO 5 | I2C |
| RGB LED (NeoPixel) | GPIO 10 | Onboard LED is GPIO 8 |

## Controls

- **Action button** — short press to navigate, long press (>500ms) to confirm/enter/toggle views
- **Back button** — short press to go back, long press (>1.5s) for deep sleep (needs RESET to wake)

## OLED Menu

### Main Menu
- **Auto Watch** — combined WiFi/BLE/deauth monitoring with 4 views (summary, top APs, top BLE, channel APs), auto channel hopping
- **RF Health** — environment health score with congestion and signal quality. Long press for RSSI graph
- **Live Monitor** — real-time packet capture: packets/sec, RSSI, beacon/data/deauth breakdown
- **Channel Analyzer** — per-channel traffic analysis across all 13 channels
- **Device Monitor** — tracks WiFi clients (via sniffing) and BLE devices over time, shows MAC, vendor, presence status
- **AP Scanner** — list APs with detail view, walk test (RSSI tracking with graph), side-by-side compare
- **BLE Monitor** — list BLE devices with detail view and walk test

### Security
- **Deauth Watch** — real-time deauth packet counting with configurable alert threshold
- **Rogue AP Watch** — detects evil twin APs (same SSID, different BSSID)
- **BLE Tracker Watch** — flags suspicious BLE tracking devices
- **Alert Settings** — deauth threshold (5-20 pkt/s), screen timeout

### Insights
- **Why Is It Slow?** — diagnoses WiFi issues, shows channel congestion. Long press to toggle RSSI graph of top 3 APs
- **Channel Recommendation** — finds the least congested channel
- **Environment Change** — compare current RF environment vs a saved baseline

### History
- **Event Log** — timestamped security/system events (up to 10)
- **Baseline Compare** — AP count, RSSI, and packet count changes over time

### System
- **Battery & Power** — uptime, free RAM, flash size
- **Display** — RGB brightness (0-100%), screen timeout
- **Radio Control** — manual channel selection (1-13)
- **About** — firmware version and build date

## Web Server

Start from the main menu. The ESP32 creates a WiFi AP you connect to from your phone/laptop:

- SSID: `ESP32-Tool` / Password: `12345678`
- URL: `http://esp32.util` or `http://192.168.4.1`

The web UI has:
- **Dashboard** — live WiFi/BLE device counts, security status, channel recommendation, signal history graph
- **WiFi Networks** — all detected APs with filters (all/open/secure), sortable, click for details (vendor, channel, signal quality, estimated distance, security). CSV export
- **BLE Devices** — detected BLE devices with filters (all/trackers/beacons), click for details. CSV export
- **Analyzer** — live channel usage bar chart, spectrum view, channel stats, best channel recommendation
- **Security** — deauth attack graph, rogue AP detection, BLE tracker count, event log. JSON export
- **Tools** — signal tracker for specific APs, full scan trigger, data export (JSON/CSV)
- **Settings** — configure scan speed, RSSI threshold, brightness, timeouts, power mode

## LED Colors

| Color | Meaning |
|-------|---------|
| Red | Deauth attack detected |
| Cyan | Signal alert / BLE active |
| Green | Low traffic (<40%) |
| Yellow | Medium traffic (40-70%) |
| Orange | High traffic (>70%) |
| Blue | Channel Analyzer active |
| Purple | Hidden SSID scanner active |
| White | Menu / idle |

## Sleep Behavior

Scanning screens (Auto Watch, Live Monitor, Channel Analyzer, Device Monitor, AP/BLE Scanner, security screens, walk tests, web server) stay awake. Everything else respects the screen timeout setting.

When sleeping: display off, LED off, WiFi off, BLE off.

- Auto sleep: press any button to wake
- Deep sleep (long press BACK): press RESET to wake

## Specs

- 2.4GHz WiFi, channels 1-13
- Bluetooth 5.0 LE
- Partition: Huge APP (3MB firmware / 1MB SPIFFS)
- Web UI stored on SPIFFS, separate from firmware
- Tracks up to 20 APs, 20 BLE devices, 15 monitored devices, 10 security events

## Build

```bash
# install dependencies
arduino-cli core install esp32:esp32
arduino-cli lib install U8g2 "Adafruit NeoPixel"

# compile
arduino-cli compile --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app .

# upload (replace COM3 with your port)
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app .
```

### Flashing the Web UI (SPIFFS)

The web UI lives in `data/index.html` and gets flashed to SPIFFS separately from firmware:

```bash
# build SPIFFS image
mkspiffs -c data -b 4096 -p 256 -s 917504 spiffs.bin

# flash it
esptool --chip esp32c3 --port COM3 --baud 921600 write_flash 0x310000 spiffs.bin
```

SPIFFS partition: 917504 bytes (0xE0000) starting at flash offset 0x310000.

Tool paths on Windows (via Arduino):
- `mkspiffs`: `%LOCALAPPDATA%/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs.exe`
- `esptool`: `%LOCALAPPDATA%/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool.exe`

After flashing new firmware, re-flash the SPIFFS image if it got erased — they're independent partitions.

## Troubleshooting

**No WiFi APs detected** — make sure you're on a screen that scans (Auto Watch, AP Scanner, etc). Check antenna.

**No BLE devices** — BLE takes a few seconds to start. Make sure devices are advertising nearby.

**Screen won't sleep** — scanning screens prevent auto-sleep by design. Check if timeout is set to "Never".

**Web server won't start** — needs the Huge APP partition scheme. Make sure SPIFFS is flashed.

**Web page says "File not found"** — SPIFFS needs to be flashed separately. See [Flashing the Web UI](#flashing-the-web-ui-spiffs).

**Can't connect to web interface** — connect to `ESP32-Tool` WiFi (password: `12345678`), then try `http://192.168.4.1` if the hostname doesn't resolve.

## Version History

**v2.0.0** (2026-02-13) — Web server with full web UI (dashboard, analyzer, security, tools, settings). Accessible from main menu.

**v1.4.0** (2026-02-12) — OLED UI overhaul: area-fill graphs, redesigned channel analyzer, updated Auto Watch/RF Health visuals. Fixed button responsiveness during WiFi scan delays.

**v1.3.3** (2026-01-28) — Fixed AP count showing 0 in Auto Watch (type mismatch).

**v1.3.2** (2026-01-28) — Fixed BLE showing random numbers in Auto Watch.

**v1.3.0** (2026-01-28) — Device Monitor now sniffs WiFi clients instead of just listing APs. Added deep sleep mode. RSSI graph in RF Health. Fixed BLE/WiFi coexistence.

**v1.2.1** (2026-01-21) — Fixed NeoPixel staying on during sleep. Added RSSI graphs to walk tests. Better button responsiveness (300ms -> 150ms cooldown). Long-press BACK to sleep.

**v1.2.0** (2026-01-21) — Fixed BLE random number bug. Added RSSI graph to "Why Is It Slow".

**v1.1.0** (2026-01-21) — Fixed WiFi scan timing across all screens (delays were too short, scans returned empty).

**v1.0.0** — Initial release.
