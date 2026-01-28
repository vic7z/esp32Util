#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

#define FW_VERSION "v1.3.3"
#define FW_DATE "2026-01-28"

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

struct Settings {
  uint8_t scanSpeed;
  int8_t rssiThreshold;
  uint8_t rgbBrightness;
  bool autoRefresh;
  bool csvLogging;
  uint8_t deauthThreshold;
  uint16_t screenTimeout;
  uint8_t powerMode;
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

struct BLEDeviceInfo {
  String address;
  String name;
  int rssi;
  bool isActive;
  uint32_t lastSeen;
  uint8_t advType;
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

#endif
