#include "screens_draw.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "menu.h"
#include "display.h"
#include "utils.h"
#include "security.h"
#include "alerts.h"
#include "ui_helpers.h"

extern uint32_t pps, peak, peakPPS;

extern uint32_t history[HISTORY_SIZE];
extern uint8_t histIdx;
extern uint8_t currentChannel, selectedChannel, analyzerChannel;
extern float avgRssi;
extern bool frozen;
extern volatile uint32_t pktBeacon, pktData, pktDeauth, pktTotal;
extern uint32_t deauthPerSecond, totalDeauthDetected;
extern bool attackActive;
extern uint8_t apCursor, apScroll, apSelectedIndex;
extern bool apSortedOnce;
extern uint8_t apCompareA, apCompareB;
extern uint8_t bleCursor, bleScroll, bleSelectedIndex;
extern uint8_t hiddenCursor, hiddenScroll;
extern uint8_t settingsCursor;
extern bool loggingActive;
extern uint32_t loggedPackets;
extern uint32_t sessionStart;
extern uint32_t totalAPsFound;

extern uint8_t whySlowView;
extern uint8_t whySlowApIdx;
extern RSSIHistory rssiHistory[MAX_TRACKED_APS];
extern uint8_t alertLevel;
extern uint32_t lastAlertBlink;
extern bool alertBlinkState;
extern bool prevAttackActive;
extern uint8_t prevRogueCount;
extern uint8_t deauthChannel;
extern uint8_t displaySettingCursor;
extern uint8_t rfHealthView;
extern int8_t rfHealthRSSIHistory[60];
extern uint8_t rfHealthHistoryIndex;
extern int8_t rfHealthMinRSSI, rfHealthMaxRSSI;

void drawGenericMenu(const char* title, const char** items, uint8_t itemCount, uint8_t& cursor) {
  oled.firstPage();
  do {
    drawHeader(title);

    uint8_t startIdx = 0;
    if (cursor >= 4) startIdx = cursor - 3;

    oled.setFont(u8g2_font_5x7_tf);
    for (int i = 0; i < 4 && (startIdx + i) < itemCount; i++) {
      int itemIdx = startIdx + i;
      uint8_t y = 23 + i * 12;

      if (itemIdx == cursor) {
        oled.drawRBox(2, y - 8, 118, 11, 2);
        oled.setDrawColor(0);
        oled.drawStr(8, y, items[itemIdx]);
        oled.setDrawColor(1);
      } else {
        oled.drawStr(8, y, items[itemIdx]);
      }
    }

    drawScrollDots(startIdx, itemCount, 4);
  } while (oled.nextPage());
}

void takeSnapshot(Baseline* snap) {
  snap->timestamp = millis();
  snap->totalAPs = apCount;

  int32_t rssiSum = 0;
  for (int i = 0; i < apCount; i++) {
    rssiSum += apList[i].rssi;
  }
  snap->avgRSSI = apCount > 0 ? rssiSum / apCount : 0;

  memset(snap->channelDist, 0, sizeof(snap->channelDist));
  for (int i = 0; i < apCount; i++) {
    if (apList[i].primary >= 1 && apList[i].primary <= 13) {
      snap->channelDist[apList[i].primary - 1]++;
    }
  }

  snap->totalPackets = totalPackets;
  snap->saved = true;
}

void drawMenu() {
  drawGenericMenu("POCKET RF TOOL", mainMenuItems, MAIN_MENU_SIZE, mainMenuIndex);
}

void drawSecurityMenu() {
  drawGenericMenu("SECURITY", securityMenuItems, SECURITY_MENU_SIZE, securityMenuIndex);
}

void drawInsightsMenu() {
  drawGenericMenu("INSIGHTS", insightsMenuItems, INSIGHTS_MENU_SIZE, insightsMenuIndex);
}

void drawHistoryMenu() {
  drawGenericMenu("HISTORY", historyMenuItems, HISTORY_MENU_SIZE, historyMenuIndex);
}

void drawSystemMenu() {
  drawGenericMenu("SYSTEM", systemMenuItems, SYSTEM_MENU_SIZE, systemMenuIndex);
}

void drawMonitor() {
  oled.firstPage();
  do {
    char hdr[30];
    sprintf(hdr, "CH%02d %luP/s L:%d%%", scanState.currentChannel, pps, liveLoad());
    drawHeader(hdr);

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 20);
    oled.printf("B:%lu D:%lu X:%lu", pktBeacon, pktData, pktDeauth);

    int rssiBar = constrain((int)avgRssi + 100, 0, 50);
    oled.drawRFrame(0, 22, 52, 6, 1);
    if (rssiBar > 0) oled.drawRBox(1, 23, rssiBar, 4, 1);
    oled.setCursor(54, 27);
    oled.printf("%ddBm", (int)avgRssi);

    if (avgRssi > settings.rssiThreshold) {
      oled.drawStr(110, 27, "!");
    }

    uint8_t graphY = 29;
    uint8_t graphH = 24;

    drawGrid(0, graphY, 128, graphH);

    for (int x = 0; x < 128; x++) {
      int idx = (histIdx + x) % HISTORY_SIZE;
      uint32_t val = history[idx];
      int barHeight = 0;
      if (peak > 0) barHeight = (val * graphH) / peak;

      if (barHeight > 0) {
        int solidHeight = barHeight / 3;
        if (solidHeight > 0) {
          oled.drawVLine(x, graphY + graphH - barHeight, solidHeight);
        }

        int midStart = graphY + graphH - barHeight + solidHeight;
        int midHeight = (barHeight * 2) / 5;
        for (int y = midStart; y < midStart + midHeight; y++) {
          if ((x + y) % 2 == 0) oled.drawPixel(x, y);
        }

        int bottomStart = midStart + midHeight;
        int bottomHeight = barHeight - solidHeight - midHeight;
        for (int y = bottomStart; y < bottomStart + bottomHeight; y++) {
          if ((x + y) % 3 == 0) oled.drawPixel(x, y);
        }
      }
    }

    oled.drawRFrame(0, graphY, 128, graphH, 2);

    if (scanState.frozen) {
      oled.drawRBox(117, graphY, 10, 10, 2);
      oled.setDrawColor(0);
      oled.drawStr(119, graphY + 7, "||");
      oled.setDrawColor(1);
    }

    drawFooter(channelInsight(), "BACK");
  } while (oled.nextPage());
}

void drawAnalyzer() {
  uint8_t best = bestChannel();
  uint8_t worst = worstChannel();

  oled.firstPage();
  do {
    drawHeader("CH ANALYZER");

    const uint8_t chartX = 2, chartY = 14, chartW = 124, chartH = 30;
    const uint8_t chartBottom = chartY + chartH;
    const uint8_t barW = 7, barSpacing = 9; // 9px per channel = 117px for 13

    for (int i = 1; i <= 2; i++) {
      uint8_t gy = chartY + (chartH * i / 3);
      for (uint8_t gx = chartX; gx < chartX + chartW; gx += 3) {
        oled.drawPixel(gx, gy);
      }
    }

    oled.setFont(u8g2_font_4x6_tf);
    for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
      uint8_t x = chartX + (ch - 1) * barSpacing + 1;
      uint8_t load = channelLoad(ch);
      int barH = (int)load * chartH / 100;
      if (barH > chartH) barH = chartH;

      if (load > 0) {
        if (barH < 2) barH = 2;
        uint8_t barTop = chartBottom - barH;

        if (ch == best) {
          oled.drawFrame(x, barTop, barW, barH);
        } else if (ch == worst && load > 0) {
          oled.drawBox(x, barTop, barW, barH);
        } else {
          drawPatternBar(x, barTop, barW, barH, load);
        }
      }

      char chStr[4];
      sprintf(chStr, "%d", ch);
      uint8_t labelX = x + 1;
      if (ch >= 10) labelX = x;

      if (ch == scanState.selectedChannel) {
        oled.drawBox(x - 1, 45, barW + 2, 8);
        oled.setDrawColor(0);
        oled.drawStr(labelX, 52, chStr);
        oled.setDrawColor(1);
      } else {
        oled.drawStr(labelX, 52, chStr);
      }

      if (ch == best) {
        oled.drawStr(x + 2, chartY - 1, "*");
      } else if (countOverlappingAPs(ch) > 2) {
        oled.drawStr(x + 2, chartY - 1, "!");
      }
    }

    oled.setFont(u8g2_font_5x7_tf);
    oled.drawRFrame(0, 54, 128, 10, 2);
    oled.setCursor(4, 62);
    oled.printf("BEST:%d  WORST:%d", best, worst);

  } while (oled.nextPage());
}


