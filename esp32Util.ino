// ESP32 Pocket RF Tool - WiFi & BLE Analyzer
#include <Arduino.h>
#include "config.h"
#include "screens.h"
#include "screens_draw.h"
#include "screens_handlers.h"
#include "settings.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "security.h"
#include "utils.h"
#include "menu.h"
#include "display.h"
#include "alerts.h"
#include "power.h"
#include "device_monitor.h"
#include <SPIFFS.h>
#include <WiFi.h>

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 5, 4);
Adafruit_NeoPixel rgb(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
Preferences prefs;

Settings settings = {1, -70, 50, true, false, 10, 60};
Screen currentScreen = SCREEN_MENU;

AutoWatchState autoWatch = {0, 0, 0};
ScanState scanState = {0};
WalkTestState walkTest = {0};

uint8_t whySlowView = 0;
uint8_t whySlowApIdx = 0;
RSSIHistory rssiHistory[MAX_TRACKED_APS];
uint32_t lastRSSISample = 0;

uint8_t rfHealthView = 0;
int8_t rfHealthRSSIHistory[60];
uint8_t rfHealthHistoryIndex = 0;
int8_t rfHealthMinRSSI = 0;
int8_t rfHealthMaxRSSI = -100;

LogEvent eventLog[MAX_EVENTS];

uint8_t eventCount = 0;
uint8_t eventCursor = 0;
uint8_t eventScroll = 0;
Baseline baseline = {0, 0, 0, {0}, 0, false};
Baseline currentSnapshot = {0, 0, 0, {0}, 0, false};
uint32_t lastEnvCheck = 0;
uint32_t lastBaselineUpdate = 0;
bool prevAttackActive = false;
uint8_t prevRogueCount = 0;

MonitoredDevice monitoredDevices[MAX_MONITORED_DEVICES];
uint8_t monitoredDeviceCount = 0;
uint8_t deviceCursor = 0;
uint8_t deviceScroll = 0;
uint8_t deviceSelectedIndex = 0;

uint8_t displaySettingCursor = 0;

ButtonEvent pendingButton = BTN_NONE;

bool lastReading = HIGH;
unsigned long lastDebounce = 0;
bool actionPressed = false;
unsigned long actionPressStart = 0;
unsigned long lastActionFired = 0;

bool lastBackReading = HIGH;
unsigned long lastBackDebounce = 0;
bool backPressed = false;
unsigned long backPressStart = 0;
unsigned long lastBackFired = 0;

ButtonEvent updateButton() {
  unsigned long now = millis();

  // Back button handling
  bool backBtn = digitalRead(BTN_BACK_PIN);
  if (backBtn != lastBackReading) {
    lastBackDebounce = now;
    lastBackReading = backBtn;
  }

  if (now - lastBackDebounce >= DEBOUNCE_MS) {
    // Button just pressed (transition from HIGH to LOW)
    if (backBtn == LOW && !backPressed) {
      backPressed = true;
      backPressStart = now;
    }
    // Button just released (transition from LOW to HIGH)
    else if (backBtn == HIGH && backPressed) {
      // Check cooldown to prevent accidental double-fires from bounce
      if (now - lastBackFired >= BUTTON_COOLDOWN_MS) {
        unsigned long backHeld = now - backPressStart;
        backPressed = false;
        backPressStart = 0;
        lastBackFired = now;

        if (backHeld >= 1500) {
          Serial.println("[BTN] BACK LONG - SLEEP");
          return BTN_BACK_LONG;
        } else {
          Serial.println("[BTN] BACK");
          return BTN_BACK;
        }
      } else {
        // In cooldown period, just reset pressed state without triggering
        backPressed = false;
        backPressStart = 0;
      }
    }
  }

  // Action button handling
  bool r = digitalRead(BTN_ACTION);
  if (r != lastReading) {
    lastDebounce = now;
    lastReading = r;
  }

  if (now - lastDebounce >= DEBOUNCE_MS) {
    // Button just pressed
    if (r == LOW && !actionPressed) {
      actionPressed = true;
      actionPressStart = now;
    }
    // Button just released
    else if (r == HIGH && actionPressed) {
      if (now - lastActionFired >= BUTTON_COOLDOWN_MS) {
        unsigned long held = now - actionPressStart;
        actionPressed = false;
        actionPressStart = 0;
        lastActionFired = now;

        if (held >= LONG_PRESS_MS) {
          Serial.printf("[BTN] LONG (held: %lums)\n", held);
          return BTN_LONG;
        }
        Serial.printf("[BTN] SHORT (held: %lums)\n", held);
        return BTN_SHORT;
      } else {
        // In cooldown, just reset state
        actionPressed = false;
        actionPressStart = 0;
      }
    }
  }

  return BTN_NONE;
}

// Delay that keeps polling buttons so presses aren't lost during WiFi scans.
// Any button press detected during the wait is stored in pendingButton.
void buttonAwareDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    ButtonEvent ev = updateButton();
    if (ev != BTN_NONE) {
      pendingButton = ev;
      lastActivity = millis();
      return; // Break early so button is processed quickly
    }
    delay(10); // Small yield to avoid tight spin
  }
}

