#ifndef LAN_ENGINE_H
#define LAN_ENGINE_H
#include <stdint.h>
#include <stddef.h>

constexpr size_t LAN_MAX_HOSTS = 32, LAN_MAX_PORTS = 32;
constexpr uint32_t LAN_MAX_ADDRESSES = 254;
constexpr bool lanUnicast(uint32_t ip) { return (ip >> 24) != 0 && (ip >> 24) != 127 && (ip >> 24) < 224; }
constexpr bool lanMaskValid(uint32_t mask) {
  uint32_t inverse = ~mask;
  return mask != 0 && inverse >= 3 && (inverse & (inverse + 1)) == 0;
}
inline bool parseLanIP(const char* text, uint32_t& result) {
  uint32_t value = 0;
  for (int part = 0; part < 4; ++part) {
    unsigned octet = 0, digits = 0;
    while (*text >= '0' && *text <= '9') {
      if (++digits > 3) return false;
      octet = octet * 10 + (*text++ - '0');
      if (octet > 255) return false;
    }
    if (!digits || (part < 3 ? *text++ != '.' : *text != 0)) return false;
    value = (value << 8) | octet;
  }
  result = value; return true;
}
struct LanContext {
  uint32_t ip = 0, mask = 0, apIP = 0xc0a80401, apMask = 0xffffff00;
  bool valid() const { return lanMaskValid(mask) && lanUnicast(ip) && ip > (ip & mask) && ip < (ip | ~mask); }
  bool overlapsAP() const { return (ip & apMask) == (apIP & apMask); }
  bool localHost(uint32_t target) const {
    return valid() && !overlapsAP() && lanUnicast(target) && (target & mask) == (ip & mask) &&
      target > (ip & mask) && target < (ip | ~mask) && target != ip && (target & apMask) != (apIP & apMask);
  }
  bool discoveryRange(uint32_t first, uint32_t last) const {
    return valid() && !overlapsAP() && first <= last && last - first < LAN_MAX_ADDRESSES &&
      lanUnicast(first) && lanUnicast(last) && (first & mask) == (ip & mask) &&
      (last & mask) == (ip & mask) && first > (ip & mask) && last < (ip | ~mask) &&
      !(first <= (apIP | ~apMask) && (apIP & apMask) <= last);
  }
  void defaultRange(uint32_t& first, uint32_t& last) const {
    uint32_t boundedMask = mask < 0xffffff00 ? 0xffffff00 : mask;
    first = (ip & boundedMask) + 1; last = (ip | ~boundedMask) - 1;
  }
};
struct LanPortList { uint16_t ports[LAN_MAX_PORTS] = {}; uint16_t count = 0; };
inline bool parseLanPorts(const char* text, LanPortList& output) {
  LanPortList parsed;
  auto spaces = [&]() { while (*text == ' ' || *text == '\t') ++text; };
  auto number = [&](unsigned& value) {
    value = 0; unsigned digits = 0;
    while (*text >= '0' && *text <= '9') {
      if (++digits > 5) return false;
      value = value * 10 + (*text++ - '0');
      if (value > 65535) return false;
    }
    return digits && value != 0;
  };
  while (true) {
    spaces(); unsigned first, last;
    if (!number(first)) return false;
    spaces(); last = first;
    if (*text == '-') { ++text; spaces(); if (!number(last)) return false; spaces(); }
    if (last < first || last - first >= LAN_MAX_PORTS) return false;
    for (unsigned port = first; port <= last; ++port) {
      bool duplicate = false;
      for (size_t i = 0; i < parsed.count; ++i) if (parsed.ports[i] == port) duplicate = true;
      if (!duplicate) {
        if (parsed.count == LAN_MAX_PORTS) return false;
        parsed.ports[parsed.count++] = port;
      }
    }
    if (!*text) { output = parsed; return true; }
    if (*text++ != ',') return false;
  }
}
enum class LanJob : uint8_t { None, Discovery, Ports, Identify };
enum class LanProgress : uint8_t { Idle, Running, Done, Cancelled, Failed };
enum class LanPortState : uint8_t { Pending, Open, Closed, Timeout, Error };
struct LanHost { uint32_t ip; uint8_t mac[6]; };
struct LanPortResult { uint16_t port; LanPortState state; };
class LanEngine {
public:
  LanContext context;
  LanJob job = LanJob::None;
  LanProgress progress = LanProgress::Idle;
  uint32_t first = 0, last = 0, target = 0;
  uint16_t completed = 0, total = 0, hostCount = 0, omitted = 0, errors = 0;
  LanHost hosts[LAN_MAX_HOSTS] = {};
  LanPortResult ports[LAN_MAX_PORTS] = {};
  bool busy() const { return progress == LanProgress::Running; }
  bool startDiscovery(uint32_t begin, uint32_t end) {
    if (busy() || !context.discoveryRange(begin, end)) return false;
    first = begin; last = end; target = 0; job = LanJob::Discovery;
    completed = hostCount = omitted = errors = 0; total = end - begin + 1;
    progress = LanProgress::Running; return true;
  }
  bool startPorts(uint32_t host, const LanPortList& list) {
    if (busy() || !context.localHost(host) || !list.count || list.count > LAN_MAX_PORTS) return false;
    for (size_t i = 0; i < list.count; ++i) if (!list.ports[i]) return false;
    for (size_t i = 0; i < list.count; ++i) ports[i] = {list.ports[i], LanPortState::Pending};
    target = host; total = list.count; completed = errors = 0;
    job = LanJob::Ports; progress = LanProgress::Running; return true;
  }
  uint32_t currentAddress() const { return job == LanJob::Discovery ? first + completed : target; }
  void neighbor(bool found, const uint8_t* mac, bool error = false) {
    if (!busy() || job != LanJob::Discovery) return;
    if (error) ++errors;
    if (found && mac) {
      if (hostCount == LAN_MAX_HOSTS) ++omitted;
      else {
        hosts[hostCount].ip = currentAddress();
        for (int i = 0; i < 6; ++i) hosts[hostCount].mac[i] = mac[i];
        ++hostCount;
      }
    }
    advance();
  }
  void portResult(LanPortState result) {
    if (!busy() || job != LanJob::Ports || result == LanPortState::Pending) return;
    ports[completed].state = result;
    if (result == LanPortState::Error) ++errors;
    advance();
  }
  void advance() { if (++completed == total) progress = LanProgress::Done; }
  void cancel() { if (busy()) progress = LanProgress::Cancelled; }
};
#endif
