# ESP32 utility stabilization and feature plan

## Current scope
Stabilization and v2.1.0 are complete. The user also authorized local network discovery, port scanning and HTTP device identification for the C3. Review baseline: esp32Util e1c3adc; Evil-M5Project 5262eac. The user has now authorized flashing; v2.3.0 was uploaded to the connected C3 on COM3 (see final validation below).

## v2.1.0 feature implementation
- [x] Inspect M5 probe/EAPOL/PCAP code and preserve reference/license attribution.
- [x] Add fixed probe history with source/SSID identity, wildcard support, first/last timestamps, observation counts, and LRU eviction.
- [x] Validate plain/QoS/four-address/HT-control EAPOL headers, reject encrypted/fragmented/A-MSDU/truncated inputs, and count activity per channel.
- [x] Add a 32-packet RAM capture with explicit truncation/drop counts, 64-bit uptime, and Radiotap channel/RSSI metadata.
- [x] Integrate callback-safe snapshots, web start/stop/clear/download controls, EAPOL log labels, and OLED summaries.
- [x] Execute the actual C++ engine/encoder through a freestanding WASM harness and independently decode the produced PCAP; cover capacity, malformed input, and timestamp rollover boundaries.
- [x] Test HTML escaping, download controls, HTTP failure recovery, desktop/phone layout, and a browser download with matching checksums using synthetic fixtures.
- [x] Complete final C3 build, rebuild the SPIFFS image, and verify artifact/source checksums.

Feature limitations: RAM-only history/capture, 24 probe identities, 32 raw records, 768 bytes per raw frame, web capture fixed to the AP channel, and no claim of handshake completeness. No physical-device or Wireshark GUI validation has been performed. The standalone binary decoder validates the generated PCAP/Radiotap format.

### Final v2.1.0 results
- C3 build passed: 1,413,695 bytes (44%) of application storage; 82,332 bytes (25%) of static RAM; 245,348 bytes remain before runtime allocations.
- Firmware checksum and all 36 staged source-file hashes match the working source.
- Rebuilt and unpacked `build/spiffs.bin`; all five extracted files match their data-source checksums.
- Capture engine/PCAP and browser regressions passed through `scripts/test-capture.ps1`; C3 compile-time parser/JSON assertions also passed.
- Desktop and 390-pixel phone viewport checked with synthetic fixture data; no horizontal page overflow. Browser PCAP download checksum matches the fixture produced by the production encoder.
- Generated firmware: `build/c3/firmware.bin`; web image: `build/spiffs.bin`; metadata: `build/c3/build-info.json`; synthetic PCAP fixture: `build/capture-test.pcap`; UI preview: `build/capture-preview.png`.
- No device was flashed, no release binaries were overwritten, and no changes were pushed.

The interface detector also reported existing whole-page contrast/typography/decorative warnings. New capture controls/table typography and button contrast were corrected locally; unrelated interface styling is preserved.

## Stabilization
- [x] Inspect boot, scan engines, radio transitions, menus, settings, web API, and browser code.
- [x] Fix frame bounds, counter resets, scan completion handling, and radio ownership.
- [x] Fix BLE capacity handling, monitor freshness, list navigation, and sleep behavior.
- [x] Fix settings validation, JSON serialization, HTML injection, and security labels.
- [x] Run regression tests and compile the primary ESP32-C3 target.
- [x] Record results and remaining physical-device checks.

## Feature roadmap after stabilization
1. Implemented in v2.1.0: probe-request history with SSID, source, channel, RSSI, first/last seen, OLED summaries, and web details.
2. Implemented in v2.1.0: EAPOL activity detection with validated parsing and per-channel authentication activity counters.
3. Implemented in v2.1.0: bounded RAM PCAP export with drop/truncation counts, timestamps, and Radiotap metadata. Format/transport validation passed; opening on Wireshark and physical RF validation remain pending.
4. Implemented in v2.2.0: local network discovery and bounded TCP checks; physical validation pending.
5. Optional GPS survey logging: confirm available pins and storage hardware first.

Each feature needs a separate implementation and validation pass. Do not merge the M5 sketches or introduce their hardware dependencies wholesale.

