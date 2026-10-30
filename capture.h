#ifndef CAPTURE_H
#define CAPTURE_H
#include "capture_engine.h"
void observeCapture(const uint8_t* frame, size_t size, uint8_t channel, int8_t rssi);
void clearCapture();
void startPcapCapture();
void stopPcapCapture();
CaptureSummary getCaptureSummary();
void snapshotProbes(ProbeRecord* out, CaptureSummary& summary);
bool getCapturedFrame(size_t index, CapturedFrame& out);
#endif
