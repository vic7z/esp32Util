#include "web_server.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "config.h"
#include "security.h"
#include "settings.h"
#include "utils.h"
#include "device_monitor.h"
#include "screens_draw.h"
#include <SPIFFS.h>

WebServer server(80);
DNSServer dnsServer;
bool webServerRunning = false;
bool triggerWifiScan = false;
bool triggerBleScan = false;
static uint32_t webLastWifiScan = 0;
static bool webScanInProgress = false;

extern int8_t walkRSSIHistory[WALK_HISTORY_SIZE];
extern uint8_t walkTargetBSSID[6];
extern bool walkTestActive;

void parseBytes(const char* str, char sep, byte* bytes, int maxBytes, int base) {
  for (int i = 0; i < maxBytes; i++) {
    bytes[i] = strtoul(str, NULL, base);
    str = strchr(str, sep);
    if (str == NULL || *str == '\0') break;
    str++;
  }
}

void handleRoot() {
  Serial.println("[WEB] Root request");
  File f = SPIFFS.open("/index.html", "r");
  if (f) {
    Serial.printf("[WEB] Serving index.html, size=%d\n", f.size());
    server.streamFile(f, "text/html");
    f.close();
  } else {
    Serial.println("[WEB] index.html not found!");
    server.send(500, "text/plain", "File not found - upload SPIFFS image");
  }
}

