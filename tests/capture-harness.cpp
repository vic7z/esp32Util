#include "../capture_engine.h"

// Freestanding WASM harness: exercises the production engine and encoder,
// not a JavaScript reimplementation. These loops supply its only libc calls.
extern "C" void* memcpy(void* out, const void* in, size_t count) {
  auto dst = (unsigned char*)out; auto src = (const unsigned char*)in;
  for (size_t i = 0; i < count; ++i) dst[i] = src[i];
  return out;
}
extern "C" void* memset(void* out, int value, size_t count) {
  auto dst = (unsigned char*)out;
  for (size_t i = 0; i < count; ++i) dst[i] = value;
  return out;
}
extern "C" int memcmp(const void* a, const void* b, size_t count) {
  auto x = (const unsigned char*)a; auto y = (const unsigned char*)b;
  for (size_t i = 0; i < count; ++i) if (x[i] != y[i]) return int(x[i]) - int(y[i]);
  return 0;
}
static CaptureEngine engine;
static uint8_t input[4096], output[24 + PCAP_CAPACITY * (16 + RADIOTAP_BYTES + PCAP_FRAME_BYTES)];
extern "C" {
__attribute__((export_name("inputBuffer"))) uint8_t* inputBuffer() { return input; }
__attribute__((export_name("resetCapture"))) void resetCapture() { engine.clear(); }
__attribute__((export_name("startRaw"))) void startRaw() { engine.startPcap(); }
__attribute__((export_name("stopRaw"))) void stopRaw() { engine.stopPcap(); }
__attribute__((export_name("observeFrame"))) void observeFrame(size_t length, uint32_t seconds, uint32_t usec, uint8_t channel, int8_t rssi) {
  engine.observe(input, length, channel, rssi, uint64_t(seconds) * 1000000 + usec);
}
__attribute__((export_name("detectEapol"))) int detectEapol(size_t length) { return isEapol(input, length); }
__attribute__((export_name("probeCount"))) uint32_t probeCount() { return engine.summary.probeCount; }
__attribute__((export_name("probeRequests"))) uint32_t probeRequests() { return engine.summary.probeRequests; }
__attribute__((export_name("probeEvictions"))) uint32_t probeEvictions() { return engine.summary.probeEvictions; }
__attribute__((export_name("probeSeen"))) uint32_t probeSeen(size_t index) { return engine.probes[index].count; }
__attribute__((export_name("probeLastSeconds"))) uint32_t probeLastSeconds(size_t index) { return engine.probes[index].lastSeenUs / 1000000; }
__attribute__((export_name("probeFirstSeconds"))) uint32_t probeFirstSeconds(size_t index) { return engine.probes[index].firstSeenUs / 1000000; }
__attribute__((export_name("eapolCount"))) uint32_t eapolCount() { return engine.summary.eapol; }
__attribute__((export_name("channelEapol"))) uint32_t channelEapol(uint8_t ch) { return engine.summary.eapolByChannel[ch]; }
__attribute__((export_name("rawCount"))) uint32_t rawCount() { return engine.summary.frameCount; }
__attribute__((export_name("rawDropped"))) uint32_t rawDropped() { return engine.summary.dropped; }
__attribute__((export_name("rawTruncated"))) uint32_t rawTruncated() { return engine.summary.truncated; }
__attribute__((export_name("pcapBuffer"))) uint8_t* pcapBuffer() { return output; }
__attribute__((export_name("encodePcap"))) size_t encodePcap() {
  writePcapHeader(output);
  size_t position = 24;
  for (size_t i = 0; i < engine.summary.frameCount; ++i) position += writePcapRecord(engine.frames[i], output + position);
  return position;
}
}
