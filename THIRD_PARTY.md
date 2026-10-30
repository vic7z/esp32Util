# Evil-M5Project reference

The probe-history, EAPOL detection, and PCAP features were informed by
[Evil-M5Project](https://github.com/vic7z/Evil-M5Project), snapshot `5262eac`,
particularly the Core2 sketch's probe sniffing, `estUnPaquetEAPOL`, and PCAP
writer. The implementation here uses its own bounded RAM engine, validated
802.11 header parsing, and Radiotap encoder instead of the M5/SD implementation.

The reference project carries this license:

MIT License

Copyright (c) 2023 7h30th3r0n3
Copyright (c) 2026 7h30th3r0n3 (Core2 sketch)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The v2.2 LAN tools also reference the Core2 discovery and port scanning workflow. This implementation uses its own bounded scan engine, lwIP-thread ARP jobs and nonblocking TCP orchestration rather than the blocking M5 interface.

The v2.3 HTTP metadata feature references Core2 `fetchWebsites`, `isLikelyWebPort` and `getHttpContentWithRedirect`. It uses independently implemented bounded parsing and nonblocking sockets, does not follow redirects and does not copy/save websites.