void handleSettings() {
  if (server.method() == HTTP_GET) {
    char buffer[512];
    char ssidSafe[66], passSafe[130];
    strncpy(ssidSafe, settings.apSSID, 65);
    strncpy(passSafe, settings.apPassword, 129);
    for (int i = 0; ssidSafe[i]; i++) if (ssidSafe[i] == '"' || ssidSafe[i] == '\\') ssidSafe[i] = '_';
    for (int i = 0; passSafe[i]; i++) if (passSafe[i] == '"' || passSafe[i] == '\\') passSafe[i] = '_';
    
    snprintf(buffer, sizeof(buffer),
      "{\"scanSpeed\":%d,\"rssiThreshold\":%d,\"rgbBrightness\":%d,\"deauthThreshold\":%d,\"screenTimeout\":%d,\"powerMode\":%d,\"apSSID\":\"%s\",\"apPassword\":\"%s\"}",
      settings.scanSpeed, settings.rssiThreshold, settings.rgbBrightness,
      settings.deauthThreshold, settings.screenTimeout, settings.powerMode,
      ssidSafe, passSafe);
    server.send(200, "application/json", buffer);
  } else if (server.method() == HTTP_POST) {
    if (server.hasArg("scanSpeed")) settings.scanSpeed = server.arg("scanSpeed").toInt();
    if (server.hasArg("rssiThreshold")) settings.rssiThreshold = server.arg("rssiThreshold").toInt();
    if (server.hasArg("rgbBrightness")) {
        settings.rgbBrightness = server.arg("rgbBrightness").toInt();
        if (RGB_ENABLED) {
            rgb.setBrightness((settings.rgbBrightness * 255) / 100);
            rgb.show();
        }
    }
    if (server.hasArg("deauthThreshold")) settings.deauthThreshold = server.arg("deauthThreshold").toInt();
    if (server.hasArg("screenTimeout")) settings.screenTimeout = server.arg("screenTimeout").toInt();
    if (server.hasArg("powerMode")) settings.powerMode = server.arg("powerMode").toInt();
    if (server.hasArg("apSSID")) {
      String ssid = server.arg("apSSID");
      strncpy(settings.apSSID, ssid.c_str(), 32);
      settings.apSSID[32] = '\0';
    }
    if (server.hasArg("apPassword")) {
      String pass = server.arg("apPassword");
      strncpy(settings.apPassword, pass.c_str(), 64);
      settings.apPassword[64] = '\0';
    }
    
    saveSettings();
    server.send(200, "text/plain", "Settings Saved. Restart device to apply WiFi changes.");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

bool snifferActive = false;
uint8_t snifferChannel = 0;
uint8_t ownApMac[6] = {0};
uint32_t lastPktTotal = 0;
uint32_t lastPktTime = 0;
uint32_t ppsHistory[60];
uint8_t ppsHistIdx = 0;

void stopSniffer(); // Forward declaration

void resetPktStats() {
  pktTotal = 0;
  pktBeacon = 0;
  pktData = 0;
  pktDeauth = 0;
  pktProbe = 0;
  pps = 0;
  peak = 0;
  lastPktTotal = 0;
  lastPktTime = 0;
  memset((void*)chPackets, 0, sizeof(chPackets));
  memset((void*)chBeacons, 0, sizeof(chBeacons));
  memset((void*)chDeauth, 0, sizeof(chDeauth));
  memset((void*)ppsHistory, 0, sizeof(ppsHistory));
  ppsHistIdx = 0;
  extern uint8_t pktLogIndex;
  extern uint8_t pktLogCount;
  pktLogIndex = 0;
  pktLogCount = 0;
  for (int i = 0; i < MAX_PKT_LOG; i++) {
    pktLogBuffer[i].active = false;
  }
}

void updatePPS() {
  uint32_t now = millis();
  if (now - lastPktTime >= 1000) {
    uint32_t diff = pktTotal - lastPktTotal;
    pps = diff;
    if (pps > peak) peak = pps;
    
    ppsHistory[ppsHistIdx] = pps;
    ppsHistIdx = (ppsHistIdx + 1) % 60;
    
    lastPktTotal = pktTotal;
    lastPktTime = now;
  }
}

void startSniffer(uint8_t ch) {
  if (snifferActive) {
    stopSniffer();
    delay(50);
  }
  
  snifferChannel = ch;
  
  resetPktStats();

  esp_wifi_set_promiscuous_rx_cb(sniffer);
  esp_wifi_set_promiscuous(true);

  if (ch > 0 && ch <= 13) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  }
  
  snifferActive = true;
}

void stopSniffer() {
  if (!snifferActive) return;
  esp_wifi_set_promiscuous(false);
  snifferActive = false;
  snifferChannel = 0;
}

void handleAction() {
  if (!server.hasArg("type")) {
    server.send(400, "text/plain", "Missing type");
    return;
  }
  
  String type = server.arg("type");
  if (type == "scan_wifi") {
    // Prevent auto-scan from interfering
    webScanInProgress = true;
    
    // Pause sniffer if active
    bool sniffPaused = false;
    if (snifferActive || deviceMonitorActive) {
      esp_wifi_set_promiscuous(false);
      sniffPaused = true;
      delay(50);
    }
    
    // Do a blocking scan for immediate results
    WiFi.scanDelete();
    delay(50);
    int n = WiFi.scanNetworks(false, true, false, 150);
    if (n > 0) {
      apCount = min((int)n, (int)MAX_APS);
      for (int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char*)apList[i].ssid, ssid.c_str(), 32);
        apList[i].ssid[31] = '\0';
        memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
        apList[i].rssi = WiFi.RSSI(i);
        apList[i].primary = WiFi.channel(i);
        apList[i].authmode = WiFi.encryptionType(i);
      }
    }
    WiFi.scanDelete();
    
    // Update security counters
    secOpen = secWEP = secWPA = secWPA2 = secWPA3 = 0;
    for (int i = 0; i < apCount; i++) {
      switch (apList[i].authmode) {
        case WIFI_AUTH_OPEN: secOpen++; break;
        case WIFI_AUTH_WEP: secWEP++; break;
        case WIFI_AUTH_WPA_PSK: secWPA++; break;
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK: secWPA2++; break;
        case WIFI_AUTH_WPA3_PSK: secWPA3++; break;
        default: secWPA2++; break;
      }
    }
    
    // Reset auto-scan timer so it doesn't overwrite these results for 15 seconds
    webLastWifiScan = millis();
    webScanInProgress = false;
    
    // Restore sniffer if it was active
    if (sniffPaused) {
      delay(100);
      esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
      esp_wifi_set_promiscuous(true);
    }
    
    server.send(200, "text/plain", "WiFi Scan Complete: " + String(apCount) + " networks");
  } else if (type == "start_sniffer") {
    uint8_t ch = 0;
    if (server.hasArg("channel")) ch = server.arg("channel").toInt();
    startSniffer(ch);
    server.send(200, "text/plain", "Sniffer started on channel " + String(ch));
  } else if (type == "stop_sniffer") {
    stopSniffer();
    server.send(200, "text/plain", "Sniffer stopped");
  } else if (type == "scan_ble") {
    triggerBleScan = true;
    server.send(200, "text/plain", "BLE Scan Started...");
  } else if (type == "clear") {
    apCount = 0;
    bleDeviceCount = 0;
    server.send(200, "text/plain", "Results Cleared");
  } else if (type == "track_wifi") {
    if (server.hasArg("bssid")) {
      String bssidStr = server.arg("bssid");
      parseBytes(bssidStr.c_str(), ':', walkTest.targetBSSID, 6, 16);
      walkTest.active = true;
      memset(walkTest.rssiHistory, 0, WALK_HISTORY_SIZE);
      server.send(200, "text/plain", "Tracking " + bssidStr);
    } else {
      server.send(400, "text/plain", "Missing bssid");
    }
  } else if (type == "stop_track") {
    walkTest.active = false;
    server.send(200, "text/plain", "Tracking Stopped");
  } else if (type == "start_monitor") {
    if (snifferActive) stopSniffer();
    esp_wifi_set_promiscuous_rx_cb(deviceMonitorSniffer);
    esp_wifi_set_promiscuous(true);
    deviceMonitorActive = true;
    webMonitorMode = true;
    server.send(200, "text/plain", "Client Monitor Started");
  } else if (type == "stop_monitor") {
    esp_wifi_set_promiscuous(false);
    deviceMonitorActive = false;
    webMonitorMode = false;
    server.send(200, "text/plain", "Client Monitor Stopped");
  } else if (type == "clear_monitor") {
    clearDeviceMonitor();
    server.send(200, "text/plain", "Monitor Cleared");
  } else if (type == "save_baseline") {
    takeSnapshot(&currentSnapshot);
    baseline = currentSnapshot;
    baseline.saved = true;
    server.send(200, "text/plain", "Baseline Saved");
  } else if (type == "take_snapshot") {
    takeSnapshot(&currentSnapshot);
    server.send(200, "text/plain", "Snapshot Taken");
  } else {
    server.send(400, "text/plain", "Unknown action");
  }
}

