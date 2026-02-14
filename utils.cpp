#include "utils.h"
#include "wifi_scanner.h"
#include <SPIFFS.h>

// ── SPIFFS-based WiFi OUI lookup ─────────────────────────────────

struct OUIEntry {
  uint8_t oui[3];
  char name[12];
};

struct BLECompany {
  uint16_t id;
  char name[14];
};

static OUIEntry* ouiTable = nullptr;
static uint16_t ouiCount = 0;
static BLECompany* bleTable = nullptr;
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

static uint8_t hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

static uint8_t hexByte(const char* s) {
  return (hexVal(s[0]) << 4) | hexVal(s[1]);
}

void loadVendorsFromSPIFFS() {
  // Load WiFi OUI table
  File ouiFile = SPIFFS.open("/oui.csv", "r");
  if (ouiFile) {
    // Count lines first
    uint16_t lines = 0;
    while (ouiFile.available()) {
      if (ouiFile.read() == '\n') lines++;
    }
    if (lines > 0) {
      ouiFile.seek(0);
      ouiTable = (OUIEntry*)malloc(sizeof(OUIEntry) * lines);
      if (ouiTable) {
        ouiCount = 0;
        char line[32];
        while (ouiFile.available() && ouiCount < lines) {
          int len = ouiFile.readBytesUntil('\n', line, sizeof(line) - 1);
          if (len < 8) continue;  // need at least "AABBCC,X"
          line[len] = '\0';
          // Remove trailing \r
          if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

          // Parse: AABBCC,VendorName
          char* comma = strchr(line, ',');
          if (!comma || (comma - line) != 6) continue;

          ouiTable[ouiCount].oui[0] = hexByte(line);
          ouiTable[ouiCount].oui[1] = hexByte(line + 2);
          ouiTable[ouiCount].oui[2] = hexByte(line + 4);

          strncpy(ouiTable[ouiCount].name, comma + 1, 11);
          ouiTable[ouiCount].name[11] = '\0';
          ouiCount++;
        }
        Serial.printf("[VENDOR] Loaded %d WiFi OUIs\n", ouiCount);
      }
    }
    ouiFile.close();
  } else {
    Serial.println("[VENDOR] oui.csv not found, using fallback");
  }

  // Load BLE company ID table
  File bleFile = SPIFFS.open("/ble.csv", "r");
  if (bleFile) {
    uint16_t lines = 0;
    while (bleFile.available()) {
      if (bleFile.read() == '\n') lines++;
    }
    if (lines > 0) {
      bleFile.seek(0);
      bleTable = (BLECompany*)malloc(sizeof(BLECompany) * lines);
      if (bleTable) {
        bleCount = 0;
        char line[32];
        while (bleFile.available() && bleCount < lines) {
          int len = bleFile.readBytesUntil('\n', line, sizeof(line) - 1);
          if (len < 6) continue;  // need at least "XXXX,X"
          line[len] = '\0';
          if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';

          // Parse: XXXX,VendorName
          char* comma = strchr(line, ',');
          if (!comma || (comma - line) != 4) continue;

          bleTable[bleCount].id = (hexByte(line) << 8) | hexByte(line + 2);
          strncpy(bleTable[bleCount].name, comma + 1, 13);
          bleTable[bleCount].name[13] = '\0';
          bleCount++;
        }
        Serial.printf("[VENDOR] Loaded %d BLE company IDs\n", bleCount);
      }
    }
    bleFile.close();
  } else {
    Serial.println("[VENDOR] ble.csv not found");
  }
}

const char* getVendor(uint8_t* mac) {
  // Search SPIFFS table first
  if (ouiTable && ouiCount > 0) {
    for (uint16_t i = 0; i < ouiCount; i++) {
      if (memcmp(mac, ouiTable[i].oui, 3) == 0) {
        return ouiTable[i].name;
      }
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
  if (bleTable && bleCount > 0) {
    for (uint16_t i = 0; i < bleCount; i++) {
      if (bleTable[i].id == mfgId) {
        return bleTable[i].name;
      }
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
