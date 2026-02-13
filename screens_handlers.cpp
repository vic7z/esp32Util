#include <esp_system.h>
#include "screens_handlers.h"
#include "screens_draw.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "menu.h"
#include "display.h"
#include "utils.h"
#include "security.h"
#include "alerts.h"
#include "settings.h"
#include "device_monitor.h"
#include "web_server.h"
#include "ui_helpers.h"

extern Screen currentScreen;

extern uint8_t apCursor, apScroll, apSelectedIndex;
extern uint8_t rfHealthView;
extern int8_t rfHealthRSSIHistory[60];
extern uint8_t rfHealthHistoryIndex;
extern int8_t rfHealthMinRSSI, rfHealthMaxRSSI;

extern uint8_t apCompareA, apCompareB;
extern bool apSortedOnce;

extern uint8_t bleCursor, bleScroll, bleSelectedIndex;

extern volatile uint32_t pktTotal, pktBeacon, pktData, pktDeauth;

extern uint32_t lastPkt;
extern float smoothPps;
extern uint32_t pps, peak, peakPPS;
extern uint32_t history[HISTORY_SIZE];
extern uint8_t histIdx;
extern float avgRssi;
extern volatile int32_t rssiAccum;
extern volatile uint32_t rssiCount;
extern uint32_t chPackets[MAX_CHANNEL + 1];

extern uint32_t chBeacons[MAX_CHANNEL + 1];
extern uint32_t chDeauth[MAX_CHANNEL + 1];

extern uint8_t hiddenCursor, hiddenScroll;

extern uint8_t whySlowView;
extern uint8_t whySlowApIdx;
extern RSSIHistory rssiHistory[MAX_TRACKED_APS];
extern uint32_t lastRSSISample;

extern uint32_t deauthPerSecond, totalDeauthDetected;
extern bool attackActive, prevAttackActive;
extern uint8_t prevRogueCount;
extern uint8_t deauthChannel;
extern uint8_t alertLevel;

extern uint8_t eventCursor, eventScroll;

extern uint8_t displaySettingCursor;

void handleMainMenu(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    mainMenuIndex = (mainMenuIndex + 1) % MAIN_MENU_SIZE;
    drawMenu();
  }

  if (ev == BTN_LONG) {
    switch (mainMenuIndex) {
      case 0:
        currentScreen = SCREEN_AUTO_WATCH;
        autoWatch.viewMode = 0;
        scanState.lastAutoWifiScan = 0;
        scanState.lastAutoBLEScan = 0;
        autoWatch.totalAPs = 0;
        autoWatch.totalBLE = 0;
        scanState.currentChannel = 1;
        resetLiveStats();
        enterSnifferMode(scanState.currentChannel);
        startBLEScan();
        drawAutoWatch();
        break;
      case 1:
        currentScreen = SCREEN_RF_HEALTH;
        stopAllWifi();
        startBLEScan();
        scanState.lastScan = 0;
        break;
      case 2:
        currentScreen = SCREEN_MONITOR;
        scanState.frozen = false;
        scanState.currentChannel = 1;
        resetLiveStats();
        enterSnifferMode(scanState.currentChannel);
        break;
      case 3:
        currentScreen = SCREEN_ANALYZER;
        scanState.selectedChannel = 1;
        resetAnalyzer();
        enterSnifferMode(1);
        break;
      case 4:
        currentScreen = SCREEN_DEVICE_MONITOR;
        deviceCursor = 0;
        deviceScroll = 0;
        scanState.lastScan = 0;
        startBLEScan();
        break;
      case 5:
        currentScreen = SCREEN_AP_LIST;
        apCursor = apScroll = 0;
        apSortedOnce = false;
        enterScanMode();
        startApScan();
        scanState.lastScan = millis();
        break;
      case 6:
        currentScreen = SCREEN_BLE_SCAN;
        bleCursor = bleScroll = 0;
        stopAllWifi();
        startBLEScan();
        scanState.lastScan = millis();
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
        currentScreen = SCREEN_WEB_SERVER;
        startWebServer();
        drawWebServer();
        break;
      case 11:
        currentScreen = SCREEN_SYSTEM_MENU;
        systemMenuIndex = 0;
        stopAllWifi();
        drawSystemMenu();
        break;
    }
  }
}