void handleApiData() {
  // Add CORS headers
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  Serial.printf("[WEB] API data request, apCount=%d, bleCount=%d\n", apCount, bleDeviceCount);
  
  // Use a large static buffer to build JSON response
  static char jsonBuffer[8192];
  int pos = 0;
  
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "{\"wifi\":[");

  for (int i = 0; i < apCount && pos < sizeof(jsonBuffer) - 256; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    
    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      apList[i].bssid[0], apList[i].bssid[1], apList[i].bssid[2],
      apList[i].bssid[3], apList[i].bssid[4], apList[i].bssid[5]);
    
    const char* vendor = getVendor(apList[i].bssid);

    char ssidSafe[33];
    strncpy(ssidSafe, (char*)apList[i].ssid, 32);
    ssidSafe[32] = '\0';
    for (int j = 0; ssidSafe[j]; j++) {
      if ((unsigned char)ssidSafe[j] < 32 || ssidSafe[j] == '"' || ssidSafe[j] == '\\') {
        ssidSafe[j] = '?';
      }
    }
    
    char grade = getQualityGrade(&apList[i]);
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
      "{\"ssid\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"sec\":%d,\"vendor\":\"%s\",\"distance\":%d,\"grade\":\"%c\"}",
      ssidSafe, bssidStr, apList[i].rssi, apList[i].primary, apList[i].authmode,
      vendor ? vendor : "Unknown", (int)estimateDistance(apList[i].rssi), grade);
  }

  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"ble\":[");

  bool first = true;
  for (int i = 0; i < bleDeviceCount && pos < sizeof(jsonBuffer) - 256; i++) {
    if (bleDevices[i].isActive) {
      if (!first) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
      first = false;
      
      char nameSafe[33];
      strncpy(nameSafe, bleDevices[i].name, 32);
      nameSafe[32] = '\0';
      for (int j = 0; nameSafe[j]; j++) {
        if ((unsigned char)nameSafe[j] < 32 || nameSafe[j] == '"' || nameSafe[j] == '\\') {
          nameSafe[j] = '?';
        }
      }
      
      pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
        "{\"name\":\"%s\",\"addr\":\"%s\",\"rssi\":%d,\"type\":%d,\"hasName\":%s}",
        nameSafe, bleDevices[i].address, bleDevices[i].rssi, bleDevices[i].advType,
        bleDevices[i].hasName ? "true" : "false");
    }
  }

  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"graph\":[");

  for (int i = 0; i < WALK_HISTORY_SIZE && pos < sizeof(jsonBuffer) - 64; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "%d", walkTest.rssiHistory[i]);
  }
  
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"events\":[");

  for (int i = 0; i < eventCount && pos < sizeof(jsonBuffer) - 128; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    
    char msgSafe[41];
    strncpy(msgSafe, eventLog[i].message, 40);
    msgSafe[40] = '\0';
    for (int j = 0; msgSafe[j]; j++) {
      if ((unsigned char)msgSafe[j] < 32 || msgSafe[j] == '"' || msgSafe[j] == '\\') {
        msgSafe[j] = '?';
      }
    }
    
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
      "{\"type\":%d,\"msg\":\"%s\",\"ts\":%lu}",
      eventLog[i].type, msgSafe, eventLog[i].timestamp);
  }
  
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"rogue\":[");
  for (int i = 0; i < rogueCount && pos < sizeof(jsonBuffer) - 256; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    
    char bssid1Str[18], bssid2Str[18];
    snprintf(bssid1Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid1[0], rogueList[i].bssid1[1], rogueList[i].bssid1[2],
      rogueList[i].bssid1[3], rogueList[i].bssid1[4], rogueList[i].bssid1[5]);
    snprintf(bssid2Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid2[0], rogueList[i].bssid2[1], rogueList[i].bssid2[2],
      rogueList[i].bssid2[3], rogueList[i].bssid2[4], rogueList[i].bssid2[5]);
    
    char ssidSafe[33];
    strncpy(ssidSafe, rogueList[i].ssid, 32);
    ssidSafe[32] = '\0';
    for (int j = 0; ssidSafe[j]; j++) {
      if ((unsigned char)ssidSafe[j] < 32 || ssidSafe[j] == '"' || ssidSafe[j] == '\\') {
        ssidSafe[j] = '?';
      }
    }
    
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
      "{\"ssid\":\"%s\",\"bssid1\":\"%s\",\"bssid2\":\"%s\"}",
      ssidSafe, bssid1Str, bssid2Str);
  }

  // Hidden SSIDs
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"hidden\":[");
  for (int i = 0; i < hiddenCount && pos < sizeof(jsonBuffer) - 128; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    char hSafe[33];
    strncpy(hSafe, hiddenList[i].ssid, 32);
    hSafe[32] = '\0';
    for (int j = 0; hSafe[j]; j++) {
      if ((unsigned char)hSafe[j] < 32 || hSafe[j] == '"' || hSafe[j] == '\\') hSafe[j] = '?';
    }
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d}", hSafe, hiddenList[i].rssi, hiddenList[i].channel);
  }

  // Session stats
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"session\":{");
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
    "\"uptime\":%lu,\"totalAPs\":%lu,\"peakPPS\":%lu,\"totalPkts\":%lu,\"totalDeauth\":%lu,\"deauthPS\":%lu,\"attackActive\":%s,\"deauthCh\":%d",
    (unsigned long)(millis() - sessionStart), (unsigned long)totalAPsFound,
    (unsigned long)peakPPS, (unsigned long)totalPackets,
    (unsigned long)totalDeauthDetected, (unsigned long)deauthPerSecond,
    attackActive ? "true" : "false", (int)deauthChannel);

  // Security breakdown
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "},\"security\":{");
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "\"open\":%d,\"wep\":%d,\"wpa\":%d,\"wpa2\":%d,\"wpa3\":%d",
    secOpen, secWEP, secWPA, secWPA2, secWPA3);

  // RF Health
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "},\"rfHealth\":{");
  uint8_t rfLoad = liveLoad();
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "\"load\":%d,\"quality\":\"%s\",\"insight\":\"%s\",\"bestCh\":%d,\"worstCh\":%d",
    rfLoad, loadQuality(rfLoad), channelInsight(), bestChannel(), worstChannel());

  // Walk test status
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "},\"walkActive\":%s",
    walkTest.active ? "true" : "false");

  // Baseline data
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",\"baseline\":{");
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
    "\"saved\":%s,\"ts\":%lu,\"bAPs\":%d,\"bRSSI\":%d,\"bPkts\":%lu,\"cAPs\":%d,\"cRSSI\":%d,\"cPkts\":%lu",
    baseline.saved ? "true" : "false",
    baseline.saved ? (unsigned long)baseline.timestamp : 0UL,
    baseline.totalAPs, baseline.avgRSSI, (unsigned long)baseline.totalPackets,
    currentSnapshot.totalAPs, currentSnapshot.avgRSSI, (unsigned long)currentSnapshot.totalPackets);

  // Channel distributions
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",\"chDist\":[");
  for (int i = 0; i < 13 && pos < sizeof(jsonBuffer) - 64; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "%d", currentSnapshot.channelDist[i]);
  }
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "],\"chDistB\":[");
  for (int i = 0; i < 13 && pos < sizeof(jsonBuffer) - 64; i++) {
    if (i > 0) pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, ",");
    pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "%d", baseline.channelDist[i]);
  }
  pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos, "]}}");
  
  Serial.printf("[WEB] Sending JSON response, len=%d\n", pos);
  server.send(200, "application/json", jsonBuffer);
}

