#include "web_server.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "config.h"
#include "security.h"
#include "settings.h"
#include "utils.h"
#include "web/web_assets.h"

WebServer server(80);
DNSServer dnsServer;
bool webServerRunning = false;
bool triggerWifiScan = false;
bool triggerBleScan = false;

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
  server.send(200, "text/html", getIndexHtml());
}

void handleSettings() {
  if (server.method() == HTTP_GET) {
    char buffer[512];
    // Escape SSID and password for JSON
    char ssidSafe[66], passSafe[130];
    strncpy(ssidSafe, settings.apSSID, 65);
    strncpy(passSafe, settings.apPassword, 129);
    // Basic JSON escaping
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
uint32_t lastPktTotal = 0;
uint32_t lastPktTime = 0;
uint32_t ppsHistory[60]; // 60 seconds of PPS history
uint8_t ppsHistIdx = 0;

void stopSniffer(); // Forward declaration

void resetPktStats() {
  pktTotal = 0;
  pktBeacon = 0;
  pktData = 0;
  pktDeauth = 0;
  pps = 0;
  peak = 0;
  lastPktTotal = 0;
  lastPktTime = 0;
  memset((void*)chPackets, 0, sizeof(chPackets));
  memset((void*)chBeacons, 0, sizeof(chBeacons));
  memset((void*)chDeauth, 0, sizeof(chDeauth));
  memset((void*)ppsHistory, 0, sizeof(ppsHistory));
  ppsHistIdx = 0;
  // Clear packet log buffer
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
    
    // Store in history for graph
    ppsHistory[ppsHistIdx] = pps;
    ppsHistIdx = (ppsHistIdx + 1) % 60;
    
    lastPktTotal = pktTotal;
    lastPktTime = now;
  }
}

void startSniffer(uint8_t ch) {
  // If already active, stop first to reset
  if (snifferActive) {
    stopSniffer();
    delay(50);
  }
  
  snifferChannel = ch;
  
  // Reset all stats before starting
  resetPktStats();
  
  // Enable promiscuous mode while keeping AP active
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
  // Don't reset stats here so user can see final counts
}

void handleAction() {
  if (!server.hasArg("type")) {
    server.send(400, "text/plain", "Missing type");
    return;
  }
  
  String type = server.arg("type");
  if (type == "scan_wifi") {
    // Do a blocking scan for immediate results
    WiFi.scanDelete();
    int n = WiFi.scanNetworks(false, true, false, 120); // blocking, show hidden, passive=false, 120ms per channel
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
  } else {
    server.send(400, "text/plain", "Unknown action");
  }
}

void handleApiData() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  char buffer[512];
  
  // Start JSON
  server.sendContent("{\"wifi\":[");

  // WiFi APs
  for (int i = 0; i < apCount; i++) {
    if (i > 0) server.sendContent(",");
    
    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      apList[i].bssid[0], apList[i].bssid[1], apList[i].bssid[2],
      apList[i].bssid[3], apList[i].bssid[4], apList[i].bssid[5]);
    
    // Get vendor name
    const char* vendor = getVendor(apList[i].bssid);
    
    // Escape SSID by copying valid chars only
    char ssidSafe[33];
    strncpy(ssidSafe, (char*)apList[i].ssid, 32);
    ssidSafe[32] = '\0';
    // Remove control chars and quotes that would break JSON
    for (int j = 0; ssidSafe[j]; j++) {
      if ((unsigned char)ssidSafe[j] < 32 || ssidSafe[j] == '"' || ssidSafe[j] == '\\') {
        ssidSafe[j] = '?';
      }
    }
    
    snprintf(buffer, sizeof(buffer),
      "{\"ssid\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"sec\":%d,\"vendor\":\"%s\",\"distance\":%d}",
      ssidSafe, bssidStr, apList[i].rssi, apList[i].primary, apList[i].authmode,
      vendor ? vendor : "Unknown", estimateDistance(apList[i].rssi));
    server.sendContent(buffer);
  }

  server.sendContent("],\"ble\":[");

  // BLE devices
  bool first = true;
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].isActive) {
      if (!first) server.sendContent(",");
      first = false;
      
      // Escape BLE name
      char nameSafe[33];
      strncpy(nameSafe, bleDevices[i].name, 32);
      nameSafe[32] = '\0';
      for (int j = 0; nameSafe[j]; j++) {
        if ((unsigned char)nameSafe[j] < 32 || nameSafe[j] == '"' || nameSafe[j] == '\\') {
          nameSafe[j] = '?';
        }
      }
      
      snprintf(buffer, sizeof(buffer),
        "{\"name\":\"%s\",\"addr\":\"%s\",\"rssi\":%d,\"type\":%d,\"hasName\":%s}",
        nameSafe, bleDevices[i].address, bleDevices[i].rssi, bleDevices[i].advType,
        bleDevices[i].hasName ? "true" : "false");
      server.sendContent(buffer);
    }
  }

  server.sendContent("],\"graph\":[");
  
  // Graph data (RSSI history)
  for (int i = 0; i < WALK_HISTORY_SIZE; i++) {
    if (i > 0) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%d", walkTest.rssiHistory[i]);
    server.sendContent(buffer);
  }
  
  server.sendContent("],\"events\":[");
  
  // Events
  for (int i = 0; i < eventCount; i++) {
    if (i > 0) server.sendContent(",");
    
    // Escape message
    char msgSafe[41];
    strncpy(msgSafe, eventLog[i].message, 40);
    msgSafe[40] = '\0';
    for (int j = 0; msgSafe[j]; j++) {
      if ((unsigned char)msgSafe[j] < 32 || msgSafe[j] == '"' || msgSafe[j] == '\\') {
        msgSafe[j] = '?';
      }
    }
    
    snprintf(buffer, sizeof(buffer),
      "{\"type\":%d,\"msg\":\"%s\",\"ts\":%lu}",
      eventLog[i].type, msgSafe, eventLog[i].timestamp);
    server.sendContent(buffer);
  }
  
  // Rogue APs
  server.sendContent("],\"rogue\":[");
  for (int i = 0; i < rogueCount; i++) {
    if (i > 0) server.sendContent(",");
    
    char bssid1Str[18], bssid2Str[18];
    snprintf(bssid1Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid1[0], rogueList[i].bssid1[1], rogueList[i].bssid1[2],
      rogueList[i].bssid1[3], rogueList[i].bssid1[4], rogueList[i].bssid1[5]);
    snprintf(bssid2Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid2[0], rogueList[i].bssid2[1], rogueList[i].bssid2[2],
      rogueList[i].bssid2[3], rogueList[i].bssid2[4], rogueList[i].bssid2[5]);
    
    // Escape SSID
    char ssidSafe[33];
    strncpy(ssidSafe, rogueList[i].ssid, 32);
    ssidSafe[32] = '\0';
    for (int j = 0; ssidSafe[j]; j++) {
      if ((unsigned char)ssidSafe[j] < 32 || ssidSafe[j] == '"' || ssidSafe[j] == '\\') {
        ssidSafe[j] = '?';
      }
    }
    
    snprintf(buffer, sizeof(buffer),
      "{\"ssid\":\"%s\",\"bssid1\":\"%s\",\"bssid2\":\"%s\"}",
      ssidSafe, bssid1Str, bssid2Str);
    server.sendContent(buffer);
  }

  server.sendContent("]}");
  server.sendContent(""); // End of chunks
}

