#include "wifi_scanner.h"
#include "security.h"
#include "utils.h"
#include "device_monitor.h"
#include <WiFi.h>
#include "packet_helpers.h"
#include "capture.h"

volatile uint32_t pktTotal = 0, pktBeacon = 0, pktData = 0, pktDeauth = 0, pktProbe = 0;
volatile int32_t rssiAccum = 0;
volatile uint32_t rssiCount = 0;
uint32_t history[HISTORY_SIZE];
uint8_t histIdx = 0;
uint32_t pps = 0, peak = 0, lastPkt = 0;
float smoothPps = 0;
float avgRssi = -80;


uint32_t chPackets[MAX_CHANNEL + 1];
uint32_t chBeacons[MAX_CHANNEL + 1];
uint32_t chDeauth[MAX_CHANNEL + 1];
uint32_t chData[MAX_CHANNEL + 1];
uint32_t chProbe[MAX_CHANNEL + 1];


wifi_ap_record_t apList[MAX_APS];
uint16_t apCount = 0;
uint8_t apCursor = 0;
uint8_t apScroll = 0;
uint8_t apSelectedIndex = 0;
uint8_t apCompareA = 0;
uint8_t apCompareB = 1;

bool apSortedOnce = false;


HiddenSSID hiddenList[MAX_HIDDEN_SSIDS];
uint8_t hiddenCount = 0;
uint8_t hiddenCursor = 0;
uint8_t hiddenScroll = 0;

uint8_t secOpen = 0, secWEP = 0, secWPA = 0, secWPA2 = 0, secWPA3 = 0;

uint32_t sessionStart = 0;
uint32_t totalAPsFound = 0;
uint32_t peakPPS = 0;
uint32_t totalPackets = 0;

uint32_t loggedPackets = 0;
bool loggingActive = false;

PacketLog pktLogBuffer[MAX_PKT_LOG];
uint8_t pktLogIndex = 0;
uint8_t pktLogCount = 0;

uint32_t totalDeauthDetected = 0;
uint32_t deauthPerSecond = 0;
uint32_t lastDeauthCount = 0;
uint32_t lastDeauthCheck = 0;
bool attackActive = false;
uint8_t deauthChannel = 0;

uint32_t beaconPps = 0;
uint32_t deauthPps = 0;
static uint32_t rateLastTime = 0, rateLastTotal = 0, rateLastBeacon = 0, rateLastDeauth = 0;
static portMUX_TYPE rssiMux = portMUX_INITIALIZER_UNLOCKED;

void updatePacketStats() {
  uint32_t now = millis(), elapsed = now - rateLastTime;
  if (elapsed < 1000) return;
  uint32_t total = pktTotal, beacon = pktBeacon, deauth = pktDeauth;
  uint32_t delta = total >= rateLastTotal ? total - rateLastTotal : total;
  pps = (uint32_t)((uint64_t)delta * 1000 / elapsed);
  beaconPps = (uint32_t)((uint64_t)(beacon >= rateLastBeacon ? beacon - rateLastBeacon : beacon) * 1000 / elapsed);
  deauthPps = (uint32_t)((uint64_t)(deauth >= rateLastDeauth ? deauth - rateLastDeauth : deauth) * 1000 / elapsed);
  smoothPps = 0.7f * smoothPps + 0.3f * pps;
  peak = max(peak, pps); peakPPS = max(peakPPS, pps);
  history[histIdx] = pps; histIdx = (histIdx + 1) % HISTORY_SIZE;
  portENTER_CRITICAL(&rssiMux);
  int32_t sum = rssiAccum;
  uint32_t count = rssiCount;
  rssiAccum = 0; rssiCount = 0;
  portEXIT_CRITICAL(&rssiMux);
  avgRssi = count ? (float)sum / count : -100;
  rateLastTime = now; rateLastTotal = total; rateLastBeacon = beacon; rateLastDeauth = deauth;
}

void initWiFi() {
  // Arduino owns driver initialization and scan-complete event handling.
  WiFi.mode(WIFI_STA);
}

static bool snifferRunning = false;

void stopAllWifi() {
  stopPcapCapture();
  esp_wifi_scan_stop();
  esp_wifi_set_promiscuous(false);
  extern bool snifferActive;
  extern uint8_t snifferChannel;
  snifferActive = false;
  snifferChannel = 0;
  snifferRunning = false;
  deviceMonitorActive = false;
  webMonitorMode = false;
}

