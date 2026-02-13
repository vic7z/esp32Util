# ESP32 Pocket RF Tool

WiFi and BLE scanner, security monitor, and RF diagnostics tool for ESP32.

![Version](https://img.shields.io/badge/version-v2.0.2-blue)
![Platform](https://img.shields.io/badge/platform-ESP32%2C%20ESP32--C3%2C%20ESP32--S2-green)

## Features

- **WiFi Scanning** — detect and list nearby access points with SSID, BSSID, channel, RSSI, security type, vendor, estimated distance, and signal quality grade (A-F)
- **BLE Scanning** — discover Bluetooth Low Energy devices with name, address, RSSI, and advertisement type (Generic, iBeacon, Eddystone)
- **Security Monitoring** — real-time deauthentication attack detection, rogue AP (evil twin) detection, and BLE tracker identification
- **Channel Analysis** — per-channel traffic breakdown across all 13 channels with overlap detection and best channel recommendation
- **Client Tracking** — passive WiFi client detection via packet sniffing (probe requests, association frames, data frames) with vendor lookup
- **Walk Tests** — RSSI signal strength tracking over time for both WiFi APs and BLE devices, with live graph and min/max/avg stats
- **RF Health Scoring** — environment health assessment with congestion percentage, quality rating, and actionable insights
- **Web Interface** — full-featured responsive web UI accessible from any phone or laptop over WiFi, with dark/light theme
- **Deep Sleep** — true deep sleep mode (~5uA) for battery-powered operation
- **NeoPixel Status LED** — color-coded RF status at a glance

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

The device has a hierarchical menu system with 11 main items and 4 submenus, all navigated with two buttons.

### Main Menu

**Auto Watch** — combined WiFi, BLE, and deauth monitoring on a single screen. Cycles through 4 views: summary (AP count, BLE count, deauth count), top APs by signal strength, top BLE devices, and APs on the current channel. Auto channel hopping captures traffic across all 13 channels.

**RF Health** — overall environment health score showing RF load percentage, quality rating (Good/OK/Busy/Avoid), and a text insight (e.g., "Heavy Traffic", "Very Clean", "Possible Attack"). Long press toggles a full-screen RSSI graph tracking average signal strength over 60 samples.

**Live Monitor** — real-time packet capture display showing packets per second, smoothed PPS, average RSSI, and a breakdown of beacon, data, probe, and deauth frame counts. Useful for diagnosing interference and attacks.

**Channel Analyzer** — scans all 13 channels and displays a bar chart of traffic per channel. Highlights the best channel (lowest traffic) and marks congested channels. Shows channel number labels and overlap indicators.

**Device Monitor** — tracks WiFi clients and BLE devices over time. WiFi clients are detected passively via promiscuous mode sniffing (probe requests, association requests, data frames). Each device shows type (C=Client, B=BLE), name or vendor, MAC address, RSSI, presence status (+/-), first/last seen times, and total times seen. Supports up to 15 tracked devices with a 30-second timeout.

**AP Scanner** — lists detected WiFi access points sorted by signal strength. Each entry shows signal bars, SSID, BSSID, channel, RSSI, and security type (Open/WEP/WPA2). Select an AP for a detail view with vendor, estimated distance, and signal grade. From the detail view you can start a walk test or compare two APs side by side.

**BLE Monitor** — lists detected BLE devices sorted by signal strength. Each entry shows name (or "Unknown"), address, RSSI, and advertisement type. Select a device for details or start a BLE walk test to track its signal over time.

### Security Submenu

**Deauth Watch** — monitors for deauthentication and disassociation frames in real time. Displays deauth packets per second with a configurable alert threshold. Triggers a red LED and logs a security event when an attack is detected.

**Rogue AP Watch** — detects evil twin access points by finding multiple BSSIDs advertising the same SSID. Lists detected rogues with both BSSIDs for investigation.

**BLE Tracker Watch** — identifies suspicious BLE devices that may be tracking you (e.g., AirTags, Tiles). Flags devices that appear consistently across scans.

**Alert Settings** — configure deauth alert threshold (5-20 packets/second) and screen timeout duration.

### Insights Submenu

**Why Is It Slow?** — diagnoses WiFi performance issues by analyzing channel congestion, nearby AP count, and signal quality. Provides a text explanation of likely causes. Long press toggles an RSSI graph tracking the top 3 strongest APs over 50 samples.

**Channel Recommendation** — scans all channels and recommends the least congested one. Prioritizes non-overlapping channels (1, 6, 11) and shows the AP count per channel.

**Environment Change** — compares the current RF environment against a saved baseline snapshot. Shows differences in AP count, average RSSI, channel distribution, and total packet count.

### History Submenu

**Event Log** — timestamped log of security and system events (deauth attacks, rogue APs, tracker alerts). Stores the last 10 events with scrollable list view.

**Baseline Compare** — shows AP count, RSSI, and packet count changes between the current snapshot and a previously saved baseline.

### System Submenu

**Battery & Power** — displays uptime, free RAM, flash size, and power-related metrics.

**Display** — adjust RGB LED brightness (0-100%) and screen timeout setting.

**Radio Control** — manually select a WiFi channel (1-13) for targeted monitoring.

**About** — shows firmware version and build date.

## Web Server

Start the web server from the main menu. The ESP32 creates a WiFi access point you connect to from your phone or laptop:

- **SSID**: `ESP32-Tool` (configurable)
- **Password**: `12345678` (configurable)
- **URL**: `http://esp32.util` or `http://192.168.4.1`

The web UI is a responsive single-page app with dark and light themes, stored on SPIFFS.

### Dashboard
Live overview with 6 stat cards (WiFi count, BLE count, alerts, best channel, RF load, uptime), a signal history chart, channel distribution bars, session stats (total APs found, peak PPS, total packets, deauth count), and RF health summary.

### WiFi Tab
Table of all detected access points with signal bars, SSID, BSSID, channel, RSSI, security badge (Open/WEP/WPA2), and quality grade (A-F). Click any row for a detail modal showing vendor, estimated distance, and a button to start a walk test. Walk test shows live RSSI graph with current/min/max/avg stats. Hidden networks shown in a separate table when detected. Scan and Clear buttons for manual control.

### BLE Tab
Table of all detected BLE devices with signal bars, name, address, RSSI, and type (Generic/iBeacon/Eddystone). Click any row for a detail modal. BLE scanning runs automatically in the background.

### Analyzer Tab
Channel usage bar chart (click any bar to open the packet monitor), best channel recommendation cards for channels 1/6/11, channel overlap visualization, and environment baseline tools (take snapshot, save baseline, compare).

### Packet Monitor
Opens when you click a channel bar in the Analyzer. Shows real-time per-channel packet capture with stats (total, beacons, data, probes, deauth, PPS, peak PPS), a live PPS graph, unique device count, and a toggleable packet log with per-packet details (timestamp, type, SSID, BSSID, channel, RSSI). Channel navigation buttons to switch between channels. Export captured packets as CSV.

### Security Tab
Three stat cards (deauth count, rogue APs, trackers), rogue AP detail list, security event log, and encryption breakdown bar chart (WPA3/WPA2/WPA/WEP/Open).

### Clients Tab
Real-time client monitor table showing device type (C=WiFi Client, B=BLE), name/vendor, MAC address, channel, RSSI, presence indicator (+/-), and times seen. Start/Stop button to control promiscuous mode sniffing. Clear button to reset the device list.

### Settings Tab
Configure scan speed (Fast/Normal/Thorough), RSSI threshold, RGB brightness, screen timeout, deauth threshold, power mode (Balanced/Performance/Power Save), and WiFi AP name/password (requires restart).

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

The web UI lives in the `data/` folder (index.html, app.js, style.css) and gets flashed to SPIFFS separately from firmware:

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

**Client monitor not detecting devices** — make sure no WiFi scan is running simultaneously. The monitor uses promiscuous mode which is paused during scans.

## Version History

**v2.0.2** (2026-02-14) — Fixed packet counting: added explicit promiscuous filter for data frames (ESP32-C3 was not delivering data packets), fixed double-counting in channel analyzer (sniffer and handler both incremented per-channel arrays), fixed `channelLoad()` double-weighting beacons, added per-channel data and probe tracking, and reset all counters between channel hops. Live Monitor now shows probe packets separately (B/D/P/X).

**v2.0.1** (2026-02-14) — Fixed web UI not loading scan data (float/int format mismatch in JSON serialization). Fixed client monitor dying permanently when a background WiFi scan failed (scan failure left promiscuous mode disabled). Increased API JSON buffer to 8KB. Auto-scan now pauses while client monitor is active.

**v2.0.0** (2026-02-13) — Web server with full web UI (dashboard, analyzer, security, tools, settings). Accessible from main menu.

**v1.4.0** (2026-02-12) — OLED UI overhaul: area-fill graphs, redesigned channel analyzer, updated Auto Watch/RF Health visuals. Fixed button responsiveness during WiFi scan delays.

**v1.3.3** (2026-01-28) — Fixed AP count showing 0 in Auto Watch (type mismatch).

**v1.3.2** (2026-01-28) — Fixed BLE showing random numbers in Auto Watch.

**v1.3.0** (2026-01-28) — Device Monitor now sniffs WiFi clients instead of just listing APs. Added deep sleep mode. RSSI graph in RF Health. Fixed BLE/WiFi coexistence.

**v1.2.1** (2026-01-21) — Fixed NeoPixel staying on during sleep. Added RSSI graphs to walk tests. Better button responsiveness (300ms -> 150ms cooldown). Long-press BACK to sleep.

**v1.2.0** (2026-01-21) — Fixed BLE random number bug. Added RSSI graph to "Why Is It Slow".

**v1.1.0** (2026-01-21) — Fixed WiFi scan timing across all screens (delays were too short, scans returned empty).

**v1.0.0** — Initial release.
