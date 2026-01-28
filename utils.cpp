#include "utils.h"
#include "wifi_scanner.h"

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

const char* getVendor(uint8_t* mac) {
  for (int i = 0; i < vendorCount; i++) {
    if (memcmp(mac, vendors[i].oui, 3) == 0) {
      return vendors[i].name;
    }
  }
  return "Unknown";
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
  if (load < 20) score += 30;
  else if (load < 40) score += 20;
  else if (load < 70) score += 10;

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