void switchSnifferChannel(uint8_t ch) {
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void enterSnifferMode(uint8_t ch) {
  // Fast path: already in sniffer mode, just switch channel
  if (snifferRunning) {
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    return;
  }

  stopAllWifi();

  esp_err_t err = esp_wifi_stop();
  delay(150);

  esp_wifi_set_mode(WIFI_MODE_NULL);

  err = esp_wifi_start();
  if (err != ESP_OK) return;

  delay(50);

  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

  esp_wifi_set_promiscuous_rx_cb(sniffer);

  // Explicitly enable all packet types including data frames
  wifi_promiscuous_filter_t filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL
  };
  esp_wifi_set_promiscuous_filter(&filter);

  esp_wifi_set_promiscuous(true);
  snifferRunning = true;
}

void enterScanMode() {
  stopAllWifi();  // also resets snifferRunning
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
}

void resetLiveStats() {
  stopAllWifi();
  pktTotal = pktBeacon = pktData = pktDeauth = pktProbe = 0;
  rssiAccum = rssiCount = 0;
  pps = peak = lastPkt = 0;
  smoothPps = 0;
  memset(history, 0, sizeof(history));
  histIdx = 0;
  scanState.lastSecond = millis();
  lastDeauthCount = totalDeauthDetected;
  deauthPerSecond = beaconPps = deauthPps = 0;
  attackActive = false;
  rateLastTime = millis();
  rateLastTotal = rateLastBeacon = rateLastDeauth = 0;
}

void resetAnalyzer() {
  stopAllWifi();
  memset(chPackets, 0, sizeof(chPackets));
  memset(chBeacons, 0, sizeof(chBeacons));
  memset(chDeauth, 0, sizeof(chDeauth));
  memset(chData, 0, sizeof(chData));
  memset(chProbe, 0, sizeof(chProbe));
  pktTotal = pktBeacon = pktData = pktDeauth = pktProbe = 0;
  scanState.analyzerChannel = 1;
  scanState.analyzerLastHop = millis();
}


void resetSession() {
  sessionStart = millis();
  totalAPsFound = 0;
  peakPPS = 0;
  totalPackets = 0;
  totalDeauthDetected = 0;
  lastDeauthCount = 0;
  deauthPerSecond = 0;
  lastDeauthCheck = millis();
  loggedPackets = 0;
}

void startApScan() {
  if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true, false, 100);
  }
}

void waitForApScan(uint32_t timeoutMs) {
  uint32_t started = millis();
  while (WiFi.scanComplete() == WIFI_SCAN_RUNNING && millis() - started < timeoutMs) {
    buttonAwareDelay(10);
    if (pendingButton != BTN_NONE) return;
  }
}

void sortApsByRssi() {
  for (int i = 0; i < apCount - 1; i++) {
    for (int j = 0; j < apCount - i - 1; j++) {
      if (apList[j].rssi < apList[j + 1].rssi) {
        wifi_ap_record_t tmp = apList[j];
        apList[j] = apList[j + 1];
        apList[j + 1] = tmp;
      }
    }
  }
}

