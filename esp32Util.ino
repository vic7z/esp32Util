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

U8G2_SSD1306_128X64_NONAME_1_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE, 5, 4);
Adafruit_NeoPixel rgb(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
Preferences prefs;

Settings settings = {1, -70, 50, true, false, 10, 60};
Screen currentScreen = SCREEN_MENU;

uint8_t autoModeView = 0;
uint32_t lastAutoWifiScan = 0;
uint32_t lastAutoBLEScan = 0;
uint16_t autoTotalAPs = 0;
uint16_t autoTotalBLE = 0;

uint8_t whySlowView = 0;
RSSIHistory rssiHistory[MAX_TRACKED_APS];
uint32_t lastRSSISample = 0;

uint8_t rfHealthView = 0;
int8_t rfHealthRSSIHistory[60]; // 60 samples of avg RSSI
uint8_t rfHealthHistoryIndex = 0;
int8_t rfHealthMinRSSI = 0;
int8_t rfHealthMaxRSSI = -100;

#define WALK_HISTORY_SIZE 60
int8_t walkRSSIHistory[WALK_HISTORY_SIZE];
uint8_t walkHistoryIndex = 0;
int8_t walkMinRSSI = 0;
int8_t walkMaxRSSI = -100;
int32_t walkRSSISum = 0;
uint16_t walkSampleCount = 0;
uint8_t walkTargetBSSID[6];  // Store BSSID instead of index
char walkTargetSSID[33];     // Store SSID for display
String walkTargetBLEAddr = "";
bool walkTestActive = false;
uint8_t walkTestView = 0;

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

bool lastReading = HIGH;
unsigned long lastDebounce = 0;
unsigned long pressStart = 0;

bool lastBackReading = HIGH;
unsigned long lastBackDebounce = 0;
bool backPressed = false;
unsigned long backPressStart = 0;
unsigned long lastBackFired = 0;

ButtonEvent updateButton() {
  unsigned long now = millis();

  // Check BACK button first (dedicated button) - fire on release
  bool backBtn = digitalRead(BTN_BACK_PIN);
  if (backBtn != lastBackReading) {
    lastBackDebounce = now;
    lastBackReading = backBtn;
  }

  if (now - lastBackDebounce >= DEBOUNCE_MS) {
    if (backBtn == LOW && !backPressed) {
      backPressed = true;
      backPressStart = now;
    } else if (backBtn == HIGH && backPressed && (now - lastBackFired > 150)) {
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
    }
  }

  bool r = digitalRead(BTN_ACTION);
  if (r != lastReading) {
    lastDebounce = now;
    lastReading = r;
  }
  if (now - lastDebounce < DEBOUNCE_MS) return BTN_NONE;

  if (r == LOW && pressStart == 0) {
    pressStart = now;
  }

  if (r == HIGH && pressStart) {
    unsigned long held = now - pressStart;
    pressStart = 0;

    if (held >= LONG_PRESS_MS) {
      Serial.printf("[BTN] LONG (held: %lums)\n", held);
      return BTN_LONG;
    }
    Serial.printf("[BTN] SHORT (held: %lums)\n", held);
    return BTN_SHORT;
  }

  return BTN_NONE;
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

  oled.firstPage();
  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(10, 25, "Wi-Fi Analyzer");
    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(15, 40, "Initializing...");
  } while (oled.nextPage());

  delay(100);

  initWiFi();

  Serial.println("[INIT] Initializing BLE...");
  initBLE();
  Serial.println("[INIT] BLE ready");

  resetSession();
  drawMenu();

  if (RGB_ENABLED) setRGB(RGB_GREEN);

  Serial.println("ESP32-C3 Wi-Fi & BLE Analyzer Ready");
}

