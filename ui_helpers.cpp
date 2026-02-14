#include "ui_helpers.h"
#include "display.h"

void handleListNavigation(ButtonEvent ev, uint8_t& cursor, uint8_t& scroll, uint16_t count, uint8_t visibleRows) {
  if (ev == BTN_SHORT && count > 0) {
    if (scroll + cursor + 1 < count) {
      if (cursor < visibleRows - 1) cursor++;
      else scroll++;
    } else {
      cursor = 0;
      scroll = 0;
    }
  }
}

const char* formatMAC(uint8_t* mac) {
  static char buf[18];
  snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buf;
}

const char* formatMAC(const char* macStr) {
  return macStr; // No formatting needed if already string
}

const char* formatElapsed(uint32_t elapsedSec) {
  static char buf[20];
  uint32_t hours = elapsedSec / 3600;
  uint32_t mins = (elapsedSec % 3600) / 60;
  uint32_t secs = elapsedSec % 60;
  if (hours > 0) snprintf(buf, 20, "%luh %lum %lus", hours, mins, secs);
  else if (mins > 0) snprintf(buf, 20, "%lum %lus", mins, secs);
  else snprintf(buf, 20, "%lus", secs);
  return buf;
}

void drawRSSIGraph(int8_t* history, uint8_t historyIndex, int8_t minRSSI, int8_t maxRSSI, const char* title, const char* subtitle) {
  oled.firstPage();
  do {
    drawHeader(title);

    const uint8_t graphX = 14;
    const uint8_t graphY = 14;
    const uint8_t graphW = 112;
    const uint8_t graphH = 36;
    const uint8_t graphBottom = graphY + graphH - 1;

    oled.drawRFrame(graphX, graphY, graphW, graphH, 2);

    oled.setFont(u8g2_font_4x6_tf);
    char buf[6];
    sprintf(buf, "%d", maxRSSI);
    oled.drawStr(0, graphY + 5, buf);
    int16_t midVal = ((int16_t)minRSSI + maxRSSI) / 2;
    sprintf(buf, "%d", midVal);
    oled.drawStr(0, graphY + graphH / 2 + 2, buf);
    sprintf(buf, "%d", minRSSI);
    oled.drawStr(0, graphY + graphH - 2, buf);

    for (int i = 1; i <= 3; i++) {
      uint8_t y = graphY + (graphH * i / 4);
      for (uint8_t x = graphX + 2; x < graphX + graphW - 2; x++) {
        if (x % 3 != 0) oled.drawPixel(x, y);
      }
    }

    uint8_t yPositions[WALK_HISTORY_SIZE];
    bool validSample[WALK_HISTORY_SIZE];
    uint8_t lastValidIdx = 0;

    for (int i = 0; i < WALK_HISTORY_SIZE; i++) {
      int idx = (historyIndex + i) % WALK_HISTORY_SIZE;
      int8_t rssi = history[idx];
      if (rssi != 0) {
        int val = constrain(rssi, minRSSI, maxRSSI);
        yPositions[i] = graphY + graphH - 1 - map(val, minRSSI, maxRSSI, 0, graphH - 2);
        validSample[i] = true;
        lastValidIdx = i;
      } else {
        yPositions[i] = graphBottom;
        validSample[i] = false;
      }
    }

    for (int i = 0; i < WALK_HISTORY_SIZE; i++) {
      if (!validSample[i]) continue;
      uint8_t x = graphX + 1 + (uint16_t)i * (graphW - 2) / WALK_HISTORY_SIZE;
      if (x <= graphX || x >= graphX + graphW - 1) continue;
      for (uint8_t y = yPositions[i] + 1; y < graphBottom; y++) {
        if ((x + y) % 3 == 0) oled.drawPixel(x, y);
      }
    }

    for (int i = 1; i < WALK_HISTORY_SIZE; i++) {
      if (!validSample[i - 1] || !validSample[i]) continue;

      uint8_t x1 = graphX + 1 + (uint16_t)(i - 1) * (graphW - 2) / WALK_HISTORY_SIZE;
      uint8_t x2 = graphX + 1 + (uint16_t)i * (graphW - 2) / WALK_HISTORY_SIZE;

      oled.drawLine(x1, yPositions[i - 1], x2, yPositions[i]);
    }

    if (validSample[lastValidIdx]) {
      uint8_t dotX = graphX + 1 + (uint16_t)lastValidIdx * (graphW - 2) / WALK_HISTORY_SIZE;
      uint8_t dotY = yPositions[lastValidIdx];
      if (dotX > graphX + 2 && dotX < graphX + graphW - 3 &&
          dotY > graphY + 2 && dotY < graphBottom - 1) {
        oled.drawDisc(dotX, dotY, 2);
      }
    }

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 55);
    oled.print(subtitle);

    drawFooter("STATS", nullptr);
  } while (oled.nextPage());
}

void drawScrollbar(uint8_t scroll, uint16_t count, uint8_t visible) {
  if (count <= visible) return;
  // Scrollbar track
  const uint8_t trackX = 126, trackY = 15, trackH = 37;
  for (uint8_t y = trackY; y < trackY + trackH; y += 2) {
    oled.drawPixel(trackX, y);
  }
  // Thumb
  uint8_t thumbH = max((uint8_t)4, (uint8_t)(trackH * visible / count));
  uint8_t thumbY = trackY + (uint16_t)scroll * (trackH - thumbH) / (count - visible);
  oled.drawBox(trackX - 1, thumbY, 3, thumbH);
}