void handleSecurityMenu(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    securityMenuIndex = (securityMenuIndex + 1) % SECURITY_MENU_SIZE;
    drawSecurityMenu();
  }
  if (ev == BTN_LONG) {
    switch (securityMenuIndex) {
      case 0:
        currentScreen = SCREEN_DEAUTH_WATCH;
        scanState.currentChannel = 1;
        enterSnifferMode(scanState.currentChannel);
        break;
      case 1:
        currentScreen = SCREEN_ROGUE_AP_WATCH;
        enterScanMode();
        startApScan();
        scanState.lastScan = millis();
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
}

void handleInsightsMenu(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    insightsMenuIndex = (insightsMenuIndex + 1) % INSIGHTS_MENU_SIZE;
    drawInsightsMenu();
  }
  if (ev == BTN_LONG) {
    switch (insightsMenuIndex) {
      case 0:
        currentScreen = SCREEN_WHY_IS_IT_SLOW;
        break;
      case 1:
        currentScreen = SCREEN_CHANNEL_RECOMMENDATION;
        scanState.lastScan = 0;
        break;
      case 2:
        currentScreen = SCREEN_ENVIRONMENT_CHANGE;
        scanState.lastEnvCheck = 0;
        break;
      case 3:
        currentScreen = SCREEN_QUICK_SNAPSHOT;
        scanState.lastScan = 0;
        break;
      case 4:
        currentScreen = SCREEN_CHANNEL_SCORECARD;
        scanState.lastScan = 0;
        break;

    }
  }
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    drawMenu();
  }
}

void handleHistoryMenu(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    historyMenuIndex = (historyMenuIndex + 1) % HISTORY_MENU_SIZE;
    drawHistoryMenu();
  }
  if (ev == BTN_LONG) {
    switch (historyMenuIndex) {
      case 0:
        currentScreen = SCREEN_EVENT_LOG;
        break;
      case 1:
        currentScreen = SCREEN_BASELINE_COMPARE;
        scanState.lastBaselineUpdate = 0;
        break;
      case 2:
        currentScreen = SCREEN_EXPORT;
        scanState.lastScan = 0;
        break;

    }
  }
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    drawMenu();
  }
}

void handleSystemMenu(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    systemMenuIndex = (systemMenuIndex + 1) % SYSTEM_MENU_SIZE;
    drawSystemMenu();
  }
  if (ev == BTN_LONG) {
    switch (systemMenuIndex) {
      case 0:
        currentScreen = SCREEN_BATTERY_POWER;
        break;
      case 1:
        currentScreen = SCREEN_DISPLAY_SETTINGS;
        break;
      case 2:
        currentScreen = SCREEN_RADIO_CONTROL;
        break;
      case 3:
        currentScreen = SCREEN_POWER_MODE;
        break;
      case 4:
        currentScreen = SCREEN_ABOUT;
        break;
      case 5:
        esp_restart();
        break;
    }
  }
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    drawMenu();
  }
}

void handleAutoWatch(ButtonEvent ev) {
  if (ev == BTN_SHORT || ev == BTN_LONG) {
    autoWatch.viewMode = (autoWatch.viewMode + 1) % 4;
    drawAutoWatch();
    return;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopAllWifi();
    stopBLEScan();
    drawMenu();
    return;
  }

  static uint16_t stableBLECount = 0;

  if (millis() - scanState.lastAutoWifiScan > 5000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(true);
    autoWatch.totalAPs = apCount;
    scanState.lastAutoWifiScan = millis();
    enterSnifferMode(scanState.currentChannel);
    startBLEScan();
  }
  else if (millis() - scanState.lastSecond > 1000) {
    scanState.currentChannel = (scanState.currentChannel % 11) + 1;
    enterSnifferMode(scanState.currentChannel);
    scanState.lastSecond = millis();
  }
  else {
    updateBLEScan();
    uint8_t newCount = getActiveBLECount();
    if (newCount <= MAX_BLE_DEVICES) {
      stableBLECount = newCount;
    }
  }

  autoWatch.totalBLE = stableBLECount;
  drawAutoWatch();
}

