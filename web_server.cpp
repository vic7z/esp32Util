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
#include <stdarg.h>
#include "json_helpers.h"
#include "capture.h"
#include "lan_network.h"

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

static bool parseMAC(const String& value, uint8_t* mac) {
  if (value.length() != 17) return false;
  for (int i = 0; i < 6; ++i) {
    int at = i * 3;
    if (!isxdigit((unsigned char)value[at]) || !isxdigit((unsigned char)value[at + 1]) ||
        (i < 5 && value[at + 2] != ':')) return false;
    char pair[3] = {value[at], value[at + 1], 0};
    mac[i] = strtoul(pair, nullptr, 16);
  }
  return true;
}

static void sendJsonFormatted(const char* format, ...) {
  char buffer[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  server.sendContent(buffer);
}

static bool readSetting(const char* name, long low, long high, long& value) {
  if (!server.hasArg(name)) return true;
  String text = server.arg(name);
  char* end = nullptr;
  value = strtol(text.c_str(), &end, 10);
  return text.length() && end != text.c_str() && *end == 0 && value >= low && value <= high;
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
    char buffer[1024], ssidSafe[193], passSafe[385];
    escapeJson(settings.apSSID, ssidSafe, sizeof(ssidSafe));
    escapeJson(settings.apPassword, passSafe, sizeof(passSafe));
    snprintf(buffer, sizeof(buffer),
      "{\"scanSpeed\":%d,\"rssiThreshold\":%d,\"rgbBrightness\":%d,\"deauthThreshold\":%d,\"screenTimeout\":%d,\"powerMode\":%d,\"apSSID\":\"%s\",\"apPassword\":\"%s\"}",
      settings.scanSpeed, settings.rssiThreshold, settings.rgbBrightness,
      settings.deauthThreshold, settings.screenTimeout, settings.powerMode, ssidSafe, passSafe);
    server.send(200, "application/json", buffer);
  } else if (server.method() == HTTP_POST) {
    long speed=settings.scanSpeed, rssi=settings.rssiThreshold, brightness=settings.rgbBrightness;
    long threshold=settings.deauthThreshold, timeout=settings.screenTimeout, power=settings.powerMode;
    if (!readSetting("scanSpeed",0,2,speed) || !readSetting("rssiThreshold",-100,-40,rssi) ||
        !readSetting("rgbBrightness",0,100,brightness) || !readSetting("deauthThreshold",1,100,threshold) ||
        !readSetting("screenTimeout",0,3600,timeout) || !readSetting("powerMode",0,2,power)) {
      server.send(400,"text/plain","Invalid setting value"); return;
    }
    String ssid=server.hasArg("apSSID")?server.arg("apSSID"):String(settings.apSSID);
    String pass=server.hasArg("apPassword")?server.arg("apPassword"):String(settings.apPassword);
    if (ssid.length()<1 || ssid.length()>32 || (pass.length()!=0 && (pass.length()<8 || pass.length()>63))) {
      server.send(400,"text/plain","SSID must be 1-32 bytes; password must be empty (default) or 8-63 bytes"); return;
    }
    settings.scanSpeed=speed; settings.rssiThreshold=rssi; settings.rgbBrightness=brightness;
    settings.deauthThreshold=threshold; settings.screenTimeout=timeout; settings.powerMode=power;
    ssid.toCharArray(settings.apSSID,sizeof(settings.apSSID));
    pass.toCharArray(settings.apPassword,sizeof(settings.apPassword));
    saveSettings();
    server.send(200,"text/plain","Settings saved. Restart device to apply WiFi changes.");
  } else server.send(405,"text/plain","Method Not Allowed");
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
  resetLiveStats();
  pktTotal = 0;
  pktBeacon = 0;
  pktData = 0;
  pktDeauth = 0;
  pktProbe = 0;
  pps = 0;
  peak = 0;
  lastPktTotal = 0;
  lastPktTime = millis();
  memset((void*)chPackets, 0, sizeof(chPackets));
  memset((void*)chBeacons, 0, sizeof(chBeacons));
  memset((void*)chDeauth, 0, sizeof(chDeauth));
  memset(chData, 0, sizeof(chData));
  memset(chProbe, 0, sizeof(chProbe));
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
    pps = (uint32_t)((uint64_t)diff * 1000 / (now - lastPktTime));
    peakPPS = max(peakPPS, pps);
    smoothPps = pps;
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

  deviceMonitorActive = false;
  webMonitorMode = false;
  uint8_t actualChannel = 6;
  wifi_second_chan_t secondary;
  if (esp_wifi_get_channel(&actualChannel, &secondary) != ESP_OK) return;
  resetPktStats();
  snifferChannel = actualChannel;

  wifi_promiscuous_filter_t filter = {};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
  if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK ||
      esp_wifi_set_promiscuous_rx_cb(sniffer) != ESP_OK ||
      esp_wifi_set_promiscuous(true) != ESP_OK) return;

  (void)ch; // Web capture stays on the AP channel to preserve connectivity.

  snifferActive = true;
}

