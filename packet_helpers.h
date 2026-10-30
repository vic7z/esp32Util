#ifndef PACKET_HELPERS_H
#define PACKET_HELPERS_H
#include <stddef.h>
#include <stdint.h>

struct SSIDElement {
  size_t offset;
  uint8_t length;
  bool found;
};

// Parse 802.11 tagged elements without trusting their advertised lengths.
constexpr SSIDElement findSSID(const uint8_t* frame, size_t length, size_t offset) {
  if (!frame) return {0, 0, false};
  while (offset < length && length - offset >= 2) {
    uint8_t id = frame[offset], size = frame[offset + 1];
    if (size > length - offset - 2) return {0, 0, false};
    if (id == 0) return size <= 32 ? SSIDElement{offset + 2, size, true} : SSIDElement{0, 0, false};
    offset += 2 + size;
  }
  return {0, 0, false};
}
#endif
