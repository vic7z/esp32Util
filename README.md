# ESP32 Pocket RF Tool

A professional WiFi and Bluetooth LE analyzer, security monitor, and diagnostic tool for ESP32 microcontrollers.

![Version](https://img.shields.io/badge/version-v1.4.0-blue)
![Platform](https://img.shields.io/badge/platform-ESP32%2C%20ESP32--C3%2C%20ESP32--S2-green)

## Overview

The ESP32 Pocket RF Tool is a comprehensive wireless analysis device that fits in your pocket. It provides real-time WiFi and BLE scanning, security monitoring, signal analysis, and now features an advanced web interface for remote monitoring and control.

## Hardware Support

This firmware is designed to run on **ESP32**, **ESP32-S2**, and **ESP32-C3** boards.

**Reference Setup (Waveshare ESP32-C3-Zero):**
The default configuration is set for a Waveshare ESP32-C3-Zero with an I2C OLED and two buttons.

| Component | Default Pin (C3 Zero) | Notes |
|-----------|-----------------------|-------|
| Action Button (Select/Navigate) | GPIO 2 | Pull-up required |
| Back Button (Cancel/Sleep) | GPIO 3 | Pull-up required |
| OLED SDA | GPIO 4 | I2C Data |
| OLED SCL | GPIO 5 | I2C Clock |
| RGB LED (NeoPixel) | GPIO 10 | **Note:** Onboard LED is GPIO 8 |

## Features

### Main Menu

#### 1. Auto Watch
Automated WiFi and BLE monitoring with channel hopping.
- **4 Views** (press SELECT to cycle):
  - Summary: Overview of APs, BLE devices, channel, packet count, deauth detection
  - Top APs: Shows strongest 4 WiFi access points
  - Top BLE: Shows strongest 4 BLE devices
  - Channel APs: Shows all APs on current sniffer channel
- **Auto-cycles** through channels 1-11 every second
- **Scans** WiFi every 5 seconds while monitoring

#### 2. RF Health
Real-time RF environment health analysis.
- Channel congestion visualization
- Network density metrics
- Signal quality indicators

#### 3. Live Monitor
Real-time packet capture and analysis.
- Packets/second (current and peak)
- Average RSSI
- Beacon/Data/Deauth packet breakdown
- Real-time load percentage with visual bar

#### 4. Channel Analyzer
Per-channel traffic analysis across all 13 WiFi channels.
- Beacon and deauth packet counts per channel
- Channel-by-channel hopping (200ms per channel)
- Identify busiest and quietest channels

#### 5. Device Monitor
Track WiFi and BLE devices over time.
- **Shows** up to 15 devices with:
  - Device type (W=WiFi, B=BLE)
  - Presence status (+/-)
  - Device name
  - **MAC address** (BSSID for WiFi, BLE address for BLE)
  - RSSI strength
- **Device Detail View**: First seen, last seen, total times seen
- **Auto-timeout**: Devices marked absent after 30 seconds

#### 6. AP Scanner
WiFi access point scanner and analyzer.
- Lists all detected access points
- **AP Detail View**: SSID, BSSID, RSSI, channel, security
- **Walk Test**: RSSI tracking over time (stats + graph views)
- **Compare**: Side-by-side AP comparison

#### 7. BLE Monitor
Bluetooth Low Energy device scanner.
- Lists all detected BLE devices
- **BLE Detail View**: Name, address, RSSI, first/last seen
- **Walk Test**: BLE RSSI tracking (stats + graph views)

### Security Menu

#### 1. Deauth Watch
Monitor for WiFi deauthentication attacks.
- Real-time deauth packet counting
- Configurable alert threshold
- Attack detection and alerts

#### 2. Rogue AP Watch
Detect rogue/evil twin access points.
- Compares known AP BSSIDs
- Alerts on SSID reuse with different BSSID

#### 3. BLE Tracker Watch
Detect suspicious BLE tracking devices.
- Monitors for known tracker patterns
- Alerts on suspicious device behavior

#### 4. Alert Settings
Configure security alert thresholds.
- Deauth threshold: 5-20 packets/second
- Screen timeout: Never, 30s, 60s, 120s, 300s

### Insights Menu

#### 1. Why Is It Slow?
Diagnose WiFi performance issues.
- **2 Views** (LONG press to toggle):
  - Analysis: Channel congestion, interference, recommendations
  - RSSI Graph: Real-time RSSI tracking of top 3 APs
- Identifies busy channels and overlapping networks

#### 2. Channel Recommendation
Find the best WiFi channel for your network.
- Analyzes all 13 channels
- Recommends least congested channel
- Shows AP count per channel

#### 3. Environment Change
Compare current RF environment vs baseline.
- Save baseline snapshot (LONG press)
- Shows changes in AP count, RSSI, channel distribution
- Useful for troubleshooting new interference

#### 4. Quick Snapshot
Quick overview of RF environment.
- Total WiFi APs
- Total BLE devices
- Average RSSI
- Busiest channel

#### 5. Channel Scorecard
Visual congestion score for all 13 channels.
- Bar graph showing AP count per channel
- 2-column layout for easy comparison

### History Menu

#### 1. Event Log
View security and system events.
- Timestamps
- Event types (security, system)
- Event descriptions
- Stores up to 10 events

#### 2. Baseline Compare
Compare current environment vs saved baseline.
- AP count changes
- RSSI changes
- Packet count changes
- Time since baseline

#### 3. Export Data
Export collected data to Serial Monitor.
- **Press SELECT** to export
- **Exports**: WiFi APs (SSID, BSSID, RSSI, channel), BLE devices (name, address, RSSI), Security events
- **Format**: CSV-style output at 115200 baud
- View exported data in Arduino Serial Monitor

### System Menu

#### 1. Battery & Power
System information display.
- Uptime (hours:minutes:seconds)
- Free RAM (KB)
- Flash size (MB)

#### 2. Display
Display settings configuration.
- **RGB Brightness**: 0-100% (increments of 10%)
- **Screen Timeout**: Never, 30s, 60s, 120s, 300s
- Settings saved to non-volatile storage

#### 3. Radio Control
Manual channel selection.
- SELECT: Next channel (1→13)
- HOLD: Previous channel
- Useful for focused monitoring

#### 4. Power Mode
Battery optimization modes.
- **Normal**: Full performance (default)
- **Eco**: Reduced scan rate for battery savings
- **Ultra**: Maximum battery conservation
- Settings saved to non-volatile storage

#### 5. About
Firmware information.
- Version number
- Build date
- Features list

### Web Server ⭐ NEW in v2.0!
The Web Server is now directly accessible from the Main Menu!

Start the web server to access a powerful web interface from any device:
- **SSID**: `ESP32-Tool`
- **Password**: `12345678`
- **URL**: `http://esp32.util` or `http://192.168.4.1`

#### Web Interface Features

**📊 Dashboard**
- Real-time WiFi/BLE device counters
- Security alert status
- Best channel recommendation
- Live signal history graph
- Channel distribution visualization

**📶 WiFi Networks**
- Complete list of all detected APs
- **Filter tabs**: All / Open Only / Secure Only
- Sortable by RSSI or Channel
- **Click any AP for detailed info:**
  - SSID and BSSID
  - Vendor identification
  - Channel and frequency
  - Signal strength with quality rating
  - Estimated distance
  - Security type
- Export to CSV

**🔷 BLE Devices**
- Complete list of detected BLE devices
- **Filter tabs**: All / Trackers / Beacons
- **Click any device for detailed info:**
  - Device name and MAC address
  - Device type (iBeacon/Eddystone/Tracker/Generic)
  - Signal strength
  - Advertisement details
- Export to CSV

**📡 Analyzer**
- **Live Channel Usage**: Visual bar chart for all 13 channels
- **Smart Recommendations**: AI-powered best channel selection
- **Live Spectrum**: Real-time RF spectrum visualization
- **Channel Statistics**: Detailed stats for channels 1, 6, 11
- Auto-updates every 2 seconds

**🛡️ Security**
- Deauth attack monitor with live graph
- Rogue AP detection with BSSID details
- BLE tracker counter
- Security event log with timestamps
- Export events as JSON

**🧰 Tools** ⭐ NEW!
- **Signal Tracker**: Track specific AP signal strength over time
- Full Scan (WiFi + BLE simultaneously)
- Export all data (JSON/CSV)

**⚙️ Settings**
- Configure all device settings remotely
- Scan speed, RSSI threshold, RGB brightness
- Screen timeout, deauth threshold, power mode

## Button Controls

### Action Button (GPIO 2)
- **SHORT Press**: Navigate down / Select next option
- **LONG Press** (>500ms): Confirm / Enter / Toggle view

### Back Button (GPIO 3)
- **SHORT Press**: Return to previous screen / Cancel
- **LONG Press** (>1.5s): **Deep Sleep** - turns off display, LED, and WiFi/BLE modems. Requires **RESET** to wake.

## LED Status Indicators

| Color | Meaning |
|-------|---------|
| 🔴 Red | Attack detected (deauth) |
| 🔵 Cyan | Signal alert / BLE active |
| 🟢 Green | Low traffic (<40% load) |
| 🟡 Yellow | Medium traffic (40-70% load) |
| 🟠 Orange | High traffic (>70% load) |
| 🔵 Blue | Channel Analyzer active |
| 🟣 Purple | Hidden SSID scanner active |
| ⚪ White | Default / Menu |

## Power Management

### Auto Sleep
- **Screens that NEVER auto-sleep** (actively scanning):
  - Auto Watch, Live Monitor, Channel Analyzer
  - Device Monitor, AP Scanner, BLE Monitor
  - All Security screens (Deauth Watch, Rogue AP Watch, BLE Tracker Watch)
  - RSSI graphs and walk tests
  - **Web Server** (active web interface)

- **When sleeping**:
  - OLED display turns off
  - RGB LED turns off
  - **WiFi modem stops** (full shutdown)
  - **BLE modem stops** (full shutdown)
  - Maximum power savings

### Wake Up
- **From Auto Sleep:** Press any button.
- **From Deep Sleep:** Press the RESET button on the board.

## Technical Specifications

- **WiFi**: 2.4GHz, Channels 1-13
- **BLE**: Bluetooth 5.0 LE
- **Partition Scheme**: Huge APP (3MB No OTA/1MB SPIFFS)
- **Web UI Storage**: SPIFFS filesystem (separate from firmware)
- **Web Interface**: Real-time updates every 2 seconds

## Data Limits

| Feature | Limit |
|---------|-------|
| WiFi APs tracked | 20 |
| BLE devices tracked | 20 |
| Monitored devices | 15 |
| Security events logged | 10 |
| Walk test history | 60 samples |
| RSSI history (graphs) | 50 samples per AP |
| Device timeout | 30 seconds |

## Build & Upload

### Prerequisites
```bash
arduino-cli core install esp32:esp32
arduino-cli lib install U8g2 "Adafruit NeoPixel"
```

### Compile
```bash
arduino-cli compile --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app .
```

### Upload
```bash
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app .
```
*Note: Replace COM3 with your actual port (check with `arduino-cli board list`)*

### Flash Web UI (SPIFFS)

The web interface HTML is stored on the ESP32's SPIFFS filesystem, separate from the firmware. This means you can update the web UI without recompiling the firmware.

The web UI source file is located at `data/index.html`.

**Build the SPIFFS image:**
```bash
mkspiffs -c data -b 4096 -p 256 -s 917504 spiffs.bin
```

**Flash the SPIFFS image to the ESP32:**
```bash
esptool --chip esp32c3 --port COM3 --baud 921600 write_flash 0x310000 spiffs.bin
```

| Parameter | Value | Description |
|-----------|-------|-------------|
| Block size (`-b`) | 4096 | SPIFFS block size |
| Page size (`-p`) | 256 | SPIFFS page size |
| Partition size (`-s`) | 917504 (0xE0000) | Size of the SPIFFS partition |
| Flash offset | 0x310000 | Start address of the SPIFFS partition |

**Tool locations (Windows, installed via Arduino):**
- `mkspiffs`: `%LOCALAPPDATA%/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs.exe`
- `esptool`: `%LOCALAPPDATA%/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool.exe`

*Note: After flashing new firmware, you must also flash the SPIFFS image if it was erased. The SPIFFS partition is independent of the firmware partition.*

## Troubleshooting

### No WiFi APs Detected
- Ensure WiFi is active (Auto Watch, AP Scanner, etc.)
- Check antenna connection
- Try different channels

### No BLE Devices
- BLE takes a few seconds to start scanning
- Ensure BLE devices are advertising nearby
- Try BLE Monitor for dedicated scanning

### Screen Won't Sleep
- Check if you're on a scanning screen (these prevent auto-sleep)
- Verify screen timeout is not set to "Never"
- Use LONG press BACK for manual sleep

### Device Monitor Shows "Scanning..."
- Wait 2-3 seconds for first scan to complete
- Ensure WiFi/BLE devices are nearby
- Check if modems are active (not in sleep mode)

### Web Server Won't Start
- Ensure you're using the **Huge APP** partition scheme
- Ensure the SPIFFS image has been flashed (see [Flash Web UI](#flash-web-ui-spiffs))
- Try resetting the device

### Web Page Shows "File not found"
- The SPIFFS image needs to be flashed separately from the firmware
- Re-flash the SPIFFS image using the commands in [Flash Web UI](#flash-web-ui-spiffs)
- Ensure `data/index.html` exists before building the SPIFFS image

### Can't Connect to Web Interface
- Verify you're connected to `ESP32-Tool` WiFi
- Check password: `12345678`
- Try both URLs: `http://esp32.util` and `http://192.168.4.1`
- Ensure web server is running (check OLED display)

## Version History

### v1.4.0 (Latest) - 2026-02-12
- **Improved OLED graphs**: Area fill under curves, current-value dots, finer dashed grids, middle axis labels
- **Walk test mini-graphs**: Connected line graphs with area fill replacing choppy vertical bars
- **Multi-AP graph**: Distinct line styles (solid/dashed/dotted) for each AP in "Why Is It Slow"
- **Channel Analyzer redesign**: Modern layout with highlighted selected channel, best/worst visual markers, grid lines
- **Auto Watch UI**: Signal quality bar, dotted separators, status indicator circles
- **RF Health**: Segmented 5-bar health meter with percentage readout
- **Button responsiveness fix**: Buttons no longer ignored during WiFi scan delays (1.5s windows)

### v1.3.3 - 2026-01-28
- Fixed AP count showing 0 in Auto Watch (type mismatch bug)
- Fixed BLE random numbers in Auto Watch with static caching
- Device Monitor shows WiFi clients, deep sleep mode, RSSI graph in RF Health

### v2.0
- Web Server moved to Main Menu
- Completely redesigned web interface
- Channel recommendations, signal tracker, live spectrum in web UI
- Click on AP/BLE devices for detailed info
- Rogue AP detection in web UI

### v1.x
- Initial release
- OLED menu system
- WiFi/BLE scanning
- Security monitoring
- Walk tests