void stopSniffer() {
  stopPcapCapture();
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
  if (lanModeActive() && (type == "scan_wifi" || type == "scan_ble" || type == "start_sniffer" ||
      type == "start_pcap" || type == "start_monitor" || type == "track_wifi")) {
    server.send(409,"text/plain","Disconnect the LAN connection before using radio capture or WiFi/BLE scans."); return;
  }
  if (type == "scan_wifi") {
    triggerWifiScan = true;
    server.send(202, "text/plain", "WiFi scan queued");
  } else if (type == "start_sniffer") {
    uint8_t ch = 0;
    if (server.hasArg("channel")) ch = server.arg("channel").toInt();
    if (webScanInProgress) { server.send(409,"text/plain","Wait for the WiFi scan to finish"); return; }
    startSniffer(ch);
    if (!snifferActive) { server.send(503,"text/plain","Capture could not start"); return; }
    server.send(200, "text/plain", "Capture started on AP channel " + String(snifferChannel));
  } else if (type == "stop_sniffer") {
    stopSniffer();
    server.send(200, "text/plain", "Sniffer stopped");
  } else if (type == "stop_capture") {
    stopSniffer();
    if (deviceMonitorActive) esp_wifi_set_promiscuous(false);
    deviceMonitorActive = webMonitorMode = false;
    server.send(200,"text/plain","Capture listening and PCAP stopped");
  } else if (type == "start_pcap") {
    if (webScanInProgress) { server.send(409,"text/plain","Wait for the WiFi scan to finish"); return; }
    if (!snifferActive) startSniffer(0);
    if (!snifferActive) { server.send(503,"text/plain","Capture could not start"); return; }
    startPcapCapture();
    server.send(200,"text/plain","New PCAP capture started; previous PCAP packets cleared");
  } else if (type == "stop_pcap") {
    stopPcapCapture();
    server.send(200,"text/plain","PCAP stopped; probe/EAPOL monitoring continues");
  } else if (type == "clear_capture") {
    bool resumeListening = snifferActive || deviceMonitorActive;
    if (resumeListening) esp_wifi_set_promiscuous(false);
    clearCapture();
    hiddenCount = hiddenCursor = hiddenScroll = 0;
    memset(hiddenList, 0, sizeof(hiddenList));
    if (resumeListening && !webScanInProgress) esp_wifi_set_promiscuous(true);
    server.send(200,"text/plain","Probe history, EAPOL counts, and PCAP cleared");
  } else if (type == "scan_ble") {
    triggerBleScan = true;
    server.send(200, "text/plain", "BLE Scan Started...");
  } else if (type == "clear") {
    apCount = 0;
    bleDeviceCount = 0;
    rogueCount = 0;
    secOpen = secWEP = secWPA = secWPA2 = secWPA3 = 0;
    server.send(200, "text/plain", "Results Cleared");
  } else if (type == "track_wifi") {
    if (server.hasArg("bssid")) {
      String bssidStr = server.arg("bssid");
      if (!parseMAC(bssidStr, walkTest.targetBSSID)) { server.send(400,"text/plain","Invalid BSSID"); return; }
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
    if (webScanInProgress) { server.send(409,"text/plain","Wait for the WiFi scan to finish"); return; }
    if (snifferActive) stopSniffer();
    initDeviceMonitorQueue();
    esp_wifi_set_promiscuous_rx_cb(deviceMonitorSniffer);
    esp_wifi_set_promiscuous(true);
    deviceMonitorActive = true;
    webMonitorMode = true;
    server.send(200, "text/plain", "Client Monitor Started");
  } else if (type == "stop_monitor") {
    if (deviceMonitorActive) esp_wifi_set_promiscuous(false);
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
  // Pause only while copying callback-owned strings, never during HTTP IO.
  static HiddenSSID hiddenSnapshot[MAX_HIDDEN_SSIDS];
  uint8_t hiddenSnapshotCount;
  if (snifferActive) esp_wifi_set_promiscuous(false);
  memcpy(hiddenSnapshot, hiddenList, sizeof(hiddenSnapshot));
  hiddenSnapshotCount = hiddenCount;
  if (snifferActive && !webScanInProgress) esp_wifi_set_promiscuous(true);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  sendJsonFormatted("{\"wifi\":[");

  for (int i = 0; i < apCount; i++) {
    if (i > 0) sendJsonFormatted(",");

    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      apList[i].bssid[0], apList[i].bssid[1], apList[i].bssid[2],
      apList[i].bssid[3], apList[i].bssid[4], apList[i].bssid[5]);

    char vendorSafe[193];
    escapeJson(getVendor(apList[i].bssid), vendorSafe, sizeof(vendorSafe));

    char ssidSafe[193];
    escapeJson((char*)apList[i].ssid, ssidSafe, sizeof(ssidSafe));

    char grade = getQualityGrade(&apList[i]);
    sendJsonFormatted("{\"ssid\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"sec\":%d,\"vendor\":\"%s\",\"distance\":%d,\"grade\":\"%c\"}",
      ssidSafe, bssidStr, apList[i].rssi, apList[i].primary, apList[i].authmode,
      vendorSafe, (int)estimateDistance(apList[i].rssi), grade);
  }

  sendJsonFormatted("],\"ble\":[");

  bool first = true;
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].isActive) {
      if (!first) sendJsonFormatted(",");
      first = false;

      char nameSafe[193];
      escapeJson(bleDevices[i].name, nameSafe, sizeof(nameSafe));
      sendJsonFormatted(
        "{\"name\":\"%s\",\"addr\":\"%s\",\"rssi\":%d,\"type\":%d,\"hasName\":%s}",
        nameSafe, bleDevices[i].address, bleDevices[i].rssi, bleDevices[i].advType,
        bleDevices[i].hasName ? "true" : "false");
    }
  }

  sendJsonFormatted("],\"graph\":[");

  for (int i = 0; i < WALK_HISTORY_SIZE; i++) {
    if (i > 0) sendJsonFormatted(",");
    sendJsonFormatted("%d", walkTest.rssiHistory[i]);
  }

  sendJsonFormatted("],\"events\":[");

  for (int i = 0; i < eventCount; i++) {
    if (i > 0) sendJsonFormatted(",");

    char msgSafe[241];
    escapeJson(eventLog[i].message, msgSafe, sizeof(msgSafe));

    sendJsonFormatted("{\"type\":%d,\"msg\":\"%s\",\"ts\":%lu}",
      eventLog[i].type, msgSafe, eventLog[i].timestamp);
  }

  sendJsonFormatted("],\"rogue\":[");
  for (int i = 0; i < rogueCount; i++) {
    if (i > 0) sendJsonFormatted(",");

    char bssid1Str[18], bssid2Str[18];
    snprintf(bssid1Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid1[0], rogueList[i].bssid1[1], rogueList[i].bssid1[2],
      rogueList[i].bssid1[3], rogueList[i].bssid1[4], rogueList[i].bssid1[5]);
    snprintf(bssid2Str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      rogueList[i].bssid2[0], rogueList[i].bssid2[1], rogueList[i].bssid2[2],
      rogueList[i].bssid2[3], rogueList[i].bssid2[4], rogueList[i].bssid2[5]);

    char ssidSafe[193];
    escapeJson(rogueList[i].ssid, ssidSafe, sizeof(ssidSafe));

    sendJsonFormatted("{\"ssid\":\"%s\",\"bssid1\":\"%s\",\"bssid2\":\"%s\"}",
      ssidSafe, bssid1Str, bssid2Str);
  }

  // Hidden SSIDs
  sendJsonFormatted("],\"hidden\":[");
  for (int i = 0; i < hiddenSnapshotCount; i++) {
    if (i > 0) sendJsonFormatted(",");
    char hSafe[193];
    escapeJson(hiddenSnapshot[i].ssid, hSafe, sizeof(hSafe));
    sendJsonFormatted("{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d}", hSafe, hiddenSnapshot[i].rssi, hiddenSnapshot[i].channel);
  }

  // Session stats
  sendJsonFormatted("],\"session\":{");
  sendJsonFormatted("\"uptime\":%lu,\"totalAPs\":%lu,\"peakPPS\":%lu,\"totalPkts\":%lu,\"totalDeauth\":%lu,\"deauthPS\":%lu,\"attackActive\":%s,\"deauthCh\":%d",
    (unsigned long)(millis() - sessionStart), (unsigned long)totalAPsFound,
    (unsigned long)peakPPS, (unsigned long)totalPackets,
    (unsigned long)totalDeauthDetected, (unsigned long)deauthPerSecond,
    attackActive ? "true" : "false", (int)deauthChannel);

  // Security breakdown
  sendJsonFormatted("},\"security\":{");
  sendJsonFormatted("\"open\":%d,\"wep\":%d,\"wpa\":%d,\"wpa2\":%d,\"wpa3\":%d",
    secOpen, secWEP, secWPA, secWPA2, secWPA3);

  // RF Health
  sendJsonFormatted("},\"rfHealth\":{");
  uint8_t rfLoad = liveLoad();
  sendJsonFormatted("\"load\":%d,\"quality\":\"%s\",\"insight\":\"%s\",\"bestCh\":%d,\"worstCh\":%d",
    rfLoad, loadQuality(rfLoad), channelInsight(), bestChannel(), worstChannel());

  // Walk test status
  sendJsonFormatted("},\"walkActive\":%s",
    walkTest.active ? "true" : "false");

  // Baseline data
  sendJsonFormatted(",\"baseline\":{");
  sendJsonFormatted("\"saved\":%s,\"ts\":%lu,\"bAPs\":%d,\"bRSSI\":%d,\"bPkts\":%lu,\"cAPs\":%d,\"cRSSI\":%d,\"cPkts\":%lu",
    baseline.saved ? "true" : "false",
    baseline.saved ? (unsigned long)baseline.timestamp : 0UL,
    baseline.totalAPs, baseline.avgRSSI, (unsigned long)baseline.totalPackets,
    currentSnapshot.totalAPs, currentSnapshot.avgRSSI, (unsigned long)currentSnapshot.totalPackets);

  // Channel distributions
  sendJsonFormatted(",\"chDist\":[");
  for (int i = 0; i < 13; i++) {
    if (i > 0) sendJsonFormatted(",");
    sendJsonFormatted("%d", currentSnapshot.channelDist[i]);
  }
  sendJsonFormatted("],\"chDistB\":[");
  for (int i = 0; i < 13; i++) {
    if (i > 0) sendJsonFormatted(",");
    sendJsonFormatted("%d", baseline.channelDist[i]);
  }
  sendJsonFormatted("]}}");

  server.sendContent("");
}