void handlePacketData() {
  char buffer[512];
  
  updatePPS();
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  
  uint8_t reportChannel = (snifferChannel > 0 && snifferChannel <= 13) ? snifferChannel : 0;

  uint32_t reportTotal = pktTotal;
  uint32_t reportBeacon = pktBeacon;
  uint32_t reportDeauth = pktDeauth;
  uint32_t reportData = pktData;
  
  snprintf(buffer, sizeof(buffer),
    "{\"total\":%lu,\"beacon\":%lu,\"data\":%lu,\"probe\":%lu,\"deauth\":%lu,\"pps\":%lu,\"peak\":%lu,\"channel\":%d,",
    (unsigned long)reportTotal, (unsigned long)reportBeacon, (unsigned long)reportData,
    (unsigned long)pktProbe, (unsigned long)reportDeauth, (unsigned long)pps, (unsigned long)peak, (int)reportChannel);
  server.sendContent(buffer);
    
  server.sendContent("\"ppsHistory\":[");
  for (int i = 0; i < 60; i++) {
    if (i > 0) server.sendContent(",");
    int idx = (ppsHistIdx - 60 + i + 60) % 60;
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)ppsHistory[idx]);
    server.sendContent(buffer);
  }
  server.sendContent("],");
  
  server.sendContent("\"channels\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chPackets[i]);
    server.sendContent(buffer);
  }
  
  server.sendContent("],\"beacons\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chBeacons[i]);
    server.sendContent(buffer);
  }
  
  server.sendContent("],\"deauths\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chDeauth[i]);
    server.sendContent(buffer);
  }
  
  server.sendContent("],\"overlap\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%d", countOverlappingAPs(i));
    server.sendContent(buffer);
  }

  server.sendContent("],\"packets\":[");
  int count = 0;
  for (int i = 0; i < MAX_PKT_LOG && count < MAX_PKT_LOG; i++) {
    int idx = (pktLogIndex - 1 - i + MAX_PKT_LOG) % MAX_PKT_LOG;
    if (!pktLogBuffer[idx].active) continue;
    if (count > 0) server.sendContent(",");
    
    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      pktLogBuffer[idx].bssid[0], pktLogBuffer[idx].bssid[1], pktLogBuffer[idx].bssid[2],
      pktLogBuffer[idx].bssid[3], pktLogBuffer[idx].bssid[4], pktLogBuffer[idx].bssid[5]);
    
    char ssidSafe[33];
    strncpy(ssidSafe, pktLogBuffer[idx].ssid, 32);
    ssidSafe[32] = 0;
    for (int j = 0; ssidSafe[j]; j++) {
      if (ssidSafe[j] == '"' || ssidSafe[j] == '\\') ssidSafe[j] = '_';
    }
    
    const char* typeStr = "OTHER";
    if (pktLogBuffer[idx].type == 1) typeStr = "DATA";
    else if (pktLogBuffer[idx].type == 2) typeStr = "DEAUTH";
    else if (pktLogBuffer[idx].type == 3) typeStr = "BEACON";
    else if (pktLogBuffer[idx].type == 4) typeStr = "PROBE";
    
    snprintf(buffer, sizeof(buffer), 
      "{\"ts\":%lu,\"type\":\"%s\",\"ch\":%d,\"rssi\":%d,\"bssid\":\"%s\",\"ssid\":\"%s\"}",
      pktLogBuffer[idx].timestamp, typeStr, pktLogBuffer[idx].channel, 
      pktLogBuffer[idx].rssi, bssidStr, ssidSafe);
    server.sendContent(buffer);
    count++;
  }
  
  server.sendContent("]}");
  server.sendContent("");
}

