#ifndef DEVICE_MONITOR_H
#define DEVICE_MONITOR_H

#include "config.h"
#include <esp_wifi.h>

extern bool deviceMonitorActive;
extern bool webMonitorMode;

void IRAM_ATTR deviceMonitorSniffer(void* buf, wifi_promiscuous_pkt_type_t type);
void updateDeviceMonitor();
void addOrUpdateWiFiClient(const uint8_t* mac, int8_t rssi, uint8_t channel);
void addOrUpdateBLEDevice(const char* address, const char* name, int8_t rssi);
void checkDeviceTimeouts();
void clearDeviceMonitor();
void startDeviceMonitorSniffer();

#endif // DEVICE_MONITOR_H
