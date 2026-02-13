#include "wifi_scanner.h"
#include "security.h"
#include "utils.h"

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

void initWiFi() {
  nvs_flash_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
}

static bool snifferRunning = false;

void stopAllWifi() {
  esp_wifi_scan_stop();
  esp_wifi_set_promiscuous(false);
  extern bool snifferActive;
  extern uint8_t snifferChannel;
  snifferActive = false;
  snifferChannel = 0;
  snifferRunning = false;
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
  pktTotal = pktBeacon = pktData = pktDeauth = pktProbe = 0;
  rssiAccum = rssiCount = 0;
  pps = peak = lastPkt = 0;
  smoothPps = 0;
  memset(history, 0, sizeof(history));
  histIdx = 0;
  scanState.lastSecond = millis();
}

void resetAnalyzer() {
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
  loggedPackets = 0;
}

void startApScan() {
  wifi_scan_config_t cfg = {};
  cfg.show_hidden = true;
  esp_wifi_scan_start(&cfg, false);
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
  esp_wifi_scan_get_ap_num(&apCount);
  apCount = min(apCount, (uint16_t)MAX_APS);
  esp_wifi_scan_get_ap_records(&apCount, apList);
  totalAPsFound = max(totalAPsFound, (uint32_t)apCount);

  secOpen = secWEP = secWPA = secWPA2 = secWPA3 = 0;
  for (int i = 0; i < apCount; i++) {
    switch (apList[i].authmode) {
      case WIFI_AUTH_OPEN: secOpen++; break;
      case WIFI_AUTH_WEP: secWEP++; break;
      case WIFI_AUTH_WPA_PSK: secWPA++; break;
      case WIFI_AUTH_WPA2_PSK:
      case WIFI_AUTH_WPA_WPA2_PSK: secWPA2++; break;
      case WIFI_AUTH_WPA3_PSK: secWPA3++; break;
    }
  }

  detectRogueAPs();

  if (!apSortedOnce || forceSort) {
    sortApsByRssi();
    apCursor = 0;
    apScroll = 0;
    apSortedOnce = true;
  }
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
  // chPackets = total (includes beacons, data, probes, deauth, other)
  // Weight: data/probes/other at 1x, beacons at 0.5x, deauth at 3x
  uint32_t other = chPackets[ch] - chBeacons[ch] - chDeauth[ch];
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
    if (s < bestScore && chPackets[ch] > 0) {
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
  const wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;

  uint8_t ch = p->rx_ctrl.channel;

  // Filter out own AP traffic in web-server sniffer mode
  if (snifferActive && ownApMac[0] != 0 && p->rx_ctrl.sig_len >= 16) {
    const uint8_t* addr1 = (const uint8_t*)&p->payload[4];
    const uint8_t* addr2 = (const uint8_t*)&p->payload[10];
    if (memcmp(addr2, ownApMac, 6) == 0 || memcmp(addr1, ownApMac, 6) == 0) return;
  }

  pktTotal++;
  totalPackets++;
  rssiAccum += p->rx_ctrl.rssi;
  rssiCount++;
  
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
        pktType = 3;

        if (p->rx_ctrl.sig_len > 38) {
          uint16_t offset = 36;
          while (offset < p->rx_ctrl.sig_len - 2) {
            uint8_t elemId = p->payload[offset];
            uint8_t elemLen = p->payload[offset + 1];
            if (elemId == 0 && elemLen <= 32 && offset + 2 + elemLen < p->rx_ctrl.sig_len) {
              memcpy(ssidStr, &p->payload[offset + 2], elemLen);
              ssidStr[elemLen] = 0;
              break;
            }
            offset += 2 + elemLen;
            if (elemLen == 0) break;
          }
        }
      } else if (st == 0x04 || st == 0x05) { // Probe request or response
        pktProbe++;
        pktType = 4;
        if (st == 0x04 && p->payload && p->rx_ctrl.sig_len > 26) {
          uint8_t ssidLen = p->payload[25];
          if (ssidLen > 0 && ssidLen <= 32) {
            memcpy(ssidStr, &p->payload[26], ssidLen);
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
        totalDeauthDetected++;
        deauthChannel = ch;
        pktType = 2;
      } else {
        pktType = 0;
      }
    }
  } else if (type == WIFI_PKT_DATA) {
    pktData++;
    pktType = 1;
  }

  if (pktTotal % 5 == 0) {
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
    Serial.printf("%lu,%d,%d,%d\n", millis(), ch, p->rx_ctrl.rssi, type);
  }
}