void handleRFHealth(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopAllWifi();
    stopBLEScan();
    rfHealthView = 0;
    drawMenu();
    return;
  }

  if (ev == BTN_LONG) {
    rfHealthView = (rfHealthView + 1) % 2;
    drawRFHealth();
    return;
  }

  updateBLEScan();

  static uint32_t lastHistoryUpdate = 0;

  if (millis() - scanState.lastScan > 3000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    startBLEScan();
    scanState.lastScan = millis();

    if (millis() - lastHistoryUpdate > 2000) {
      int avgRSSI = 0;
      if (apCount > 0) {
        long rssiSum = 0;
        for (int i = 0; i < apCount; i++) {
          rssiSum += apList[i].rssi;
        }
        avgRSSI = rssiSum / apCount;
      } else {
        avgRSSI = -100;
      }

      rfHealthRSSIHistory[rfHealthHistoryIndex] = (int8_t)avgRSSI;
      rfHealthHistoryIndex = (rfHealthHistoryIndex + 1) % 60;

      if (avgRSSI < rfHealthMinRSSI) rfHealthMinRSSI = avgRSSI;
      if (avgRSSI > rfHealthMaxRSSI) rfHealthMaxRSSI = avgRSSI;

      lastHistoryUpdate = millis();
    }
  }

  drawRFHealth();
}

void handleMonitor(ButtonEvent ev) {
  if (ev == BTN_SHORT && !scanState.frozen) {
    scanState.currentChannel = scanState.currentChannel % MAX_CHANNEL + 1;
    resetLiveStats();
    enterSnifferMode(scanState.currentChannel);
  }

  if (ev == BTN_LONG) {
    scanState.frozen = !scanState.frozen;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopAllWifi();
    drawMenu();
    return;
  }

  if (!scanState.frozen && millis() - scanState.lastSecond >= 1000) {
    uint32_t d = pktTotal - lastPkt;
    lastPkt = pktTotal;
    smoothPps = 0.7 * smoothPps + 0.3 * d;
    pps = (uint32_t)smoothPps;
    peak = max(peak, pps);
    peakPPS = max(peakPPS, pps);
    history[histIdx] = pps;
    histIdx = (histIdx + 1) % HISTORY_SIZE;

      static uint32_t lastBeaconPkt = 0;
    static uint32_t lastDeauthPkt = 0;
    beaconPps = (uint32_t)pktBeacon - lastBeaconPkt;
    deauthPps = (uint32_t)pktDeauth - lastDeauthPkt;
    lastBeaconPkt = (uint32_t)pktBeacon;
    lastDeauthPkt = (uint32_t)pktDeauth;

    if (rssiCount) {
      float r = (float)rssiAccum / rssiCount;
      avgRssi = 0.8 * avgRssi + 0.2 * r;
      rssiAccum = rssiCount = 0;
    }
    scanState.lastSecond = millis();
  }

  drawMonitor();
}

void handleAnalyzer(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopAllWifi();
    drawMenu();
    return;
  }

  if (ev == BTN_SHORT) {
    scanState.selectedChannel = scanState.selectedChannel % MAX_CHANNEL + 1;
  }

  if (ev == BTN_LONG) {
    scanState.currentChannel = scanState.selectedChannel;
    currentScreen = SCREEN_MONITOR;
    scanState.frozen = false;
    resetLiveStats();
    enterSnifferMode(scanState.currentChannel);
    drawMonitor();
    return;
  }

  uint32_t hopDelay = 30;
  if (settings.scanSpeed == 0) hopDelay = 15;      // Fast
  else if (settings.scanSpeed == 2) hopDelay = 50;  // Slow

  if (millis() - scanState.analyzerLastHop > hopDelay) {
    chPackets[scanState.analyzerChannel] += pktTotal;
    chBeacons[scanState.analyzerChannel] += pktBeacon;
    chDeauth[scanState.analyzerChannel] += pktDeauth;

    pktTotal = pktBeacon = pktDeauth = 0;
    scanState.analyzerChannel = scanState.analyzerChannel % MAX_CHANNEL + 1;
    enterSnifferMode(scanState.analyzerChannel);
    scanState.analyzerLastHop = millis();
  }

  drawAnalyzer();
}