#define BOOT_MAX_LINES 10
#define BOOT_VISIBLE 4
const char* bootLines[BOOT_MAX_LINES];
uint8_t bootStatus[BOOT_MAX_LINES];
uint8_t bootLineCount = 0;
uint8_t bootTotalSteps = 9;

void drawBootScreen() {
  uint8_t startIdx = 0;
  if (bootLineCount > BOOT_VISIBLE) startIdx = bootLineCount - BOOT_VISIBLE;

  oled.firstPage();
  do {
    oled.drawBox(0, 0, 128, 11);
    oled.setDrawColor(0);
    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(3, 8, "POCKET RF TOOL");
    uint8_t vw = strlen(FW_VERSION) * 5;
    oled.drawStr(125 - vw, 8, FW_VERSION);
    oled.setDrawColor(1);

    oled.drawHLine(0, 12, 128);

    oled.setFont(u8g2_font_5x7_tf);
    for (uint8_t i = 0; i < BOOT_VISIBLE && (startIdx + i) < bootLineCount; i++) {
      uint8_t idx = startIdx + i;
      uint8_t y = 22 + i * 9;

      if (bootStatus[idx] == 1) {
        oled.drawDisc(5, y - 2, 2);
      } else if (bootStatus[idx] == 2) {
        oled.drawLine(3, y - 4, 7, y);
        oled.drawLine(7, y - 4, 3, y);
      } else {
        oled.drawCircle(5, y - 2, 2);
      }

      oled.drawStr(12, y, bootLines[idx]);
    }

    oled.drawFrame(2, 57, 124, 6);
    uint8_t filled = (uint8_t)((uint16_t)bootLineCount * 120 / bootTotalSteps);
    if (filled > 120) filled = 120;
    if (filled > 0) {
      oled.drawBox(4, 59, filled, 2);
    }
    oled.setFont(u8g2_font_4x6_tf);
    char pct[5];
    uint8_t pctVal = (uint8_t)((uint16_t)bootLineCount * 100 / bootTotalSteps);
    if (pctVal > 100) pctVal = 100;
    sprintf(pct, "%d%%", pctVal);
    uint8_t pw = strlen(pct) * 4;
    oled.drawStr(64 - pw / 2, 56, pct);
  } while (oled.nextPage());
}

void bootLog(const char* msg) {
  if (bootLineCount < BOOT_MAX_LINES) {
    bootLines[bootLineCount] = msg;
    bootStatus[bootLineCount] = 0; // running
    bootLineCount++;
  }
  drawBootScreen();
  Serial.printf("[BOOT] %s\n", msg);
  // Button-aware delay so presses aren't lost during boot
  buttonAwareDelay(150);
}

void bootOK() {
  if (bootLineCount > 0) {
    bootStatus[bootLineCount - 1] = 1;
  }
  drawBootScreen();
  buttonAwareDelay(100);
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_ACTION, INPUT_PULLUP);
  pinMode(BTN_BACK_PIN, INPUT_PULLUP);
  delay(100);

  loadSettings();

  if (RGB_ENABLED) {
    rgb.begin();
    setRGB(RGB_WHITE);
  }

  oled.begin();
  oled.setContrast(128);

  bootLog("Hardware init");
  bootOK();

  bootLog("Loading settings");
  bootOK();

  bootLog("Mounting SPIFFS");
  if (SPIFFS.begin(true)) {
    bootOK();
    bootLog("Loading vendors");
    loadVendorsFromSPIFFS();
    bootOK();
  } else {
    Serial.println("[ERROR] SPIFFS mount failed!");
  }

  bootLog("Starting WiFi modem");
  initWiFi();
  // Set STA mode for normal operation (AP mode used only when web server starts)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  bootOK();

  bootLog("Starting BLE modem");
  initBLE();
  bootOK();

  bootLog("Calibrating radio");
  resetSession();
  bootOK();

  bootLog("Starting scan engine");
  bootOK();

  bootLog("Loading UI");
  bootOK();

  bootLog("System ready");
  bootOK();

  if (RGB_ENABLED) setRGB(RGB_GREEN);
  Serial.println("ESP32-C3 Wi-Fi & BLE Analyzer Ready");

  // Show completed boot for 1.5 seconds, but keep polling buttons
  unsigned long bootDisplayStart = millis();
  while (millis() - bootDisplayStart < 1500) {
    ButtonEvent ev = updateButton();
    if (ev != BTN_NONE) {
      pendingButton = ev;
      lastActivity = millis();
      break; // Exit early if button pressed
    }
    delay(10);
  }
  drawMenu();
}

