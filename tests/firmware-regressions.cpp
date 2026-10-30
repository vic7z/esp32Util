#include "../packet_helpers.h"
#include "../json_helpers.h"
#include "../capture_engine.h"

constexpr uint8_t complete[] = {0, 3, 'l', 'a', 'b'};
constexpr uint8_t truncated[] = {0, 32, 'a'};
constexpr uint8_t oversize[] = {0, 33};
constexpr uint8_t emptyTag[] = {1, 0, 0, 1, 'x'};
constexpr uint8_t hidden[] = {0, 0};
constexpr uint8_t tagHeader[] = {0};
static_assert(findSSID(complete, sizeof(complete), 0).found);
static_assert(findSSID(complete, sizeof(complete), 0).length == 3);
static_assert(!findSSID(truncated, sizeof(truncated), 0).found);
static_assert(!findSSID(oversize, sizeof(oversize), 0).found);
static_assert(findSSID(emptyTag, sizeof(emptyTag), 0).offset == 4);
static_assert(findSSID(hidden, sizeof(hidden), 0).found);
static_assert(findSSID(hidden, sizeof(hidden), 0).length == 0);
static_assert(!findSSID(tagHeader, sizeof(tagHeader), 0).found);
static_assert(!findSSID(nullptr, 100, 0).found);
static_assert(!findSSID(complete, sizeof(complete), 999).found);

constexpr bool jsonEscapesControlCharacters() {
  char output[32] = {};
  escapeJson("a\"\\\n", output, sizeof(output));
  const char expected[] = "a\\\"\\\\\\u000a";
  for (size_t i = 0; i < sizeof(expected); ++i) if (output[i] != expected[i]) return false;
  return true;
}
static_assert(jsonEscapesControlCharacters());
constexpr bool jsonPreservesBounds() {
  char output[3] = {'x', 'x', 'z'};
  escapeJson("\n", output, 2);
  return output[0] == 0 && output[2] == 'z';
}
static_assert(jsonPreservesBounds());
constexpr bool jsonRetainsBinaryNames() {
  const char input[] = {'a', 0, 'b'};
  char output[16] = {};
  escapeJsonSpan(input, sizeof(input), output, sizeof(output));
  const char expected[] = "a\\u0000b";
  for (size_t i = 0; i < sizeof(expected); ++i) if (output[i] != expected[i]) return false;
  return true;
}
static_assert(jsonRetainsBinaryNames());
constexpr bool eapolHeaderBounds() {
  uint8_t frame[36] = {};
  frame[0] = 8;
  frame[24] = frame[25] = 0xaa; frame[26] = 3;
  frame[30] = 0x88; frame[31] = 0x8e; frame[32] = 2; frame[33] = 1;
  return isEapol(frame, sizeof(frame)) && !isEapol(frame, sizeof(frame) - 1);
}
static_assert(eapolHeaderBounds());
