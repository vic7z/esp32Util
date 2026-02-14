#include "ble_scanner.h"
#include "wifi_scanner.h"
#include "utils.h"

BLEDeviceInfo bleDevices[MAX_BLE_DEVICES];
uint8_t bleDeviceCount = 0;
uint8_t bleCursor = 0;
uint8_t bleScroll = 0;
uint8_t bleSelectedIndex = 0;
bool bleScanning = false;
bool bleInitialized = false;
uint32_t bleScanStart = 0;
BLEScan* pBLEScan = nullptr;

void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
}

void initBLE() {
  if (!bleInitialized) {
    BLEDevice::init("ESP32_Analyzer");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    pBLEScan->setDuplicateFilter(false);
    bleInitialized = true;
  }
}

void startBLEScan() {
  if (bleInitialized && !bleScanning) {
    bleScanning = true;
    bleScanStart = millis();
    scanState.lastBLELoop = 0;
  }
}

void stopBLEScan() {
  if (bleScanning && pBLEScan != nullptr) {
    pBLEScan->stop();
    pBLEScan->clearResults();
    bleScanning = false;
  }
}

void updateBLEScan() {
  if (!bleInitialized || !bleScanning || pBLEScan == nullptr) {
    return;
  }

  uint32_t now = millis();
  
  if (now - scanState.lastBLELoop > BLE_SCAN_INTERVAL_MS) {
    if (!pBLEScan->isScanning()) {
      pBLEScan->clearResults();
      BLEScanResults* results = pBLEScan->start(1, false);

      if (results != nullptr) {
        int count = results->getCount();
        for (int i = 0; i < count && bleDeviceCount < MAX_BLE_DEVICES; i++) {
          BLEAdvertisedDevice device = results->getDevice(i);
          String addrStr = device.getAddress().toString();
          char addr[18];
          strncpy(addr, addrStr.c_str(), 17);
          addr[17] = '\0';
          
          bool found = false;
          for (int j = 0; j < bleDeviceCount; j++) {
            if (strcmp(bleDevices[j].address, addr) == 0) {
              bleDevices[j].rssi = (int8_t)device.getRSSI();
              bleDevices[j].lastSeen = now;
              bleDevices[j].isActive = true;
              if (device.haveName()) {
                strncpy(bleDevices[j].name, device.getName().c_str(), 32);
                bleDevices[j].name[32] = '\0';
                bleDevices[j].hasName = true;
              } else if (!bleDevices[j].hasName && bleDevices[j].mfgId == 0xFFFF) {
                // Try to get manufacturer ID on re-scan
                if (device.haveManufacturerData()) {
                  String mfgData = device.getManufacturerData();
                  if (mfgData.length() >= 2) {
                    bleDevices[j].mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
                    const char* vendor = getBLEVendor(bleDevices[j].mfgId);
                    if (vendor) {
                      strncpy(bleDevices[j].name, vendor, 32);
                      bleDevices[j].name[32] = '\0';
                    }
                  }
                }
              }
              found = true;
              break;
            }
          }
          
          if (!found && bleDeviceCount < MAX_BLE_DEVICES) {
            strncpy(bleDevices[bleDeviceCount].address, addr, 17);
            bleDevices[bleDeviceCount].address[17] = '\0';
            bleDevices[bleDeviceCount].rssi = (int8_t)device.getRSSI();
            bleDevices[bleDeviceCount].lastSeen = now;
            bleDevices[bleDeviceCount].isActive = true;
            bleDevices[bleDeviceCount].advType = 0;
            bleDevices[bleDeviceCount].mfgId = 0xFFFF;

            // Extract manufacturer company ID from advertisement data
            if (device.haveManufacturerData()) {
              String mfgData = device.getManufacturerData();
              if (mfgData.length() >= 2) {
                // Company ID is first 2 bytes, little-endian
                bleDevices[bleDeviceCount].mfgId =
                  (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
              }
            }

            if (device.haveName()) {
              strncpy(bleDevices[bleDeviceCount].name, device.getName().c_str(), 32);
              bleDevices[bleDeviceCount].name[32] = '\0';
              bleDevices[bleDeviceCount].hasName = true;
            } else {
              // Try to resolve name from BLE company ID
              const char* vendor = getBLEVendor(bleDevices[bleDeviceCount].mfgId);
              if (vendor) {
                strncpy(bleDevices[bleDeviceCount].name, vendor, 32);
                bleDevices[bleDeviceCount].name[32] = '\0';
                bleDevices[bleDeviceCount].hasName = false;  // still mark as no broadcast name
              } else {
                strcpy(bleDevices[bleDeviceCount].name, "Unknown");
                bleDevices[bleDeviceCount].hasName = false;
              }
            }

            bleDeviceCount++;
          }
        }
      }
    }
    scanState.lastBLELoop = now;
  }

  for (int i = 0; i < bleDeviceCount; i++) {
    if (now - bleDevices[i].lastSeen > BLE_TIMEOUT_MS) {
      bleDevices[i].isActive = false;
    }
  }

  cleanupInactiveBLE();
}

void sortBLEByRSSI() {
  for (int i = 0; i < bleDeviceCount - 1; i++) {
    for (int j = 0; j < bleDeviceCount - i - 1; j++) {
      if (bleDevices[j].rssi < bleDevices[j + 1].rssi) {
        BLEDeviceInfo tmp = bleDevices[j];
        bleDevices[j] = bleDevices[j + 1];
        bleDevices[j + 1] = tmp;
      }
    }
  }
}

void cleanupInactiveBLE() {
  uint8_t writeIdx = 0;
  for (uint8_t readIdx = 0; readIdx < bleDeviceCount; readIdx++) {
    if (bleDevices[readIdx].isActive) {
      if (writeIdx != readIdx) {
        bleDevices[writeIdx] = bleDevices[readIdx];
      }
      writeIdx++;
    }
  }
  bleDeviceCount = writeIdx;
}

uint8_t getActiveBLECount() {
  uint8_t count = 0;
  uint8_t maxCheck = (bleDeviceCount <= MAX_BLE_DEVICES) ? bleDeviceCount : MAX_BLE_DEVICES;
  for (int i = 0; i < maxCheck; i++) {
    if (bleDevices[i].isActive) {
      count++;
    }
  }
  return count;
}