## Hardware acceptance checks
Not yet performed: OLED/button operation; scan-to-monitor transitions; web connectivity during capture; BLE/Wi-Fi coexistence; repeated web-server entry/exit; device disappearance/reappearance; sleep and reset; measured memory and power usage on the actual board.

Flashing was initially deferred because no cable was available. The user later connected the C3 and authorized flashing; v2.3.0 was uploaded and verified on COM3. Physical application acceptance checks remain pending.

## Repairs in this pass

| Area | Defects repaired |
| --- | --- |
| Packet parsing | Reject short headers and truncated SSID elements; support empty intervening tags and SSIDs ending exactly at the buffer boundary. |
| Radio initialization | Let Arduino initialize its driver and register scan-complete events instead of initializing the same driver twice. |
| Wi-Fi scans | Share checked result handling across OLED/web modes; retain completed data during an unfinished scan, clear zero-result scans, preserve 32-byte names, and update security counts consistently. |
| Capture | Attribute channel counters in the receive callback; retain web connectivity by capturing on the AP channel; reset all channel/log counters together; filter the device's own AP traffic even when its MAC begins with zero. |
| Rates and alerts | Calculate packet rates with elapsed time; refresh stats outside the Live Monitor; use lifetime deauth totals across capture resets; clear alerts when traffic falls below the threshold. |
| BLE | Scan asynchronously, refresh known devices even when the table is full, identify advertised iBeacon/Eddystone types, clamp cursors after expiry, and compile out BLE on unsupported chips. |
| Client monitoring | Start the OLED Wi-Fi client monitor; defer callback observations to a bounded queue; count fresh BLE observations rather than every UI poll; saturate visit counters and reuse absent slots at capacity. |
| Navigation | Avoid modulo-by-zero in AP comparison, clamp list cursors, guard missing AP/BLE detail records, and prevent a zero-range graph division. |
| Power/storage | Return early while asleep, exempt active scanning screens, avoid automatically formatting SPIFFS on a mount error, and avoid applying LED brightness twice. |
| Vendor lookup | Validate packed table lengths and reads, free old allocations on reload, and terminate fixed-width vendor names before use. |
| Settings | Validate complete updates before mutation/narrowing/persistence, reject invalid SSIDs/password lengths, repair invalid stored values, and clarify the actual default-password behavior. |
| Web responses | Stream bounded JSON fragments, escape control characters/quotes without losing names, snapshot callback-owned packet/name records, and register HTTP routes once. |
| Browser | Escape radio-derived HTML; replace SSID-containing inline JavaScript with an event handler; correct authentication enum labels; report HTTP errors; preserve monitor state on failed requests; stop stale capture requests from restarting polling; escape CSV cells and bound the unique-device set. |
| Security interpretation | Require an authentication mismatch for duplicate-SSID warnings; label BLE candidates and probed names accurately; distinguish system events from rogue-AP events. |

## Stabilization validation (before v2.1.0)

- Browser JavaScript syntax check: passed.
- `node tests/web-regressions.cjs`: passed, including malicious names, CSV downloads, auth labels, rejected requests, and monitor state.
- `riscv32-esp-elf-g++ -std=c++17 -fsyntax-only tests/firmware-regressions.cpp`: passed compile-time assertions for valid/truncated/oversized SSID elements, empty tags, control-character JSON escaping, and destination bounds.
- Browser smoke test with local fixture data: connected dashboard, correct Open/WPA2 labels, and literal special-character names in the AP list/detail modal.
- Impeccable mechanical check of browser JavaScript: no findings.
- Primary build target: `esp32:esp32:esp32c3:PartitionScheme=huge_app`, Arduino ESP32 core 3.3.7, U8g2 2.36.19, Adafruit NeoPixel 1.15.5.
- Build environment: Windows Application Control blocks the bundled GNU archiver. The build script can use Zig 0.13.0's LLVM archive tool instead; security settings are unchanged.
- SPIFFS image generated from all five data files with the 917,504-byte Huge APP partition size; vendor tables included. Output: `build/spiffs.bin`.
- Final firmware build: passed. Sketch uses 1,407,889 bytes (44% of the 3 MB application partition); static globals use 53,692 bytes (16%), leaving 273,988 bytes before runtime allocations.
- Verified the saved firmware checksum and all 33 staged source-file checksums against the working source. Build metadata: `build/c3/build-info.json`; firmware: `build/c3/firmware.bin`; bootloader and partition table are alongside it.
- Rechecked JavaScript syntax, browser regressions, compile-time firmware assertions, whitespace errors, and the five SPIFFS image entries after the final build. All passed.
- Other chip targets and physical behavior remain unverified. No flashing was performed.
- Optional removal of temporary downloads/build copies was blocked by automatic policy review. Those ignored files remain; project sources and final outputs are unaffected.