void handlePacketData() {
  char buffer[1024];

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
  static PacketLog snapshot[MAX_PKT_LOG];
  bool resumeCapture = snifferActive;
  if (resumeCapture) esp_wifi_set_promiscuous(false);
  memcpy(snapshot, pktLogBuffer, sizeof(snapshot));
  uint8_t snapshotIndex = pktLogIndex;
  if (resumeCapture) esp_wifi_set_promiscuous(true);
  int count = 0;
  for (int i = 0; i < MAX_PKT_LOG && count < MAX_PKT_LOG; i++) {
    int idx = (snapshotIndex - 1 - i + MAX_PKT_LOG) % MAX_PKT_LOG;
    if (!snapshot[idx].active) continue;
    if (count > 0) server.sendContent(",");

    char bssidStr[18];
    snprintf(bssidStr, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
      snapshot[idx].bssid[0], snapshot[idx].bssid[1], snapshot[idx].bssid[2],
      snapshot[idx].bssid[3], snapshot[idx].bssid[4], snapshot[idx].bssid[5]);

    char ssidSafe[193];
    escapeJson(snapshot[idx].ssid, ssidSafe, sizeof(ssidSafe));

    const char* typeStr = "OTHER";
    if (snapshot[idx].type == 1) typeStr = "DATA";
    else if (snapshot[idx].type == 2) typeStr = "DEAUTH";
    else if (snapshot[idx].type == 3) typeStr = "BEACON";
    else if (snapshot[idx].type == 4) typeStr = "PROBE";
    else if (snapshot[idx].type == 5) typeStr = "EAPOL";

    snprintf(buffer, sizeof(buffer),
      "{\"ts\":%lu,\"type\":\"%s\",\"ch\":%d,\"rssi\":%d,\"bssid\":\"%s\",\"ssid\":\"%s\"}",
      snapshot[idx].timestamp, typeStr, snapshot[idx].channel,
      snapshot[idx].rssi, bssidStr, ssidSafe);
    server.sendContent(buffer);
    count++;
  }

  server.sendContent("]}");
  server.sendContent("");
}