void handleDeviceMonitor(ButtonEvent ev) {
  uint8_t activeCount = 0;
  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    if (monitoredDevices[i].active) activeCount++;
  }

  if (activeCount == 0) {
    deviceCursor = 0;
    deviceScroll = 0;
  } else {
    if (deviceScroll >= activeCount) deviceScroll = activeCount - 1;
    uint8_t maxCursor = min((uint8_t)2, (uint8_t)(activeCount - 1 - deviceScroll));
    if (deviceCursor > maxCursor) deviceCursor = maxCursor;
  }

  handleListNavigation(ev, deviceCursor, deviceScroll, activeCount, 3);

  if (ev == BTN_LONG && monitoredDeviceCount > 0) {
    uint8_t activeIndex = 0;
    for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
      if (monitoredDevices[i].active) {
        if (activeIndex == deviceScroll + deviceCursor) {
          deviceSelectedIndex = i;
          currentScreen = SCREEN_DEVICE_DETAIL;
          drawDeviceDetail();
          return;
        }
        activeIndex++;
      }
    }
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopAllWifi();
    stopBLEScan();
    drawMenu();
    return;
  }

  updateDeviceMonitor();
  updateBLEScan();

  drawDeviceMonitor();
}

void handleDeviceDetail(ButtonEvent ev) {
  drawDeviceDetail();
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_DEVICE_MONITOR;
    drawDeviceMonitor();
  }
}

void handleApList(ButtonEvent ev) {
  handleListNavigation(ev, apCursor, apScroll, apCount, AP_VISIBLE);

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

  if (millis() - scanState.lastScan > 2000) {
    fetchApResults(false);
    startApScan();
    scanState.lastScan = millis();
  }

  drawApList();
}

void handleApDetail(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    currentScreen = SCREEN_AP_LIST;
  }
  if (ev == BTN_LONG) {
    if (apSelectedIndex < apCount) {
      memcpy(walkTest.targetBSSID, apList[apSelectedIndex].bssid, 6);
      strncpy(walkTest.targetSSID, (char*)apList[apSelectedIndex].ssid, 32);
      walkTest.targetSSID[32] = '\0';

      walkTest.historyIndex = 0;
      walkTest.minRSSI = 0;
      walkTest.maxRSSI = -100;
      walkTest.rssiSum = 0;
      walkTest.sampleCount = 0;
      walkTest.active = true;
      memset(walkTest.rssiHistory, 0, sizeof(walkTest.rssiHistory));
      currentScreen = SCREEN_AP_WALK_TEST;
      scanState.lastScan = 0;
    }
  }
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_AP_LIST;
    drawApList();
  }
  drawApDetail();
}

void handleAPWalkTest(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    walkTest.viewMode = (walkTest.viewMode + 1) % 2;
    drawAPWalkTest();
    return;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_AP_DETAIL;
    stopAllWifi();
    walkTest.active = false;
    walkTest.viewMode = 0;
    drawApDetail();
    return;
  }

  if (millis() - scanState.lastScan > WIFI_SCAN_DELAY_MS) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);

    if (apCount > 0) {
      for (int i = 0; i < apCount; i++) {
        if (memcmp(apList[i].bssid, walkTest.targetBSSID, 6) == 0) {
          int8_t rssi = apList[i].rssi;

          walkTest.rssiHistory[walkTest.historyIndex] = rssi;
          walkTest.historyIndex = (walkTest.historyIndex + 1) % WALK_HISTORY_SIZE;

          if (walkTest.sampleCount == 0 || rssi < walkTest.minRSSI) walkTest.minRSSI = rssi;
          if (walkTest.sampleCount == 0 || rssi > walkTest.maxRSSI) walkTest.maxRSSI = rssi;

          walkTest.rssiSum += rssi;
          walkTest.sampleCount++;
          break;
        }
      }
    }

    scanState.lastScan = millis();
  }

  drawAPWalkTest();
}