This is a source audit and regression pass, not proof that every possible defect has been eliminated. Hardware checks above are required before treating the firmware as device-validated.

## v2.2.0 implementation and validation
- [x] RAM-only station connection, 20-second timeout, radio coordination and disconnect cleanup.
- [x] ARP discovery: 254 addresses, 32 retained neighbors, subnet/AP/self validation, cancellation and stale callback protection.
- [x] Nonblocking TCP: one socket, 32 unique ports, 350 ms deadline, distinct outcomes and cleanup on cancellation/network change.
- [x] Web connection/discovery/port controls, escaped results, host selection, progress and failure handling.
- [x] Production C++ engine and transport orchestration tested through WASM simulations: malformed inputs, subnet boundaries, capacity, callback generations, worker/queue failures, socket errors, deadlines/rollover, cancellation and network loss/address changes.
- [x] C3 build passed: 1,425,345 bytes (45%); static RAM 82,972 bytes (25%); 244,708 bytes remain before runtime allocations.

Physical validation remains pending: DHCP/AP channel transitions, actual ARP/TCP responses, client isolation, responsiveness, memory pressure, reconnect and sleep. Simulated tests do not establish physical radio behavior. No desktop network scan or flashing was performed.
- Final checks passed: LAN policy/runtime/browser regressions, capture/PCAP regressions, JavaScript syntax, C3 parser/JSON assertions and whitespace validation.
- Verified firmware checksum and all 39 staged source hashes. Rebuilt/unpacked SPIFFS; all five entries match source hashes.
- Browser synthetic fixture confirmed host selection and scan submission; phone layout had no page overflow and browser logged no errors. Preview: `build/lan-preview.png`.
- Outputs: `build/c3/firmware.bin`, `build/spiffs.bin`, `build/c3/build-info.json`. No flashing, release overwrite or push.

## OLED readability pass (2026-10-30)
- Larger 6x10 text for menus and primary scan/monitor content; three spacious rows with one selected row and a position indicator. Existing button events and menu ordering preserved.
- Quiet shared headers/footers replace filled title banners, shadow dots, boxed button labels and footer rules. Tap/hold hints added to primary screens.
- WiFi, BLE and client lists show names and signal strength; secondary information stays on detail screens/web. Long text is sanitized to the supported ASCII font and ellipsized by pixel width.
- Simplified Auto Watch, RF Health, live trace, and detail screens. Removed decorative graph texture and redundant signal bars. Changed the web screen to use configured SSID instead of fixed credentials.
- `scripts/test-oled.ps1` extracts current production drawing functions and renders them through U8g2 2.36.19 with the real fonts into 128x64 buffers, using synthetic observations. Sixteen fixtures cover primary layouts, empty lists, long names, large counters, paused monitoring and menu boundaries; zero text overflow. Twelve layouts visually inspected in `build/oled-preview.png`. This simulates pixels, not OLED brightness or physical readability.
- C3 build passed: 1,422,377 bytes (45%) flash; 82,972 bytes (25%) static RAM; 244,708 bytes before runtime allocations. Firmware and all 39 staged source hashes verified.
- LAN, capture/PCAP, browser, JavaScript syntax and C3 parser assertions passed. Web assets unchanged during this pass; existing SPIFFS image remains applicable.
- Physical OLED/button checks remain pending; flashing remains deferred. No release binaries overwritten or changes pushed.