void fetchApResults(bool forceSort) {
  int16_t count = WiFi.scanComplete();
  if (count < 0) return; // Keep the last complete scan until a new one finishes.
  uint8_t selectedBSSID[6] = {};
  bool hadSelection = apSelectedIndex < apCount;
  if (hadSelection) memcpy(selectedBSSID, apList[apSelectedIndex].bssid, 6);
  apCount = min(count, (int16_t)MAX_APS);
  for (uint16_t i = 0; i < apCount; ++i) {
    apList[i] = {};
    strncpy((char*)apList[i].ssid, WiFi.SSID(i).c_str(), 32);
    apList[i].ssid[32] = '\0';
    memcpy(apList[i].bssid, WiFi.BSSID(i), 6);
    apList[i].rssi = WiFi.RSSI(i);
    apList[i].primary = WiFi.channel(i);
    apList[i].authmode = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  totalAPsFound = max(totalAPsFound, (uint32_t)apCount);

  secOpen = secWEP = secWPA = secWPA2 = secWPA3 = 0;
  for (int i = 0; i < apCount; i++) {
    switch (apList[i].authmode) {
      case WIFI_AUTH_OPEN: secOpen++; break;
      case WIFI_AUTH_WEP: secWEP++; break;
      case WIFI_AUTH_WPA_PSK: secWPA++; break;
      case WIFI_AUTH_WPA2_PSK:
      case WIFI_AUTH_WPA2_ENTERPRISE:
      case WIFI_AUTH_WPA_WPA2_PSK: secWPA2++; break;
      case WIFI_AUTH_WPA3_PSK:
      case WIFI_AUTH_WPA2_WPA3_PSK: secWPA3++; break;
    }
  }

  detectRogueAPs();

  if (!apSortedOnce || forceSort) {
    sortApsByRssi();
    apCursor = 0;
    apScroll = 0;
    apSortedOnce = true;
  }
  apSelectedIndex = 0;
  if (hadSelection) {
    for (uint16_t i = 0; i < apCount; ++i) {
      if (memcmp(selectedBSSID, apList[i].bssid, 6) == 0) { apSelectedIndex = i; break; }
    }
  }
  if (apScroll + apCursor >= apCount) apScroll = apCursor = 0;
}

uint8_t liveLoad() {
  // Use per-second rates, not cumulative totals.  pktBeacon/pktDeauth are
  // cumulative and grow without bound, which previously pegged load at 100%
  // after only a few minutes of sniffing.
  uint32_t score = pps + beaconPps / 2 + deauthPps * 3;
  return min(score / 5, 100UL);
}

const char* channelInsight() {
  if (attackActive) return "ATTACK DETECTED!";
  if (deauthPerSecond > 5) return "Deauth Storm!";
  if (pktDeauth > pktBeacon / 4 && pktDeauth > 5) return "Possible Attack";

  uint8_t load = liveLoad();
  if (load > 80) return "Overloaded";
  if (load > 60) return "Congested";

  if (pktData > pktBeacon * 3) return "Heavy Traffic";
  if (pktData > pktBeacon * 2) return "Active Data";
  if (pktBeacon > pktData * 4) return "Idle/Beacons";

  if (pps < 10) return "Very Clean";
  if (pps < 30) return "Clean";
  if (pps < 100) return "Moderate";

  return "Mixed Traffic";
}

uint8_t channelLoad(uint8_t ch) {
  if (ch < 1 || ch > MAX_CHANNEL) return 0;
  // chPackets = total (includes beacons, data, probes, deauth, other)
  // Weight: data/probes/other at 1x, beacons at 0.5x, deauth at 3x
  uint32_t classified = chBeacons[ch] + chDeauth[ch];
  uint32_t other = chPackets[ch] >= classified ? chPackets[ch] - classified : 0;
  uint32_t score = other + chBeacons[ch] / 2 + chDeauth[ch] * 3;
  return min(score / 5, 100UL);
}

const char* loadQuality(uint8_t load) {
  if (load < LOAD_GOOD) return "GOOD";
  if (load < LOAD_OK) return "OK";
  if (load < LOAD_BUSY) return "BUSY";
  return "AVOID";
}


uint8_t bestChannel() {
  uint8_t best = 1;
  uint8_t bestScore = 255;
  for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
    uint8_t s = channelLoad(ch);
    if (s < bestScore) {
      bestScore = s;
      best = ch;
    }
  }
  return best;
}

uint8_t worstChannel() {
  uint8_t worst = 1;
  uint8_t worstScore = 0;
  for (int ch = 1; ch <= MAX_CHANNEL; ch++) {
    uint8_t s = channelLoad(ch);
    if (s > worstScore && chPackets[ch] > 0) {
      worstScore = s;
      worst = ch;
    }
  }
  return worst;
}

uint8_t bestAPIndex() {
  uint8_t bestIdx = 0;
  char bestGrade = 'F';
  for (int i = 0; i < apCount; i++) {
    char grade = getQualityGrade(&apList[i]);
    if (grade < bestGrade || (grade == bestGrade && apList[i].rssi > apList[bestIdx].rssi)) {
      bestGrade = grade;
      bestIdx = i;
    }
  }
  return bestIdx;
}

extern uint8_t snifferChannel;
extern bool    snifferActive;
extern uint8_t ownApMac[6];