void handleCompare(ButtonEvent ev) {

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
}

void handleHiddenSSID(ButtonEvent ev) {
  static uint32_t lastHop = 0;
  if (millis() - lastHop > 200) {
    scanState.currentChannel = (scanState.currentChannel % MAX_CHANNEL) + 1;
    enterSnifferMode(scanState.currentChannel);
    lastHop = millis();
  }

  handleListNavigation(ev, hiddenCursor, hiddenScroll, hiddenCount, AP_VISIBLE);

  if (ev == BTN_BACK) {

    currentScreen = SCREEN_MENU;
    stopAllWifi();
    drawMenu();
  }

  drawHiddenSSID();
}

void handleBLEScan(ButtonEvent ev) {
  handleListNavigation(ev, bleCursor, bleScroll, bleDeviceCount, BLE_VISIBLE);

  if (ev == BTN_LONG && bleDeviceCount > 0) {
    bleSelectedIndex = bleScroll + bleCursor;
    currentScreen = SCREEN_BLE_DETAIL;
    stopBLEScan();
    drawBLEDetail();
    return;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_MENU;
    stopBLEScan();
    drawMenu();
    return;
  }

  updateBLEScan();

  if (bleDeviceCount > 1 && millis() - scanState.lastBLESort > BLE_SCAN_INTERVAL_MS) {
    sortBLEByRSSI();
    scanState.lastBLESort = millis();

    if (bleScroll + bleCursor >= bleDeviceCount) {
      if (bleDeviceCount > BLE_VISIBLE) {
        bleScroll = bleDeviceCount - BLE_VISIBLE;
        bleCursor = BLE_VISIBLE - 1;
      } else {
        bleScroll = 0;
        bleCursor = bleDeviceCount > 0 ? bleDeviceCount - 1 : 0;
      }
    }
  }

  if (bleScroll + bleCursor >= bleDeviceCount && bleDeviceCount > 0) {
    bleCursor = 0;
    bleScroll = 0;
  }

  drawBLEScan();

  yield();
}

void handleBLEDetail(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    currentScreen = SCREEN_BLE_SCAN;
    startBLEScan();
  }

  if (ev == BTN_LONG) {
    if (bleSelectedIndex < bleDeviceCount) {
      strncpy(walkTest.targetBLEAddr, bleDevices[bleSelectedIndex].address, 17);
      walkTest.targetBLEAddr[17] = '\0';

      walkTest.historyIndex = 0;
      walkTest.minRSSI = 0;
      walkTest.maxRSSI = -100;
      walkTest.rssiSum = 0;
      walkTest.sampleCount = 0;
      walkTest.active = true;
      memset(walkTest.rssiHistory, 0, sizeof(walkTest.rssiHistory));
      currentScreen = SCREEN_BLE_WALK_TEST;
      startBLEScan();
      scanState.lastScan = 0;
    }
  }


  if (ev == BTN_BACK) {
    currentScreen = SCREEN_BLE_SCAN;
    startBLEScan();
  }

  drawBLEDetail();
}

