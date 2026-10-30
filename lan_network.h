#ifndef LAN_NETWORK_H
#define LAN_NETWORK_H
#include "lan_engine.h"
#include "lan_identity.h"
enum class LanConnection : uint8_t { Disconnected, Connecting, Connected, Error };
struct LanStatus {
  LanConnection connection = LanConnection::Disconnected;
  char ssid[33] = {};
  const char* message = "Connect to a WiFi network to use LAN tools.";
  LanEngine scan;
  LanIdentity identity;
  uint16_t portCount = 0;
  uint32_t portTarget = 0;
};
bool lanModeActive();
const LanStatus& getLanStatus();
bool connectLan(const char* ssid, const char* password);
void disconnectLan();
void cancelLanScan();
bool startLanDiscovery(uint32_t first, uint32_t last);
bool startLanPortScan(uint32_t target, const LanPortList& ports);
bool startLanIdentification(uint32_t target, uint16_t port);
void updateLanNetwork();
#endif