void handlePacketData() {
  char buffer[512];
  
  // Update PPS before sending
  updatePPS();
  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  
  // Send packet statistics
  snprintf(buffer, sizeof(buffer),
    "{\"total\":%lu,\"beacon\":%lu,\"data\":%lu,\"deauth\":%lu,\"pps\":%lu,\"peak\":%lu,\"channel\":%d,",
    (unsigned long)pktTotal, (unsigned long)pktBeacon, (unsigned long)pktData, 
    (unsigned long)pktDeauth, (unsigned long)pps, (unsigned long)peak, (int)scanState.currentChannel);
  server.sendContent(buffer);
    
  // Send PPS history for graph
  server.sendContent("\"ppsHistory\":[");
  for (int i = 0; i < 60; i++) {
    if (i > 0) server.sendContent(",");
    int idx = (ppsHistIdx - 60 + i + 60) % 60;
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)ppsHistory[idx]);
    server.sendContent(buffer);
  }
  server.sendContent("],");
  
  // Send channel distribution
  server.sendContent("\"channels\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chPackets[i]);
    server.sendContent(buffer);
  }
  
  // Send beacon counts per channel
  server.sendContent("],\"beacons\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chBeacons[i]);
    server.sendContent(buffer);
  }
  
  // Send deauth counts per channel
  server.sendContent("],\"deauths\":[");
  for (int i = 1; i <= MAX_CHANNEL; i++) {
    if (i > 1) server.sendContent(",");
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)chDeauth[i]);
    server.sendContent(buffer);
  }
  
  // Send packet log
  server.sendContent("],\"packets\":[");
  int count = 0;
  for (int i = 0; i < MAX_PKT_LOG && count < 20; i++) {
    int idx = (pktLogIndex - 1 - i + MAX_PKT_LOG) % MAX_PKT_LOG;
    if (!pktLogBuffer[idx].active) continue;
    if (count > 0) server.sendContent(",");
    
    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      pktLogBuffer[idx].bssid[0], pktLogBuffer[idx].bssid[1], pktLogBuffer[idx].bssid[2],
      pktLogBuffer[idx].bssid[3], pktLogBuffer[idx].bssid[4], pktLogBuffer[idx].bssid[5]);
    
    // Escape SSID
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

