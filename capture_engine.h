#ifndef CAPTURE_ENGINE_H
#define CAPTURE_ENGINE_H
#include "packet_helpers.h"
#include <string.h>

// Small, fixed allocations suitable for the C3. A full buffer preserves its
// first packets and counts subsequent drops; it never overwrites a download.
constexpr size_t PROBE_HISTORY_CAPACITY = 24;
constexpr size_t PCAP_CAPACITY = 32;
constexpr size_t PCAP_FRAME_BYTES = 768;
constexpr size_t RADIOTAP_BYTES = 15;

constexpr bool isEapol(const uint8_t* frame, size_t size) {
  if (!frame || size < 24 || (frame[0] & 0x0f) != 0x08 ||
      (frame[0] & 0x40) || (frame[1] & 0x44) || (frame[22] & 0x0f)) return false;
  size_t offset = 24 + ((frame[1] & 3) == 3 ? 6 : 0);
  if (frame[0] & 0x80) {
    if (size - (offset < size ? offset : size) < 2 || (frame[offset] & 0x80)) return false;
    offset += 2 + (frame[1] & 0x80 ? 4 : 0);
  }
  if (offset > size || size - offset < 12) return false;
  const uint8_t snap[] = {0xaa, 0xaa, 3, 0, 0, 0, 0x88, 0x8e};
  for (size_t i = 0; i < 8; ++i) if (frame[offset + i] != snap[i]) return false;
  size_t bodyLength = (size_t(frame[offset + 10]) << 8) | frame[offset + 11];
  return frame[offset + 8] != 0 && bodyLength <= size - offset - 12;
}

inline void putLE16(uint8_t* out, uint16_t value) {
  out[0] = value; out[1] = value >> 8;
}
inline void putLE32(uint8_t* out, uint32_t value) {
  for (int i = 0; i < 4; ++i) out[i] = value >> (8 * i);
}
inline void writePcapHeader(uint8_t* out) {
  memset(out, 0, 24);
  putLE32(out, 0xa1b2c3d4); putLE16(out + 4, 2); putLE16(out + 6, 4);
  putLE32(out + 16, RADIOTAP_BYTES + PCAP_FRAME_BYTES);
  putLE32(out + 20, 127); // LINKTYPE_IEEE802_11_RADIOTAP
}
inline void writeRadiotap(uint8_t* out, uint8_t channel, int8_t rssi) {
  memset(out, 0, RADIOTAP_BYTES);
  putLE16(out + 2, RADIOTAP_BYTES); putLE32(out + 4, 0x2a);
  uint16_t frequency = channel == 14 ? 2484 : 2407 + 5 * channel;
  putLE16(out + 10, frequency); putLE16(out + 12, 0x0080); // 2 GHz
  out[14] = static_cast<uint8_t>(rssi);
}

struct ProbeRecord {
  uint8_t source[6];
  uint8_t ssid[33];
  uint8_t ssidLength, channel;
  int8_t rssi;
  uint64_t firstSeenUs, lastSeenUs;
  uint32_t count;
};
struct CapturedFrame {
  uint64_t timestampUs;
  uint16_t originalLength, capturedLength;
  uint8_t channel;
  int8_t rssi;
  uint8_t bytes[PCAP_FRAME_BYTES];
};
inline size_t writePcapRecord(const CapturedFrame& frame, uint8_t* out) {
  putLE32(out, frame.timestampUs / 1000000);
  putLE32(out + 4, frame.timestampUs % 1000000);
  putLE32(out + 8, RADIOTAP_BYTES + frame.capturedLength);
  putLE32(out + 12, RADIOTAP_BYTES + frame.originalLength);
  writeRadiotap(out + 16, frame.channel, frame.rssi);
  memcpy(out + 16 + RADIOTAP_BYTES, frame.bytes, frame.capturedLength);
  return 16 + RADIOTAP_BYTES + frame.capturedLength;
}
struct CaptureSummary {
  uint32_t probeRequests, probeEvictions, eapol, eapolByChannel[14];
  uint32_t dropped, truncated;
  uint16_t probeCount, frameCount;
  bool recording;
};

class CaptureEngine {
public:
  CaptureSummary summary = {};
  ProbeRecord probes[PROBE_HISTORY_CAPACITY] = {};
  CapturedFrame frames[PCAP_CAPACITY] = {};

  void clear() { summary = {}; } // Counts gate access; stale bytes are never exported.
  void startPcap() {
    summary.frameCount = 0; summary.dropped = summary.truncated = 0;
    summary.recording = true;
  }
  void stopPcap() { summary.recording = false; }
  static void increment(uint32_t& value) { if (value != UINT32_MAX) ++value; }

  // Caller strips the hardware FCS and supplies a monotonic 64-bit timestamp.
  void observe(const uint8_t* frame, size_t size, uint8_t channel, int8_t rssi, uint64_t now) {
    if (!frame || size < 10 || size > UINT16_MAX || channel < 1 || channel > 13) return;
    if (size >= 24 && frame[0] == 0x40) {
      SSIDElement ssid = findSSID(frame, size, 24);
      if (ssid.found) {
        increment(summary.probeRequests);
        size_t slot = summary.probeCount;
        for (size_t i = 0; i < summary.probeCount; ++i) {
          if (memcmp(probes[i].source, frame + 10, 6) == 0 && probes[i].ssidLength == ssid.length &&
              memcmp(probes[i].ssid, frame + ssid.offset, ssid.length) == 0) { slot = i; break; }
        }
        if (slot == summary.probeCount) {
          if (summary.probeCount == PROBE_HISTORY_CAPACITY) {
            slot = 0;
            for (size_t i = 1; i < PROBE_HISTORY_CAPACITY; ++i)
              if (probes[i].lastSeenUs < probes[slot].lastSeenUs) slot = i;
            increment(summary.probeEvictions);
          } else ++summary.probeCount;
          probes[slot] = {};
          memcpy(probes[slot].source, frame + 10, 6);
          memcpy(probes[slot].ssid, frame + ssid.offset, ssid.length);
          probes[slot].ssidLength = ssid.length;
          probes[slot].firstSeenUs = now;
        }
        probes[slot].lastSeenUs = now; probes[slot].channel = channel;
        probes[slot].rssi = rssi; increment(probes[slot].count);
      }
    }
    if (isEapol(frame, size)) {
      increment(summary.eapol); increment(summary.eapolByChannel[channel]);
    }
    if (!summary.recording) return;
    if (summary.frameCount == PCAP_CAPACITY) { increment(summary.dropped); return; }
    CapturedFrame& captured = frames[summary.frameCount++];
    captured.timestampUs = now; captured.originalLength = size;
    captured.capturedLength = size > PCAP_FRAME_BYTES ? PCAP_FRAME_BYTES : size;
    captured.channel = channel; captured.rssi = rssi;
    if (size > PCAP_FRAME_BYTES) increment(summary.truncated);
    memcpy(captured.bytes, frame, captured.capturedLength);
  }
};
#endif
