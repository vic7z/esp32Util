#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H
#include <stddef.h>
#include <stdint.h>

// Destination needs at most 6 * input bytes + 1 bytes. No surrounding quotes.
constexpr void escapeJsonSpan(const char* source, size_t length, char* target, size_t capacity) {
  if (capacity == 0) return;
  const char hex[] = "0123456789abcdef";
  size_t used = 0;
  for (size_t i = 0; i < length; ++i) {
    const char* p = source + i;
    unsigned char c = static_cast<unsigned char>(*p);
    size_t needed = c < 32 ? 6 : (c == '"' || c == '\\' ? 2 : 1);
    if (used + needed >= capacity) break;
    if (c < 32) {
      target[used++] = '\\'; target[used++] = 'u';
      target[used++] = '0'; target[used++] = '0';
      target[used++] = hex[c >> 4]; target[used++] = hex[c & 15];
    } else {
      if (*p == '"' || *p == '\\') target[used++] = '\\';
      target[used++] = *p;
    }
  }
  target[used] = 0;
}
constexpr void escapeJson(const char* source, char* target, size_t capacity) {
  size_t length = 0;
  while (source[length]) ++length;
  escapeJsonSpan(source, length, target, capacity);
}
#endif
