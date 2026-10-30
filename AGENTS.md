# Repository guidance

## Hardware and scope
- Primary target: ESP32-C3, 128x64 SSD1306 I2C OLED, two buttons, NeoPixel.
- Preserve the existing controls and web layout when fixing bugs.
- Do not claim ESP32-S2 BLE support: that chip has no Bluetooth radio.
- Treat Evil-M5Project as a reference, not a drop-in dependency. Preserve attribution and license notices for copied code.

## Implementation
- Keep packet callbacks bounded: validate frame lengths before every field read, and defer file, serial, and vendor lookup work to the main loop.
- Coordinate scan, promiscuous capture, client monitoring, and AP modes explicitly. An AP serving the web interface must keep its operating channel.
- Check driver return values; failed or incomplete scans must not publish stale/partial records.
- Validate settings before narrowing integer types or saving. Escape radio-derived text for JSON and HTML independently.
- Keep bounded arrays, cursors, counters, and ring buffers valid at zero and full capacity.
- Never flash attached hardware, overwrite release binaries, push, or publish without a user request.

## Verification
- Compile the C3 sketch with the documented Arduino core and libraries when available.
- Run `node --check data/app.js` and the regression checks in `tests/`.
- Document exact checks and remaining hardware validation in PLAN.md. A successful compile does not verify RF timing, power draw, or physical buttons.
- Keep changes focused; do not add M5 features during the stabilization pass.
- After stabilization, implement only user-authorized roadmap features. For capture changes run `scripts/test-capture.ps1` (Node.js plus full Zig distribution) and the C3 firmware assertions; keep PCAP data bounded, preserve Radiotap/length/timestamp invariants, and recheck software and hardware validation status in PLAN.md.

- For LAN changes run `scripts/test-lan.ps1`; keep targets on the connected subnet, credentials RAM-only, work bounded/cancellable and ARP calls on the lwIP task. Automated tests must use simulated transports.
- For OLED changes run `scripts/test-oled.ps1`, inspect the actual U8g2 raster preview and compile C3. Keep primary text at 6x10 where space permits, fit untrusted names by pixel width and preserve tap/hold/back behavior.
- Preserve the existing channel analyzer and patterned graphs: the user prefers their original styling. OLED readability changes should focus on menus, lists and labels.
- Live Monitor is explicitly exempt from the cleaner OLED UI: preserve its full pre-cleanup layout, including counters, RSSI bar, title banner, patterned graph and boxed footer.
- For HTTP identification keep one selected local host/port, bounded nonblocking I/O, explicit unverified categories and inert text output. Run `scripts/test-lan.ps1`; never follow redirects or enable authentication/page execution implicitly.