void handleBLEWalkTest(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    walkTest.viewMode = (walkTest.viewMode + 1) % 2;
    drawBLEWalkTest();
    return;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_BLE_DETAIL;
    stopBLEScan();
    walkTest.viewMode = 0;
    drawBLEDetail();
    return;
  }

  updateBLEScan();

  if (millis() - scanState.lastScan > 500) {
    if (bleDeviceCount > 0 && strlen(walkTest.targetBLEAddr) > 0) {
      for (int i = 0; i < bleDeviceCount; i++) {
        if (strcmp(bleDevices[i].address, walkTest.targetBLEAddr) == 0) {
          int8_t rssi = bleDevices[i].rssi;

          walkTest.rssiHistory[walkTest.historyIndex] = rssi;
          walkTest.historyIndex = (walkTest.historyIndex + 1) % WALK_HISTORY_SIZE;

          if (walkTest.sampleCount == 0 || rssi < walkTest.minRSSI) walkTest.minRSSI = rssi;
          if (walkTest.sampleCount == 0 || rssi > walkTest.maxRSSI) walkTest.maxRSSI = rssi;

          walkTest.rssiSum += rssi;
          walkTest.sampleCount++;
          break;
        }
      }
    }

    scanState.lastScan = millis();
  }

  drawBLEWalkTest();
}


void handleDeauthWatch(ButtonEvent ev) {
  static uint32_t lastChannelHop = 0;
  if (millis() - lastChannelHop > 500) {
    scanState.currentChannel = (scanState.currentChannel % MAX_CHANNEL) + 1;
    enterSnifferMode(scanState.currentChannel);
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
    snprintf(msg, 40, "Deauth attack! Ch%d %lu/sec", scanState.currentChannel, deauthPerSecond);
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
}

void handleRogueAPWatch(ButtonEvent ev) {
  if (millis() - scanState.lastScan > 3000) {
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
    scanState.lastScan = millis();
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
}

void handleBLETrackerWatch(ButtonEvent ev) {
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
}

void handleAlertSettings(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    alertSettingIndex = (alertSettingIndex + 1) % 2;
  } else if (ev == BTN_LONG) {
    switch (alertSettingIndex) {
      case 0:
        settings.deauthThreshold = (settings.deauthThreshold + 5) % 55;  // 0, 5, 10, ... 50
        if (settings.deauthThreshold == 0) settings.deauthThreshold = 5;
        break;
      case 1:
        if (settings.screenTimeout == 0) settings.screenTimeout = 30;
        else if (settings.screenTimeout == 30) settings.screenTimeout = 60;
        else if (settings.screenTimeout == 60) settings.screenTimeout = 120;
        else if (settings.screenTimeout == 120) settings.screenTimeout = 300;
        else settings.screenTimeout = 0;  // Never
        break;
    }
  }

  drawAlertSettings();

  if (ev == BTN_BACK) {
    saveSettings();
    currentScreen = SCREEN_SECURITY_MENU;
    drawSecurityMenu();
  }
}

void updateRSSIHistory() {
  if (apCount == 0) return;

  uint8_t topIndices[MAX_TRACKED_APS] = {0, 0, 0};
  int8_t topRSSI[MAX_TRACKED_APS] = {-128, -128, -128};

  for (int i = 0; i < apCount && i < MAX_APS; i++) {
    for (int j = 0; j < MAX_TRACKED_APS; j++) {
      if (apList[i].rssi > topRSSI[j]) {
        for (int k = MAX_TRACKED_APS - 1; k > j; k--) {
          topRSSI[k] = topRSSI[k - 1];
          topIndices[k] = topIndices[k - 1];
        }
        topRSSI[j] = apList[i].rssi;
        topIndices[j] = i;
        break;
      }
    }
  }

  for (int i = 0; i < MAX_TRACKED_APS; i++) {
    if (topRSSI[i] > -128) {
      uint8_t apIdx = topIndices[i];

      bool found = false;
      for (int j = 0; j < MAX_TRACKED_APS; j++) {
        if (rssiHistory[j].active &&
            memcmp(rssiHistory[j].bssid, apList[apIdx].bssid, 6) == 0) {
          rssiHistory[j].rssiSamples[rssiHistory[j].sampleIndex] = apList[apIdx].rssi;
          rssiHistory[j].sampleIndex = (rssiHistory[j].sampleIndex + 1) % RSSI_HISTORY_SIZE;
          found = true;
          break;
        }
      }

      if (!found) {
        for (int j = 0; j < MAX_TRACKED_APS; j++) {
          if (!rssiHistory[j].active) {
            memcpy(rssiHistory[j].bssid, apList[apIdx].bssid, 6);
            memset(rssiHistory[j].rssiSamples, -100, RSSI_HISTORY_SIZE);
            rssiHistory[j].rssiSamples[0] = apList[apIdx].rssi;
            rssiHistory[j].sampleIndex = 1;
            rssiHistory[j].active = true;
            break;
          }
        }
      }
    }
  }
}

void handleWhyIsItSlow(ButtonEvent ev) {
  if (ev == BTN_LONG) {
    whySlowView = (whySlowView + 1) % 2;
    whySlowApIdx = 0;
  }

  if (ev == BTN_SHORT && whySlowView == 1) {
    uint8_t activeCount = 0;
    for (int i = 0; i < MAX_TRACKED_APS; i++) {
      if (rssiHistory[i].active) activeCount++;
    }
    whySlowApIdx = (whySlowApIdx + 1) % (activeCount + 1);
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_INSIGHTS_MENU;
    stopAllWifi();
    whySlowView = 0;
    whySlowApIdx = 0;
    drawInsightsMenu();
    return;
  }

  if (millis() - scanState.lastScan > 2000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    updateRSSIHistory();
    scanState.lastScan = millis();
  }

  drawWhyIsItSlow();
}

void handleChannelRecommendation(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_INSIGHTS_MENU;
    stopAllWifi();
    drawInsightsMenu();
    return;
  }

  if (millis() - scanState.lastScan > 3000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    scanState.lastScan = millis();
  }
  drawChannelRecommendation();
}

