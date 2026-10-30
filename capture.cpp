#include <Arduino.h>
#include <esp_timer.h>
#include "capture.h"

static CaptureEngine engine;
static portMUX_TYPE captureMux = portMUX_INITIALIZER_UNLOCKED;

void observeCapture(const uint8_t* frame, size_t size, uint8_t channel, int8_t rssi) {
  uint64_t now = esp_timer_get_time();
  portENTER_CRITICAL(&captureMux);
  engine.observe(frame, size, channel, rssi, now);
  portEXIT_CRITICAL(&captureMux);
}
void clearCapture() {
  portENTER_CRITICAL(&captureMux); engine.clear(); portEXIT_CRITICAL(&captureMux);
}
void startPcapCapture() {
  portENTER_CRITICAL(&captureMux); engine.startPcap(); portEXIT_CRITICAL(&captureMux);
}
void stopPcapCapture() {
  portENTER_CRITICAL(&captureMux); engine.stopPcap(); portEXIT_CRITICAL(&captureMux);
}
CaptureSummary getCaptureSummary() {
  portENTER_CRITICAL(&captureMux);
  CaptureSummary result = engine.summary;
  portEXIT_CRITICAL(&captureMux);
  return result;
}
void snapshotProbes(ProbeRecord* out, CaptureSummary& summary) {
  portENTER_CRITICAL(&captureMux);
  summary = engine.summary;
  memcpy(out, engine.probes, summary.probeCount * sizeof(ProbeRecord));
  portEXIT_CRITICAL(&captureMux);
}
bool getCapturedFrame(size_t index, CapturedFrame& out) {
  portENTER_CRITICAL(&captureMux);
  bool found = index < engine.summary.frameCount;
  if (found) out = engine.frames[index];
  portEXIT_CRITICAL(&captureMux);
  return found;
}