void loop() {
  ButtonEvent ev = updateButton();

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
                            currentScreen == SCREEN_BLE_WALK_TEST);

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

  if (currentScreen == SCREEN_MENU) {
    if (ev == BTN_SHORT) {
      mainMenuIndex = (mainMenuIndex + 1) % MAIN_MENU_SIZE;
      drawMenu();
    }

    if (ev == BTN_LONG) {
      switch (mainMenuIndex) {
        case 0:
          currentScreen = SCREEN_AUTO_WATCH;
          autoModeView = 0;
          lastAutoWifiScan = 0;
          lastAutoBLEScan = 0;
          autoTotalAPs = 0;
          autoTotalBLE = 0;
          currentChannel = 1;
          resetLiveStats();
          enterSnifferMode(currentChannel);
          startBLEScan();
          drawAutoWatch();
          break;
        case 1:
          currentScreen = SCREEN_RF_HEALTH;
          stopAllWifi();
          break;
        case 2:
          currentScreen = SCREEN_MONITOR;
          frozen = false;
          currentChannel = 1;
          resetLiveStats();
          enterSnifferMode(currentChannel);
          break;
        case 3:
          currentScreen = SCREEN_ANALYZER;
          selectedChannel = 1;
          resetAnalyzer();
          enterSnifferMode(1);
          break;
        case 4:
          currentScreen = SCREEN_DEVICE_MONITOR;
          deviceCursor = 0;
          deviceScroll = 0;
          startDeviceMonitorSniffer();
          startBLEScan();
          break;
        case 5:
          currentScreen = SCREEN_AP_LIST;
          apCursor = apScroll = 0;
          apSortedOnce = false;
          enterScanMode();
          startApScan();
          lastScan = millis();
          break;
        case 6:
          currentScreen = SCREEN_BLE_SCAN;
          bleCursor = bleScroll = 0;
          stopAllWifi();
          startBLEScan();
          lastScan = millis();
          break;
        case 7:
          currentScreen = SCREEN_SECURITY_MENU;
          securityMenuIndex = 0;
          stopAllWifi();
          drawSecurityMenu();
          break;
        case 8:
          currentScreen = SCREEN_INSIGHTS_MENU;
          insightsMenuIndex = 0;
          stopAllWifi();
          drawInsightsMenu();
          break;
        case 9:
          currentScreen = SCREEN_HISTORY_MENU;
          historyMenuIndex = 0;
          stopAllWifi();
          drawHistoryMenu();
          break;
        case 10:
          currentScreen = SCREEN_SYSTEM_MENU;
          systemMenuIndex = 0;
          stopAllWifi();
          drawSystemMenu();
          break;
      }
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_SECURITY_MENU) {
    if (ev == BTN_SHORT) {
      securityMenuIndex = (securityMenuIndex + 1) % SECURITY_MENU_SIZE;
      drawSecurityMenu();
    }
    if (ev == BTN_LONG) {
      switch (securityMenuIndex) {
        case 0:
          currentScreen = SCREEN_DEAUTH_WATCH;
          currentChannel = 1;
          enterSnifferMode(currentChannel);
          break;
        case 1:
          currentScreen = SCREEN_ROGUE_AP_WATCH;
          enterScanMode();
          startApScan();
          lastScan = millis();
          break;
        case 2:
          currentScreen = SCREEN_BLE_TRACKER_WATCH;
          break;
        case 3:
          currentScreen = SCREEN_ALERT_SETTINGS;
          break;
      }
    }
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      drawMenu();
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_INSIGHTS_MENU) {
    handleInsightsMenu(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_HISTORY_MENU) {
    handleHistoryMenu(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_SYSTEM_MENU) {
    handleSystemMenu(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_AUTO_WATCH) {
    handleAutoWatch(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_RF_HEALTH) {
    if (ev == BTN_LONG) {
      rfHealthView = (rfHealthView + 1) % 2;
    }

    if (millis() - lastScan > 2000) {
      enterScanMode();
      startApScan();
      delay(1500);
      fetchApResults(false);
      updateBLEScan();

      if (apCount > 0) {
        long rssiSum = 0;
        for (int i = 0; i < apCount; i++) {
          rssiSum += apList[i].rssi;
        }
        int8_t avgRSSI = rssiSum / apCount;

        rfHealthRSSIHistory[rfHealthHistoryIndex] = avgRSSI;
        rfHealthHistoryIndex = (rfHealthHistoryIndex + 1) % 60;

        if (avgRSSI < rfHealthMinRSSI || rfHealthMinRSSI == 0) rfHealthMinRSSI = avgRSSI;
        if (avgRSSI > rfHealthMaxRSSI) rfHealthMaxRSSI = avgRSSI;
      }

      lastScan = millis();
    }

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      stopAllWifi();
      stopBLEScan();
      rfHealthView = 0;
      drawMenu();
      return;
    }

    drawRFHealth();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_DEVICE_MONITOR) {
    handleDeviceMonitor(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_DEVICE_DETAIL) {
    handleDeviceDetail(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_AP_WALK_TEST) {
    handleAPWalkTest(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BLE_WALK_TEST) {
    handleBLEWalkTest(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BLE_TRACKER_WATCH) {
    updateBLEScan();

    int trackerCount = 0;
    for (int i = 0; i < bleDeviceCount; i++) {
      if (bleDevices[i].advType > 0 || !bleDevices[i].hasName) {
        trackerCount++;
      }
    }

    if (trackerCount > 0) {
      alertLevel = 1;
    } else {
      alertLevel = 0;
    }
    updateAlertLED();

    drawBLETrackerWatch();
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_SECURITY_MENU;
      stopBLEScan();
      drawSecurityMenu();
      alertLevel = 0;
      setRGB(RGB_GREEN);
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_ALERT_SETTINGS) {
    static uint8_t settingIndex = 0;

    if (ev == BTN_SHORT) {
      settingIndex = (settingIndex + 1) % 2;
    } else if (ev == BTN_LONG) {
      switch (settingIndex) {
        case 0:
          settings.deauthThreshold = (settings.deauthThreshold + 5) % 55;
          if (settings.deauthThreshold == 0) settings.deauthThreshold = 5;
          break;
        case 1:
          if (settings.screenTimeout == 0) settings.screenTimeout = 30;
          else if (settings.screenTimeout == 30) settings.screenTimeout = 60;
          else if (settings.screenTimeout == 60) settings.screenTimeout = 120;
          else if (settings.screenTimeout == 120) settings.screenTimeout = 300;
          else settings.screenTimeout = 0;
          break;
      }
    }

    drawAlertSettings();

    if (ev == BTN_BACK) {
      saveSettings();
      currentScreen = SCREEN_SECURITY_MENU;
      drawSecurityMenu();
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_WHY_IS_IT_SLOW) {
    handleWhyIsItSlow(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_CHANNEL_RECOMMENDATION) {
    handleChannelRecommendation(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_ENVIRONMENT_CHANGE) {
    handleEnvironmentChange(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_QUICK_SNAPSHOT) {
    handleQuickSnapshot(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_CHANNEL_SCORECARD) {
    handleChannelScorecard(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_EVENT_LOG) {
    handleEventLog(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BASELINE_COMPARE) {
    handleBaselineCompare(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_EXPORT) {
    handleExport(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_DISPLAY_SETTINGS) {
    if (ev == BTN_SHORT) {
      settings.rgbBrightness = min(100, settings.rgbBrightness + 10);
      rgb.setBrightness((settings.rgbBrightness * 255) / 100);
      rgb.show();
    } else if (ev == BTN_LONG) {
      settings.rgbBrightness = max(0, settings.rgbBrightness - 10);
      rgb.setBrightness((settings.rgbBrightness * 255) / 100);
      rgb.show();
    }

    drawDisplaySettings();

    if (ev == BTN_BACK) {
      saveSettings();
      currentScreen = SCREEN_SYSTEM_MENU;
      drawSystemMenu();
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_RADIO_CONTROL) {
    if (ev == BTN_SHORT) {
      currentChannel = (currentChannel % MAX_CHANNEL) + 1;
    } else if (ev == BTN_LONG) {
      currentChannel = (currentChannel - 2 + MAX_CHANNEL) % MAX_CHANNEL + 1;
    }

    drawRadioControl();

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_SYSTEM_MENU;
      drawSystemMenu();
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BATTERY_POWER || currentScreen == SCREEN_ABOUT) {
    if (currentScreen == SCREEN_BATTERY_POWER) drawBatteryPower();
    else drawAbout();

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_SYSTEM_MENU;
      drawSystemMenu();
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_POWER_MODE) {
    handlePowerMode(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_DEAUTH_WATCH) {
    static uint32_t lastChannelHop = 0;
    if (millis() - lastChannelHop > 500) {
      currentChannel = (currentChannel % MAX_CHANNEL) + 1;
      enterSnifferMode(currentChannel);
      lastChannelHop = millis();
    }

    totalDeauthDetected = pktDeauth;

    if (attackActive) {
      alertLevel = 2;
    } else if (deauthPerSecond > 0) {
      alertLevel = 1;
    } else {
      alertLevel = 0;
    }
    updateAlertLED();

    if (attackActive && !prevAttackActive) {
      char msg[40];
      snprintf(msg, 40, "Deauth attack! Ch%d %lu/sec", currentChannel, deauthPerSecond);
      logEvent(0, msg);
    }
    prevAttackActive = attackActive;

    drawDeauthWatch();
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_SECURITY_MENU;
      stopAllWifi();
      drawSecurityMenu();
      alertLevel = 0;
      setRGB(RGB_GREEN);
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_ROGUE_AP_WATCH) {
    if (millis() - lastScan > 3000) {
      fetchApResults(false);
      uint8_t oldRogueCount = rogueCount;
      detectRogueAPs();

      if (rogueCount > prevRogueCount) {
        for (int i = prevRogueCount; i < rogueCount; i++) {
          char msg[40];
          snprintf(msg, 40, "Rogue AP: %.15s", rogueList[i].ssid);
          logEvent(1, msg);
        }
      }
      prevRogueCount = rogueCount;

      startApScan();
      lastScan = millis();
    }

    if (rogueCount > 0) {
      alertLevel = 2;
    } else {
      alertLevel = 0;
    }
    updateAlertLED();

    drawRogueAPWatch();
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_SECURITY_MENU;
      stopAllWifi();
      drawSecurityMenu();
      alertLevel = 0;
      setRGB(RGB_GREEN);
    }
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_AP_LIST) {
    if (ev == BTN_SHORT && apCount > 0) {
      if (apScroll + apCursor + 1 < apCount) {
        if (apCursor < AP_VISIBLE - 1) apCursor++;
        else apScroll++;
      } else {
        apCursor = 0;
        apScroll = 0;
      }
    }

    if (ev == BTN_LONG && apCount > 0) {
      apSelectedIndex = apScroll + apCursor;
      currentScreen = SCREEN_AP_DETAIL;
    }

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      stopAllWifi();
      drawMenu();
      return;
    }

    if (millis() - lastScan > 2000) {
      fetchApResults(false);
      startApScan();
      lastScan = millis();
    }

    drawApList();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_AP_DETAIL) {
    if (ev == BTN_SHORT) {
      currentScreen = SCREEN_AP_LIST;
    }
    if (ev == BTN_LONG) {
      if (apSelectedIndex < apCount) {
        memcpy(walkTargetBSSID, apList[apSelectedIndex].bssid, 6);
        strncpy(walkTargetSSID, (char*)apList[apSelectedIndex].ssid, 32);
        walkTargetSSID[32] = '\0';

        walkHistoryIndex = 0;
        walkMinRSSI = 0;
        walkMaxRSSI = -100;
        walkRSSISum = 0;
        walkSampleCount = 0;
        walkTestActive = true;
        memset(walkRSSIHistory, 0, sizeof(walkRSSIHistory));
        currentScreen = SCREEN_AP_WALK_TEST;
        lastScan = 0;
      }
    }
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_AP_LIST;
      drawApList();
    }
    drawApDetail();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_COMPARE) {
    if (ev == BTN_SHORT) {
      apCompareB = (apCompareB + 1) % apCount;
    }
    if (ev == BTN_LONG) {
      apCompareA = (apCompareA + 1) % apCount;
      if (apCompareA == apCompareB) apCompareB = (apCompareB + 1) % apCount;
    }
    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      drawMenu();
    }
    drawCompare();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_HIDDEN_SSID) {
    static uint32_t lastHop = 0;
    if (millis() - lastHop > 200) {
      currentChannel = (currentChannel % MAX_CHANNEL) + 1;
      enterSnifferMode(currentChannel);
      lastHop = millis();
    }

    if (ev == BTN_SHORT && hiddenCount > 0) {
      if (hiddenScroll + hiddenCursor + 1 < hiddenCount) {
        if (hiddenCursor < AP_VISIBLE - 1) hiddenCursor++;
        else hiddenScroll++;
      } else {
        hiddenCursor = 0;
        hiddenScroll = 0;
      }
    }

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      stopAllWifi();
      drawMenu();
    }

    drawHiddenSSID();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_STATS) {
    if (ev == BTN_SHORT) {
      currentScreen = SCREEN_MENU;
      drawMenu();
    }

    if (ev == BTN_LONG) {
      resetSession();
    }

    drawStats();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_MONITOR) {
    if (ev == BTN_SHORT && !frozen) {
      currentChannel = currentChannel % MAX_CHANNEL + 1;
      resetLiveStats();
      enterSnifferMode(currentChannel);
    }

    if (ev == BTN_LONG) {
      frozen = !frozen;
    }

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_MENU;
      stopAllWifi();
      drawMenu();
      return;
    }

    if (!frozen && millis() - lastSecond >= 1000) {
      uint32_t d = pktTotal - lastPkt;
      lastPkt = pktTotal;
      smoothPps = 0.7 * smoothPps + 0.3 * d;
      pps = (uint32_t)smoothPps;
      peak = max(peak, pps);
      peakPPS = max(peakPPS, pps);
      history[histIdx] = pps;
      histIdx = (histIdx + 1) % HISTORY_SIZE;

      if (rssiCount) {
        float r = (float)rssiAccum / rssiCount;
        avgRssi = 0.8 * avgRssi + 0.2 * r;
        rssiAccum = rssiCount = 0;
      }
      lastSecond = millis();
    }

    drawMonitor();
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_ANALYZER) {
    handleAnalyzer(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BLE_SCAN) {
    handleBLEScan(ev);
    delay(40);
    return;
  }

  if (currentScreen == SCREEN_BLE_DETAIL) {
    if (ev == BTN_SHORT) {
      currentScreen = SCREEN_BLE_SCAN;
      startBLEScan();
    }

    if (ev == BTN_LONG) {
      if (bleSelectedIndex < bleDeviceCount) {
        walkTargetBLEAddr = bleDevices[bleSelectedIndex].address;

        walkHistoryIndex = 0;
        walkMinRSSI = 0;
        walkMaxRSSI = -100;
        walkRSSISum = 0;
        walkSampleCount = 0;
        walkTestActive = true;
        memset(walkRSSIHistory, 0, sizeof(walkRSSIHistory));
        currentScreen = SCREEN_BLE_WALK_TEST;
        startBLEScan();
        lastScan = 0;
      }
    }

    if (ev == BTN_BACK) {
      currentScreen = SCREEN_BLE_SCAN;
      startBLEScan();
    }

    drawBLEDetail();
    delay(40);
    return;
  }
}