void handleMonitorData() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  char buffer[256];
  server.sendContent("{\"devices\":[");

  bool first = true;
  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    if (!monitoredDevices[i].active) continue;
    if (!first) server.sendContent(",");
    first = false;

    char macStr[18];
    if (monitoredDevices[i].type == 0) {
      snprintf(macStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
        monitoredDevices[i].bssid[0], monitoredDevices[i].bssid[1],
        monitoredDevices[i].bssid[2], monitoredDevices[i].bssid[3],
        monitoredDevices[i].bssid[4], monitoredDevices[i].bssid[5]);
    } else {
      strncpy(macStr, monitoredDevices[i].bleAddr, 17);
      macStr[17] = '\0';
    }

    char nameSafe[33];
    strncpy(nameSafe, monitoredDevices[i].name, 32);
    nameSafe[32] = '\0';
    for (int j = 0; nameSafe[j]; j++) {
      if ((unsigned char)nameSafe[j] < 32 || nameSafe[j] == '"' || nameSafe[j] == '\\') nameSafe[j] = '?';
    }

    snprintf(buffer, sizeof(buffer),
      "{\"type\":%d,\"mac\":\"%s\",\"name\":\"%s\",\"rssi\":%d,\"ch\":%d,\"first\":%lu,\"last\":%lu,\"seen\":%d,\"present\":%s}",
      monitoredDevices[i].type, macStr, nameSafe,
      monitoredDevices[i].rssi, monitoredDevices[i].channel,
      (unsigned long)monitoredDevices[i].firstSeen,
      (unsigned long)monitoredDevices[i].lastSeen,
      monitoredDevices[i].seenCount,
      monitoredDevices[i].isPresent ? "true" : "false");
    server.sendContent(buffer);
  }

  snprintf(buffer, sizeof(buffer), "],\"count\":%d,\"active\":%s}",
    monitoredDeviceCount, deviceMonitorActive ? "true" : "false");
  server.sendContent(buffer);
  server.sendContent("");
}

