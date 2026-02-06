#include "ui_helpers.h"

// Navigation
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

// Formatting
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

// Drawing
void drawRSSIGraph(int8_t* history, uint8_t historyIndex, int8_t minRSSI, int8_t maxRSSI, const char* title, const char* subtitle) {
  oled.firstPage();
  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(20, 10, title);

    const uint8_t graphX = 10;
    const uint8_t graphY = 15;
    const uint8_t graphW = 108;
    const uint8_t graphH = 35;

    oled.drawFrame(graphX, graphY, graphW, graphH);

    oled.setFont(u8g2_font_4x6_tf);
    char buf[5];
    sprintf(buf, "%d", maxRSSI);
    oled.drawStr(0, graphY + 5, buf);
    sprintf(buf, "%d", minRSSI);
    oled.drawStr(0, graphY + graphH - 2, buf);

    // Draw Grid Lines (simplified)
    for (int i = 1; i < 3; i++) {
      uint8_t y = graphY + (graphH * i / 3);
      for (uint8_t x = graphX; x < graphX + graphW; x += 4) {
        oled.drawPixel(x, y);
      }
    }

    // Draw Graph
    for (int i = 1; i < WALK_HISTORY_SIZE; i++) {
      int idx1 = (historyIndex + i - 1) % WALK_HISTORY_SIZE;
      int idx2 = (historyIndex + i) % WALK_HISTORY_SIZE;
      int8_t rssi1 = history[idx1];
      int8_t rssi2 = history[idx2];

      if (rssi1 != 0 && rssi2 != 0) {
        // Map RSSI (minRSSI to maxRSSI) to graph Y coordinates
        // constrain to ensure we stay within bounds
        int val1 = constrain(rssi1, minRSSI, maxRSSI);
        int val2 = constrain(rssi2, minRSSI, maxRSSI);
        
        uint8_t y1 = graphY + graphH - map(val1, minRSSI, maxRSSI, 0, graphH);
        uint8_t y2 = graphY + graphH - map(val2, minRSSI, maxRSSI, 0, graphH);

        uint8_t x1 = graphX + (i - 1) * graphW / WALK_HISTORY_SIZE;
        uint8_t x2 = graphX + i * graphW / WALK_HISTORY_SIZE;

        oled.drawLine(x1, y1, x2, y2);
      }
    }

    oled.setFont(u8g2_font_4x6_tf);
    oled.setCursor(0, 54);
    oled.print(subtitle);

    oled.drawStr(60, 64, "SHORT=Stats");
  } while (oled.nextPage());
}

void drawScrollbar(uint8_t scroll, uint16_t count, uint8_t visible) {
  if (scroll > 0) oled.drawStr(122, 18, "^");
  if (scroll + visible < count) oled.drawStr(122, 61, "v");
}
