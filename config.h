#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#define FW_VERSION "v1.4.0"
#define FW_DATE "2026-02-12"

#define BTN_ACTION 2
#define BTN_BACK_PIN 3
#define RGB_LED_PIN 10
#define RGB_LED_COUNT 1
#define RGB_ENABLED true

#define RGB_OFF     rgb.Color(0, 0, 0)
#define RGB_GREEN   rgb.Color(0, 50, 0)
#define RGB_BLUE    rgb.Color(0, 0, 50)
#define RGB_YELLOW  rgb.Color(50, 50, 0)
#define RGB_ORANGE  rgb.Color(50, 15, 0)
#define RGB_RED     rgb.Color(50, 0, 0)
#define RGB_PURPLE  rgb.Color(25, 0, 25)
#define RGB_WHITE   rgb.Color(20, 20, 20)
#define RGB_CYAN    rgb.Color(0, 25, 25)

#define DEBOUNCE_MS 30
#define SHORT_PRESS_MS 300
#define LONG_PRESS_MS 700
#define DOUBLE_CLICK_MS 600

enum ButtonEvent { BTN_NONE, BTN_SHORT, BTN_LONG, BTN_BACK, BTN_BACK_LONG };

#define MAX_CHANNEL 13
#define HISTORY_SIZE 128
#define MAX_APS 20
#define AP_VISIBLE 3
#define MAX_HIDDEN_SSIDS 10
#define MAX_SSID_LEN 32
#define MAX_ROGUE_APS 5

#define MAX_BLE_DEVICES 20
#define BLE_VISIBLE 3
#define MAX_EVENTS 10
#define WALK_HISTORY_SIZE 60

#define RSSI_HISTORY_SIZE 50
#define MAX_TRACKED_APS 3

#define MAX_MONITORED_DEVICES 15
#define DEVICE_TIMEOUT_MS 30000

// Timing
#define WIFI_SCAN_DELAY_MS      1500
#define BLE_SCAN_INTERVAL_MS    2000
#define BLE_TIMEOUT_MS          10000
#define BUTTON_COOLDOWN_MS      150

// RSSI Thresholds
#define RSSI_EXCELLENT          -40
#define RSSI_GOOD               -55
#define RSSI_FAIR               -70
#define RSSI_POOR               -85

// Load Thresholds
#define LOAD_GOOD               20
#define LOAD_OK                 40
#define LOAD_BUSY               70

struct Settings {
  uint8_t scanSpeed;
  int8_t rssiThreshold;
  uint8_t rgbBrightness;
  bool autoRefresh;
  bool csvLogging;
  uint8_t deauthThreshold;
  uint16_t screenTimeout;
  uint8_t powerMode;
  char apSSID[33];      // AP SSID (max 32 + null)
  char apPassword[65];  // AP Password (max 64 + null)
};

struct HiddenSSID {
  char ssid[MAX_SSID_LEN + 1];
  int8_t rssi;
  uint8_t channel;
  bool active;
};

struct RogueAP {
  char ssid[MAX_SSID_LEN + 1];
  uint8_t bssid1[6];
  uint8_t bssid2[6];
  bool active;
};

// Refactoring Structs
struct ScanState {
  uint32_t lastScan;
  uint32_t lastAutoWifiScan;
  uint32_t lastAutoBLEScan;
  uint32_t lastBLELoop;
  uint32_t lastEnvCheck;
  uint32_t lastBaselineUpdate;
  uint32_t lastBLESort;
  uint32_t lastSecond;
  uint32_t analyzerLastHop;
  uint8_t currentChannel;
  uint8_t selectedChannel;
  uint8_t analyzerChannel;
  bool frozen;
};

struct AutoWatchState {
  uint16_t totalAPs;
  uint16_t totalBLE;
  uint8_t viewMode;
};

struct WalkTestState {
  bool active;
  char targetSSID[33];
  uint8_t targetBSSID[6];
  char targetBLEAddr[18];
  int8_t rssiHistory[WALK_HISTORY_SIZE];
  uint8_t historyIndex;
  int8_t minRSSI;
  int8_t maxRSSI;
  int32_t rssiSum;
  uint16_t sampleCount;
  uint8_t viewMode;
};

struct ListCursor {
  uint8_t cursor;
  uint8_t scroll;
  uint8_t selectedIndex;
};

struct BLEDeviceInfo {

  char address[18];     // "XX:XX:XX:XX:XX:XX\0" - fixed allocation (no heap fragmentation)
  char name[33];        // Max BLE name (32) + null terminator - fixed allocation
  int8_t rssi;          // RSSI range is -127 to 0, int8_t saves 3 bytes per device
  bool isActive;
  uint32_t lastSeen;
  uint8_t advType;      // 0=unknown, 1=iBeacon
  bool hasName;
};

struct Vendor {
  uint8_t oui[3];
  const char* name;
};

struct LogEvent {
  uint32_t timestamp;
  uint8_t type;
  char message[40];
  bool active;
};

struct Baseline {
  uint32_t timestamp;
  uint16_t totalAPs;
  int16_t avgRSSI;
  uint8_t channelDist[13];
  uint32_t totalPackets;
  bool saved;
};

struct MonitoredDevice {
  uint8_t type;
  uint8_t bssid[6];
  char bleAddr[18];
  char name[33];
  int8_t rssi;
  uint8_t channel;
  uint32_t firstSeen;
  uint32_t lastSeen;
  uint16_t seenCount;
  bool isPresent;
  bool active;
};

struct RSSIHistory {
  uint8_t bssid[6];
  int8_t rssiSamples[RSSI_HISTORY_SIZE];
  uint8_t sampleIndex;
  bool active;
};

// Packet log for web UI verbose mode
#define MAX_PKT_LOG 50
struct PacketLog {
  uint32_t timestamp;
  uint8_t type;      // 0=mgmt, 1=data, 2=deauth
  uint8_t subtype;   // frame subtype
  uint8_t channel;
  int8_t rssi;
  uint8_t bssid[6];
  char ssid[33];     // for beacons
  bool active;
};

extern U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled;
extern Adafruit_NeoPixel rgb;
extern Preferences prefs;

extern Settings settings;

extern LogEvent eventLog[MAX_EVENTS];
extern uint8_t eventCount;
extern uint8_t eventCursor;
extern uint8_t eventScroll;
extern Baseline baseline;
extern Baseline currentSnapshot;

extern MonitoredDevice monitoredDevices[MAX_MONITORED_DEVICES];
extern uint8_t monitoredDeviceCount;
extern uint8_t deviceCursor;
extern uint8_t deviceScroll;
extern uint8_t deviceSelectedIndex;

extern ScanState scanState;
extern AutoWatchState autoWatch;
extern WalkTestState walkTest;
extern ListCursor apListState;
extern ListCursor bleListState;

// Button-aware delay (polls buttons during WiFi scan waits)
extern ButtonEvent pendingButton;
void buttonAwareDelay(unsigned long ms);

// Packet log for web UI
extern PacketLog pktLogBuffer[MAX_PKT_LOG];
extern uint8_t pktLogIndex;
extern uint8_t pktLogCount;

#endif

