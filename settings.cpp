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
  
  String ssid = prefs.getString("apSSID", "ESP32-Tool");
  String pass = prefs.getString("apPass", "");
  strncpy(settings.apSSID, ssid.c_str(), 32);
  strncpy(settings.apPassword, pass.c_str(), 64);
  settings.apSSID[32] = '\0';
  settings.apPassword[64] = '\0';
  if (settings.scanSpeed > 2) settings.scanSpeed = 1;
  if (settings.rssiThreshold < -100 || settings.rssiThreshold > -40) settings.rssiThreshold = -70;
  if (settings.rgbBrightness > 100) settings.rgbBrightness = 50;
  if (settings.deauthThreshold < 1 || settings.deauthThreshold > 100) settings.deauthThreshold = 10;
  if (settings.screenTimeout > 3600) settings.screenTimeout = 60;
  if (settings.powerMode > 2) settings.powerMode = 0;
  if (!settings.apSSID[0]) strcpy(settings.apSSID, "ESP32-Tool");
  size_t passLength = strlen(settings.apPassword);
  if (passLength && (passLength < 8 || passLength > 63)) settings.apPassword[0] = '\0';
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
