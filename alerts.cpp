#include "alerts.h"
#include "display.h"

uint8_t alertLevel = 0;
uint32_t lastAlertBlink = 0;
bool alertBlinkState = false;
bool signalAlert = false;
uint8_t alertSettingIndex = 0;

void logEvent(uint8_t type, const char* msg) {
  if (eventCount >= MAX_EVENTS) {
    for (int i = 0; i < MAX_EVENTS - 1; i++) {
      eventLog[i] = eventLog[i + 1];
    }
    eventCount = MAX_EVENTS - 1;
  }

  eventLog[eventCount].timestamp = millis();
  eventLog[eventCount].type = type;
  strncpy(eventLog[eventCount].message, msg, 39);
  eventLog[eventCount].message[39] = '\0';
  eventLog[eventCount].active = true;
  eventCount++;
}

void updateAlertLED() {
  if (!RGB_ENABLED) return;

  uint32_t blinkInterval = (alertLevel == 2) ? 200 : 500;

  if (millis() - lastAlertBlink > blinkInterval) {
    lastAlertBlink = millis();
    alertBlinkState = !alertBlinkState;

    if (alertLevel == 0) {
      setRGB(rgb.Color(0, 10, 0));
    } else if (alertLevel == 1) {
      if (alertBlinkState) {
        setRGB(RGB_ORANGE);
      } else {
        setRGB(rgb.Color(10, 3, 0));
      }
    } else if (alertLevel == 2) {
      if (alertBlinkState) {
        setRGB(RGB_RED);
      } else {
        setRGB(rgb.Color(10, 0, 0));
      }
    }
  }
}