void handleCaptureData() {
  static ProbeRecord probes[PROBE_HISTORY_CAPACITY];
  CaptureSummary summary;
  snapshotProbes(probes, summary);
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  sendJsonFormatted("{\"listening\":%s,\"scanning\":%s,\"recording\":%s,\"frames\":%u,\"capacity\":%u,\"snaplen\":%u,\"dropped\":%lu,\"truncated\":%lu,\"eapol\":%lu,\"probeRequests\":%lu,\"evicted\":%lu,\"probes\":[",
    snifferActive || deviceMonitorActive ? "true" : "false", webScanInProgress ? "true" : "false",
    summary.recording ? "true" : "false", summary.frameCount, (unsigned)PCAP_CAPACITY,
    (unsigned)(RADIOTAP_BYTES + PCAP_FRAME_BYTES), (unsigned long)summary.dropped,
    (unsigned long)summary.truncated, (unsigned long)summary.eapol,
    (unsigned long)summary.probeRequests, (unsigned long)summary.probeEvictions);
  for (size_t i = 0; i < summary.probeCount; ++i) {
    if (i) server.sendContent(",");
    char name[193], mac[18];
    escapeJsonSpan((const char*)probes[i].ssid, probes[i].ssidLength, name, sizeof(name));
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", probes[i].source[0],
      probes[i].source[1], probes[i].source[2], probes[i].source[3], probes[i].source[4], probes[i].source[5]);
    sendJsonFormatted("{\"ssid\":\"%s\",\"source\":\"%s\",\"wildcard\":%s,\"ch\":%u,\"rssi\":%d,\"firstMs\":%llu,\"lastMs\":%llu,\"count\":%lu}",
      name, mac, probes[i].ssidLength == 0 ? "true" : "false", probes[i].channel, probes[i].rssi,
      (unsigned long long)(probes[i].firstSeenUs / 1000), (unsigned long long)(probes[i].lastSeenUs / 1000),
      (unsigned long)probes[i].count);
  }
  server.sendContent("],\"eapolByChannel\":[");
  for (int ch = 1; ch <= MAX_CHANNEL; ++ch) {
    if (ch > 1) server.sendContent(",");
    sendJsonFormatted("%lu", (unsigned long)summary.eapolByChannel[ch]);
  }
  server.sendContent("]}");
  server.sendContent("");
}