void startWebServer() {
  if (webServerRunning) return;
  
  // Disable WiFi first to clear any previous state
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  
  // Use AP mode
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  
  // Configure AP IP first
  IPAddress localIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  // Must call softAPConfig BEFORE softAP
  if (!WiFi.softAPConfig(localIP, gateway, subnet)) {
    Serial.println("[ERROR] AP Config failed!");
    return;
  }
  
  // Start AP with settings (use open network if password is empty)
  const char* ssid = settings.apSSID[0] ? settings.apSSID : "ESP32-Tool";
  const char* pass = settings.apPassword[0] ? settings.apPassword : "";
  bool result;
  
  if (pass[0] == '\0') {
    // Open network - no password
    result = WiFi.softAP(ssid, NULL, 6, 0, 4);
    Serial.println("[INFO] Starting OPEN network (no password)");
  } else {
    result = WiFi.softAP(ssid, pass, 6, 0, 4);
    Serial.printf("[INFO] Starting secure network, SSID: %s\n", ssid);
  }
  
  if (!result) {
    Serial.println("[ERROR] Failed to start AP!");
    return;
  }
  
  // Wait longer for DHCP to initialize
  delay(1000);
  
  Serial.print("[INFO] AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("[INFO] AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  
  // Start DNS Server for "esp32.util"
  dnsServer.start(53, "esp32.util", localIP);

  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/packets", handlePacketData);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/settings", handleSettings);
  
  server.begin();
  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  server.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  webServerRunning = false;
}

void handleWebServerLoop() {
  if (!webServerRunning) return;
  dnsServer.processNextRequest();
  server.handleClient();
  
  static uint32_t lastWifiScan = 0;
  static uint32_t lastBleScan = 0;
  static bool scanInProgress = false;
  uint32_t now = millis();
  
  // Handle WiFi Scan - use blocking scan for reliability in AP mode
  if (triggerWifiScan && !scanInProgress) {
    triggerWifiScan = false;
    scanInProgress = true;
    
    // Delete any old scan results
    WiFi.scanDelete();
    
    // Start async scan first
    int16_t err = WiFi.scanNetworks(true, true, false, 150); // async, show hidden, passive=false, maxTime=150ms per channel
    if (err == WIFI_SCAN_RUNNING) {
      // Scan started successfully
    } else if (err >= 0) {
      // Scan completed immediately (cached results)
      apCount = min((int)err, (int)MAX_APS);
      for (int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char*)apList[i].ssid, ssid.c_str(), 32);
        apList[i].ssid[31] = '\0'; // Ensure null termination
        memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
        apList[i].rssi = WiFi.RSSI(i);
        apList[i].primary = WiFi.channel(i);
        apList[i].authmode = WiFi.encryptionType(i);
      }
      WiFi.scanDelete();
      scanInProgress = false;
      lastWifiScan = now;
    }
  }
  
  // Check for async scan completion
  if (scanInProgress) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
      apCount = min((int)n, (int)MAX_APS);
      for (int i = 0; i < apCount; i++) {
        String ssid = WiFi.SSID(i);
        strncpy((char*)apList[i].ssid, ssid.c_str(), 32);
        apList[i].ssid[31] = '\0'; // Ensure null termination
        memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
        apList[i].rssi = WiFi.RSSI(i);
        apList[i].primary = WiFi.channel(i);
        apList[i].authmode = WiFi.encryptionType(i);
      }
      WiFi.scanDelete();
      scanInProgress = false;
      lastWifiScan = now;
    }
  }
  
  // Auto-scan every 10 seconds if not manually triggered
  if (!scanInProgress && now - lastWifiScan > 10000) {
    triggerWifiScan = true;
  }
  
  // Handle BLE Scan
  if (triggerBleScan) {
    triggerBleScan = false;
    startBLEScan();
    lastBleScan = now;
  }
  
  // Keep BLE scanning active
  if (bleScanning) {
    updateBLEScan();
  }
  
  // Auto-restart BLE scan every 5 seconds
  if (!bleScanning && now - lastBleScan > 5000) {
    startBLEScan();
    lastBleScan = now;
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