void handleEnvironmentChange(ButtonEvent ev) {
  if (ev == BTN_LONG) {
    baseline = currentSnapshot;
    logEvent(1, "Baseline saved");
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_INSIGHTS_MENU;
    stopAllWifi();
    drawInsightsMenu();
    return;
  }

  if (millis() - scanState.lastEnvCheck > 2000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    takeSnapshot(&currentSnapshot);
    scanState.lastEnvCheck = millis();
  }
  drawEnvironmentChange();
}

void handleQuickSnapshot(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_INSIGHTS_MENU;
    stopAllWifi();
    drawInsightsMenu();
    return;
  }

  if (millis() - scanState.lastScan > 2000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    updateBLEScan();
    scanState.lastScan = millis();
  }

  drawQuickSnapshot();
}

static uint8_t scorecardPage = 0;

void handleChannelScorecard(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    scorecardPage = (scorecardPage + 1) % 4;
  }

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_INSIGHTS_MENU;
    stopAllWifi();
    scorecardPage = 0;
    drawInsightsMenu();
    return;
  }

  if (millis() - scanState.lastScan > 2000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    scanState.lastScan = millis();
  }

  drawChannelScorecard(scorecardPage);
}

void handleEventLog(ButtonEvent ev) {
  drawEventLog();

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_HISTORY_MENU;
    drawHistoryMenu();
  }
}

void handleBaselineCompare(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    currentScreen = SCREEN_HISTORY_MENU;
    stopAllWifi();
    drawHistoryMenu();
    return;
  }

  if (millis() - scanState.lastBaselineUpdate > 2000) {
    enterScanMode();
    startApScan();
    buttonAwareDelay(WIFI_SCAN_DELAY_MS);
    fetchApResults(false);
    takeSnapshot(&currentSnapshot);
    scanState.lastBaselineUpdate = millis();
  }

  drawBaselineCompare();
}

void handleBatteryPower(ButtonEvent ev) {
  drawBatteryPower();

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_SYSTEM_MENU;
    drawSystemMenu();
  }
}