void drawAutoWatch() {
  oled.firstPage();
  do {
    if (autoWatch.viewMode == 0) {
      drawHeader("AUTO WATCH");
      oled.setFont(u8g2_font_5x7_tf);
      char buf[22];

      sprintf(buf, "APs:%d BLE:%d", autoWatch.totalAPs, autoWatch.totalBLE);
      oled.drawStr(2, 24, buf);

      if (apCount > 0) {
        long rssiSum = 0;
        for (int i = 0; i < apCount; i++) rssiSum += apList[i].rssi;
        int avg = rssiSum / apCount;
        int barW = constrain(map(avg, -90, -30, 0, 30), 0, 30);
        oled.drawRFrame(96, 17, 30, 6, 1);
        if (barW > 0) oled.drawBox(97, 18, barW - 1, 4);
      }

      for (uint8_t x = 2; x < 126; x += 3) oled.drawPixel(x, 27);

      sprintf(buf, "CH:%d PKT:%lu", scanState.currentChannel, pktTotal);
      oled.drawStr(2, 36, buf);

      for (uint8_t x = 2; x < 126; x += 3) oled.drawPixel(x, 39);

      if (deauthPerSecond > 0) {
        oled.drawDisc(6, 46, 3);
        sprintf(buf, "DEAUTH:%lu/s", deauthPerSecond);
        oled.drawStr(12, 49, buf);
      } else {
        oled.drawCircle(6, 46, 3);
        oled.drawStr(12, 49, "No attacks");
      }

    } else if (autoWatch.viewMode == 1) {
      drawHeader("TOP APs");
      oled.setFont(u8g2_font_5x7_tf);
      uint8_t showCount = min(apCount, (uint16_t)4);
      for (int i = 0; i < showCount; i++) {
        char ssid[13];
        strncpy(ssid, (char*)apList[i].ssid, 12);
        ssid[12] = '\0';
        char buf[22];
        sprintf(buf, "%s %d", strlen((char*)apList[i].ssid) ? ssid : "<hidden>", apList[i].rssi);
        oled.drawStr(2, 23 + i * 10, buf);
      }

    } else if (autoWatch.viewMode == 2) {
      drawHeader("TOP BLE");
      oled.setFont(u8g2_font_5x7_tf);
      uint8_t displayedCount = 0;
      for (int i = 0; i < bleDeviceCount && displayedCount < 4; i++) {
        if (!bleDevices[i].isActive) continue;
        char name[13];
        if (strlen(bleDevices[i].name) > 0) {
          strncpy(name, bleDevices[i].name, 12);
          name[12] = '\0';
        } else {
          strcpy(name, "Unknown");
        }
        char buf[22];
        sprintf(buf, "%s %d", name, bleDevices[i].rssi);
        oled.drawStr(2, 23 + displayedCount * 10, buf);
        displayedCount++;
      }

    } else if (autoWatch.viewMode == 3) {
      char title[20];
      sprintf(title, "CH%d APs", scanState.currentChannel);
      drawHeader(title);
      oled.setFont(u8g2_font_4x6_tf);
      uint8_t channelAPCount = 0;
      uint8_t shown = 0;
      for (int i = 0; i < apCount && shown < 4; i++) {
        if (apList[i].primary == scanState.currentChannel) {
          char ssid[11];
          strncpy(ssid, (char*)apList[i].ssid, 10);
          ssid[10] = '\0';
          char buf[24];
          sprintf(buf, "%s %d", strlen((char*)apList[i].ssid) ? ssid : "<hidden>", apList[i].rssi);
          oled.drawStr(2, 22 + shown * 9, buf);
          shown++;
          channelAPCount++;
        }
      }
      for (int i = 0; i < apCount; i++) {
        if (apList[i].primary == scanState.currentChannel) channelAPCount++;
      }
    }

    drawFooter("VIEW", "BACK");
  } while (oled.nextPage());
}

void drawRFHealth() {
  if (rfHealthView == 0) {
    int totalDevices = apCount + bleDeviceCount;
    int avgRSSI = 0;
    int channelLoad[13] = {0};

    if (apCount > 0) {
      long rssiSum = 0;
      for (int i = 0; i < apCount; i++) {
        rssiSum += apList[i].rssi;
        if (apList[i].primary >= 1 && apList[i].primary <= 13) {
          channelLoad[apList[i].primary - 1]++;
        }
      }
      avgRSSI = rssiSum / apCount;
    }

    int busiestCh = 0;
    int maxLoad = 0;
    for (int i = 0; i < 13; i++) {
      if (channelLoad[i] > maxLoad) {
        maxLoad = channelLoad[i];
        busiestCh = i + 1;
      }
    }

    int healthScore = 100;
    if (maxLoad > 10) healthScore -= 30;
    else if (maxLoad > 5) healthScore -= 15;
    if (avgRSSI < -80) healthScore -= 20;
    else if (avgRSSI < -70) healthScore -= 10;
    if (totalDevices > 50) healthScore -= 20;
    else if (totalDevices > 30) healthScore -= 10;

    healthScore = constrain(healthScore, 0, 100);

    oled.firstPage();
    do {
      drawHeader("RF HEALTH");
      oled.setFont(u8g2_font_5x7_tf);

      oled.setCursor(2, 23);
      oled.printf("Devices: %d (%dAP+%dBLE)", totalDevices, apCount, bleDeviceCount);
      oled.setCursor(2, 33);
      oled.printf("Avg RSSI: %ddBm", avgRSSI);
      oled.setCursor(2, 43);
      oled.printf("Busy CH: %d (%d APs)", busiestCh, maxLoad);

      oled.setFont(u8g2_font_4x6_tf);
      oled.setCursor(2, 51);
      oled.printf("Health: %d%%", healthScore);
      {
        const uint8_t barX = 60, barY = 46, segW = 11, segH = 7, gap = 2;
        int filledSegs = (healthScore + 19) / 20; // 0-5 segments
        for (int s = 0; s < 5; s++) {
          uint8_t sx = barX + s * (segW + gap);
          if (s < filledSegs) {
            oled.drawRBox(sx, barY, segW, segH, 1);
          } else {
            oled.drawRFrame(sx, barY, segW, segH, 1);
          }
        }
        uint8_t tick1x = barX + (uint16_t)5 * (segW + gap) * 33 / 100;
        uint8_t tick2x = barX + (uint16_t)5 * (segW + gap) * 66 / 100;
        oled.drawPixel(tick1x, barY + segH + 1);
        oled.drawPixel(tick2x, barY + segH + 1);
      }

      drawFooter("GRAPH", "BACK");
    } while (oled.nextPage());
  } else {
    char subtitle[30];
    int8_t currentAvg = rfHealthRSSIHistory[(rfHealthHistoryIndex - 1 + 60) % 60];
    sprintf(subtitle, "Now:%d Min:%d Max:%d", currentAvg, rfHealthMinRSSI, rfHealthMaxRSSI);
    
    drawRSSIGraph(rfHealthRSSIHistory, rfHealthHistoryIndex, -90, -30, "Avg RSSI Graph", subtitle);
  }
}


