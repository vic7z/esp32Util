# ESP32 WiFi/BLE Utility

A WiFi and Bluetooth LE analyzer and security monitor for ESP32 microcontrollers.

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
arduino-cli compile --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app esp32Util.ino
```

### Upload
```bash
arduino-cli upload -p COM7 --fqbn esp32:esp32:esp32c3:PartitionScheme=huge_app esp32Util.ino
```
*Note: Replace COM7 with your actual port*

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