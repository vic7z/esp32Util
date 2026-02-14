#include "utils.h"
#include "wifi_scanner.h"
#include <SPIFFS.h>

// ── SPIFFS binary vendor lookup (sorted, binary search) ─────────

// Binary file formats (packed, no padding):
//   oui.bin: [3B OUI][12B name] = 15 bytes per entry, sorted by OUI
//   ble.bin: [2B ID big-endian][14B name] = 16 bytes per entry, sorted by ID

#define OUI_ENTRY_SIZE 15
#define BLE_ENTRY_SIZE 16

static uint8_t* ouiData = nullptr;
static uint16_t ouiCount = 0;
static uint8_t* bleData = nullptr;
static uint16_t bleCount = 0;

// Fallback hardcoded table (used if SPIFFS fails)
const Vendor vendors[] = {
  {{0x00, 0x03, 0x93}, "Apple"},
  {{0x00, 0x50, 0xF2}, "Microsoft"},
  {{0x00, 0x1B, 0x63}, "Apple"},
  {{0x3C, 0x22, 0xFB}, "Apple"},
  {{0xDC, 0x2C, 0x6E}, "Apple"},
  {{0x28, 0xE1, 0x4C}, "Samsung"},
  {{0xE8, 0x50, 0x8B}, "Samsung"},
  {{0x34, 0x08, 0xBC}, "Samsung"},
  {{0xD8, 0xEB, 0x97}, "Raspberry"},
  {{0xB8, 0x27, 0xEB}, "Raspberry"},
  {{0xDC, 0xA6, 0x32}, "Raspberry"},
  {{0x18, 0xFE, 0x34}, "Espressif"},
  {{0xAC, 0x67, 0xB2}, "Espressif"},
  {{0x24, 0x0A, 0xC4}, "Espressif"},
  {{0x84, 0xCC, 0xA8}, "Tp-Link"},
  {{0xC0, 0x4A, 0x00}, "Tp-Link"},
  {{0xE8, 0x94, 0xF6}, "Tp-Link"}
};
const uint8_t vendorCount = sizeof(vendors) / sizeof(Vendor);

void loadVendorsFromSPIFFS() {
  // Load WiFi OUI binary table
  File ouiFile = SPIFFS.open("/oui.bin", "r");
  if (ouiFile) {
    size_t sz = ouiFile.size();
    ouiCount = sz / OUI_ENTRY_SIZE;
    if (ouiCount > 0) {
      ouiData = (uint8_t*)malloc(sz);
      if (ouiData) {
        ouiFile.read(ouiData, sz);
        Serial.printf("[VENDOR] Loaded %d WiFi OUIs (binary)\n", ouiCount);
      }
    }
    ouiFile.close();
  } else {
    Serial.println("[VENDOR] oui.bin not found, using fallback");
  }

  // Load BLE company ID binary table
  File bleFile = SPIFFS.open("/ble.bin", "r");
  if (bleFile) {
    size_t sz = bleFile.size();
    bleCount = sz / BLE_ENTRY_SIZE;
    if (bleCount > 0) {
      bleData = (uint8_t*)malloc(sz);
      if (bleData) {
        bleFile.read(bleData, sz);
        Serial.printf("[VENDOR] Loaded %d BLE company IDs (binary)\n", bleCount);
      }
    }
    bleFile.close();
  } else {
    Serial.println("[VENDOR] ble.bin not found");
  }
}

const char* getVendor(uint8_t* mac) {
  // Binary search on sorted OUI table
  if (ouiData && ouiCount > 0) {
    int lo = 0, hi = ouiCount - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      int cmp = memcmp(mac, ouiData + mid * OUI_ENTRY_SIZE, 3);
      if (cmp == 0) return (const char*)(ouiData + mid * OUI_ENTRY_SIZE + 3);
      if (cmp < 0) hi = mid - 1;
      else lo = mid + 1;
    }
  }
  // Fallback to hardcoded table
  for (int i = 0; i < vendorCount; i++) {
    if (memcmp(mac, vendors[i].oui, 3) == 0) {
      return vendors[i].name;
    }
  }
  return "Unknown";
}

const char* getBLEVendor(uint16_t mfgId) {
  if (mfgId == 0xFFFF) return nullptr;
  // Binary search on sorted BLE table
  if (bleData && bleCount > 0) {
    int lo = 0, hi = bleCount - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      uint16_t id = (bleData[mid * BLE_ENTRY_SIZE] << 8) | bleData[mid * BLE_ENTRY_SIZE + 1];
      if (id == mfgId) return (const char*)(bleData + mid * BLE_ENTRY_SIZE + 2);
      if (mfgId < id) hi = mid - 1;
      else lo = mid + 1;
    }
  }
  return nullptr;
}

float estimateDistance(int rssi) {
  const int A = -40;
  const float n = 2.5;

  if (rssi >= A) return 0.5; // Very close

  float ratio = (float)(A - rssi) / (10.0 * n);
  float distance = pow(10.0, ratio);

  return distance;
}

char getQualityGrade(wifi_ap_record_t* ap) {
  int score = 0;

  if (ap->rssi >= -50) score += 40;
  else if (ap->rssi >= -60) score += 30;
  else if (ap->rssi >= -70) score += 20;
  else if (ap->rssi >= -80) score += 10;

  if (ap->authmode == WIFI_AUTH_WPA3_PSK) score += 30;
  else if (ap->authmode == WIFI_AUTH_WPA2_PSK) score += 25;
  else if (ap->authmode == WIFI_AUTH_WPA_WPA2_PSK) score += 20;
  else if (ap->authmode == WIFI_AUTH_WPA_PSK) score += 10;
  else if (ap->authmode == WIFI_AUTH_WEP) score += 5;

  uint8_t load = channelLoad(ap->primary);
  if (load < LOAD_GOOD) score += 30;
  else if (load < LOAD_OK) score += 20;
  else if (load < LOAD_BUSY) score += 10;


  if (score >= 90) return 'A';
  if (score >= 75) return 'B';
  if (score >= 60) return 'C';
  if (score >= 45) return 'D';
  return 'F';
}

bool hasOverlap(uint8_t ch1, uint8_t ch2) {
  return abs(ch1 - ch2) <= 4 && abs(ch1 - ch2) > 0;
}

uint8_t countOverlappingAPs(uint8_t channel) {
  uint8_t count = 0;
  for (int i = 0; i < apCount; i++) {
    if (hasOverlap(channel, apList[i].primary)) {
      count++;
    }
  }
  return count;
}

const char* authStr(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    default: return "?";
  }
}
