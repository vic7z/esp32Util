#include "device_monitor.h"
#include "wifi_scanner.h"
#include "ble_scanner.h"
#include "utils.h"
#include <string.h>

void IRAM_ATTR deviceMonitorSniffer(void* buf, wifi_promiscuous_pkt_type_t type);

static bool deviceMonitorActive = false;
static uint8_t monitorChannel = 1;
static uint32_t lastChannelHop = 0;

void addOrUpdateWiFiClient(const uint8_t* mac, int8_t rssi, uint8_t channel) {
  uint32_t now = millis();

  if (mac[0] & 0x01) return;

  bool allZero = true, allFF = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0x00) allZero = false;
    if (mac[i] != 0xFF) allFF = false;
  }
  if (allZero || allFF) return;

  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    if (monitoredDevices[i].active &&
        monitoredDevices[i].type == 0 &&
        memcmp(monitoredDevices[i].bssid, mac, 6) == 0) {
      monitoredDevices[i].rssi = rssi;
      monitoredDevices[i].channel = channel;
      monitoredDevices[i].lastSeen = now;
      monitoredDevices[i].seenCount++;
      monitoredDevices[i].isPresent = true;
      return;
    }
  }

  if (monitoredDeviceCount < MAX_MONITORED_DEVICES) {
    for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
      if (!monitoredDevices[i].active) {
        monitoredDevices[i].type = 0;
        memcpy(monitoredDevices[i].bssid, mac, 6);

        const char* vendor = getVendor((uint8_t*)mac);
        if (vendor && strlen(vendor) > 0) {
          strncpy(monitoredDevices[i].name, vendor, 32);
        } else {
          sprintf(monitoredDevices[i].name, "%02X:%02X:%02X", mac[0], mac[1], mac[2]);
        }
        monitoredDevices[i].name[32] = '\0';

        monitoredDevices[i].rssi = rssi;
        monitoredDevices[i].channel = channel;
        monitoredDevices[i].firstSeen = now;
        monitoredDevices[i].lastSeen = now;
        monitoredDevices[i].seenCount = 1;
        monitoredDevices[i].isPresent = true;
        monitoredDevices[i].active = true;
        monitoredDeviceCount++;
        return;
      }
    }
  }
}

void addOrUpdateBLEDevice(const char* address, const char* name, int8_t rssi) {
  uint32_t now = millis();

  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    if (monitoredDevices[i].active &&
        monitoredDevices[i].type == 1 &&
        strcmp(monitoredDevices[i].bleAddr, address) == 0) {
      monitoredDevices[i].rssi = rssi;
      monitoredDevices[i].lastSeen = now;
      monitoredDevices[i].seenCount++;
      monitoredDevices[i].isPresent = true;

      if (name && strlen(name) > 0) {
        strncpy(monitoredDevices[i].name, name, 32);
        monitoredDevices[i].name[32] = '\0';
      }
      return;
    }
  }

  if (monitoredDeviceCount < MAX_MONITORED_DEVICES) {
    for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
      if (!monitoredDevices[i].active) {
        monitoredDevices[i].type = 1;
        strncpy(monitoredDevices[i].bleAddr, address, 17);
        monitoredDevices[i].bleAddr[17] = '\0';

        if (name && strlen(name) > 0) {
          strncpy(monitoredDevices[i].name, name, 32);
        } else {
          strcpy(monitoredDevices[i].name, "Unknown");
        }
        monitoredDevices[i].name[32] = '\0';

        monitoredDevices[i].rssi = rssi;
        monitoredDevices[i].channel = 0;
        monitoredDevices[i].firstSeen = now;
        monitoredDevices[i].lastSeen = now;
        monitoredDevices[i].seenCount = 1;
        monitoredDevices[i].isPresent = true;
        monitoredDevices[i].active = true;
        monitoredDeviceCount++;
        return;
      }
    }
  }
}

void checkDeviceTimeouts() {
  uint32_t now = millis();

  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    if (monitoredDevices[i].active && monitoredDevices[i].isPresent) {
      if (now - monitoredDevices[i].lastSeen > DEVICE_TIMEOUT_MS) {
        monitoredDevices[i].isPresent = false;
      }
    }
  }
}

void IRAM_ATTR deviceMonitorSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;

  if (p->rx_ctrl.sig_len < 24) return;

  const uint8_t* frame = p->payload;
  uint8_t frameType = (frame[0] >> 2) & 0x03;
  uint8_t frameSubtype = (frame[0] >> 4) & 0x0F;

  const uint8_t* addr1 = &frame[4];
  const uint8_t* addr2 = &frame[10];

  int8_t rssi = p->rx_ctrl.rssi;
  uint8_t channel = p->rx_ctrl.channel;

  if (frameType == 0) {
    if (frameSubtype == 4 || frameSubtype == 0 || frameSubtype == 2) {
      addOrUpdateWiFiClient(addr2, rssi, channel);
    }
  }
  else if (frameType == 2) {
    uint8_t toDS = (frame[1] >> 0) & 0x01;
    uint8_t fromDS = (frame[1] >> 1) & 0x01;

    if (toDS && !fromDS) {
      addOrUpdateWiFiClient(addr2, rssi, channel);
    }
    else if (!toDS && fromDS) {
      addOrUpdateWiFiClient(addr1, rssi, channel);
    }
  }
}

void startDeviceMonitorSniffer() {
  deviceMonitorActive = true;
  monitorChannel = 1;
  lastChannelHop = millis();

  esp_wifi_scan_stop();
  esp_wifi_set_promiscuous(false);

  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_channel(monitorChannel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(deviceMonitorSniffer);
  esp_wifi_set_promiscuous(true);
}

void updateDeviceMonitor() {
  if (deviceMonitorActive && millis() - lastChannelHop > 300) {
    monitorChannel = (monitorChannel % 13) + 1;
    esp_wifi_set_channel(monitorChannel, WIFI_SECOND_CHAN_NONE);
    lastChannelHop = millis();
  }

  extern uint8_t bleDeviceCount;
  extern BLEDeviceInfo bleDevices[];
  for (int i = 0; i < bleDeviceCount; i++) {
    if (bleDevices[i].isActive) {
      addOrUpdateBLEDevice(
        bleDevices[i].address.c_str(),
        bleDevices[i].name.c_str(),
        bleDevices[i].rssi
      );
    }
  }

  checkDeviceTimeouts();
}

void clearDeviceMonitor() {
  deviceMonitorActive = false;
  for (int i = 0; i < MAX_MONITORED_DEVICES; i++) {
    monitoredDevices[i].active = false;
    monitoredDevices[i].isPresent = false;
    monitoredDevices[i].seenCount = 0;
  }
  monitoredDeviceCount = 0;
  deviceCursor = 0;
  deviceScroll = 0;
}
