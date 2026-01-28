#include "power.h"
#include "display.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include <esp_sleep.h>

uint32_t lastActivity = 0;
bool screenSleeping = false;

void updateScreenTimeout(bool buttonPressed) {
  if (buttonPressed) {
    lastActivity = millis();
    if (screenSleeping) {
      screenSleeping = false;
      oled.setPowerSave(0);
      setRGB(RGB_GREEN);
    }
  }

  if (settings.screenTimeout > 0 && !screenSleeping) {
    if (millis() - lastActivity > (settings.screenTimeout * 1000UL)) {
      screenSleeping = true;
      oled.setPowerSave(1);
      setRGB(RGB_OFF);
    }
  }
}

void enterDeepSleep() {
  Serial.println("[SLEEP] Entering DEEP SLEEP - press RESET to wake");
  Serial.flush();

  oled.setPowerSave(1);
  setRGB(RGB_OFF);

  stopAllWifi();
  stopBLEScan();
  esp_wifi_stop();

  delay(100);
  esp_deep_sleep_start();
}