void loop() {
  ButtonEvent ev;
  if (pendingButton != BTN_NONE) {
    ev = pendingButton;
    pendingButton = BTN_NONE;
  } else {
    ev = updateButton();
  }

  if (ev == BTN_BACK_LONG) {
    enterDeepSleep();
    return;
  }

  if (ev != BTN_NONE) {
    lastActivity = millis();
    if (screenSleeping) {
      screenSleeping = false;
      oled.setPowerSave(0);
      setRGB(RGB_GREEN);
      esp_wifi_start();
      Serial.println("[WAKE] Light sleep wake - button pressed");
      return;
    }
  }

  bool activelyScanning = (currentScreen == SCREEN_AUTO_WATCH ||
                            currentScreen == SCREEN_MONITOR ||
                            currentScreen == SCREEN_ANALYZER ||
                            currentScreen == SCREEN_DEVICE_MONITOR ||
                            currentScreen == SCREEN_AP_LIST ||
                            currentScreen == SCREEN_BLE_SCAN ||
                            currentScreen == SCREEN_DEAUTH_WATCH ||
                            currentScreen == SCREEN_ROGUE_AP_WATCH ||
                            currentScreen == SCREEN_BLE_TRACKER_WATCH ||
                            currentScreen == SCREEN_WHY_IS_IT_SLOW ||
                            currentScreen == SCREEN_AP_WALK_TEST ||
                            currentScreen == SCREEN_BLE_WALK_TEST ||
                            currentScreen == SCREEN_WEB_SERVER);

  if (settings.screenTimeout > 0 && !screenSleeping && !activelyScanning) {
    if (millis() - lastActivity > (settings.screenTimeout * 1000UL)) {
      screenSleeping = true;
      oled.setPowerSave(1);
      setRGB(RGB_OFF);
      stopAllWifi();
      stopBLEScan();
      esp_wifi_stop();
      Serial.println("[SLEEP] Light sleep - press any button to wake");
      return;
    }
  }

  if (millis() - lastDeauthCheck > 1000) {
    deauthPerSecond = pktDeauth - lastDeauthCount;
    lastDeauthCount = pktDeauth;
    lastDeauthCheck = millis();

    if (deauthPerSecond > settings.deauthThreshold) {
      attackActive = true;
    } else if (deauthPerSecond == 0 && attackActive) {
      attackActive = false;
    }
  }

  signalAlert = (avgRssi > settings.rssiThreshold);
  updateRGBStatus();

  switch (currentScreen) {
    case SCREEN_MENU: handleMainMenu(ev); break;
    case SCREEN_AUTO_WATCH: handleAutoWatch(ev); break;
    case SCREEN_RF_HEALTH: handleRFHealth(ev); break;
    case SCREEN_MONITOR: handleMonitor(ev); break;
    case SCREEN_ANALYZER: handleAnalyzer(ev); break;
    case SCREEN_DEVICE_MONITOR: handleDeviceMonitor(ev); break;
    case SCREEN_DEVICE_DETAIL: handleDeviceDetail(ev); break;
    case SCREEN_AP_LIST: handleApList(ev); break;
    case SCREEN_AP_DETAIL: handleApDetail(ev); break;
    case SCREEN_AP_WALK_TEST: handleAPWalkTest(ev); break;
    case SCREEN_BLE_SCAN: handleBLEScan(ev); break;
    case SCREEN_BLE_DETAIL: handleBLEDetail(ev); break;
    case SCREEN_BLE_WALK_TEST: handleBLEWalkTest(ev); break;
    case SCREEN_SECURITY_MENU: handleSecurityMenu(ev); break;
    case SCREEN_DEAUTH_WATCH: handleDeauthWatch(ev); break;
    case SCREEN_ROGUE_AP_WATCH: handleRogueAPWatch(ev); break;
    case SCREEN_BLE_TRACKER_WATCH: handleBLETrackerWatch(ev); break;
    case SCREEN_ALERT_SETTINGS: handleAlertSettings(ev); break;
    case SCREEN_INSIGHTS_MENU: handleInsightsMenu(ev); break;
    case SCREEN_WHY_IS_IT_SLOW: handleWhyIsItSlow(ev); break;
    case SCREEN_CHANNEL_RECOMMENDATION: handleChannelRecommendation(ev); break;
    case SCREEN_ENVIRONMENT_CHANGE: handleEnvironmentChange(ev); break;
    case SCREEN_QUICK_SNAPSHOT: handleQuickSnapshot(ev); break;
    case SCREEN_CHANNEL_SCORECARD: handleChannelScorecard(ev); break;
    case SCREEN_HISTORY_MENU: handleHistoryMenu(ev); break;
    case SCREEN_EVENT_LOG: handleEventLog(ev); break;
    case SCREEN_BASELINE_COMPARE: handleBaselineCompare(ev); break;
    case SCREEN_EXPORT: handleExport(ev); break;
    case SCREEN_SYSTEM_MENU: handleSystemMenu(ev); break;
    case SCREEN_BATTERY_POWER: handleBatteryPower(ev); break;
    case SCREEN_DISPLAY_SETTINGS: handleDisplaySettings(ev); break;
    case SCREEN_RADIO_CONTROL: handleRadioControl(ev); break;
    case SCREEN_POWER_MODE: handlePowerMode(ev); break;
    case SCREEN_ABOUT: handleAbout(ev); break;
    case SCREEN_COMPARE: handleCompare(ev); break;
    case SCREEN_HIDDEN_SSID: handleHiddenSSID(ev); break;
    case SCREEN_STATS: handleStats(ev); break;
    case SCREEN_WEB_SERVER: handleWebServer(ev); break;
  }
  
  delay(15);
}

