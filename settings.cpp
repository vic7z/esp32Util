#include "settings.h"

uint8_t settingsCursor = 0;
const uint8_t SETTINGS_COUNT = 5;

void loadSettings() {
  prefs.begin("wifianalyzer", false);
  settings.scanSpeed = prefs.getUChar("scanSpeed", 1);
  settings.rssiThreshold = prefs.getChar("rssiThresh", -70);
  settings.rgbBrightness = prefs.getUChar("rgbBright", 50);
  settings.autoRefresh = prefs.getBool("autoRefresh", true);
  settings.csvLogging = prefs.getBool("csvLog", false);
  settings.deauthThreshold = prefs.getUChar("deauthThresh", 10);
  settings.screenTimeout = prefs.getUShort("screenTimeout", 60);
  settings.powerMode = prefs.getUChar("powerMode", 0);
  
  // Load AP settings with defaults
  String ssid = prefs.getString("apSSID", "ESP32-Tool");
  String pass = prefs.getString("apPass", "");
  strncpy(settings.apSSID, ssid.c_str(), 32);
  strncpy(settings.apPassword, pass.c_str(), 64);
  settings.apSSID[32] = '\0';
  settings.apPassword[64] = '\0';
}

void saveSettings() {
  prefs.putUChar("scanSpeed", settings.scanSpeed);
  prefs.putChar("rssiThresh", settings.rssiThreshold);
  prefs.putUChar("rgbBright", settings.rgbBrightness);
  prefs.putBool("autoRefresh", settings.autoRefresh);
  prefs.putBool("csvLog", settings.csvLogging);
  prefs.putUChar("deauthThresh", settings.deauthThreshold);
  prefs.putUShort("screenTimeout", settings.screenTimeout);
  prefs.putUChar("powerMode", settings.powerMode);
  prefs.putString("apSSID", settings.apSSID);
  prefs.putString("apPass", settings.apPassword);
}