void handlePcapDownload() {
  // Freeze the raw buffer before sizing it; history/EAPOL observations continue.
  stopPcapCapture();
  CaptureSummary summary = getCaptureSummary();
  if (!summary.frameCount) { server.send(409,"text/plain","No PCAP packets yet. Start PCAP and wait for traffic."); return; }
  CapturedFrame frame;
  size_t length = 24;
  for (size_t i = 0; i < summary.frameCount; ++i) {
    if (getCapturedFrame(i, frame)) length += 16 + RADIOTAP_BYTES + frame.capturedLength;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=esp32-capture.pcap");
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Capture-Dropped", String(summary.dropped));
  server.sendHeader("X-Capture-Truncated", String(summary.truncated));
  server.setContentLength(length);
  server.send(200, "application/vnd.tcpdump.pcap", "");
  uint8_t bytes[16 + RADIOTAP_BYTES + PCAP_FRAME_BYTES];
  writePcapHeader(bytes);
  server.sendContent((const char*)bytes, 24);
  for (size_t i = 0; i < summary.frameCount && server.client().connected(); ++i) {
    if (getCapturedFrame(i, frame)) server.sendContent((const char*)bytes, writePcapRecord(frame, bytes));
  }
}

static String lanIP(uint32_t ip) {
  return IPAddress((uint8_t)(ip >> 24), (uint8_t)(ip >> 16), (uint8_t)(ip >> 8), (uint8_t)ip).toString();
}
void handleLanData() {
  const LanStatus& state = getLanStatus();
  const LanEngine& scan = state.scan;
  const char* connections[] = {"disconnected","connecting","connected","error"};
  const char* jobs[] = {"none","discovery","ports","identify"};
  const char* progress[] = {"idle","running","done","cancelled","failed"};
  const char* portStates[] = {"not tested","open","closed","timed out","error"};
  char ssid[193]; escapeJson(state.ssid, ssid, sizeof(ssid));
  uint32_t first = 0, last = 0;
  if (scan.context.valid()) scan.context.defaultRange(first,last);
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); server.send(200,"application/json","");
  sendJsonFormatted("{\"connection\":\"%s\",\"ssid\":\"%s\",\"message\":\"%s\",\"ip\":\"%s\",\"mask\":\"%s\",\"first\":\"%s\",\"last\":\"%s\",\"target\":\"%s\",\"job\":\"%s\",\"progress\":\"%s\",\"completed\":%u,\"total\":%u,\"omitted\":%u,\"errors\":%u,\"hosts\":[",
    connections[(unsigned)state.connection], ssid, state.message, lanIP(scan.context.ip).c_str(),
    lanIP(scan.context.mask).c_str(), lanIP(first).c_str(), lanIP(last).c_str(), lanIP(scan.target).c_str(),
    jobs[(unsigned)scan.job], progress[(unsigned)scan.progress], scan.completed, scan.total, scan.omitted, scan.errors);
  for (size_t i = 0; i < scan.hostCount; ++i) {
    if (i) server.sendContent(",");
    char mac[18], vendor[193];
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",scan.hosts[i].mac[0],scan.hosts[i].mac[1],scan.hosts[i].mac[2],scan.hosts[i].mac[3],scan.hosts[i].mac[4],scan.hosts[i].mac[5]);
    escapeJson(getVendor((uint8_t*)scan.hosts[i].mac),vendor,sizeof(vendor));
    sendJsonFormatted("{\"ip\":\"%s\",\"mac\":\"%s\",\"vendor\":\"%s\"}",lanIP(scan.hosts[i].ip).c_str(),mac,vendor);
  }
  sendJsonFormatted("],\"portTarget\":\"%s\",\"ports\":[",lanIP(state.portTarget).c_str());
  for (size_t i = 0; i < state.portCount; ++i) {
    if (i) server.sendContent(",");
    sendJsonFormatted("{\"port\":%u,\"state\":\"%s\"}",scan.ports[i].port,portStates[(unsigned)scan.ports[i].state]);
  }
  server.sendContent("],\"identity\":{");
  const LanIdentity& identity = state.identity;
  const char* states[] = {"idle","running","done","cancelled","timed out","error"};
  char escaped[583];
  sendJsonFormatted("\"target\":\"%s\",\"port\":%u,\"state\":\"%s\",\"status\":%u,\"limited\":%s,\"headersComplete\":%s,\"encoded\":%s,\"chunked\":%s,\"category\":\"%s\"", lanIP(identity.target).c_str(), identity.port, states[(unsigned)identity.state], identity.statusCode, identity.limited ? "true" : "false", identity.headersComplete ? "true" : "false", identity.encoded ? "true" : "false", identity.chunked ? "true" : "false", identity.category);
  escapeJson(identity.server,escaped,sizeof(escaped)); sendJsonFormatted(",\"server\":\"%s\"",escaped);
  escapeJson(identity.title,escaped,sizeof(escaped)); sendJsonFormatted(",\"title\":\"%s\"",escaped);
  escapeJson(identity.contentType,escaped,sizeof(escaped)); sendJsonFormatted(",\"contentType\":\"%s\"",escaped);
  server.sendContent("}}"); server.sendContent("");
}
void handleLanAction() {
  String action = server.arg("type");
  if (action == "connect") {
    if (webScanInProgress) { server.send(409,"text/plain","Wait for the WiFi scan to finish."); return; }
    String ssid = server.arg("ssid"), password = server.arg("password");
    if (ssid.length() < 1 || ssid.length() > 32 || strlen(ssid.c_str()) != ssid.length() ||
        strlen(password.c_str()) != password.length() || (password.length() && (password.length() < 8 || password.length() > 63))) {
      server.send(400,"text/plain","SSID must be 1-32 bytes; password must be empty or 8-63 bytes."); return;
    }
    stopAllWifi(); stopBLEScan(); walkTest.active = false; triggerWifiScan = triggerBleScan = false;
    if (!connectLan(ssid.c_str(),password.c_str())) { server.send(503,"text/plain","LAN connection could not start."); return; }
    server.send(202,"text/plain","Connecting. If the device WiFi briefly drops, reconnect to the device access point and reopen this page.");
  } else if (action == "disconnect") {
    disconnectLan(); server.send(200,"text/plain","LAN disconnected; scan sockets closed and WiFi credentials cleared from RAM.");
  } else if (action == "cancel") {
    cancelLanScan(); server.send(200,"text/plain","LAN scan cancelled; completed results retained.");
  } else if (action == "discover") {
    uint32_t first, last;
    if (!parseLanIP(server.arg("first").c_str(),first) || !parseLanIP(server.arg("last").c_str(),last)) {
      server.send(400,"text/plain","Enter valid start and end IPv4 addresses."); return;
    }
    if (!startLanDiscovery(first,last)) { server.send(409,"text/plain","Connect first, finish/cancel the current scan, and select up to 254 LAN addresses outside the device AP subnet."); return; }
    server.send(202,"text/plain","ARP discovery started.");
  } else if (action == "ports") {
    uint32_t target; LanPortList ports;
    if (server.arg("ports").length() > 256 || !parseLanIP(server.arg("target").c_str(),target) || !parseLanPorts(server.arg("ports").c_str(),ports)) {
      server.send(400,"text/plain","Enter one IPv4 target and up to 32 TCP ports (commas or short ranges)."); return;
    }
    if (!startLanPortScan(target,ports)) { server.send(409,"text/plain","Connect first, finish/cancel the current scan, and choose a host in the connected LAN outside the device AP subnet."); return; }
    server.send(202,"text/plain","TCP port scan started.");
  } else if (action == "identify") {
    uint32_t target; LanPortList ports;
    String targetText = server.arg("target"), portText = server.arg("port");
    if (targetText.length() > 15 || strlen(targetText.c_str()) != targetText.length() || portText.length() > 5 || strlen(portText.c_str()) != portText.length() || !parseLanIP(targetText.c_str(), target) || !parseLanPorts(portText.c_str(), ports) || ports.count != 1) {
      server.send(400,"text/plain","Enter one LAN IPv4 address and one supported HTTP port."); return;
    }
    if (!startLanIdentification(target,ports.ports[0])) { server.send(409,"text/plain","Connect first, finish/cancel current work, and use a local host on HTTP port 80, 8080, 8000, 8888, 3000, 5000 or 7001."); return; }
    server.send(202,"text/plain","HTTP identification started; no redirects or authentication requests will be followed.");
  } else server.send(400,"text/plain","Unknown LAN action.");
}