void startWebServer() {
  if (webServerRunning) return;

  Serial.println("[WEB] Starting web server...");

  // Stop any existing WiFi operations
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  delay(200);
  
  // Use AP_STA mode for best compatibility
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  delay(100);
  
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  if (!WiFi.softAPConfig(localIP, gateway, subnet)) {
    Serial.println("[ERROR] AP Config failed!");
    return;
  }
  
  const char* ssid = settings.apSSID[0] ? settings.apSSID : "ESP32-Tool";
  const char* pass = settings.apPassword[0] ? settings.apPassword : "";
  bool result;
  
  // Start AP with default password if none set
  if (pass[0] == '\0') {
    result = WiFi.softAP(ssid, "12345678", 6, 0, 4);
    Serial.println("[INFO] Starting AP with password: 12345678");
  } else {
    result = WiFi.softAP(ssid, pass, 6, 0, 4);
    Serial.printf("[INFO] Starting AP, SSID: %s\n", ssid);
  }
  
  if (!result) {
    Serial.println("[ERROR] Failed to start AP!");
    return;
  }
  
  delay(500);
  
  Serial.print("[INFO] AP IP: ");
  Serial.println(WiFi.softAPIP());

  WiFi.softAPmacAddress(ownApMac);
  Serial.printf("[INFO] AP MAC (own): %02X:%02X:%02X:%02X:%02X:%02X\n",
    ownApMac[0], ownApMac[1], ownApMac[2],
    ownApMac[3], ownApMac[4], ownApMac[5]);

  Serial.print("[INFO] AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("[INFO] AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  
  dnsServer.start(53, "esp32.util", localIP);

  server.on("/", handleRoot);
  server.on("/style.css", []() {
    File f = SPIFFS.open("/style.css", "r");
    if (f) { server.streamFile(f, "text/css"); f.close(); }
    else server.send(404, "text/plain", "CSS not found");
  });
  server.on("/app.js", []() {
    File f = SPIFFS.open("/app.js", "r");
    if (f) { server.streamFile(f, "application/javascript"); f.close(); }
    else server.send(404, "text/plain", "JS not found");
  });
  server.on("/api/data", handleApiData);
  server.on("/api/packets", handlePacketData);
  server.on("/api/monitor", handleMonitorData);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/settings", handleSettings);
  
  server.begin();
  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  server.stop();
  dnsServer.stop();
  stopSniffer();
  if (deviceMonitorActive) {
    esp_wifi_set_promiscuous(false);
    deviceMonitorActive = false;
    webMonitorMode = false;
  }
  
  // Disconnect AP and restore STA mode for normal scanning
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(200);
  
  // Restore STA mode for normal WiFi scanning functionality
  WiFi.mode(WIFI_STA);
  delay(100);
  
  webServerRunning = false;
  Serial.println("[INFO] Web server stopped, restored STA mode");
}

void handleWebServerLoop() {
  if (!webServerRunning) return;
  dnsServer.processNextRequest();
  server.handleClient();

  static uint32_t lastBleScan = 0;
  uint32_t now = millis();
  
  // WiFi.scanNetworks() kills promiscuous mode; pause sniffer before scan and restore after
  static bool snifferWasPaused = false;

  if (triggerWifiScan && !webScanInProgress) {
    triggerWifiScan = false;
    webScanInProgress = true;

    if (snifferActive || deviceMonitorActive) {
      esp_wifi_set_promiscuous(false);
      snifferWasPaused = true;
    }

    WiFi.scanDelete();
    int16_t err = WiFi.scanNetworks(true, true, false, 150);
    if (err == WIFI_SCAN_RUNNING) {
    } else if (err >= 0) {
      apCount = min((int)err, (int)MAX_APS);
      for (int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char*)apList[i].ssid, ssid.c_str(), 32);
        apList[i].ssid[31] = '\0';
        memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
        apList[i].rssi = WiFi.RSSI(i);
        apList[i].primary = WiFi.channel(i);
        apList[i].authmode = WiFi.encryptionType(i);
      }
      WiFi.scanDelete();
      webScanInProgress = false;
      webLastWifiScan = now;
      if (snifferWasPaused) {
        snifferWasPaused = false;
        esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
        esp_wifi_set_promiscuous(true);
      }
    } else {
      // Scan failed to start - restore state immediately
      WiFi.scanDelete();
      webScanInProgress = false;
      if (snifferWasPaused) {
        snifferWasPaused = false;
        esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
        esp_wifi_set_promiscuous(true);
      }
    }
  }

  if (webScanInProgress) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
      apCount = min((int)n, (int)MAX_APS);
      for (int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char*)apList[i].ssid, ssid.c_str(), 32);
        apList[i].ssid[31] = '\0';
        memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
        apList[i].rssi = WiFi.RSSI(i);
        apList[i].primary = WiFi.channel(i);
        apList[i].authmode = WiFi.encryptionType(i);
      }
      WiFi.scanDelete();
      webScanInProgress = false;
      webLastWifiScan = now;
      if (snifferWasPaused) {
        snifferWasPaused = false;
        esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
        esp_wifi_set_promiscuous(true);
      }
    } else if (n == WIFI_SCAN_FAILED) {
      // Scan failed - restore state so monitor isn't permanently killed
      WiFi.scanDelete();
      webScanInProgress = false;
      if (snifferWasPaused) {
        snifferWasPaused = false;
        esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
        esp_wifi_set_promiscuous(true);
      }
    }
  }

  // Auto-scan every 15 seconds if no recent manual scan and not in sniffer/monitor mode
  if (!webScanInProgress && now - webLastWifiScan > 15000 && !snifferActive && !deviceMonitorActive) {
    triggerWifiScan = true;
  }
  
  if (triggerBleScan) {
    triggerBleScan = false;
    startBLEScan();
    lastBleScan = now;
  }
  
  if (bleScanning) {
    updateBLEScan();
  }
  
  if (!bleScanning && now - lastBleScan > 5000) {
    startBLEScan();
    lastBleScan = now;
  }

  // Device monitor: check timeouts and update BLE devices
  if (deviceMonitorActive) {
    static uint32_t lastMonitorUpdate = 0;
    if (now - lastMonitorUpdate > 1000) {
      checkDeviceTimeouts();
      // Add BLE devices to monitor
      for (int i = 0; i < bleDeviceCount; i++) {
        if (bleDevices[i].isActive) {
          addOrUpdateBLEDevice(bleDevices[i].address, bleDevices[i].name, bleDevices[i].rssi);
        }
      }
      lastMonitorUpdate = now;
    }
  }

  if (walkTest.active) {
    static uint32_t lastGraphUpdate = 0;
    if (millis() - lastGraphUpdate > 1000) {
      bool found = false;
      for (int i = 0; i < apCount; i++) {
        if (memcmp(apList[i].bssid, walkTest.targetBSSID, 6) == 0) {
          for (int j = 0; j < WALK_HISTORY_SIZE - 1; j++) {
            walkTest.rssiHistory[j] = walkTest.rssiHistory[j+1];
          }
          walkTest.rssiHistory[WALK_HISTORY_SIZE - 1] = apList[i].rssi;
          found = true;
          break;
        }
      }
      if (!found) {
        for (int j = 0; j < WALK_HISTORY_SIZE - 1; j++) {
            walkTest.rssiHistory[j] = walkTest.rssiHistory[j+1];
        }
        walkTest.rssiHistory[WALK_HISTORY_SIZE - 1] = -100;
      }
      lastGraphUpdate = millis();
    }
  }
}