void IRAM_ATTR sniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!buf || type == WIFI_PKT_MISC) return;
  const wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
  if (p->rx_ctrl.sig_len < 2) return;
  if (type == WIFI_PKT_MGMT && p->rx_ctrl.sig_len < 24) return;

  uint8_t ch = p->rx_ctrl.channel;
  if (p->rx_ctrl.rx_state != 0 || p->rx_ctrl.sig_len < 4) return;

  // Filter out own AP traffic in web-server sniffer mode
  if (snifferActive && p->rx_ctrl.sig_len >= 14) {
    const uint8_t* addr1 = (const uint8_t*)&p->payload[4];
    const uint8_t* addr2 = (const uint8_t*)&p->payload[10];
    if (memcmp(addr1, ownApMac, 6) == 0 ||
        (p->rx_ctrl.sig_len >= 20 && memcmp(addr2, ownApMac, 6) == 0)) return;
  }

  observeCapture(p->payload, p->rx_ctrl.sig_len - 4, ch, p->rx_ctrl.rssi);
  pktTotal++;
  totalPackets++;
  if (ch >= 1 && ch <= MAX_CHANNEL) chPackets[ch]++;
  portENTER_CRITICAL(&rssiMux);
  rssiAccum += p->rx_ctrl.rssi;
  rssiCount++;
  portEXIT_CRITICAL(&rssiMux);
  
  uint8_t st = 0;
  uint8_t pktType = 0;
  char ssidStr[33] = {0};
  uint8_t* bssid = NULL;
  
  if (type == WIFI_PKT_MGMT) {
    if (p->payload) {
      st = (p->payload[0] >> 4) & 0x0F;
      
      if (p->rx_ctrl.sig_len >= 24) {
        bssid = (uint8_t*)&p->payload[16];
      }
      
      if (st == 0x08) { // Beacon
        pktBeacon++;
        if (ch >= 1 && ch <= MAX_CHANNEL) chBeacons[ch]++;
        pktType = 3;

        SSIDElement element = findSSID(p->payload, p->rx_ctrl.sig_len - 4, 36);
        if (element.found) {
          memcpy(ssidStr, p->payload + element.offset, element.length);
          ssidStr[element.length] = 0;
        }
      } else if (st == 0x04 || st == 0x05) { // Probe request or response
        pktProbe++;
        if (ch >= 1 && ch <= MAX_CHANNEL) chProbe[ch]++;
        pktType = 4;
        if (st == 0x04) {
          SSIDElement element = findSSID(p->payload, p->rx_ctrl.sig_len - 4, 24);
          uint8_t ssidLen = element.length;
          if (element.found && ssidLen > 0) {
            memcpy(ssidStr, p->payload + element.offset, ssidLen);
            ssidStr[ssidLen] = 0;

            bool found = false;
            for (int i = 0; i < hiddenCount; i++) {
              if (strcmp(hiddenList[i].ssid, ssidStr) == 0) {
                if (p->rx_ctrl.rssi > hiddenList[i].rssi) hiddenList[i].rssi = p->rx_ctrl.rssi;
                hiddenList[i].active = true;
                found = true;
                break;
              }
            }
            if (!found && hiddenCount < MAX_HIDDEN_SSIDS && strlen(ssidStr) > 0) {
              strcpy(hiddenList[hiddenCount].ssid, ssidStr);
              hiddenList[hiddenCount].rssi = p->rx_ctrl.rssi;
              hiddenList[hiddenCount].channel = ch;
              hiddenList[hiddenCount].active = true;
              hiddenCount++;
            }
          }
        }
      } else if (st == 0x0C || st == 0x0A) { // Deauth
        pktDeauth++;
        if (ch >= 1 && ch <= MAX_CHANNEL) chDeauth[ch]++;
        totalDeauthDetected++;
        deauthChannel = ch;
        pktType = 2;
      } else {
        pktType = 0;
      }
    }
  } else if (type == WIFI_PKT_DATA) {
    pktData++;
    if (ch >= 1 && ch <= MAX_CHANNEL) chData[ch]++;
    pktType = 1;
    if (p->rx_ctrl.sig_len >= 28) {
      uint8_t ds = p->payload[1] & 3;
      bssid = ds == 1 ? (uint8_t*)&p->payload[4] : ds == 2 ? (uint8_t*)&p->payload[10] : ds == 0 ? (uint8_t*)&p->payload[16] : nullptr;
    }
    if (isEapol(p->payload, p->rx_ctrl.sig_len - 4)) pktType = 5;
  }

  if (pktTotal % 5 == 0 || pktType == 5) {
    PacketLog* pl = &pktLogBuffer[pktLogIndex];
    pl->timestamp = millis();
    pl->type = pktType;
    pl->subtype = st;
    pl->channel = ch;
    pl->rssi = p->rx_ctrl.rssi;
    if (bssid) memcpy(pl->bssid, bssid, 6);
    else memset(pl->bssid, 0, 6);
    strncpy(pl->ssid, ssidStr, 32);
    pl->ssid[32] = 0;
    pl->active = true;
    
    pktLogIndex = (pktLogIndex + 1) % MAX_PKT_LOG;
    if (pktLogCount < MAX_PKT_LOG) pktLogCount++;
  }

  if (loggingActive && settings.csvLogging && loggedPackets < 1000) {
    loggedPackets++;
    // Serial output must not block the WiFi receive task.
  }
}