void handleMonitorData() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  char buffer[512];
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

    char nameSafe[193];
    escapeJson(monitoredDevices[i].name, nameSafe, sizeof(nameSafe));

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

  stopAllWifi();
  triggerWifiScan = triggerBleScan = webScanInProgress = false;
  webLastWifiScan = 0;

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

  static bool routesRegistered = false;
  if (!routesRegistered) {
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
  server.on("/api/capture", HTTP_GET, handleCaptureData);
  server.on("/api/capture.pcap", HTTP_GET, handlePcapDownload);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/settings", handleSettings);
  server.on("/api/lan", HTTP_GET, handleLanData);
  server.on("/api/lan/action", HTTP_POST, handleLanAction);
  routesRegistered = true;
  }

  server.begin();
  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  disconnectLan();
  esp_wifi_scan_stop();
  WiFi.scanDelete();
  webScanInProgress = triggerWifiScan = triggerBleScan = false;
  walkTest.active = false;
  stopBLEScan();
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
  updateLanNetwork();

  static uint32_t lastBleScan = 0;
  uint32_t now = millis();

  if (triggerWifiScan && !webScanInProgress && !lanModeActive()) {
    triggerWifiScan = false;
    if (snifferActive || deviceMonitorActive) esp_wifi_set_promiscuous(false);
    WiFi.scanDelete();
    int16_t result = WiFi.scanNetworks(true, true, false, 100);
    webScanInProgress = result == WIFI_SCAN_RUNNING || result >= 0;
    webLastWifiScan = now;
    if (!webScanInProgress && (snifferActive || deviceMonitorActive)) {
      esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
      esp_wifi_set_promiscuous(true);
    }
  }
  if (webScanInProgress && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    fetchApResults(true);
    webScanInProgress = false;
    webLastWifiScan = now;
    if (snifferActive || deviceMonitorActive) {
      esp_wifi_set_promiscuous_rx_cb(deviceMonitorActive ? deviceMonitorSniffer : sniffer);
      esp_wifi_set_promiscuous(true);
    }
  }
  if (snifferActive && !webScanInProgress) updatePPS();

  // Auto-scan every 15 seconds if no recent manual scan and not in sniffer/monitor mode
  if (!lanModeActive() && !webScanInProgress && now - webLastWifiScan > 15000 && !snifferActive && !deviceMonitorActive) {
    triggerWifiScan = true;
  }

  if (triggerBleScan && !lanModeActive()) {
    triggerBleScan = false;
    startBLEScan();
    lastBleScan = now;
  }

  if (bleScanning) {
    updateBLEScan();
  }

  if (!lanModeActive() && !bleScanning && now - lastBleScan > 5000) {
    startBLEScan();
    lastBleScan = now;
  }

  if (deviceMonitorActive) updateDeviceMonitor();

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
