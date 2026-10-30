#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <lwip/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <fcntl.h>
#include <errno.h>
#include "lan_network.h"

static LanStatus status;
static uint32_t connectionStarted = 0, socketStarted = 0, arpWaitingSince = 0;
static int socketFD = -1;
static char identityBytes[LAN_HTTP_LIMIT], identityRequest[192];
static size_t identityLength = 0, identitySent = 0, identityRequestLength = 0;
static uint8_t identityStage = 0;
static uint32_t identityStarted = 0;
static uint8_t arpStage = 0; // 0=request, 1=await request callback, 2=wait, 3=await lookup
static portMUX_TYPE arpMux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t arpGeneration = 0;
struct ArpJob {
  uint32_t ip, generation;
  bool request, pending, done, found;
  uint8_t mac[6];
  err_t error;
};
static ArpJob arpJob = {};
static uint32_t ipNumber(IPAddress ip) { return (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) | (uint32_t(ip[2]) << 8) | ip[3]; }

static void closeScanSocket() { if (socketFD >= 0) { lwip_close(socketFD); socketFD = -1; } }
bool lanModeActive() { return status.connection == LanConnection::Connecting || status.connection == LanConnection::Connected; }
const LanStatus& getLanStatus() { return status; }
void cancelLanScan() {
  if (status.identity.state == LanIdentityState::Running) status.identity.state = LanIdentityState::Cancelled;
  closeScanSocket(); status.scan.cancel(); arpStage = 0;
  portENTER_CRITICAL(&arpMux); ++arpGeneration; portEXIT_CRITICAL(&arpMux);
}
void disconnectLan() {
  cancelLanScan();
  WiFi.setAutoReconnect(false); WiFi.disconnect(false, false);
  // Credentials are stored only in the driver's RAM and cleared on disconnect.
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  wifi_config_t cleared = {}; esp_wifi_set_config(WIFI_IF_STA, &cleared);
  status = {};
}
bool connectLan(const char* ssid, const char* password) {
  size_t ssidLength = strlen(ssid), passLength = strlen(password);
  if (!ssidLength || ssidLength > 32 || (passLength && (passLength < 8 || passLength > 63))) return false;
  disconnectLan();
  WiFi.persistent(false); WiFi.setAutoReconnect(false);
  if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) return false;
  strncpy(status.ssid, ssid, 32);
  status.connection = LanConnection::Connecting;
  status.message = "Connecting; the device access point may briefly change channel.";
  connectionStarted = millis();
  WiFi.begin(ssid, password);
  return true;
}
static void arpCallback(void*) {
  portENTER_CRITICAL(&arpMux);
  bool valid = arpJob.generation == arpGeneration;
  uint32_t target = arpJob.ip; bool request = arpJob.request;
  portEXIT_CRITICAL(&arpMux);
  bool found = false; uint8_t mac[6] = {}; err_t result = ERR_IF;
  if (valid) {
    esp_netif_t* handle = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    struct netif* net = handle ? (struct netif*)esp_netif_get_netif_impl(handle) : nullptr;
    ip4_addr_t address; address.addr = lwip_htonl(target);
    if (net && netif_is_up(net)) {
      result = ERR_OK;
      if (request) result = etharp_request(net, &address);
      else {
        const ip4_addr_t* resolvedIP = nullptr; struct eth_addr* resolvedMAC = nullptr;
        found = etharp_find_addr(net, &address, &resolvedMAC, &resolvedIP) >= 0 && resolvedMAC;
        if (found) memcpy(mac, resolvedMAC->addr, 6);
      }
    }
  }
  portENTER_CRITICAL(&arpMux);
  arpJob.found = found; memcpy(arpJob.mac, mac, 6); arpJob.error = result; arpJob.done = true;
  portEXIT_CRITICAL(&arpMux);
}
static bool submitARP(uint32_t target, bool request) {
  portENTER_CRITICAL(&arpMux);
  if (arpJob.pending) { portEXIT_CRITICAL(&arpMux); return false; }
  arpJob = {}; arpJob.ip = target; arpJob.request = request;
  arpJob.generation = arpGeneration; arpJob.pending = true;
  portEXIT_CRITICAL(&arpMux);
  if (tcpip_try_callback(arpCallback, nullptr) == ERR_OK) return true;
  portENTER_CRITICAL(&arpMux); arpJob.pending = false; portEXIT_CRITICAL(&arpMux);
  return false;
}
static bool takeARPResult(ArpJob& out) {
  portENTER_CRITICAL(&arpMux);
  bool ready = arpJob.pending && arpJob.done;
  if (ready) { out = arpJob; arpJob.pending = false; }
  portEXIT_CRITICAL(&arpMux);
  return ready;
}
bool startLanDiscovery(uint32_t first, uint32_t last) {
  portENTER_CRITICAL(&arpMux); bool workerBusy = arpJob.pending; portEXIT_CRITICAL(&arpMux);
  if (status.connection != LanConnection::Connected || workerBusy) return false;
  if (!status.scan.startDiscovery(first, last)) return false;
  status.portCount = 0; status.portTarget = 0;
  arpStage = 0; status.message = "Discovering ARP neighbors."; return true;
}
bool startLanPortScan(uint32_t target, const LanPortList& ports) {
  if (status.connection != LanConnection::Connected || !status.scan.startPorts(target, ports)) return false;
  status.portCount = ports.count; status.portTarget = target;
  closeScanSocket(); status.message = "Checking selected TCP ports."; return true;
}
bool startLanIdentification(uint32_t target, uint16_t port) {
  const uint16_t supported[] = {80, 8080, 8000, 8888, 3000, 5000, 7001};
  bool allowed = false; for (uint16_t p : supported) if (p == port) allowed = true;
  if (!allowed || status.connection != LanConnection::Connected || status.scan.busy() || !status.scan.context.localHost(target)) return false;
  closeScanSocket(); status.identity = {}; status.identity.target = target; status.identity.port = port;
  status.identity.state = LanIdentityState::Running;
  status.scan.job = LanJob::Identify; status.scan.target = target; status.scan.total = 1;
  status.scan.completed = status.scan.errors = 0; status.scan.progress = LanProgress::Running;
  identityLength = identitySent = identityRequestLength = 0; identityStage = 0; identityStarted = millis();
  auto append = [&](const char* text) { while (*text && identityRequestLength < sizeof(identityRequest)) identityRequest[identityRequestLength++] = *text++; };
  auto number = [&](unsigned value) { char digits[6]; unsigned n = 0; do { digits[n++] = '0' + value % 10; value /= 10; } while (value); while (n) identityRequest[identityRequestLength++] = digits[--n]; };
  append("GET / HTTP/1.0\r\nHost: ");
  for (int shift = 24; shift >= 0; shift -= 8) { number((target >> shift) & 255); if (shift) append("."); }
  append(":"); number(port);
  append("\r\nAccept: text/html\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
  status.message = "Reading a bounded HTTP identification sample."; return true;
}
static void finishIdentification(LanIdentityState outcome, bool limited) {
  closeScanSocket();
  bool valid = parseLanIdentity(identityBytes, identityLength, status.identity);
  status.identity.limited = limited;
  if (outcome == LanIdentityState::Done && (!valid || !status.identity.headersComplete)) outcome = LanIdentityState::Error;
  status.identity.state = outcome;
  status.scan.completed = 1; status.scan.errors = outcome == LanIdentityState::Error ? 1 : 0;
  status.scan.progress = outcome == LanIdentityState::Done ? LanProgress::Done : LanProgress::Failed;
  status.message = outcome == LanIdentityState::Done ? "HTTP identification sample complete; reported details are unverified." : outcome == LanIdentityState::Timeout ? "Identification timed out; any reported details are partial." : "No complete HTTP response; device identity remains unknown.";
}
static void updateIdentification() {
  if (millis() - identityStarted >= 2500) { finishIdentification(LanIdentityState::Timeout, true); return; }
  if (identityStage == 0) {
    socketFD = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketFD < 0) { finishIdentification(LanIdentityState::Error, false); return; }
    int flags = lwip_fcntl(socketFD, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) < 0) { finishIdentification(LanIdentityState::Error, false); return; }
    sockaddr_in address = {}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = lwip_htonl(status.identity.target); address.sin_port = lwip_htons(status.identity.port);
    socketStarted = millis();
    int connected = lwip_connect(socketFD, (sockaddr*)&address, sizeof(address));
    if (connected == 0) identityStage = 2;
    else if (errno == EINPROGRESS || errno == EWOULDBLOCK) identityStage = 1;
    else { finishIdentification(LanIdentityState::Error, false); return; }
  }
  if (identityStage == 1) {
    if (millis() - socketStarted >= 350) { finishIdentification(LanIdentityState::Timeout, true); return; }
    fd_set ready, error; FD_ZERO(&ready); FD_ZERO(&error); FD_SET(socketFD, &ready); FD_SET(socketFD, &error); timeval wait = {};
    int count = lwip_select(socketFD + 1, nullptr, &ready, &error, &wait);
    if (count < 0) { finishIdentification(LanIdentityState::Error, false); return; }
    if (!count) return;
    int outcome = 0; socklen_t size = sizeof(outcome);
    if (lwip_getsockopt(socketFD,SOL_SOCKET,SO_ERROR,&outcome,&size) < 0 || outcome) { finishIdentification(LanIdentityState::Error, false); return; }
    identityStage = 2;
  }
  if (identityStage == 2) {
    int sent = lwip_send(socketFD, identityRequest + identitySent, identityRequestLength - identitySent, 0);
    if (sent < 0 && (errno == EWOULDBLOCK || errno == EINPROGRESS)) return;
    if (sent <= 0) { finishIdentification(LanIdentityState::Error, false); return; }
    identitySent += sent;
    if (identitySent < identityRequestLength) return;
    identityStage = 3;
  }
  size_t remaining = sizeof(identityBytes) - identityLength;
  int received = lwip_recv(socketFD,identityBytes + identityLength,remaining < 512 ? remaining : 512,0);
  if (received < 0 && (errno == EWOULDBLOCK || errno == EINPROGRESS)) return;
  if (received < 0) { finishIdentification(LanIdentityState::Error, true); return; }
  if (!received) { finishIdentification(LanIdentityState::Done, false); return; }
  identityLength += received;
  if (identityLength == sizeof(identityBytes)) finishIdentification(LanIdentityState::Done, true);
}
static void finishPort(LanPortState result) { closeScanSocket(); status.scan.portResult(result); }
static void updatePortScan() {
  if (socketFD < 0) {
    socketFD = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socketFD < 0) { status.scan.portResult(LanPortState::Error); return; }
    int flags = lwip_fcntl(socketFD, F_GETFL, 0);
    if (flags < 0 || lwip_fcntl(socketFD, F_SETFL, flags | O_NONBLOCK) < 0) { finishPort(LanPortState::Error); return; }
    sockaddr_in address = {}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = lwip_htonl(status.scan.target);
    address.sin_port = lwip_htons(status.scan.ports[status.scan.completed].port);
    socketStarted = millis();
    if (lwip_connect(socketFD, (sockaddr*)&address, sizeof(address)) == 0) { finishPort(LanPortState::Open); return; }
    if (errno != EINPROGRESS && errno != EWOULDBLOCK) { finishPort(errno == ECONNREFUSED ? LanPortState::Closed : LanPortState::Error); return; }
  }
  if (millis() - socketStarted >= 350) { finishPort(LanPortState::Timeout); return; }
  fd_set writable, exceptional; FD_ZERO(&writable); FD_ZERO(&exceptional);
  FD_SET(socketFD, &writable); FD_SET(socketFD, &exceptional); timeval wait = {};
  int ready = lwip_select(socketFD + 1, nullptr, &writable, &exceptional, &wait);
  if (ready < 0) { finishPort(LanPortState::Error); return; }
  if (ready > 0) {
    int error = 0; socklen_t size = sizeof(error);
    if (lwip_getsockopt(socketFD, SOL_SOCKET, SO_ERROR, &error, &size) < 0) finishPort(LanPortState::Error);
    else finishPort(error == 0 ? LanPortState::Open : error == ECONNREFUSED ? LanPortState::Closed : LanPortState::Error);
  } else if (millis() - socketStarted >= 350) finishPort(LanPortState::Timeout);
}
void updateLanNetwork() {
  ArpJob result = {}; bool arpReady = takeARPResult(result);
  if (!lanModeActive()) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (status.connection == LanConnection::Connecting && millis() - connectionStarted < 20000) return;
    disconnectLan(); status.connection = LanConnection::Error; status.message = "WiFi connection failed or was lost. Connect again."; return;
  }
  LanContext context; context.ip = ipNumber(WiFi.localIP()); context.mask = ipNumber(WiFi.subnetMask());
  context.apIP = ipNumber(WiFi.softAPIP()); context.apMask = 0xffffff00;
  if (!context.valid() || context.overlapsAP()) {
    disconnectLan(); status.connection = LanConnection::Error;
    status.message = "LAN subnet is invalid or overlaps the device's 192.168.4.0/24 access point."; return;
  }
  if (status.connection == LanConnection::Connecting) {
    status.connection = LanConnection::Connected; status.scan.context = context;
    status.message = "Connected. Start discovery or select a local host for TCP checks.";
  } else if (context.ip != status.scan.context.ip || context.mask != status.scan.context.mask) {
    cancelLanScan(); status.scan = {}; status.identity = {}; status.portCount = 0; status.portTarget = 0; status.scan.context = context;
    status.message = "LAN address changed; previous scan results cleared.";
  }
  if (!status.scan.busy()) return;
  if (status.scan.job == LanJob::Identify) { updateIdentification(); return; }
  if (status.scan.job == LanJob::Ports) updatePortScan();
  else {
    if (arpReady && result.generation == arpGeneration) {
      if (result.error != ERR_OK) { status.scan.neighbor(false, nullptr, true); arpStage = 0; }
      else if (result.request) { arpStage = 2; arpWaitingSince = millis(); }
      else { status.scan.neighbor(result.found, result.mac); arpStage = 0; }
    }
    if (status.scan.busy()) {
      if (arpStage == 0) {
        if (!context.localHost(status.scan.currentAddress())) status.scan.neighbor(false, nullptr);
        else if (submitARP(status.scan.currentAddress(), true)) { arpStage = 1; arpWaitingSince = millis(); }
        else { status.scan.neighbor(false, nullptr, true); }
      } else if (arpStage == 2 && millis() - arpWaitingSince >= 200) {
        if (submitARP(status.scan.currentAddress(), false)) { arpStage = 3; arpWaitingSince = millis(); }
        else if (millis() - arpWaitingSince > 2000) { status.scan.neighbor(false, nullptr, true); arpStage = 0; }
      } else if ((arpStage == 1 || arpStage == 3) && millis() - arpWaitingSince > 2000) {
        cancelLanScan(); status.scan.progress = LanProgress::Failed;
        status.message = "ARP worker timed out; scan stopped.";
      }
    }
  }
  if (status.scan.progress == LanProgress::Done) status.message = "Scan complete.";
}