void handleDisplaySettings(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    displaySettingCursor = (displaySettingCursor + 1) % 2;
  } else if (ev == BTN_LONG) {
    if (displaySettingCursor == 0) {
      settings.rgbBrightness = (settings.rgbBrightness + 10) % 110;
      if (settings.rgbBrightness > 100) settings.rgbBrightness = 0;
      rgb.setBrightness((settings.rgbBrightness * 255) / 100);
      rgb.show();
    } else if (displaySettingCursor == 1) {
      if (settings.screenTimeout == 0) settings.screenTimeout = 30;
      else if (settings.screenTimeout == 30) settings.screenTimeout = 60;
      else if (settings.screenTimeout == 60) settings.screenTimeout = 120;
      else if (settings.screenTimeout == 120) settings.screenTimeout = 300;
      else settings.screenTimeout = 0;
    }
  }

  drawDisplaySettings();

  if (ev == BTN_BACK) {
    saveSettings();
    displaySettingCursor = 0;
    currentScreen = SCREEN_SYSTEM_MENU;
    drawSystemMenu();
  }
}

void handleRadioControl(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    scanState.currentChannel = (scanState.currentChannel % MAX_CHANNEL) + 1;
  } else if (ev == BTN_LONG) {
    scanState.currentChannel = (scanState.currentChannel - 2 + MAX_CHANNEL) % MAX_CHANNEL + 1;
  }

  drawRadioControl();

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_SYSTEM_MENU;
    drawSystemMenu();
  }
}

void handleAbout(ButtonEvent ev) {
  drawAbout();

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_SYSTEM_MENU;
    drawSystemMenu();
  }
}

void handlePowerMode(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    settings.powerMode = (settings.powerMode + 1) % 3;
  }

  drawPowerMode();

  if (ev == BTN_BACK) {
    saveSettings();
    currentScreen = SCREEN_SYSTEM_MENU;
    drawSystemMenu();
    return;
  }
}

void handleExport(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    Serial.println("\n========== DATA EXPORT ==========");
    Serial.printf("Timestamp: %lu ms\n\n", millis());

    Serial.printf("WiFi Networks: %d\n", apCount);
    for (int i = 0; i < apCount; i++) {
      Serial.printf("%d,%s,%02X:%02X:%02X:%02X:%02X:%02X,%d,%d\n",
        i + 1,
        strlen((char*)apList[i].ssid) ? (char*)apList[i].ssid : "<hidden>",
        apList[i].bssid[0], apList[i].bssid[1], apList[i].bssid[2],
        apList[i].bssid[3], apList[i].bssid[4], apList[i].bssid[5],
        apList[i].rssi,
        apList[i].primary
      );
    }

    Serial.printf("\nBLE Devices: %d\n", getActiveBLECount());
    for (int i = 0; i < bleDeviceCount; i++) {
      if (!bleDevices[i].isActive) continue;
      Serial.printf("%d,%s,%s,%d\n",
        i + 1,
        strlen(bleDevices[i].name) > 0 ? bleDevices[i].name : "<unknown>",
        bleDevices[i].address,
        bleDevices[i].rssi
      );
    }

    Serial.printf("\nSecurity Events: %d\n", eventCount);
    for (int i = 0; i < eventCount; i++) {
      Serial.printf("%d,%d,%s\n",
        i + 1,
        eventLog[i].type,
        eventLog[i].message
      );
    }

    Serial.println("========== END EXPORT ==========\n");
  }

  drawExport();

  if (ev == BTN_BACK) {
    currentScreen = SCREEN_HISTORY_MENU;
    stopAllWifi();
    drawHistoryMenu();
    return;
  }
}

void handleStats(ButtonEvent ev) {
  if (ev == BTN_SHORT) {
    currentScreen = SCREEN_MENU;
    drawMenu();
  }

  if (ev == BTN_LONG) {
    resetSession();
  }

  drawStats();
}

void handleWebServer(ButtonEvent ev) {
  if (ev == BTN_BACK) {
    stopWebServer();
    currentScreen = SCREEN_MENU;
    drawMenu();
    return;
  }

  startWebServer();
  handleWebServerLoop();
  
  static uint32_t lastWebDraw = 0;
  if (millis() - lastWebDraw > 1000) {
    drawWebServer();
    lastWebDraw = millis();
  }
}