### Graph preference follow-up
Restored the live monitor's original 24-pixel filled/patterned graph, dotted grid, frame and pause badge while retaining the clearer top metrics and menus/lists. The channel analyzer bars/patterns were unchanged by the readability pass; its original inverted title banner is now restored, as is the shared RSSI graph banner. Kept wide arithmetic for graph scaling and adjusted the pause glyph one pixel to remain in bounds. Updated the actual U8g2 preview to include the channel analyzer; all 16 layout fixtures pass with zero text overflow. Physical testing/flashing remains deferred.
- Final graph follow-up: C3 compile passed (45% flash, 25% static RAM); layout fixtures include patterned live graphs and channel analyzer. Dedicated synthetic preview: `build/oled-channel-preview.png`. Final saved source/firmware hash verification follows the rebuild.
- Final firmware and all 39 staged source hashes match the restored-graph source.

### Full Live Monitor restoration
Restored the complete pre-cleanup Live Monitor layout: inverted channel/packet-rate/load header, packet-type counters, RSSI bar/value and threshold indicator, 24-pixel patterned graph and pause badge, and boxed footer. Retained the pre-cleanup v2.1 EAPOL/probe footer and safe arithmetic/text bounds. Its legacy footer is isolated to Live Monitor; other screens retain their current cleaner UI. Sixteen actual-U8g2 layout fixtures pass; dedicated synthetic render: `build/oled-live-preview.png`. No flashing.
- Final C3 build passed: 1,423,383 bytes (45%) flash, 82,972 bytes (25%) static RAM. Firmware and all 39 staged source hashes verified against current source.

## v2.3.0 device identification
- [x] Review M5 website-fetch workflow and adapt HTTP evidence collection for C3 resources.
- [x] Add one-host/one-port `GET /`, same-subnet exclusions, single nonblocking socket, partial I/O, 350 ms connect / 2.5 s overall deadlines, 4 KB sample and 512-byte reads per loop.
- [x] Parse HTTP status, Server, content type and completed HTML title with header/field bounds, whitespace/entity handling, no redirects/authentication/page execution, conservative encoded-body behavior and tentative category labels.
- [x] Add web controls and inert text rendering; retain prior port results, preserve OLED layouts and clear identity on network changes.
- [x] Test the actual production orchestration/parser through simulated socket responses: strict/malformed status, incomplete responses, field/header/sample caps, entity/case variants, non-HTML/compressed/chunked bodies, local-target/port restrictions, partial sends/reads, redirect non-following, one socket, cancellation, network changes, errors, deadlines and timer rollover. Existing LAN/browser regressions pass.
- Physical validation remains pending: real embedded HTTP variants and socket timing, memory pressure, AP responsiveness and reconnection. All transport tests are synthetic; no actual desktop network scans or device flashing.

### v2.3 final validation and flashing
- C3 build passed: 1,429,293 bytes (45%) flash; static RAM 87,564 bytes (26%); 240,116 bytes remain before runtime allocations.
- Verified final firmware checksum and all 40 staged source-file hashes against the working source. Rebuilt and unpacked SPIFFS; all five files match their data-source hashes.
- LAN policy/runtime/HTTP-identification/browser checks, capture/PCAP checks, JavaScript syntax and C3 parser assertions passed. Desktop and 390-pixel phone workflow checked with synthetic fixture data, no page overflow or browser errors. Preview: `build/identity-preview.png`.
- The interface detector reports existing whole-page typography/decorative/contrast advisories; new identification controls use the existing LAN-specific dark-text button rules and inert text output. Unrelated page design preserved.
- User subsequently connected the board and explicitly authorized flashing. Detected ESP32-C3 revision v0.4, 4 MB flash, USB-Serial/JTAG on COM3. Uploaded generated bootloader (0x0), partition table (0x8000), official boot_app0 (0xe000), firmware (0x10000), and SPIFFS (0x310000); all five writes passed esptool data-hash verification, then the device was reset. NVS sector was not part of the write.
- No startup text was received during a six-second USB serial check. Application/OLED behavior and real-network identification are not yet physically verified. No network scan, release-binary overwrite or push was performed.
- Build defines confirm USB CDC-on-boot is disabled (`ARDUINO_USB_CDC_ON_BOOT=0`), so the silent USB serial check does not establish an application failure.
