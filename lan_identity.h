#ifndef LAN_IDENTITY_H
#define LAN_IDENTITY_H
#include <stdint.h>
#include <stddef.h>

constexpr size_t LAN_HTTP_LIMIT = 4096;
enum class LanIdentityState : uint8_t { Idle, Running, Done, Cancelled, Timeout, Error };
struct LanIdentity {
  uint32_t target = 0;
  uint16_t port = 0, statusCode = 0;
  LanIdentityState state = LanIdentityState::Idle;
  bool limited = false, headersComplete = false, chunked = false, encoded = false;
  char server[97] = {}, title[97] = {}, contentType[65] = {};
  const char* category = "Unknown device";
};
inline char identityLower(char c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
inline bool identityEqual(const char* a, size_t n, const char* b) {
  size_t i = 0; for (; i < n && b[i]; ++i) if (identityLower(a[i]) != b[i]) return false;
  return i == n && !b[i];
}
inline bool identityContains(const char* text, size_t n, const char* needle) {
  size_t actual = 0; while (actual < n && text[actual]) ++actual; n = actual;
  size_t length = 0; while (needle[length]) ++length;
  for (size_t i = 0; i + length <= n; ++i) if (identityEqual(text + i, length, needle)) return true;
  return false;
}
inline void identityText(const char* text, size_t n, char* out, size_t cap, bool entities = false) {
  size_t used = 0; bool space = false, tag = false;
  for (size_t i = 0; i < n && used + 1 < cap; ++i) {
    unsigned char c = text[i];
    if (entities && c == '<') { tag = true; continue; }
    if (entities && c == '>') { tag = false; space = true; continue; }
    if (tag) continue;
    if (entities && c == '&') {
      size_t end = i + 1; while (end < n && end - i < 12 && text[end] != ';') ++end;
      if (end < n && text[end] == ';') {
        const char* entity = text + i + 1; size_t len = end - i - 1; unsigned value = 0;
        if (identityEqual(entity,len,"amp")) value = '&';
        else if (identityEqual(entity,len,"lt")) value = '<';
        else if (identityEqual(entity,len,"gt")) value = '>';
        else if (identityEqual(entity,len,"quot")) value = '"';
        else if (identityEqual(entity,len,"apos")) value = '\'';
        else if (identityEqual(entity,len,"nbsp")) value = ' ';
        else if (len > 1 && entity[0] == '#') {
          size_t k = 1; unsigned base = 10;
          if (k < len && (entity[k] == 'x' || entity[k] == 'X')) { base = 16; ++k; }
          bool valid = k < len;
          for (; k < len; ++k) {
            char d = identityLower(entity[k]); unsigned digit = d >= '0' && d <= '9' ? d-'0' : d >= 'a' && d <= 'f' ? d-'a'+10 : 99;
            if (digit >= base || value > 127) { valid = false; break; }
            value = value * base + digit;
          }
          if (!valid || value > 126 || value < 32) value = '?';
        }
        if (value) { c = value; i = end; }
      }
    }
    if (c <= 32 || c == 127) { space = used > 0; continue; }
    if (space && used + 2 < cap) out[used++] = ' ';
    space = false; out[used++] = c;
  }
  out[used] = 0;
}
// Parse a bounded HTTP sample. No page is executed and no URL is followed.
inline bool parseLanIdentity(const char* bytes, size_t length, LanIdentity& result) {
  result.statusCode = 0; result.headersComplete = result.chunked = result.encoded = false;
  result.server[0] = result.title[0] = result.contentType[0] = 0; result.category = "Unknown device";
  if (length < 12 || !(identityEqual(bytes,8,"http/1.0") || identityEqual(bytes,8,"http/1.1")) || bytes[8] != ' ') return false;
  if (bytes[9] < '1' || bytes[9] > '5' || bytes[10] < '0' || bytes[10] > '9' || bytes[11] < '0' || bytes[11] > '9' || (length > 12 && bytes[12] != ' ' && bytes[12] != '\r')) return false;
  result.statusCode = (bytes[9]-'0')*100+(bytes[10]-'0')*10+bytes[11]-'0';
  size_t start = 0, body = 0;
  while (start + 1 < length && start < 2048) {
    size_t end = start; while (end + 1 < length && !(bytes[end] == '\r' && bytes[end+1] == '\n')) ++end;
    if (end + 1 >= length || end >= 2048) break;
    if (end == start) { body = end + 2; result.headersComplete = true; break; }
    size_t colon = start; while (colon < end && bytes[colon] != ':') ++colon;
    size_t value = colon + 1; while (value < end && (bytes[value] == ' ' || bytes[value] == '\t')) ++value;
    if (colon < end) {
      if (identityEqual(bytes+start,colon-start,"server")) identityText(bytes+value,end-value,result.server,sizeof(result.server));
      if (identityEqual(bytes+start,colon-start,"content-type")) identityText(bytes+value,end-value,result.contentType,sizeof(result.contentType));
      if (identityEqual(bytes+start,colon-start,"transfer-encoding")) result.chunked = true;
      if (identityEqual(bytes+start,colon-start,"content-encoding") && !identityEqual(bytes+value,end-value,"identity")) result.encoded = true;
    }
    start = end + 2;
  }
  // Compressed/transfer-encoded samples retain headers; never invent a title from framing.
  if (result.headersComplete && !result.chunked && !result.encoded && (!result.contentType[0] || identityContains(result.contentType,sizeof(result.contentType)-1,"html"))) {
    for (size_t i = body; i + 6 < length; ++i) {
      if (!identityEqual(bytes+i,6,"<title") || (bytes[i+6] != '>' && bytes[i+6] != ' ' && bytes[i+6] != '\t')) continue;
      size_t begin = i + 6; while (begin < length && bytes[begin] != '>') ++begin;
      if (++begin >= length) break;
      for (size_t end = begin; end + 8 <= length; ++end) if (identityEqual(bytes+end,8,"</title>")) {
        identityText(bytes+begin,end-begin,result.title,sizeof(result.title),true); i = length; break;
      }
      break;
    }
  }
  if (identityContains(result.title,sizeof(result.title)-1,"laserjet") || identityContains(result.title,sizeof(result.title)-1,"epson") || identityContains(result.title,sizeof(result.title)-1,"brother")) result.category = "Possible printer";
  else if (identityContains(result.title,sizeof(result.title)-1,"synology") || identityContains(result.title,sizeof(result.title)-1,"qnap")) result.category = "Possible NAS";
  else if (identityContains(result.title,sizeof(result.title)-1,"openwrt") || identityContains(result.title,sizeof(result.title)-1,"routeros") || identityContains(result.title,sizeof(result.title)-1,"fritz!box")) result.category = "Possible network appliance";
  else if (result.headersComplete) result.category = "HTTP service; device unknown";
  return true;
}
#endif