void drawDeviceMonitor() {
  oled.firstPage();
  do {
    drawHeader("CLIENT MONITOR");

    if (monitoredDeviceCount == 0) {
      oled.setFont(u8g2_font_5x7_tf);
      oled.setCursor(10, 30);
      oled.print("No devices yet");
      oled.setCursor(5, 42);
      oled.print("Scanning...");
    } else {
      oled.setFont(u8g2_font_4x6_tf);

      uint8_t shown = 0;
      uint8_t activeIndex = 0;
      for (int i = 0; i < MAX_MONITORED_DEVICES && shown < 3; i++) {
        if (!monitoredDevices[i].active) continue;

        if (activeIndex < deviceScroll) {
          activeIndex++;
          continue;
        }

        uint8_t y = 16 + (shown * 16);

        if (shown == deviceCursor) {
          oled.drawRBox(0, y - 6, 128, 15, 2);
          oled.setDrawColor(0);
        }

        oled.setCursor(0, y);
        oled.printf("%c%c", monitoredDevices[i].type == 0 ? 'C' : 'B',
                            monitoredDevices[i].isPresent ? '+' : '-');

        char name[11];
        strncpy(name, monitoredDevices[i].name, 10);
        name[10] = '\0';
        oled.setCursor(16, y);
        oled.print(name);

        oled.setCursor(104, y);
        oled.printf("%d", monitoredDevices[i].rssi);

        oled.setCursor(8, y + 7);
        if (monitoredDevices[i].type == 0) {
          oled.printf("%02X:%02X:%02X:%02X:%02X:%02X",
            monitoredDevices[i].bssid[0], monitoredDevices[i].bssid[1],
            monitoredDevices[i].bssid[2], monitoredDevices[i].bssid[3],
            monitoredDevices[i].bssid[4], monitoredDevices[i].bssid[5]);
        } else {
          oled.print(monitoredDevices[i].bleAddr);
        }

        if (shown == deviceCursor) oled.setDrawColor(1);

        shown++;
        activeIndex++;
      }

      drawScrollDots(deviceScroll, monitoredDeviceCount, 3);
    }

    drawFooterPill(1, "SEL");
    drawFooterPill(32, "DETAIL");
    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawDeviceDetail() {
  if (deviceSelectedIndex >= MAX_MONITORED_DEVICES ||
      !monitoredDevices[deviceSelectedIndex].active) {
    drawPlaceholder("DEVICE DETAIL", "Invalid Device");
    return;
  }

  MonitoredDevice* dev = &monitoredDevices[deviceSelectedIndex];

  oled.firstPage();
  do {
    drawHeader(dev->type == 0 ? "WiFi CLIENT" : "BLE DEVICE");

    oled.setFont(u8g2_font_4x6_tf);

    oled.setCursor(0, 22);
    oled.print("Name:");
    oled.setCursor(30, 22);
    char name[20];
    strncpy(name, dev->name, 19);
    name[19] = '\0';
    oled.print(name);

    oled.setCursor(0, 30);
    if (dev->type == 0) {
      oled.print("BSSID:");
      oled.setCursor(30, 30);
      oled.printf("%02X:%02X:%02X:%02X:%02X:%02X",
        dev->bssid[0], dev->bssid[1], dev->bssid[2],
        dev->bssid[3], dev->bssid[4], dev->bssid[5]);
    } else {
      oled.print("Addr:");
      oled.setCursor(30, 30);
      oled.print(dev->bleAddr);
    }

    oled.setCursor(0, 38);
    oled.printf("RSSI: %ddBm", dev->rssi);

    if (dev->type == 0) {
      oled.setCursor(64, 38);
      oled.printf("CH: %d", dev->channel);
    }

    oled.setCursor(0, 44);
    oled.print("Status:");
    oled.setCursor(35, 44);
    oled.print(dev->isPresent ? "Present" : "Not Seen");

    oled.setCursor(0, 50);
    oled.printf("Seen: %d times", dev->seenCount);

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawApList() {
  oled.firstPage();
  do {
    char hdr[25];
    sprintf(hdr, "APs:%d BEST:#%d", apCount, bestAPIndex() + 1);
    drawHeader(hdr);

    for (int row = 0; row < AP_VISIBLE; row++) {
      int idx = apScroll + row;
      if (idx >= apCount) break;

      int yPos = 22 + row * 14;

      if (row == apCursor) {
        oled.drawRBox(0, yPos - 7, 120, 14, 2);
        oled.setDrawColor(0);
      }

      oled.setFont(u8g2_font_4x6_tf);
      if (apList[idx].authmode == WIFI_AUTH_OPEN) {
        oled.drawStr(2, yPos, "!");
      } else if (apList[idx].authmode >= WIFI_AUTH_WPA2_PSK) {
        oled.drawStr(2, yPos, "*");
      } else {
        oled.drawStr(2, yPos, "~");
      }

      oled.setFont(u8g2_font_5x7_tf);
      char ssidBuf[11] = {0};
      const char* ssid = strlen((char*)apList[idx].ssid) ? (char*)apList[idx].ssid : "<hidden>";
      strncpy(ssidBuf, ssid, 10);
      ssidBuf[10] = 0;
      oled.drawStr(9, yPos, ssidBuf);

      oled.setFont(u8g2_font_4x6_tf);
      oled.setCursor(72, yPos);
      oled.printf("CH%d", apList[idx].primary);

      char grade = getQualityGrade(&apList[idx]);
      oled.setCursor(98, yPos);
      oled.print(grade);

      int rssi = apList[idx].rssi;
      int bars = 0;
      if (rssi >= -50) bars = 4;
      else if (rssi >= -60) bars = 3;
      else if (rssi >= -70) bars = 2;
      else if (rssi >= -80) bars = 1;

      int barX = 107;
      for (int b = 0; b < 4; b++) {
        int barH = 2 + b * 2;
        if (b < bars) {
          oled.drawBox(barX + b * 5, yPos - barH, 3, barH);
        } else {
          oled.drawFrame(barX + b * 5, yPos - barH, 3, barH);
        }
      }

      oled.setCursor(9, yPos + 6);
      oled.printf("%s %.1fm", getVendor(apList[idx].bssid),
                  estimateDistance(apList[idx].rssi));

      if (row == apCursor) oled.setDrawColor(1);
    }

    drawScrollDots(apScroll, apCount, AP_VISIBLE);
  } while (oled.nextPage());
}

void drawApDetail() {
  wifi_ap_record_t* ap = &apList[apSelectedIndex];
  oled.firstPage();
  do {
    char hdr[22] = {0};
    strncpy(hdr, strlen((char*)ap->ssid) ? (char*)ap->ssid : "<hidden>", 16);
    drawHeader(hdr);

    oled.setFont(u8g2_font_5x7_tf);

    oled.setCursor(0, 22);
    oled.printf("CH:%d RSSI:%d (%c)", ap->primary, ap->rssi, getQualityGrade(ap));

    oled.setCursor(0, 30);
    oled.printf("SEC:%s", authStr(ap->authmode));

    oled.setCursor(0, 38);
    oled.printf("Vendor:%s", getVendor(ap->bssid));

    oled.setCursor(0, 46);
    oled.printf("Distance:%.1fm", estimateDistance(ap->rssi));

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 52);
    oled.printf("MAC:%02X:%02X:%02X:%02X:%02X:%02X",
      ap->bssid[0], ap->bssid[1], ap->bssid[2],
      ap->bssid[3], ap->bssid[4], ap->bssid[5]);

    drawFooter("WALK", "LIST");
  } while (oled.nextPage());
}

void drawAPWalkTest() {
  if (walkTest.viewMode == 0) {
    oled.firstPage();
    do {
      drawHeader("AP WALK TEST");

      if (walkTest.active) {
        oled.setFont(u8g2_font_5x7_tf);
        char ssid[17];
        strncpy(ssid, walkTest.targetSSID, 16);
        ssid[16] = '\0';
        oled.setCursor(0, 22);
        oled.printf("%s", strlen(walkTest.targetSSID) ? ssid : "<hidden>");

        int8_t currentRSSI = walkTest.sampleCount > 0 ? walkTest.rssiHistory[(walkTest.historyIndex - 1 + WALK_HISTORY_SIZE) % WALK_HISTORY_SIZE] : 0;
        oled.setCursor(0, 32);
        if (currentRSSI != 0) {
          oled.printf("Now: %ddBm", currentRSSI);
        } else {
          oled.print("Scanning...");
        }

        int8_t avgRSSI = walkTest.sampleCount > 0 ? (walkTest.rssiSum / walkTest.sampleCount) : 0;
        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(0, 40);
        oled.printf("Min:%d Max:%d Avg:%d", walkTest.minRSSI, walkTest.maxRSSI, avgRSSI);

        oled.drawRFrame(0, 45, 128, 16, 2);
        {
          const uint8_t mgY = 46, mgH = 13, mgBottom = mgY + mgH;
          int8_t prevX = -1, prevYpos = -1;
          uint8_t lastX = 0, lastYpos = mgBottom;
          bool hasLast = false;

          for (int i = 0; i < WALK_HISTORY_SIZE && i < 126; i++) {
            int idx = (walkTest.historyIndex + i) % WALK_HISTORY_SIZE;
            if (walkTest.rssiHistory[idx] == 0) { prevX = -1; continue; }
            int h = map(constrain(walkTest.rssiHistory[idx], -90, -30), -90, -30, 1, mgH);
            uint8_t x = 1 + (i * 126 / WALK_HISTORY_SIZE);
            uint8_t yp = mgBottom - h;

            for (uint8_t fy = yp + 1; fy < mgBottom; fy++) {
              if ((x + fy) % 3 == 0) oled.drawPixel(x, fy);
            }

            if (prevX >= 0) oled.drawLine(prevX, prevYpos, x, yp);
            prevX = x; prevYpos = yp;
            lastX = x; lastYpos = yp; hasLast = true;
          }

          if (hasLast && lastX > 2 && lastX < 126 && lastYpos > mgY + 1 && lastYpos < mgBottom - 1) {
            oled.drawDisc(lastX, lastYpos, 1);
          }
        }

        drawFooter("GRAPH", nullptr);
      } else {
        oled.setFont(u8g2_font_5x7_tf);
        oled.drawStr(10, 35, "No AP selected");
      }
    } while (oled.nextPage());
  } else {
    char ssid[17];
    strncpy(ssid, walkTest.targetSSID, 16);
    ssid[16] = '\0';
    char subtitle[40];
    int8_t avgRSSI = walkTest.sampleCount > 0 ? (walkTest.rssiSum / walkTest.sampleCount) : 0;
    sprintf(subtitle, "Avg:%d Min:%d Max:%d", avgRSSI, walkTest.minRSSI, walkTest.maxRSSI);

    drawRSSIGraph(walkTest.rssiHistory, walkTest.historyIndex, -90, -30, strlen(walkTest.targetSSID) ? ssid : "<hidden>", subtitle);
  }
}


void drawCompare() {
  wifi_ap_record_t* apA = &apList[apCompareA];
  wifi_ap_record_t* apB = &apList[apCompareB];

  oled.firstPage();
  do {
    drawHeader("AP COMPARE");

    oled.setFont(u8g2_font_4x6_tf);

    oled.setCursor(0, 22);
    char ssidA[9] = {0};
    strncpy(ssidA, (char*)apA->ssid, 8);
    oled.printf("A:%s", ssidA);

    oled.setCursor(0, 28);
    oled.printf("RSSI:%d", apA->rssi);

    oled.setCursor(0, 34);
    oled.printf("CH:%d", apA->primary);

    oled.setCursor(0, 40);
    oled.printf("SEC:%s", apA->authmode >= WIFI_AUTH_WPA2_PSK ? "Good" : "Weak");

    oled.setCursor(0, 46);
    oled.printf("GRADE:%c", getQualityGrade(apA));

    oled.setCursor(0, 52);
    oled.printf("DIST:%.1fm", estimateDistance(apA->rssi));

    for (int y = 14; y < 53; y += 2) {
      oled.drawPixel(64, y);
    }

    oled.setCursor(68, 22);
    char ssidB[9] = {0};
    strncpy(ssidB, (char*)apB->ssid, 8);
    oled.printf("B:%s", ssidB);

    oled.setCursor(68, 28);
    oled.printf("RSSI:%d", apB->rssi);

    oled.setCursor(68, 34);
    oled.printf("CH:%d", apB->primary);

    oled.setCursor(68, 40);
    oled.printf("SEC:%s", apB->authmode >= WIFI_AUTH_WPA2_PSK ? "Good" : "Weak");

    oled.setCursor(68, 46);
    oled.printf("GRADE:%c", getQualityGrade(apB));

    oled.setCursor(68, 52);
    oled.printf("DIST:%.1fm", estimateDistance(apB->rssi));

    char gradeA = getQualityGrade(apA);
    char gradeB = getQualityGrade(apB);
    if (gradeA < gradeB) {
      drawFooter("WINNER", nullptr);
    } else if (gradeB < gradeA) {
      drawFooter(nullptr, "WINNER");
    } else {
      drawFooter("TIE", nullptr);
    }

  } while (oled.nextPage());
}

void drawBLEScan() {
  oled.firstPage();
  do {
    uint8_t activeCount = 0;
    for (int i = 0; i < bleDeviceCount; i++) {
      if (bleDevices[i].isActive) activeCount++;
    }
    char hdr[22];
    sprintf(hdr, "BLE:%d (%d)", bleDeviceCount, activeCount);
    drawHeader(hdr);

    if (bleDeviceCount == 0) {
      oled.setFont(u8g2_font_5x7_tf);
      oled.drawStr(30, 35, "Scanning...");
    } else {
      for (int row = 0; row < BLE_VISIBLE; row++) {
        int idx = bleScroll + row;
        if (idx >= bleDeviceCount) break;

        int yPos = 22 + row * 14;

        if (row == bleCursor) {
          oled.drawRBox(0, yPos - 7, 120, 14, 2);
          oled.setDrawColor(0);
        }

        oled.setFont(u8g2_font_4x6_tf);
        if (bleDevices[idx].advType == 1) {
          oled.drawStr(2, yPos, "B");
        } else if (bleDevices[idx].isActive) {
          oled.drawStr(2, yPos, "*");
        } else {
          oled.drawStr(2, yPos, "-");
        }

        oled.setFont(u8g2_font_5x7_tf);
        char nameBuf[11] = {0};
        strncpy(nameBuf, bleDevices[idx].name, 10);
        nameBuf[10] = 0;
        oled.drawStr(9, yPos, nameBuf);

        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(78, yPos);
        oled.printf("%d", bleDevices[idx].rssi);

        int rssi = bleDevices[idx].rssi;
        int bars = 0;
        if (rssi >= -50) bars = 4;
        else if (rssi >= -65) bars = 3;
        else if (rssi >= -75) bars = 2;
        else if (rssi >= -85) bars = 1;

        int barX = 107;
        for (int b = 0; b < 4; b++) {
          int barH = 2 + b * 2;
          if (b < bars) {
            oled.drawBox(barX + b * 5, yPos - barH, 3, barH);
          } else {
            oled.drawFrame(barX + b * 5, yPos - barH, 3, barH);
          }
        }

        oled.setCursor(9, yPos + 6);
        char macBuf[13] = {0};
        strncpy(macBuf, bleDevices[idx].address, 12);
        oled.print(macBuf);

        if (row == bleCursor) oled.setDrawColor(1);
      }

      drawScrollDots(bleScroll, bleDeviceCount, BLE_VISIBLE);
    }

  } while (oled.nextPage());
}

void drawBLEDetail() {
  BLEDeviceInfo* dev = &bleDevices[bleSelectedIndex];

  oled.firstPage();
  do {
    char hdr[17] = {0};
    strncpy(hdr, dev->name, 16);
    drawHeader(strlen(dev->name) ? hdr : "BLE DEVICE");

    oled.setFont(u8g2_font_5x7_tf);

    oled.setCursor(0, 22);
    oled.printf("RSSI:%d dBm", dev->rssi);

    oled.setCursor(0, 30);
    float dist = estimateDistance(dev->rssi);
    oled.printf("Distance:~%.1fm", dist);

    oled.setCursor(0, 38);
    const char* typeStr = "Unknown";
    if (dev->advType == 1) typeStr = "iBeacon";
    else if (dev->advType == 2) typeStr = "Eddystone";
    oled.printf("Type:%s", typeStr);

    oled.setCursor(0, 46);
    oled.printf("Status:%s", dev->isActive ? "Active" : "Lost");

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 52);
    oled.printf("MAC:%s", dev->address);

    drawFooter("WALK", "LIST");
  } while (oled.nextPage());
}

void drawBLEWalkTest() {
  if (walkTest.viewMode == 0) {
    oled.firstPage();
    do {
      drawHeader("BLE WALK TEST");

      if (walkTest.active && strlen(walkTest.targetBLEAddr) > 0) {
        oled.setFont(u8g2_font_5x7_tf);
        char addr[17];
        strncpy(addr, walkTest.targetBLEAddr, 16);
        addr[16] = '\0';
        oled.setCursor(0, 22);
        oled.print(addr);

        int8_t currentRSSI = walkTest.rssiHistory[(walkTest.historyIndex - 1 + WALK_HISTORY_SIZE) % WALK_HISTORY_SIZE];
        if (currentRSSI == 0 && walkTest.sampleCount > 0) {
          for (int i = 1; i < WALK_HISTORY_SIZE; i++) {
            int idx = (walkTest.historyIndex - i + WALK_HISTORY_SIZE) % WALK_HISTORY_SIZE;
            if (walkTest.rssiHistory[idx] != 0) {
              currentRSSI = walkTest.rssiHistory[idx];
              break;
            }
          }
        }

        oled.setCursor(0, 32);
        oled.printf("Now: %ddBm", currentRSSI);

        int8_t avgRSSI = walkTest.sampleCount > 0 ? (walkTest.rssiSum / walkTest.sampleCount) : currentRSSI;
        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(0, 40);
        oled.printf("Min:%d Max:%d Avg:%d", walkTest.minRSSI, walkTest.maxRSSI, avgRSSI);

        oled.drawRFrame(0, 45, 128, 16, 2);
        {
          const uint8_t mgY = 46, mgH = 13, mgBottom = mgY + mgH;
          int8_t prevX = -1, prevYpos = -1;
          uint8_t lastX = 0, lastYpos = mgBottom;
          bool hasLast = false;

          for (int i = 0; i < WALK_HISTORY_SIZE && i < 126; i++) {
            int idx = (walkTest.historyIndex + i) % WALK_HISTORY_SIZE;
            if (walkTest.rssiHistory[idx] == 0) { prevX = -1; continue; }
            int h = map(constrain(walkTest.rssiHistory[idx], -90, -30), -90, -30, 1, mgH);
            uint8_t x = 1 + (i * 126 / WALK_HISTORY_SIZE);
            uint8_t yp = mgBottom - h;

            for (uint8_t fy = yp + 1; fy < mgBottom; fy++) {
              if ((x + fy) % 3 == 0) oled.drawPixel(x, fy);
            }

            if (prevX >= 0) oled.drawLine(prevX, prevYpos, x, yp);
            prevX = x; prevYpos = yp;
            lastX = x; lastYpos = yp; hasLast = true;
          }

          if (hasLast && lastX > 2 && lastX < 126 && lastYpos > mgY + 1 && lastYpos < mgBottom - 1) {
            oled.drawDisc(lastX, lastYpos, 1);
          }
        }

        drawFooter("GRAPH", nullptr);
      } else {
        oled.setFont(u8g2_font_5x7_tf);
        oled.drawStr(5, 35, "No BLE device selected");
      }
    } while (oled.nextPage());
  } else {
    char addr[17];
    strncpy(addr, walkTest.targetBLEAddr, 16);
    addr[16] = '\0';
    char subtitle[40];
    int8_t avgRSSI = walkTest.sampleCount > 0 ? (walkTest.rssiSum / walkTest.sampleCount) : 0;
    sprintf(subtitle, "Avg:%d Min:%d Max:%d", avgRSSI, walkTest.minRSSI, walkTest.maxRSSI);
    
    drawRSSIGraph(walkTest.rssiHistory, walkTest.historyIndex, -90, -30, addr, subtitle);
  }
}


void drawDeauthWatch() {
  oled.firstPage();
  do {
    drawHeader("DEAUTH WATCH");

    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(0, 24);
    oled.printf("Channel: %d", scanState.currentChannel);

    oled.setCursor(0, 34);
    oled.printf("Rate: %lu/sec", deauthPerSecond);

    oled.setCursor(0, 44);
    oled.printf("Total: %lu", totalDeauthDetected);

    oled.setCursor(0, 52);
    if (attackActive) {
      oled.setFont(u8g2_font_6x10_tf);
      oled.print("!! ATTACK !!");
    } else {
      oled.print("Status: Normal");
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawRogueAPWatch() {
  oled.firstPage();
  do {
    drawHeader("ROGUE AP WATCH");

    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(0, 24);
    oled.printf("Scanning: %d APs", apCount);

    oled.setCursor(0, 34);
    if (rogueCount > 0) {
      oled.printf("!! %d Rogue(s) !!", rogueCount);
    } else {
      oled.print("Status: Clean");
    }

    if (rogueCount > 0) {
      oled.setFont(u8g2_font_4x6_tf);
      oled.setCursor(0, 42);
      oled.print("Evil Twin Detected:");
      for (int i = 0; i < min((int)rogueCount, 2); i++) {
        oled.setCursor(5, 50 + i * 8);
        char ssid[17];
        strncpy(ssid, rogueList[i].ssid, 16);
        ssid[16] = '\0';
        oled.printf("%s", ssid);
      }
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawBLETrackerWatch() {
  int trackerCount = 0;
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].advType > 0 || !bleDevices[i].hasName) {
      trackerCount++;
    }
  }

  oled.firstPage();
  do {
    drawHeader("BLE TRACKER");

    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(0, 24);
    oled.printf("Total BLE: %d", bleDeviceCount);

    oled.setCursor(0, 34);
    if (trackerCount > 0) {
      oled.printf("Trackers: %d", trackerCount);
    } else {
      oled.print("No trackers found");
    }

    if (trackerCount > 0) {
      oled.setFont(u8g2_font_4x6_tf);
      oled.setCursor(0, 42);
      oled.print("Detected:");

      int shown = 0;
      for (int i = 0; i < bleDeviceCount && shown < 2; i++) {
        if (bleDevices[i].advType > 0 || !bleDevices[i].hasName) {
          oled.setCursor(5, 50 + shown * 8);
          char addr[13];
          strncpy(addr, bleDevices[i].address, 12);
          addr[12] = '\0';
          oled.printf("%s %ddBm", addr, bleDevices[i].rssi);
          shown++;
        }
      }
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawAlertSettings() {
  oled.firstPage();
  do {
    drawHeader("ALERT SETTINGS");

    oled.setFont(u8g2_font_5x7_tf);

    const char* labels[2];
    char buf0[22], buf1[22];
    sprintf(buf0, "Deauth: %d/sec", settings.deauthThreshold);
    if (settings.screenTimeout == 0) {
      strcpy(buf1, "Timeout: Never");
    } else {
      sprintf(buf1, "Timeout: %ds", settings.screenTimeout);
    }
    labels[0] = buf0;
    labels[1] = buf1;

    for (int i = 0; i < 2; i++) {
      uint8_t y = 24 + i * 12;
      if (alertSettingIndex == i) {
        oled.drawRBox(2, y - 8, 118, 11, 2);
        oled.setDrawColor(0);
        oled.drawStr(8, y, labels[i]);
        oled.setDrawColor(1);
      } else {
        oled.drawStr(8, y, labels[i]);
      }
    }

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 50);
    oled.print("SHORT=Next LONG=Adjust");

    drawFooter(nullptr, "SAVE");
  } while (oled.nextPage());
}

void drawWhyIsItSlow() {
  if (whySlowView == 0) {
    int channelLoad[13] = {0};
    int maxLoad = 0;
    int avgRSSITotal = 0;
    int weakSignalCount = 0;

    for (int i = 0; i < apCount; i++) {
      if (apList[i].primary >= 1 && apList[i].primary <= 13) {
        channelLoad[apList[i].primary - 1]++;
      }
      avgRSSITotal += apList[i].rssi;
      if (apList[i].rssi < -75) weakSignalCount++;
    }

    for (int i = 0; i < 13; i++) {
      if (channelLoad[i] > maxLoad) maxLoad = channelLoad[i];
    }

    int avgRSSI = apCount > 0 ? avgRSSITotal / apCount : -100;

    oled.firstPage();
    do {
      drawHeader("WHY IS IT SLOW?");

      oled.setFont(u8g2_font_4x6_tf);

      oled.setCursor(0, 22);
      if (maxLoad > 10) {
        oled.print("! Congested channel");
      } else if (maxLoad > 5) {
        oled.print("  Moderate congestion");
      } else {
        oled.print("  Channel load OK");
      }

      oled.setCursor(0, 30);
      if (avgRSSI < -80) {
        oled.print("! Weak signals");
      } else if (avgRSSI < -70) {
        oled.print("  Fair signal strength");
      } else {
        oled.print("  Signal strength OK");
      }

      oled.setCursor(0, 38);
      if (deauthPerSecond > 5) {
        oled.print("! High interference");
      } else {
        oled.print("  Interference OK");
      }

      oled.setCursor(0, 46);
      if (apCount > 30) {
        oled.print("! Too many APs nearby");
      } else {
        oled.print("  AP count OK");
      }

      oled.setFont(u8g2_font_5x7_tf);
      oled.setCursor(0, 53);
      int issues = (maxLoad > 10 ? 1 : 0) + (avgRSSI < -80 ? 1 : 0) +
                   (deauthPerSecond > 5 ? 1 : 0) + (apCount > 30 ? 1 : 0);
      oled.printf("%d issue(s) found", issues);

      drawFooter("GRAPH", "BACK");
    } while (oled.nextPage());
  } else if (whySlowApIdx == 0) {
    oled.firstPage();
    do {
      drawHeader("RSSI ALL APs");

      uint8_t activeCount = 0;
      uint8_t activeIdx[MAX_TRACKED_APS];
      for (int i = 0; i < MAX_TRACKED_APS; i++) {
        if (rssiHistory[i].active) activeIdx[activeCount++] = i;
      }

      if (activeCount == 0) {
        oled.setFont(u8g2_font_5x7_tf);
        oled.drawStr(10, 35, "Scanning APs...");
      } else {
        const uint8_t areaY = 13, areaH = 39;
        uint8_t rowH = areaH / activeCount;
        if (rowH > 16) rowH = 16; // Cap row height

        for (uint8_t a = 0; a < activeCount; a++) {
          uint8_t apIdx = activeIdx[a];
          uint8_t ry = areaY + a * rowH;
          uint8_t graphH = rowH - 2;

          oled.setFont(u8g2_font_4x6_tf);
          char label[8] = "?";
          for (int j = 0; j < apCount; j++) {
            if (memcmp(rssiHistory[apIdx].bssid, apList[j].bssid, 6) == 0) {
              strncpy(label, (char*)apList[j].ssid, 6);
              label[6] = '\0';
              break;
            }
          }

          int8_t lastRSSI = rssiHistory[apIdx].rssiSamples[
            (rssiHistory[apIdx].sampleIndex - 1 + RSSI_HISTORY_SIZE) % RSSI_HISTORY_SIZE];

          oled.drawStr(0, ry + graphH / 2 + 2, label);
          char rssiStr[6];
          sprintf(rssiStr, "%d", lastRSSI);
          oled.drawStr(108, ry + graphH / 2 + 2, rssiStr);

          const uint8_t gx = 28, gw = 78;
          oled.drawFrame(gx, ry, gw, graphH);

          for (int i = 1; i < RSSI_HISTORY_SIZE; i++) {
            int8_t r1 = rssiHistory[apIdx].rssiSamples[i - 1];
            int8_t r2 = rssiHistory[apIdx].rssiSamples[i];
            if (r1 < -100 || r2 < -100) continue;

            uint8_t y1 = ry + graphH - 1 - ((r1 + 100) * (graphH - 2) / 60);
            uint8_t y2 = ry + graphH - 1 - ((r2 + 100) * (graphH - 2) / 60);
            uint8_t x1 = gx + 1 + (uint16_t)(i - 1) * (gw - 2) / RSSI_HISTORY_SIZE;
            uint8_t x2 = gx + 1 + (uint16_t)i * (gw - 2) / RSSI_HISTORY_SIZE;

            oled.drawLine(x1, y1, x2, y2);
          }
        }
      }

      drawFooter("NEXT", "BACK");
    } while (oled.nextPage());
  } else {
    uint8_t targetApIdx = 255;
    uint8_t activeCount = 0;
    uint8_t totalActive = 0;
    for (int i = 0; i < MAX_TRACKED_APS; i++) {
      if (rssiHistory[i].active) {
        totalActive++;
        if (totalActive == whySlowApIdx) targetApIdx = i;
      }
    }

    if (targetApIdx == 255) {
      whySlowApIdx = 0;
      drawWhyIsItSlow();
      return;
    }

    char hdr[22] = "AP ";
    char numStr[4];
    sprintf(numStr, "%d/", whySlowApIdx);
    strcat(hdr, numStr);
    sprintf(numStr, "%d ", totalActive);
    strcat(hdr, numStr);
    for (int j = 0; j < apCount; j++) {
      if (memcmp(rssiHistory[targetApIdx].bssid, apList[j].bssid, 6) == 0) {
        char ssid[12];
        strncpy(ssid, (char*)apList[j].ssid, 10);
        ssid[10] = '\0';
        strcat(hdr, ssid);
        break;
      }
    }

    int8_t curRSSI = rssiHistory[targetApIdx].rssiSamples[
      (rssiHistory[targetApIdx].sampleIndex - 1 + RSSI_HISTORY_SIZE) % RSSI_HISTORY_SIZE];
    int8_t minR = 0, maxR = -127;
    for (int i = 0; i < RSSI_HISTORY_SIZE; i++) {
      int8_t s = rssiHistory[targetApIdx].rssiSamples[i];
      if (s >= -100) {
        if (s < minR || minR == 0) minR = s;
        if (s > maxR) maxR = s;
      }
    }
    char subtitle[30];
    sprintf(subtitle, "Now:%d Min:%d Max:%d", curRSSI, minR, maxR);

    oled.firstPage();
    do {
      drawHeader(hdr);

      const uint8_t graphX = 14, graphY = 14, graphW = 112, graphH = 36;
      const uint8_t graphBottom = graphY + graphH - 1;

      oled.drawRFrame(graphX, graphY, graphW, graphH, 2);

      oled.setFont(u8g2_font_4x6_tf);
      oled.drawStr(0, graphY + 5, "-40");
      oled.drawStr(0, graphY + graphH / 2 + 2, "-70");
      oled.drawStr(0, graphY + graphH - 2, "-100");

      for (int i = 1; i <= 3; i++) {
        uint8_t y = graphY + (graphH * i / 4);
        for (uint8_t x = graphX + 2; x < graphX + graphW - 2; x++) {
          if (x % 3 != 0) oled.drawPixel(x, y);
        }
      }

      int8_t* samples = rssiHistory[targetApIdx].rssiSamples;
      uint8_t lastValidI = 0;

      for (int i = 0; i < RSSI_HISTORY_SIZE; i++) {
        if (samples[i] < -100) continue;
        uint8_t y = graphY + graphH - 1 - ((samples[i] + 100) * (graphH - 2) / 60);
        uint8_t x = graphX + 1 + (uint16_t)i * (graphW - 2) / RSSI_HISTORY_SIZE;
        if (x <= graphX || x >= graphX + graphW - 1) continue;
        for (uint8_t fy = y + 1; fy < graphBottom; fy++) {
          if ((x + fy) % 3 == 0) oled.drawPixel(x, fy);
        }
        lastValidI = i;
      }

      for (int i = 1; i < RSSI_HISTORY_SIZE; i++) {
        int8_t r1 = samples[i - 1], r2 = samples[i];
        if (r1 < -100 || r2 < -100) continue;

        uint8_t y1 = graphY + graphH - 1 - ((r1 + 100) * (graphH - 2) / 60);
        uint8_t y2 = graphY + graphH - 1 - ((r2 + 100) * (graphH - 2) / 60);
        uint8_t x1 = graphX + 1 + (uint16_t)(i - 1) * (graphW - 2) / RSSI_HISTORY_SIZE;
        uint8_t x2 = graphX + 1 + (uint16_t)i * (graphW - 2) / RSSI_HISTORY_SIZE;
        oled.drawLine(x1, y1, x2, y2);
        lastValidI = i;
      }

      if (samples[lastValidI] >= -100) {
        uint8_t dotX = graphX + 1 + (uint16_t)lastValidI * (graphW - 2) / RSSI_HISTORY_SIZE;
        uint8_t dotY = graphY + graphH - 1 - ((samples[lastValidI] + 100) * (graphH - 2) / 60);
        if (dotX > graphX + 2 && dotX < graphX + graphW - 3 &&
            dotY > graphY + 2 && dotY < graphBottom - 1) {
          oled.drawDisc(dotX, dotY, 2);
        }
      }

      oled.setFont(u8g2_font_4x6_tf);
      oled.setCursor(0, 55);
      oled.print(subtitle);
      oled.drawStr(72, 64, "SHORT=Next");
    } while (oled.nextPage());
  }
}

void drawChannelRecommendation() {
  int channelLoad[13] = {0};
  int channelScore[13];

  for (int i = 0; i < apCount; i++) {
    if (apList[i].primary >= 1 && apList[i].primary <= 13) {
      channelLoad[apList[i].primary - 1]++;
    }
  }

  for (int i = 0; i < 13; i++) {
    channelScore[i] = channelLoad[i] * 10;
  }

  int bestCh[3] = {1, 6, 11};
  int bestScores[3] = {999, 999, 999};

  for (int ch = 0; ch < 13; ch++) {
    for (int rank = 0; rank < 3; rank++) {
      if (channelScore[ch] < bestScores[rank]) {
        for (int j = 2; j > rank; j--) {
          bestCh[j] = bestCh[j - 1];
          bestScores[j] = bestScores[j - 1];
        }
        bestCh[rank] = ch + 1;
        bestScores[rank] = channelScore[ch];
        break;
      }
    }
  }

  oled.firstPage();
  do {
    drawHeader("BEST CHANNELS");

    oled.setFont(u8g2_font_5x7_tf);

    for (int i = 0; i < 3; i++) {
      int y = 26 + (i * 12);
      int ch = bestCh[i];
      int aps = channelLoad[ch - 1];

      oled.setCursor(0, y);
      oled.printf("%d. CH %d (%d APs)", i + 1, ch, aps);
    }

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 52);
    oled.printf("Total APs scanned: %d", apCount);

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawEnvironmentChange() {
  oled.firstPage();
  do {
    drawHeader("ENV CHANGE");

    oled.setFont(u8g2_font_5x7_tf);

    if (!baseline.saved) {
      oled.setCursor(0, 28);
      oled.print("No baseline saved");
      oled.setCursor(0, 38);
      oled.print("Hold ACTION to save");
    } else {
      int apDiff = currentSnapshot.totalAPs - baseline.totalAPs;
      int rssiDiff = currentSnapshot.avgRSSI - baseline.avgRSSI;

      oled.setCursor(0, 24);
      oled.printf("APs: %d (%+d)", currentSnapshot.totalAPs, apDiff);

      oled.setCursor(0, 33);
      oled.printf("RSSI: %ddB (%+d)", currentSnapshot.avgRSSI, rssiDiff);

      int maxChChange = 0;
      int maxCh = 0;
      for (int i = 0; i < 13; i++) {
        int diff = abs((int)currentSnapshot.channelDist[i] - (int)baseline.channelDist[i]);
        if (diff > maxChChange) {
          maxChChange = diff;
          maxCh = i + 1;
        }
      }

      oled.setCursor(0, 42);
      if (maxChChange > 0) {
        oled.printf("CH%d: %+d APs", maxCh,
          (int)currentSnapshot.channelDist[maxCh-1] - (int)baseline.channelDist[maxCh-1]);
      } else {
        oled.print("Channels stable");
      }

      uint32_t elapsed = (millis() - baseline.timestamp) / 1000;
      oled.setCursor(0, 50);
      oled.printf("Age: %lus ago", elapsed);
    }

    if (baseline.saved) {
      drawFooter("SAVE", "BACK");
    } else {
      drawFooter(nullptr, "BACK");
    }
  } while (oled.nextPage());
}

void drawQuickSnapshot() {
  oled.firstPage();
  do {
    drawHeader("QUICK SNAPSHOT");

    oled.setFont(u8g2_font_5x7_tf);

    oled.setCursor(0, 24);
    oled.printf("WiFi APs: %d", apCount);

    oled.setCursor(0, 33);
    uint8_t activeBLE = getActiveBLECount();
    oled.printf("BLE Devices: %d", activeBLE);

    int avgRSSI = 0;
    if (apCount > 0) {
      long rssiSum = 0;
      for (int i = 0; i < apCount; i++) {
        rssiSum += apList[i].rssi;
      }
      avgRSSI = rssiSum / apCount;
    }
    oled.setCursor(0, 42);
    oled.printf("Avg RSSI: %d dBm", avgRSSI);

    int channelLoad[13] = {0};
    int maxLoad = 0;
    int busiestCh = 1;
    for (int i = 0; i < apCount; i++) {
      if (apList[i].primary >= 1 && apList[i].primary <= 13) {
        channelLoad[apList[i].primary - 1]++;
        if (channelLoad[apList[i].primary - 1] > maxLoad) {
          maxLoad = channelLoad[apList[i].primary - 1];
          busiestCh = apList[i].primary;
        }
      }
    }
    oled.setCursor(0, 51);
    oled.printf("Busiest: CH%d (%d)", busiestCh, maxLoad);

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawChannelScorecard(uint8_t page) {
  int chLoad[13] = {0};
  int maxLoad = 1;
  for (int i = 0; i < apCount; i++) {
    if (apList[i].primary >= 1 && apList[i].primary <= 13) {
      chLoad[apList[i].primary - 1]++;
    }
  }
  for (int i = 0; i < 13; i++) {
    if (chLoad[i] > maxLoad) maxLoad = chLoad[i];
  }

  uint8_t best = bestChannel();

  uint8_t startCh = page * 4 + 1;
  uint8_t endCh = startCh + 3;
  if (endCh > 13) endCh = 13;
  uint8_t count = endCh - startCh + 1;

  oled.firstPage();
  do {
    char hdr[20];
    sprintf(hdr, "SCORE CH%d-%d  %d/4", startCh, endCh, page + 1);
    drawHeader(hdr);

    oled.setFont(u8g2_font_5x7_tf);
    const uint8_t rowH = 10;
    const uint8_t barX = 24, barMaxW = 60;

    for (uint8_t i = 0; i < count; i++) {
      uint8_t ch = startCh + i;
      int load = chLoad[ch - 1];
      uint8_t y = 15 + i * rowH;
      uint8_t barW = (uint8_t)((uint16_t)load * barMaxW / maxLoad);

      char label[6];
      sprintf(label, "CH%d", ch);
      oled.drawStr(0, y + 8, label);

      oled.drawFrame(barX, y + 1, barMaxW + 2, 7);

      if (barW > 0) {
        if (ch == best) {
          oled.drawFrame(barX + 1, y + 2, barW, 5);
        } else if (load >= maxLoad * 3 / 4) {
          oled.drawBox(barX + 1, y + 2, barW, 5);
        } else if (load >= maxLoad / 2) {
          for (uint8_t py = y + 2; py < y + 7; py++) {
            for (uint8_t px = barX + 1; px < barX + 1 + barW; px++) {
              if ((px + py) % 2 == 0) oled.drawPixel(px, py);
            }
          }
        } else {
          for (uint8_t py = y + 2; py < y + 7; py++) {
            for (uint8_t px = barX + 1; px < barX + 1 + barW; px++) {
              if ((px + py) % 3 == 0) oled.drawPixel(px, py);
            }
          }
        }
      }

      oled.setFont(u8g2_font_4x6_tf);
      uint8_t infoX = barX + barMaxW + 4;
      char info[12];
      if (ch == best) {
        sprintf(info, "%dAP best", load);
      } else if (ch == 1 || ch == 6 || ch == 11) {
        sprintf(info, "%dAP nol", load);
      } else {
        sprintf(info, "%dAP", load);
      }
      oled.drawStr(infoX, y + 7, info);
      oled.setFont(u8g2_font_5x7_tf);
    }

    drawFooter("NEXT", "BACK");
  } while (oled.nextPage());
}

void drawEventLog() {
  oled.firstPage();
  do {
    drawHeader("EVENT LOG");

    if (eventCount == 0) {
      oled.setFont(u8g2_font_5x7_tf);
      oled.setCursor(15, 35);
      oled.print("No events logged");
    } else {
      oled.setFont(u8g2_font_4x6_tf);

      for (int i = 0; i < min(5, (int)eventCount); i++) {
        int idx = eventScroll + i;
        if (idx >= eventCount) break;

        LogEvent* ev = &eventLog[idx];
        uint8_t y = 20 + (i * 7);

        if (i == eventCursor) {
          oled.drawRBox(0, y - 6, 120, 7, 2);
          oled.setDrawColor(0);
        }

        const char* icon = "?";
        if (ev->type == 0) icon = "D";
        else if (ev->type == 1) icon = "R";
        else if (ev->type == 2) icon = "T";

        oled.setCursor(0, y);
        oled.printf("%s:", icon);

        char msg[23];
        strncpy(msg, ev->message, 22);
        msg[22] = '\0';
        oled.setCursor(10, y);
        oled.print(msg);

        if (i == eventCursor) oled.setDrawColor(1);
      }

      drawScrollDots(eventScroll, eventCount, 5);
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawBaselineCompare() {
  oled.firstPage();
  do {
    drawHeader("BASELINE VS NOW");

    oled.setFont(u8g2_font_4x6_tf);

    if (!baseline.saved) {
      oled.setCursor(0, 28);
      oled.print("No baseline saved");
      oled.setCursor(0, 38);
      oled.print("Use Env Change to save");
    } else {
      oled.setCursor(0, 22);
      oled.print("Metric");
      oled.setCursor(60, 22);
      oled.print("Base");
      oled.setCursor(95, 22);
      oled.print("Now");

      oled.setCursor(0, 30);
      oled.print("APs:");
      oled.setCursor(60, 30);
      oled.printf("%d", baseline.totalAPs);
      oled.setCursor(95, 30);
      oled.printf("%d", currentSnapshot.totalAPs);

      oled.setCursor(0, 38);
      oled.print("RSSI:");
      oled.setCursor(60, 38);
      oled.printf("%d", baseline.avgRSSI);
      oled.setCursor(95, 38);
      oled.printf("%d", currentSnapshot.avgRSSI);

      oled.setCursor(0, 46);
      oled.print("Pkts:");
      oled.setCursor(60, 46);
      oled.printf("%lu", baseline.totalPackets);
      oled.setCursor(95, 46);
      oled.printf("%lu", currentSnapshot.totalPackets);

      uint32_t elapsed = (millis() - baseline.timestamp) / 1000;
      oled.setCursor(0, 52);
      oled.printf("Baseline: %lus ago", elapsed);
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawBatteryPower() {
  oled.firstPage();
  do {
    drawHeader("SYSTEM INFO");

    oled.setFont(u8g2_font_5x7_tf);

    uint32_t uptime = millis() / 1000;
    uint32_t hours = uptime / 3600;
    uint32_t minutes = (uptime % 3600) / 60;
    uint32_t seconds = uptime % 60;

    oled.setCursor(0, 24);
    oled.print("Uptime:");
    oled.setCursor(0, 34);
    oled.printf("  %02luh %02lum %02lus", hours, minutes, seconds);

    oled.setCursor(0, 44);
    oled.printf("Free RAM: %d KB", ESP.getFreeHeap() / 1024);

    oled.setCursor(0, 52);
    oled.printf("Flash: %d MB", ESP.getFlashChipSize() / (1024 * 1024));

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawDisplaySettings() {
  oled.firstPage();
  do {
    drawHeader("DISPLAY");

    oled.setFont(u8g2_font_5x7_tf);

    const char* labels[2];
    char buf0[22], buf1[22];
    sprintf(buf0, "RGB: %d%%", settings.rgbBrightness);
    if (settings.screenTimeout == 0) {
      strcpy(buf1, "Sleep: Never");
    } else {
      sprintf(buf1, "Sleep: %ds", settings.screenTimeout);
    }
    labels[0] = buf0;
    labels[1] = buf1;

    for (int i = 0; i < 2; i++) {
      uint8_t y = 24 + i * 12;
      if (displaySettingCursor == i) {
        oled.drawRBox(2, y - 8, 118, 11, 2);
        oled.setDrawColor(0);
        oled.drawStr(8, y, labels[i]);
        oled.setDrawColor(1);
      } else {
        oled.drawStr(8, y, labels[i]);
      }
    }

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 50);
    oled.print("SHORT=Next LONG=Change");

    drawFooter(nullptr, "SAVE");
  } while (oled.nextPage());
}

void drawRadioControl() {
  oled.firstPage();
  do {
    drawHeader("RADIO CONTROL");

    oled.setFont(u8g2_font_5x7_tf);

    oled.setCursor(0, 24);
    oled.printf("Monitor Channel: %d", scanState.currentChannel);

    oled.setFont(u8g2_font_4x6_tf);
    oled.drawRFrame(0, 28, 128, 12, 2);
    int chPos = ((scanState.currentChannel - 1) * 116) / 12 + 1;
    oled.drawRBox(chPos, 29, 8, 10, 2);

    oled.setCursor(0, 48);
    oled.print("1        6        11  13");

    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(0, 52);
    oled.print("SHORT=Next LONG=Prev");

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawAbout() {
  oled.firstPage();
  do {
    drawHeader("ABOUT");

    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(5, 24, "Pocket RF Tool");
    oled.drawStr(5, 34, "ESP32-C3/S3");
    oled.setCursor(5, 44);
    oled.printf("Version: %s", FW_VERSION);
    oled.drawStr(5, 52, "Wi-Fi + BLE");

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawHiddenSSID() {
  oled.firstPage();
  do {
    char hdr[20];
    sprintf(hdr, "Hidden:%d", hiddenCount);
    drawHeader(hdr);

    if (hiddenCount == 0) {
      oled.setFont(u8g2_font_6x10_tf);
      oled.drawStr(10, 35, "Scanning...");
    } else {
      for (int row = 0; row < AP_VISIBLE; row++) {
        int idx = hiddenScroll + row;
        if (idx >= hiddenCount) break;

        uint8_t y = 22 + row * 14;

        if (row == hiddenCursor) {
          oled.drawRBox(0, y - 7, 120, 14, 2);
          oled.setDrawColor(0);
        }

        oled.setFont(u8g2_font_5x7_tf);

        char ssidBuf[13] = {0};
        strncpy(ssidBuf, hiddenList[idx].ssid, 12);
        oled.drawStr(4, y, ssidBuf);

        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(80, y);
        oled.printf("CH%d", hiddenList[idx].channel);

        oled.setCursor(108, y);
        oled.printf("%d", hiddenList[idx].rssi);

        if (row == hiddenCursor) oled.setDrawColor(1);
      }

      drawScrollDots(hiddenScroll, hiddenCount, AP_VISIBLE);
    }
  } while (oled.nextPage());
}

void drawStats() {
  uint32_t runtime = (millis() - sessionStart) / 1000;
  uint32_t hours = runtime / 3600;
  uint32_t mins = (runtime % 3600) / 60;
  uint32_t secs = runtime % 60;

  oled.firstPage();
  do {
    drawHeader("SESSION STATS");

    oled.setFont(u8g2_font_6x10_tf);
    oled.setCursor(0, 24);
    oled.printf("Time: %02d:%02d:%02d", hours, mins, secs);

    oled.setCursor(0, 34);
    oled.printf("Packets: %lu", totalPackets);

    oled.setCursor(0, 44);
    oled.printf("APs: %lu  Peak:%lu", totalAPsFound, peakPPS);

    oled.setCursor(0, 52);
    oled.printf("Deauths: %lu", totalDeauthDetected);

    drawFooter("RESET", "BACK");
  } while (oled.nextPage());
}

void drawRSSIMeter() {
  oled.firstPage();
  do {
    drawHeader("RSSI METER");

    if (apCount > 0) {
      oled.setFont(u8g2_font_5x7_tf);
      wifi_ap_record_t* ap = &apList[0];

      char ssid[17];
      strncpy(ssid, (char*)ap->ssid, 16);
      ssid[16] = '\0';
      char buf[20];
      sprintf(buf, "%s", strlen((char*)ap->ssid) ? ssid : "<hidden>");
      oled.drawStr(0, 24, buf);

      sprintf(buf, "RSSI: %d dBm", ap->rssi);
      oled.drawStr(0, 34, buf);

      sprintf(buf, "CH: %d", ap->primary);
      oled.drawStr(0, 44, buf);

      int barWidth = map(constrain(ap->rssi, -90, -30), -90, -30, 0, 120);
      oled.drawRFrame(0, 47, 122, 8, 2);
      if (barWidth > 0) oled.drawRBox(1, 48, barWidth, 6, 1);

    } else {
      oled.setFont(u8g2_font_5x7_tf);
      oled.drawStr(10, 35, "Scanning for APs...");
    }

    drawFooter(nullptr, "BACK");

  } while (oled.nextPage());
}

void drawExport() {
  oled.firstPage();
  do {
    drawHeader("EXPORT DATA");

    oled.setFont(u8g2_font_5x7_tf);

    oled.setCursor(0, 26);
    oled.print("SHORT = Export to Serial");

    oled.setCursor(0, 38);
    oled.printf("APs: %d  BLE: %d", apCount, getActiveBLECount());

    oled.setCursor(0, 48);
    oled.printf("Events: %d", eventCount);

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawPowerMode() {
  oled.firstPage();
  do {
    drawHeader("POWER MODE");

    oled.setFont(u8g2_font_5x7_tf);

    const char* modes[] = {"Normal", "Eco", "Ultra"};
    const char* desc[] = {
      "Full performance",
      "Reduced scan rate",
      "Maximum battery"
    };

    for (int i = 0; i < 3; i++) {
      uint8_t y = 24 + i * 12;
      if (settings.powerMode == i) {
        oled.drawRBox(2, y - 8, 118, 11, 2);
        oled.setDrawColor(0);
        oled.drawStr(8, y, modes[i]);
        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(50, y);
        oled.printf("%s", desc[i]);
        oled.setFont(u8g2_font_5x7_tf);
        oled.setDrawColor(1);
      } else {
        oled.drawStr(8, y, modes[i]);
        oled.setFont(u8g2_font_4x6_tf);
        oled.setCursor(50, y);
        oled.printf("%s", desc[i]);
        oled.setFont(u8g2_font_5x7_tf);
      }
    }

    drawFooter("CHANGE", "SAVE");
  } while (oled.nextPage());
}

void drawPlaceholder(const char* title, const char* subtitle) {
  oled.firstPage();
  do {
    drawHeader(title);

    if (subtitle) {
      oled.setFont(u8g2_font_5x7_tf);
      uint8_t subWidth = strlen(subtitle) * 5;
      uint8_t subX = (128 - subWidth) / 2;
      oled.drawStr(subX, 36, subtitle);
    }

    drawFooter(nullptr, "BACK");
  } while (oled.nextPage());
}

void drawWebServer() {
  oled.firstPage();
  do {
    drawHeader("WEB SERVER");

    oled.setFont(u8g2_font_5x7_tf);
    oled.setCursor(0, 24);
    oled.print("SSID: ESP32-Tool");
    oled.setCursor(0, 33);
    oled.print("IP: 192.168.4.1");
    oled.setCursor(0, 42);
    oled.print("URL: esp32.util");
    oled.setCursor(0, 51);
    oled.print("Pass: 12345678");

    drawFooter(nullptr, "STOP");
  } while (oled.nextPage());
}
