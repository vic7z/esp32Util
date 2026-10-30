function escapeHTML(value) {
  return String(value ?? "").replace(
    /[&<>"']/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        c
      ],
  );
}
function authLabel(mode) {
  return (
    {
      0: "Open",
      1: "WEP",
      2: "WPA",
      3: "WPA2",
      4: "WPA/WPA2",
      5: "Enterprise",
      6: "WPA3",
      7: "WPA2/WPA3",
      8: "WAPI",
      9: "OWE",
    }[mode] || "Unknown"
  );
}
function authClass(mode) {
  return mode === 0 ? "open" : mode === 1 ? "wep" : "secure";
}
async function checkedFetch(url, options) {
  const response = await fetch(url, options);
  if (!response.ok)
    throw new Error((await response.text()) || "HTTP " + response.status);
  return response;
}
function csvCell(value) {
  let text = String(value ?? "");
  if (/^[=+@\t\r-]/.test(text)) text = "'" + text;
  return '"' + text.replace(/"/g, '""') + '"';
}
let data = { wifi: [], ble: [], events: [], rogue: [] },
  rssiHist = [],
  pktData = {
    total: 0,
    beacon: 0,
    data: 0,
    deauth: 0,
    probe: 0,
    pps: 0,
    peak: 0,
  },
  pktHistory = [],
  pktVerbose = false,
  pktInterval = null,
  monitoringCh = 0,
  pktLoading = true,
  pktDevices = new Set(),
  pktStartTime = 0;
let captureGeneration = 0;
let isDark = localStorage.getItem("theme") !== "light";
function applyTheme() {
  document.documentElement.setAttribute(
    "data-theme",
    isDark ? "dark" : "light",
  );
  document.getElementById("theme-btn").innerHTML = isDark
    ? "&#9790;"
    : "&#9728;";
  localStorage.setItem("theme", isDark ? "dark" : "light");
}
function toggleTheme() {
  isDark = !isDark;
  applyTheme();
}
applyTheme();
function showTab(t, b) {
  document
    .querySelectorAll(".tab")
    .forEach((x) => x.classList.remove("active"));
  document
    .querySelectorAll(".nav button")
    .forEach((x) => x.classList.remove("active"));
  document.getElementById(t).classList.add("active");
  b.classList.add("active");
}
function toast(m, t = "info") {
  const d = document.createElement("div");
  d.className = "toast " + t;
  d.textContent = m;
  document.body.appendChild(d);
  setTimeout(() => d.remove(), 3000);
}
function initCH() {
  ["ch-bars", "analyzer-ch"].forEach((id) => {
    const g = document.getElementById(id);
    if (!g) return;
    g.innerHTML = "";
    for (let i = 1; i <= 13; i++) {
      const b = document.createElement("div");
      b.className = "ch-bar";
      b.id = id + "-" + i;
      b.style.height = "4px";
      b.setAttribute("data-ch", i);
      b.onclick = () => showPktMonitor(i);
      g.appendChild(b);
    }
  });
}
function getChartColors() {
  return isDark
    ? {
        bg: "#101014",
        grid: "#2a2a36",
        label: "#7a7a8c",
        line: "#f59e0b",
        fill1: "rgba(245,158,11,0.15)",
        fill2: "rgba(245,158,11,0)",
      }
    : {
        bg: "#f4f4f8",
        grid: "#e4e4ec",
        label: "#6b6b80",
        line: "#d97706",
        fill1: "rgba(217,119,6,0.12)",
        fill2: "rgba(217,119,6,0)",
      };
}
function drawChart() {
  const c = document.getElementById("chart"),
    x = c.getContext("2d"),
    w = c.width,
    h = c.height,
    cl = getChartColors();
  x.fillStyle = cl.bg;
  x.fillRect(0, 0, w, h);
  if (rssiHist.length < 2) return;
  x.strokeStyle = cl.grid;
  x.setLineDash([4, 4]);
  for (let i = 0; i < 5; i++) {
    const y = h - 25 - (i * (h - 35)) / 4;
    x.beginPath();
    x.moveTo(40, y);
    x.lineTo(w, y);
    x.stroke();
  }
  x.setLineDash([]);
  x.fillStyle = cl.label;
  x.font = "10px monospace";
  x.textAlign = "right";
  for (let i = 0; i < 5; i++)
    x.fillText(-100 + i * 20 + "", 35, h - 25 - (i * (h - 35)) / 4);
  x.beginPath();
  x.strokeStyle = cl.line;
  x.lineWidth = 2;
  x.lineJoin = "round";
  x.lineCap = "round";
  const step = (w - 50) / (rssiHist.length - 1);
  const pts = [];
  rssiHist.forEach((v, i) => {
    const px = 45 + i * step,
      py = h - 25 - ((v + 100) / 70) * (h - 35);
    pts.push({ x: px, y: py });
    if (i === 0) x.moveTo(px, py);
    else x.lineTo(px, py);
  });
  x.stroke();
  const g = x.createLinearGradient(0, 0, 0, h);
  g.addColorStop(0, cl.fill1);
  g.addColorStop(1, cl.fill2);
  x.fillStyle = g;
  x.lineTo(pts[pts.length - 1].x, h - 25);
  x.lineTo(pts[0].x, h - 25);
  x.fill();
  x.fillStyle = cl.line;
  pts.forEach((p, i) => {
    if (i === pts.length - 1 || i % 3 === 0) {
      x.beginPath();
      x.arc(p.x, p.y, i === pts.length - 1 ? 4 : 2.5, 0, Math.PI * 2);
      x.fill();
    }
  });
}
function sigClass(r) {
  return r > -60 ? "good" : r > -75 ? "fair" : "poor";
}
function sigBars(c) {
  return (
    '<div class="signal ' +
    c +
    '"><div></div><div></div><div></div><div></div></div>'
  );
}
function calcChLoad(w) {
  const cl = {};
  for (let i = 1; i <= 13; i++) cl[i] = 0;
  w.forEach((ap) => {
    if (ap.ch >= 1 && ap.ch <= 13) cl[ap.ch]++;
  });
  return cl;
}
function calcBestCh(cl) {
  let bc = 1,
    ml = 999;
  for (let ch = 1; ch <= 13; ch++)
    if (cl[ch] < ml) {
      ml = cl[ch];
      bc = ch;
    }
  if (cl[1] <= cl[6] && cl[1] <= cl[11]) bc = 1;
  else if (cl[6] <= cl[1] && cl[6] <= cl[11]) bc = 6;
  else if (cl[11] <= cl[1] && cl[11] <= cl[6]) bc = 11;
  return bc;
}
function update() {
  checkedFetch("/api/data")
    .then((r) => r.json())
    .then((d) => {
      data = d;
      const w = d.wifi || [],
        b = d.ble || [],
        e = d.events || [];
      document.getElementById("d-w").textContent = w.length;
      document.getElementById("d-b").textContent = b.length;
      document.getElementById("nav-w").textContent = w.length;
      document.getElementById("nav-b").textContent = b.length;
      const a = e.length;
      document.getElementById("d-a").textContent = a;
      document.getElementById("nav-s").textContent = a;
      document.getElementById("d-a").style.color =
        a > 0 ? "var(--red)" : "var(--green)";
      const sa = document.getElementById("stat-alerts");
      a > 0 ? sa.classList.add("alert") : sa.classList.remove("alert");
      const cl = calcChLoad(w);
      const bc = calcBestCh(cl);
      document.getElementById("d-c").textContent = "CH" + bc;
      if (w.length > 0) {
        const avg = w.reduce((a, b) => a + b.rssi, 0) / w.length;
        rssiHist.push(avg);
        if (rssiHist.length > 40) rssiHist.shift();
      }
      drawChart();
      updateCH(cl, bc);
      updateWiFi(w);
      updateBLE(b);
      const reviewCount = b.filter(
        (device) => device.type > 0 || !device.hasName,
      ).length;
      document.getElementById("s-t").textContent = reviewCount;
      document.getElementById("s-t").style.color = reviewCount
        ? "var(--orange)"
        : "var(--green)";
      updateAlerts(e);
      updateRogue(d.rogue || []);
      updateRec(cl, bc);
      updateSession(d.session);
      updateRfHealth(d.rfHealth);
      updateSecBreakdown(d.security);
      updateHidden(d.hidden);
      updateWalkTest(d);
      updateBaseline(d.baseline);
      fetchCapture();
      fetchLAN();
      document.getElementById("status-text").textContent = "Connected";
      document.getElementById("status-wrap").classList.remove("error");
    })
    .catch((e) => {
      console.error(e);
      document.getElementById("status-text").textContent = "Error";
      document.getElementById("status-wrap").classList.add("error");
    });
}
function updateCH(cl, bc) {
  const mx = Math.max(...Object.values(cl), 1);
  for (let i = 1; i <= 13; i++) {
    const v = cl[i] || 0,
      h = Math.max(4, (v / mx) * 100);
    ["ch-bars", "analyzer-ch"].forEach((id) => {
      const b = document.getElementById(id + "-" + i);
      if (b) {
        b.style.height = h + "px";
        b.className =
          "ch-bar" +
          (v ? " active" : "") +
          (i === bc ? " best" : "") +
          (v > 8 ? " hot" : "");
      }
    });
  }
}
function updateWiFi(w) {
  document.getElementById("w-count").textContent = w.length + " found";
  const t = document.getElementById("w-tbl");
  if (!w.length) {
    t.innerHTML =
      '<tr><td colspan="7" class="empty">No networks found</td></tr>';
    return;
  }
  t.innerHTML = w
    .map((ap, i) => {
      const sc = sigClass(ap.rssi),
        sec = authClass(ap.sec),
        st = authLabel(ap.sec),
        rc =
          ap.rssi > -60
            ? "var(--green)"
            : ap.rssi > -80
              ? "var(--orange)"
              : "var(--red)";
      const gc =
        ap.grade === "A" || ap.grade === "B"
          ? "var(--green)"
          : ap.grade === "C"
            ? "var(--orange)"
            : "var(--red)";
      return (
        '<tr onclick="showAP(' +
        i +
        ')"><td class="col-sig">' +
        sigBars(sc) +
        '</td><td class="col-ssid"><b>' +
        (ap.ssid
          ? escapeHTML(ap.ssid)
          : '<i style="color:var(--muted)">Hidden</i>') +
        '</b></td><td class="col-bssid mono">' +
        escapeHTML(ap.bssid) +
        '</td><td class="col-ch">' +
        ap.ch +
        '</td><td class="col-rssi" style="color:' +
        rc +
        '">' +
        ap.rssi +
        '</td><td class="col-sec"><span class="badge badge-' +
        sec +
        '">' +
        st +
        '</span></td><td style="text-align:center;color:' +
        gc +
        ';font-weight:700">' +
        (ap.grade || "-") +
        "</td></tr>"
      );
    })
    .join("");
}
function updateBLE(b) {
  document.getElementById("b-count").textContent = b.length + " found";
  const t = document.getElementById("b-tbl");
  if (!b.length) {
    t.innerHTML =
      '<tr><td colspan="5" class="empty">No devices found</td></tr>';
    return;
  }
  t.innerHTML = b
    .map((d, i) => {
      const sc = sigClass(d.rssi),
        tp = d.type === 1 ? "iBeacon" : d.type === 2 ? "Eddystone" : "Generic",
        rc = d.rssi > -70 ? "var(--green)" : "var(--orange)";
      return (
        '<tr onclick="showBLE(' +
        i +
        ')"><td class="col-sig">' +
        sigBars(sc) +
        '</td><td class="col-name"><b>' +
        (d.name
          ? escapeHTML(d.name)
          : '<i style="color:var(--muted)">Unknown</i>') +
        '</b></td><td class="col-addr mono">' +
        escapeHTML(d.addr) +
        '</td><td class="col-rssi" style="color:' +
        rc +
        '">' +
        d.rssi +
        '</td><td class="col-type">' +
        tp +
        "</td></tr>"
      );
    })
    .join("");
}
function showAP(i) {
  const ap = data.wifi[i];
  if (!ap) return;
  const m = document.getElementById("ap-modal"),
    b = document.getElementById("ap-modal-body");
  const sec = authLabel(ap.sec),
    sc =
      ap.sec === 0
        ? "var(--red)"
        : ap.sec === 1
          ? "var(--orange)"
          : "var(--green)";
  const gc =
    ap.grade === "A" || ap.grade === "B"
      ? "var(--green)"
      : ap.grade === "C"
        ? "var(--orange)"
        : "var(--red)";
  b.innerHTML =
    '<div class="detail-row"><span class="detail-label">SSID</span><span class="detail-value">' +
    escapeHTML(ap.ssid || "Hidden") +
    '</span></div><div class="detail-row"><span class="detail-label">BSSID</span><span class="detail-value mono">' +
    escapeHTML(ap.bssid) +
    '</span></div><div class="detail-row"><span class="detail-label">Channel</span><span class="detail-value">' +
    ap.ch +
    '</span></div><div class="detail-row"><span class="detail-label">RSSI</span><span class="detail-value">' +
    ap.rssi +
    ' dBm</span></div><div class="detail-row"><span class="detail-label">Security</span><span class="detail-value" style="color:' +
    sc +
    '">' +
    sec +
    '</span></div><div class="detail-row"><span class="detail-label">Vendor</span><span class="detail-value">' +
    escapeHTML(ap.vendor || "Unknown") +
    '</span></div><div class="detail-row"><span class="detail-label">Distance</span><span class="detail-value">~' +
    (ap.distance || "?") +
    'm</span></div><div class="detail-row"><span class="detail-label">Grade</span><span class="detail-value" style="color:' +
    gc +
    ';font-weight:700">' +
    (ap.grade || "-") +
    '</span></div><div style="margin-top:12px;text-align:center"><button class="btn btn-primary" id="start-walk">Walk Test</button></div>';
  document.getElementById("start-walk").onclick = () =>
    startWalk(ap.bssid, ap.ssid);
  m.classList.add("active");
}
function showBLE(i) {
  const d = data.ble[i];
  if (!d) return;
  const m = document.getElementById("ble-modal"),
    b = document.getElementById("ble-modal-body");
  b.innerHTML =
    '<div class="detail-row"><span class="detail-label">Name</span><span class="detail-value">' +
    escapeHTML(d.name || "Unknown") +
    '</span></div><div class="detail-row"><span class="detail-label">Address</span><span class="detail-value mono">' +
    escapeHTML(d.addr) +
    '</span></div><div class="detail-row"><span class="detail-label">RSSI</span><span class="detail-value">' +
    d.rssi +
    " dBm</span></div>";
  m.classList.add("active");
}
function closeModal() {
  document.getElementById("ap-modal").classList.remove("active");
  document.getElementById("ble-modal").classList.remove("active");
  closePktModal();
}
function showPktMonitor(ch) {
  const generation = ++captureGeneration;
  monitoringCh = ch;
  pktVerbose = false;
  pktDevices = new Set();
  pktStartTime = Date.now();
  document.getElementById("pkt-ch-num").textContent = ch;
  document.getElementById("pkt-modal").classList.add("active");
  document.getElementById("pkt-log").innerHTML = "";
  document.getElementById("pkt-log").classList.remove("active");
  document.getElementById("pkt-graph").style.display = "";
  document.getElementById("verb-btn").textContent = "Log";
  document.getElementById("verb-btn").classList.remove("active");
  if (pktInterval) {
    clearInterval(pktInterval);
    pktInterval = null;
  }
  pktHistory = [];
  pktLoading = true;
  [
    "pkt-total",
    "pkt-beacon",
    "pkt-data",
    "pkt-probe",
    "pkt-deauth",
    "pkt-pps",
    "pkt-peak",
  ].forEach((id) => {
    document.getElementById(id).textContent = "\u2026";
  });
  document.getElementById("pkt-unique").textContent = "Devices: 0";
  drawPktGraph();
  checkedFetch("/api/action?type=start_sniffer&channel=" + ch, {
    method: "POST",
  })
    .then((r) => r.text())
    .then((m) => {
      if (generation !== captureGeneration) return;
      toast(m);
      fetchPktData();
      pktInterval = setInterval(fetchPktData, 1000);
    })
    .catch((e) => toast(e.message, "error"));
}
function changePktCh(dir) {
  var ch = monitoringCh + dir;
  if (ch < 1) ch = 13;
  if (ch > 13) ch = 1;
  showPktMonitor(ch);
}
function closePktModal() {
  const wasOpen = document
    .getElementById("pkt-modal")
    .classList.contains("active");
  ++captureGeneration;
  document.getElementById("pkt-modal").classList.remove("active");
  if (pktInterval) {
    clearInterval(pktInterval);
    pktInterval = null;
  }
  pktHistory = [];
  if (wasOpen)
    checkedFetch("/api/action?type=stop_sniffer", { method: "POST" }).catch(
      (e) => toast(e.message, "error"),
    );
}
function fetchPktData() {
  const generation = captureGeneration;
  checkedFetch("/api/packets")
    .then((r) => r.json())
    .then((d) => {
      if (generation !== captureGeneration) return;
      pktData = d;
      pktHistory = d.ppsHistory || [];
      pktLoading = false;
      if (d.channel) {
        monitoringCh = d.channel;
        document.getElementById("pkt-ch-num").textContent = d.channel;
      }
      updatePktDisplay();
      if (!pktVerbose) drawPktGraph();
      if (pktVerbose && d.packets) updatePktLog(d.packets);
      if (d.overlap) updateOverlap(d.overlap);
    })
    .catch((e) => console.error(e));
}
function updatePktDisplay() {
  document.getElementById("pkt-total").textContent = pktData.total;
  document.getElementById("pkt-beacon").textContent = pktData.beacon;
  document.getElementById("pkt-data").textContent = pktData.data;
  document.getElementById("pkt-probe").textContent = pktData.probe || 0;
  document.getElementById("pkt-deauth").textContent = pktData.deauth;
  document.getElementById("pkt-pps").textContent = pktData.pps;
  document.getElementById("pkt-peak").textContent = pktData.peak;
  const ds = document.getElementById("pkt-deauth-stat");
  pktData.deauth > 0 ? ds.classList.add("alert") : ds.classList.remove("alert");
  if (pktData.packets)
    pktData.packets.forEach((p) => {
      if (p.bssid && p.bssid !== "00:00:00:00:00:00") {
        if (pktDevices.size < 1024) pktDevices.add(p.bssid);
      }
    });
  document.getElementById("pkt-unique").textContent =
    "Devices: " + pktDevices.size;
}
function drawPktGraph() {
  const c = document.getElementById("pkt-graph"),
    x = c.getContext("2d"),
    w = c.width,
    h = c.height,
    cl = getChartColors();
  x.fillStyle = cl.bg;
  x.fillRect(0, 0, w, h);
  let hist = pktHistory.slice();
  const fst = hist.findIndex((v) => v > 0);
  if (fst > 0) hist = hist.slice(fst);
  if (hist.length < 2) {
    x.fillStyle = cl.label;
    x.font = "14px -apple-system,sans-serif";
    x.textAlign = "center";
    x.fillText(pktLoading ? "Loading..." : "No data", w / 2, h / 2);
    return;
  }
  const max = Math.max(...hist, 10);
  x.strokeStyle = cl.grid;
  x.setLineDash([4, 4]);
  for (let i = 0; i < 5; i++) {
    const y = h - 30 - (i * (h - 50)) / 4;
    x.beginPath();
    x.moveTo(50, y);
    x.lineTo(w, y);
    x.stroke();
  }
  x.setLineDash([]);
  x.fillStyle = cl.label;
  x.font = "10px monospace";
  x.textAlign = "right";
  for (let i = 0; i < 5; i++)
    x.fillText(Math.round((max * i) / 4) + "", 40, h - 30 - (i * (h - 50)) / 4);
  x.beginPath();
  x.strokeStyle = cl.line;
  x.lineWidth = 2;
  x.lineJoin = "round";
  x.lineCap = "round";
  const step = (w - 70) / (hist.length > 1 ? hist.length - 1 : 1);
  const pts = [];
  hist.forEach((v, i) => {
    const px = 50 + i * step,
      py = h - 30 - (v / max) * (h - 50);
    pts.push({ x: px, y: py });
    if (i === 0) x.moveTo(px, py);
    else x.lineTo(px, py);
  });
  x.stroke();
  const g = x.createLinearGradient(0, 0, 0, h);
  g.addColorStop(0, cl.fill1);
  g.addColorStop(1, cl.fill2);
  x.fillStyle = g;
  x.lineTo(pts[pts.length - 1].x, h - 30);
  x.lineTo(pts[0].x, h - 30);
  x.fill();
  x.fillStyle = cl.line;
  pts.forEach((p, i) => {
    if (i === pts.length - 1 || i % 4 === 0) {
      x.beginPath();
      x.arc(p.x, p.y, i === pts.length - 1 ? 4 : 2, 0, Math.PI * 2);
      x.fill();
    }
  });
}
function updatePktLog(packets) {
  const log = document.getElementById("pkt-log");
  packets
    .filter((p) => p.ch === monitoringCh)
    .forEach((p) => {
      const id = p.ts + "_" + escapeHTML(p.type) + "_" + p.bssid;
      if (!document.getElementById("pkt-" + id)) {
        const e = document.createElement("div");
        e.id = "pkt-" + id;
        e.className = "pkt-entry";
        const rc = p.rssi >= -60 ? "good" : p.rssi >= -80 ? "mid" : "weak";
        const tc =
          p.type === "DEAUTH"
            ? "deauth"
            : p.type === "BEACON"
              ? "beacon"
              : p.type === "PROBE"
                ? "probe"
                : "data";
        const secs = Math.floor(p.ts / 1000);
        const mm = Math.floor(secs / 60);
        const ss = secs % 60;
        const ts = (mm > 0 ? mm + "m" : "") + ss + "s";
        const ssid = p.ssid && p.ssid.length > 0 ? p.ssid : "\u2014";
        e.innerHTML =
          '<span class="pkt-time">' +
          ts +
          '</span><span class="pkt-type ' +
          tc +
          '">' +
          escapeHTML(p.type) +
          '</span><span class="pkt-ssid" title="' +
          escapeHTML(ssid) +
          '">' +
          escapeHTML(ssid) +
          '</span><span class="pkt-bssid">' +
          escapeHTML(p.bssid) +
          '</span><span class="pkt-ch">CH' +
          p.ch +
          '</span><span class="pkt-rssi ' +
          rc +
          '">' +
          p.rssi +
          "dBm</span>";
        log.appendChild(e);
        log.scrollTop = log.scrollHeight;
        if (log.children.length > 100) log.removeChild(log.firstChild);
      }
    });
}
function toggleVerbose() {
  pktVerbose = !pktVerbose;
  const btn = document.getElementById("verb-btn"),
    log = document.getElementById("pkt-log"),
    graph = document.getElementById("pkt-graph");
  btn.textContent = pktVerbose ? "Graph" : "Log";
  btn.classList.toggle("active", pktVerbose);
  log.classList.toggle("active", pktVerbose);
  graph.style.display = pktVerbose ? "none" : "";
}
function exportPktLog() {
  return checkedFetch("/api/packets")
    .then((r) => r.json())
    .then((d) => {
      let csv = "Time,Type,SSID,BSSID,Channel,RSSI\n";
      (d.packets || []).forEach((p) => {
        const secs = Math.floor(p.ts / 1000);
        const mm = Math.floor(secs / 60);
        const ss = secs % 60;
        const ts = (mm > 0 ? mm + "m" : "") + ss + "s";
        csv +=
          [ts, p.type, p.ssid || "", p.bssid, p.ch, p.rssi]
            .map(csvCell)
            .join(",") + "\n";
      });
      const blob = new Blob([csv], { type: "text/csv" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = "packets.csv";
      a.click();
      URL.revokeObjectURL(url);
      toast(
        "Exported " + (d.packets || []).length + " packets (all channels)",
        "success",
      );
    })
    .catch(() => toast("Export failed", "error"));
}
function updateAlerts(e) {
  const c = document.getElementById("s-alerts");
  if (!e || !e.length) {
    c.innerHTML =
      '<div class="empty"><div class="empty-icon">&#128737;</div><p>No security events</p></div>';
    document.getElementById("s-d").textContent = "0";
    document.getElementById("s-d").style.color = "var(--green)";
    document.getElementById("s-r").textContent = "0";
    document.getElementById("s-r").style.color = "var(--green)";
    return;
  }
  let nd = 0,
    nr = 0;
  const h = e
    .slice()
    .reverse()
    .map((ev) => {
      let cls = "info",
        ic = "&#8505;";
      if (ev.type === 0) {
        cls = "critical";
        ic = "&#9888;";
        nd++;
      } else if (ev.type === 1) {
        cls = "warning";
        ic = "&#9888;";
        nr++;
      }
      return (
        '<div class="alert-item ' +
        cls +
        '"><div class="alert-icon">' +
        ic +
        '</div><div class="alert-content"><div class="alert-title">' +
        (ev.type === 0
          ? "Deauth Attack"
          : ev.type === 1
            ? "Rogue AP"
            : ev.type === 2
              ? "Tracker"
              : "System") +
        '</div><div class="alert-meta">' +
        escapeHTML(ev.msg) +
        "</div></div></div>"
      );
    })
    .join("");
  c.innerHTML = h;
  document.getElementById("s-d").textContent = nd;
  document.getElementById("s-d").style.color = nd
    ? "var(--red)"
    : "var(--green)";
  document.getElementById("s-r").textContent = nr;
  document.getElementById("s-r").style.color = nr
    ? "var(--orange)"
    : "var(--green)";
}
function updateRogue(r) {
  const c = document.getElementById("rogue-list");
  if (!r || !r.length) {
    c.innerHTML =
      '<div class="empty"><div class="empty-icon">&#10003;</div><p>No rogue APs detected</p></div>';
    return;
  }
  c.innerHTML = r
    .map(
      (ap) =>
        '<div class="alert-item warning"><div class="alert-icon">&#9888;</div><div class="alert-content"><div class="alert-title">Rogue: ' +
        escapeHTML(ap.ssid) +
        '</div><div class="alert-meta mono">' +
        escapeHTML(ap.bssid1) +
        "</div></div></div>",
    )
    .join("");
}
function updateRec(cl, bc) {
  const c = document.getElementById("ch-rec");
  if (!cl || Object.values(cl).every((v) => v === 0)) {
    c.innerHTML =
      '<div class="empty"><div class="empty-icon">&#128202;</div><p>Run a scan to get recommendations</p></div>';
    return;
  }
  const rec = [1, 6, 11]
    .map((ch) => ({ ch, l: cl[ch] || 0 }))
    .sort((a, b) => a.l - b.l);
  c.innerHTML = rec
    .map(
      (r, i) =>
        '<div class="rec-card ' +
        (i === 0 ? "best" : "") +
        '"><div class="rec-rank">' +
        (i + 1) +
        '</div><div class="rec-info"><div class="rec-title">Channel ' +
        r.ch +
        '</div><div class="rec-sub">' +
        r.l +
        " networks</div></div>" +
        (i === 0
          ? '<span style="color:var(--green);font-weight:600">Best</span>'
          : "") +
        "</div>",
    )
    .join("");
}
function scanWiFi() {
  toast("Scanning WiFi...");
  cmd("scan_wifi").catch(() => {});
}
function scanBLE() {
  toast("Scanning BLE...");
  cmd("scan_ble").catch(() => {});
}
function cmd(t, p) {
  return checkedFetch("/api/action?type=" + t + (p || ""), { method: "POST" })
    .then((r) => r.text())
    .then((m) => {
      toast(m, "success");
      setTimeout(update, 500);
    })
    .catch((e) => {
      toast(e.message, "error");
      return Promise.reject(e);
    });
}
function save() {
  const fd = new FormData();
  fd.append("scanSpeed", document.getElementById("s-speed").value);
  fd.append("rssiThreshold", document.getElementById("s-rssi").value);
  fd.append("rgbBrightness", document.getElementById("s-rgb").value);
  fd.append("deauthThreshold", document.getElementById("s-deauth").value);
  fd.append("screenTimeout", document.getElementById("s-to").value);
  fd.append("powerMode", document.getElementById("s-pwr").value);
  fd.append("apSSID", document.getElementById("s-ap-ssid").value);
  fd.append("apPassword", document.getElementById("s-ap-pass").value);
  toast("Saving...");
  checkedFetch("/api/settings", { method: "POST", body: fd })
    .then((r) => r.text())
    .then((m) => toast(m, "success"))
    .catch((e) => toast(e.message, "error"));
}
function load() {
  checkedFetch("/api/settings")
    .then((r) => r.json())
    .then((s) => {
      document.getElementById("s-speed").value = s.scanSpeed;
      document.getElementById("s-rssi").value = s.rssiThreshold;
      document.getElementById("s-rssi").nextElementSibling.textContent =
        s.rssiThreshold + "dBm";
      document.getElementById("s-rgb").value = s.rgbBrightness;
      document.getElementById("s-rgb").nextElementSibling.textContent =
        s.rgbBrightness + "%";
      document.getElementById("s-deauth").value = s.deauthThreshold;
      document.getElementById("s-to").value = s.screenTimeout;
      document.getElementById("s-pwr").value = s.powerMode;
      document.getElementById("s-ap-ssid").value = s.apSSID || "";
      document.getElementById("s-ap-pass").value = s.apPassword || "";
    })
    .catch(() => toast("Failed to load settings", "error"));
}
let monitorActive = false,
  monitorInterval = null;
async function toggleMonitor() {
  const button = document.getElementById("mon-toggle");
  button.disabled = true;
  try {
    await cmd(monitorActive ? "stop_monitor" : "start_monitor");
    monitorActive = !monitorActive;
    button.textContent = monitorActive ? "Stop" : "Start";
    button.className = monitorActive ? "btn btn-danger" : "btn btn-primary";
    if (monitorInterval) {
      clearInterval(monitorInterval);
      monitorInterval = null;
    }
    if (monitorActive) {
      monitorInterval = setInterval(fetchMonitor, 2000);
      fetchMonitor();
    }
  } catch {
  } finally {
    button.disabled = false;
  }
}
function fetchMonitor() {
  checkedFetch("/api/monitor")
    .then((r) => r.json())
    .then((d) => {
      monitorActive = !!d.active;
      document.getElementById("mon-toggle").textContent = monitorActive
        ? "Stop"
        : "Start";
      document.getElementById("mon-toggle").className = monitorActive
        ? "btn btn-danger"
        : "btn btn-primary";
      document.getElementById("nav-m").textContent = d.count;
      document.getElementById("m-count").textContent = d.count + " devices";
      const t = document.getElementById("m-tbl");
      if (!d.devices || !d.devices.length) {
        t.innerHTML =
          '<tr><td colspan="7" class="empty">' +
          (d.active ? "Scanning..." : "Start monitor to detect clients") +
          "</td></tr>";
        return;
      }
      t.innerHTML = d.devices
        .map((dev) => {
          const tc = dev.type === 0 ? "C" : "B";
          const pc = dev.present ? "+" : "-";
          const rc =
            dev.rssi > -60
              ? "var(--green)"
              : dev.rssi > -80
                ? "var(--orange)"
                : "var(--red)";
          return (
            '<tr><td style="text-align:center;font-weight:700;color:' +
            (dev.type === 0 ? "var(--blue)" : "var(--purple)") +
            '">' +
            tc +
            "</td><td><b>" +
            escapeHTML(dev.name) +
            '</b></td><td class="mono">' +
            escapeHTML(dev.mac) +
            '</td><td style="text-align:center">' +
            (dev.ch || "--") +
            '</td><td style="color:' +
            rc +
            '">' +
            dev.rssi +
            '</td><td style="text-align:center;color:' +
            (dev.present ? "var(--green)" : "var(--muted)") +
            '">' +
            pc +
            '</td><td style="text-align:center">' +
            dev.seen +
            "</td></tr>"
          );
        })
        .join("");
    })
    .catch(() => {});
}
function startWalk(bssid, ssid) {
  closeModal();
  cmd("track_wifi", "&bssid=" + encodeURIComponent(bssid))
    .then(() => {
      document.getElementById("walk-card").style.display = "";
      document.getElementById("walk-target").textContent =
        " - " + (ssid || bssid);
    })
    .catch(() => {});
}
function stopWalk() {
  cmd("stop_track")
    .then(() => {
      document.getElementById("walk-card").style.display = "none";
    })
    .catch(() => {});
}
function drawWalkChart(graph) {
  const c = document.getElementById("walk-chart");
  if (!c) return;
  const x = c.getContext("2d"),
    w = c.width,
    h = c.height,
    cl = getChartColors();
  x.fillStyle = cl.bg;
  x.fillRect(0, 0, w, h);
  const valid = graph.filter((v) => v !== 0 && v !== -100);
  if (valid.length < 2) {
    x.fillStyle = cl.label;
    x.font = "13px -apple-system,sans-serif";
    x.textAlign = "center";
    x.fillText("Collecting data...", w / 2, h / 2);
    return;
  }
  x.strokeStyle = cl.grid;
  x.setLineDash([4, 4]);
  for (let i = 0; i < 5; i++) {
    const y = h - 25 - (i * (h - 35)) / 4;
    x.beginPath();
    x.moveTo(40, y);
    x.lineTo(w, y);
    x.stroke();
  }
  x.setLineDash([]);
  x.fillStyle = cl.label;
  x.font = "10px monospace";
  x.textAlign = "right";
  for (let i = 0; i < 5; i++)
    x.fillText(-100 + i * 20 + "", 35, h - 25 - (i * (h - 35)) / 4);
  x.beginPath();
  x.strokeStyle = "#10b981";
  x.lineWidth = 2;
  x.lineJoin = "round";
  x.lineCap = "round";
  const step = (w - 50) / (graph.length - 1);
  const pts = [];
  graph.forEach((v, i) => {
    const clamped = Math.max(-100, Math.min(-20, v === 0 ? -100 : v));
    const px = 45 + i * step,
      py = h - 25 - ((clamped + 100) / 80) * (h - 35);
    pts.push({ x: px, y: py });
    if (i === 0) x.moveTo(px, py);
    else x.lineTo(px, py);
  });
  x.stroke();
  const g = x.createLinearGradient(0, 0, 0, h);
  g.addColorStop(0, "rgba(16,185,129,0.15)");
  g.addColorStop(1, "rgba(16,185,129,0)");
  x.fillStyle = g;
  x.lineTo(pts[pts.length - 1].x, h - 25);
  x.lineTo(pts[0].x, h - 25);
  x.fill();
}
function updateSession(s) {
  if (!s) return;
  const mins = Math.floor(s.uptime / 60000);
  const hrs = Math.floor(mins / 60);
  document.getElementById("d-up").textContent =
    hrs > 0 ? hrs + "h" + (mins % 60) + "m" : mins + "m";
  const el = document.getElementById("session-stats");
  el.innerHTML =
    '<div class="detail-row"><span class="detail-label">Total APs Found</span><span class="detail-value">' +
    s.totalAPs +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Peak PPS</span><span class="detail-value">' +
    s.peakPPS +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Total Packets</span><span class="detail-value">' +
    s.totalPkts.toLocaleString() +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Deauth Detected</span><span class="detail-value" style="color:' +
    (s.totalDeauth > 0 ? "var(--red)" : "var(--green)") +
    '">' +
    s.totalDeauth +
    "</span></div>" +
    (s.attackActive
      ? '<div class="detail-row"><span class="detail-label" style="color:var(--red);font-weight:700">ATTACK ACTIVE</span><span class="detail-value" style="color:var(--red)">CH ' +
        s.deauthCh +
        " (" +
        s.deauthPS +
        "/s)</span></div>"
      : "");
}
function updateRfHealth(rf) {
  if (!rf) return;
  const lc =
    rf.load < 20
      ? "var(--green)"
      : rf.load < 50
        ? "var(--orange)"
        : "var(--red)";
  document.getElementById("d-load").textContent = rf.load + "%";
  document.getElementById("d-load").style.color = lc;
  document.getElementById("rf-health").innerHTML =
    '<div class="detail-row"><span class="detail-label">RF Load</span><span class="detail-value" style="color:' +
    lc +
    '">' +
    rf.load +
    "% (" +
    rf.quality +
    ")</span></div>" +
    '<div class="detail-row"><span class="detail-label">Insight</span><span class="detail-value">' +
    rf.insight +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Best Channel</span><span class="detail-value" style="color:var(--green)">CH ' +
    rf.bestCh +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Worst Channel</span><span class="detail-value" style="color:var(--red)">CH ' +
    rf.worstCh +
    "</span></div>";
}
function updateSecBreakdown(sc) {
  if (!sc) return;
  const total = sc.open + sc.wep + sc.wpa + sc.wpa2 + sc.wpa3;
  if (total === 0) {
    document.getElementById("sec-breakdown").innerHTML =
      '<div class="empty"><div class="empty-icon">&#128274;</div><p>Scan WiFi to see security breakdown</p></div>';
    return;
  }
  const bars = [
    { l: "WPA3", v: sc.wpa3, c: "var(--green)" },
    { l: "WPA2", v: sc.wpa2, c: "var(--green)" },
    { l: "WPA", v: sc.wpa, c: "var(--orange)" },
    { l: "WEP", v: sc.wep, c: "var(--red)" },
    { l: "Open", v: sc.open, c: "var(--red)" },
  ];
  document.getElementById("sec-breakdown").innerHTML = bars
    .map(
      (b) =>
        '<div style="display:flex;align-items:center;gap:8px;margin-bottom:6px"><span style="min-width:40px;font-size:.78rem;color:var(--muted)">' +
        b.l +
        '</span><div style="flex:1;height:8px;background:var(--border);border-radius:4px;overflow:hidden"><div style="width:' +
        (b.v / total) * 100 +
        "%;height:100%;background:" +
        b.c +
        ';border-radius:4px;transition:width .3s"></div></div><span style="min-width:20px;font-size:.78rem;font-weight:600">' +
        b.v +
        "</span></div>",
    )
    .join("");
}
function updateHidden(h) {
  const card = document.getElementById("hidden-card");
  if (!h || !h.length) {
    card.style.display = "none";
    return;
  }
  card.style.display = "";
  document.getElementById("h-count").textContent = h.length;
  document.getElementById("h-tbl").innerHTML = h
    .map((n) => {
      const rc =
        n.rssi > -60
          ? "var(--green)"
          : n.rssi > -80
            ? "var(--orange)"
            : "var(--red)";
      return (
        "<tr><td><b>" +
        escapeHTML(n.ssid) +
        '</b></td><td style="text-align:center">' +
        n.ch +
        '</td><td style="color:' +
        rc +
        '">' +
        n.rssi +
        "</td></tr>"
      );
    })
    .join("");
}
function updateBaseline(bl) {
  const el = document.getElementById("baseline-body");
  if (!bl || !bl.saved) {
    el.innerHTML =
      '<div class="empty"><div class="empty-icon">&#128202;</div><p>Take a snapshot and save as baseline</p></div>';
    return;
  }
  const apDiff = bl.cAPs - bl.bAPs;
  const rssiDiff = bl.cRSSI - bl.bRSSI;
  el.innerHTML =
    '<div class="detail-row"><span class="detail-label">Baseline APs</span><span class="detail-value">' +
    bl.bAPs +
    "</span></div>" +
    '<div class="detail-row"><span class="detail-label">Current APs</span><span class="detail-value">' +
    bl.cAPs +
    ' <span style="color:' +
    (apDiff >= 0 ? "var(--green)" : "var(--red)") +
    '">(' +
    (apDiff >= 0 ? "+" : "") +
    apDiff +
    ")</span></span></div>" +
    '<div class="detail-row"><span class="detail-label">Baseline RSSI</span><span class="detail-value">' +
    bl.bRSSI +
    " dBm</span></div>" +
    '<div class="detail-row"><span class="detail-label">Current RSSI</span><span class="detail-value">' +
    bl.cRSSI +
    ' dBm <span style="color:' +
    (rssiDiff >= 0 ? "var(--green)" : "var(--red)") +
    '">(' +
    (rssiDiff >= 0 ? "+" : "") +
    rssiDiff +
    ")</span></span></div>" +
    '<div class="detail-row"><span class="detail-label">Packets</span><span class="detail-value">' +
    bl.cPkts.toLocaleString() +
    " vs " +
    bl.bPkts.toLocaleString() +
    "</span></div>";
}
function updateWalkTest(d) {
  const card = document.getElementById("walk-card");
  if (!d.walkActive) {
    card.style.display = "none";
    return;
  }
  card.style.display = "";
  const g = d.graph || [];
  const valid = g.filter((v) => v !== 0 && v !== -100);
  if (valid.length > 0) {
    const cur = valid[valid.length - 1];
    const mn = Math.min(...valid);
    const mx = Math.max(...valid);
    const avg = Math.round(valid.reduce((a, b) => a + b, 0) / valid.length);
    document.getElementById("walk-stats").innerHTML =
      '<div class="pkt-stats" style="grid-template-columns:repeat(4,1fr)"><div class="pkt-stat"><div class="pkt-stat-value">' +
      cur +
      '</div><div class="pkt-stat-label">Current</div></div><div class="pkt-stat"><div class="pkt-stat-value">' +
      mn +
      '</div><div class="pkt-stat-label">Min</div></div><div class="pkt-stat"><div class="pkt-stat-value">' +
      mx +
      '</div><div class="pkt-stat-label">Max</div></div><div class="pkt-stat"><div class="pkt-stat-value">' +
      avg +
      '</div><div class="pkt-stat-label">Avg</div></div></div>';
  }
  drawWalkChart(g);
}
function initOverlapBars() {
  const g = document.getElementById("overlap-bars");
  if (!g) return;
  g.innerHTML = "";
  for (let i = 1; i <= 13; i++) {
    const b = document.createElement("div");
    b.className = "ch-bar";
    b.id = "overlap-" + i;
    b.style.height = "4px";
    b.setAttribute("data-ch", i);
    g.appendChild(b);
  }
}
function updateOverlap(overlap) {
  if (!overlap) return;
  const mx = Math.max(...overlap, 1);
  for (let i = 0; i < 13; i++) {
    const b = document.getElementById("overlap-" + (i + 1));
    if (b) {
      const h = Math.max(4, (overlap[i] / mx) * 60);
      b.style.height = h + "px";
      b.className =
        "ch-bar" +
        (overlap[i] > 0 ? " active" : "") +
        (overlap[i] > 3 ? " hot" : "");
    }
  }
}
let capturePollInFlight = false,
  captureBusy = false;
function captureTime(ms) {
  const seconds = Math.floor(Number(ms) / 1000);
  return Math.floor(seconds / 60) + "m " + (seconds % 60) + "s";
}
function updateCapture(state) {
  const status = state.scanning
    ? "WiFi scan in progress; listening paused"
    : state.listening
      ? "Listening"
      : "Stopped";
  document.getElementById("capture-status").textContent =
    status +
    " · " +
    state.probeRequests +
    " probe requests · " +
    (state.probes || []).length +
    "/24 history entries · " +
    state.evicted +
    " evicted";
  const channels = (state.eapolByChannel || [])
    .map((count, i) => (count ? "CH" + (i + 1) + ": " + count : ""))
    .filter(Boolean)
    .join(", ");
  document.getElementById("capture-eapol").textContent =
    "EAPOL: " +
    state.eapol +
    (channels ? " (" + channels + ")" : "") +
    " · Authentication activity.";
  document.getElementById("capture-buffer").textContent =
    (state.recording ? "Recording" : "PCAP stopped") +
    ": " +
    state.frames +
    "/" +
    state.capacity +
    " packets · " +
    state.dropped +
    " dropped · " +
    state.truncated +
    " truncated. Timestamps use device uptime; channel/RSSI are included.";
  document.getElementById("capture-listen").disabled =
    captureBusy || state.listening || state.scanning;
  document.getElementById("capture-stop").disabled =
    captureBusy || !state.listening;
  document.getElementById("capture-record").disabled =
    captureBusy || state.scanning || state.recording;
  document.getElementById("capture-stop-pcap").disabled =
    captureBusy || !state.recording;
  document.getElementById("capture-download").disabled =
    captureBusy || !state.frames;
  const probes = (state.probes || [])
    .slice()
    .sort((a, b) => b.lastMs - a.lastMs);
  document.getElementById("capture-probes").innerHTML = probes.length
    ? probes
        .map(
          (probe) =>
            "<tr><td>" +
            escapeHTML(probe.wildcard ? "(wildcard)" : probe.ssid) +
            '</td><td class="mono">' +
            escapeHTML(probe.source) +
            "</td><td>" +
            probe.ch +
            "</td><td>" +
            probe.rssi +
            "</td><td>" +
            probe.count +
            "</td><td>" +
            captureTime(probe.firstMs) +
            "</td><td>" +
            captureTime(probe.lastMs) +
            "</td></tr>",
        )
        .join("")
    : '<tr><td colspan="7" class="empty">No probe requests recorded yet</td></tr>';
}
async function fetchCapture() {
  if (capturePollInFlight) return;
  capturePollInFlight = true;
  try {
    const response = await checkedFetch("/api/capture");
    updateCapture(await response.json());
  } catch {
    document.getElementById("capture-status").textContent =
      "Capture status unavailable; retrying on the next update.";
  } finally {
    capturePollInFlight = false;
  }
}
async function captureAction(type) {
  if (captureBusy) return;
  captureBusy = true;
  document
    .querySelectorAll(".capture-actions button")
    .forEach((button) => (button.disabled = true));
  try {
    await cmd(type);
  } catch {
    /* cmd already displays the server error. */
  } finally {
    captureBusy = false;
    document
      .querySelectorAll(".capture-actions button")
      .forEach((button) => (button.disabled = false));
    await fetchCapture();
  }
}
async function downloadPCAP() {
  if (captureBusy) return;
  captureBusy = true;
  document.getElementById("capture-download").disabled = true;
  try {
    const response = await checkedFetch("/api/capture.pcap");
    const blob = await response.blob();
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = "esp32-capture.pcap";
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    toast("PCAP downloaded; recording stopped", "success");
  } catch (error) {
    toast(error.message, "error");
  } finally {
    captureBusy = false;
    await fetchCapture();
  }
}
let lanPollInFlight = false,
  lanBusy = false,
  lanSignature = "";
const lanCommonPorts = {
  21: "FTP",
  22: "SSH",
  23: "Telnet",
  25: "SMTP",
  53: "DNS",
  80: "HTTP",
  110: "POP3",
  139: "NetBIOS",
  143: "IMAP",
  443: "HTTPS",
  445: "SMB",
  3306: "MySQL",
  3389: "RDP",
  8080: "HTTP alternate",
  8443: "HTTPS alternate",
  9090: "Web alternate",
};
function selectLanHost(ip) {
  document.getElementById("lan-target").value = ip;
}
function updateLAN(state) {
  const connected = state.connection === "connected",
    connecting = state.connection === "connecting";
  const running = state.progress === "running";
  document.getElementById("lan-status").textContent =
    state.connection +
    (connected
      ? " · " + state.ssid + " · " + state.ip + " / " + state.mask
      : "") +
    " · " +
    state.message;
  document.getElementById("lan-progress").textContent =
    state.job === "none"
      ? "No LAN scan running."
      : state.job +
        ": " +
        state.progress +
        " · " +
        state.completed +
        "/" +
        state.total +
        " checked · " +
        state.errors +
        " errors · " +
        state.omitted +
        " neighbors omitted";
  const signature = state.ip + "/" + state.mask;
  if (connected && signature !== lanSignature) {
    lanSignature = signature;
    document.getElementById("lan-first").value = state.first;
    document.getElementById("lan-last").value = state.last;
    document.getElementById("lan-target").value = "";
  }
  if (!connected && !connecting) lanSignature = "";
  document.getElementById("lan-connect").disabled =
    lanBusy || connected || connecting;
  document.getElementById("lan-disconnect").disabled =
    lanBusy || (!connected && !connecting);
  document.getElementById("lan-discover").disabled =
    lanBusy || !connected || running;
  document.getElementById("lan-scan-ports").disabled =
    lanBusy || !connected || running;
  document.getElementById("lan-identify").disabled =
    lanBusy || !connected || running;
  const identity = state.identity || {};
  document.getElementById("lan-identity-details").hidden = !identity.status;
  document.getElementById("lan-identity-status").textContent =
    identity.state && identity.state !== "idle"
      ? identity.target +
        ":" +
        identity.port +
        " · " +
        identity.state +
        (identity.limited ? " · partial / sample limit" : "") +
        (identity.chunked || identity.encoded
          ? " · encoded body: headers only"
          : "")
      : "Select a host to read its HTTP identity.";
  for (const [field, value] of Object.entries({
    category: identity.category,
    code: identity.status,
    title: identity.title,
    server: identity.server,
    type: identity.contentType,
  })) {
    document.getElementById("lan-identity-" + field).textContent =
      value || "Not reported";
  }
  document.getElementById("lan-cancel").disabled = lanBusy || !running;
  const hosts = state.hosts || [];
  document.getElementById("lan-hosts").innerHTML = hosts.length
    ? hosts
        .map(
          (host, index) =>
            "<tr><td>" +
            escapeHTML(host.ip) +
            '</td><td class="mono">' +
            escapeHTML(host.mac) +
            "</td><td>" +
            escapeHTML(host.vendor) +
            '</td><td><button type="button" class="btn" data-lan-host="' +
            index +
            '">Use host</button></td></tr>',
        )
        .join("")
    : '<tr><td colspan="4" class="empty">No ARP neighbors recorded</td></tr>';
  document
    .querySelectorAll("#lan-hosts button[data-lan-host]")
    .forEach(
      (button) =>
        (button.onclick = () =>
          selectLanHost(hosts[Number(button.dataset.lanHost)].ip)),
    );
  document.getElementById("lan-port-caption").textContent =
    "TCP ports · " + (state.portTarget || state.target || "No target");
  document.getElementById("lan-port-results").innerHTML = (state.ports || [])
    .length
    ? state.ports
        .map(
          (port) =>
            "<tr><td>" +
            port.port +
            "</td><td>" +
            escapeHTML(port.state) +
            "</td><td>" +
            escapeHTML(lanCommonPorts[port.port] || "Unassigned label") +
            "</td></tr>",
        )
        .join("")
    : '<tr><td colspan="3" class="empty">No port scan results yet</td></tr>';
}
async function fetchLAN() {
  if (lanPollInFlight) return;
  lanPollInFlight = true;
  try {
    const response = await checkedFetch("/api/lan");
    updateLAN(await response.json());
  } catch {
    document.getElementById("lan-status").textContent =
      "LAN status unavailable; retrying on the next update.";
  } finally {
    lanPollInFlight = false;
  }
}
async function lanAction(type, fields = {}) {
  if (lanBusy) return;
  lanBusy = true;
  document
    .querySelectorAll(".lan-actions button")
    .forEach((button) => (button.disabled = true));
  const body = new FormData();
  body.append("type", type);
  Object.entries(fields).forEach(([key, value]) => body.append(key, value));
  try {
    const response = await checkedFetch("/api/lan/action", {
      method: "POST",
      body,
    });
    toast(await response.text(), "success");
    if (type === "connect") document.getElementById("lan-password").value = "";
  } catch (error) {
    toast(error.message, "error");
  } finally {
    lanBusy = false;
    document
      .querySelectorAll(".lan-actions button")
      .forEach((button) => (button.disabled = false));
    await fetchLAN();
  }
}
function connectLAN(event) {
  event.preventDefault();
  return lanAction("connect", {
    ssid: document.getElementById("lan-ssid").value,
    password: document.getElementById("lan-password").value,
  });
}
function discoverLAN(event) {
  event.preventDefault();
  return lanAction("discover", {
    first: document.getElementById("lan-first").value,
    last: document.getElementById("lan-last").value,
  });
}
function scanLANPorts(event) {
  event.preventDefault();
  return lanAction("ports", {
    target: document.getElementById("lan-target").value,
    ports: document.getElementById("lan-ports").value,
  });
}
initCH();
initOverlapBars();
load();
update();
setInterval(update, 2000);

function identifyLAN(event) {
  event.preventDefault();
  return lanAction("identify", {
    target: document.getElementById("lan-target").value,
    port: document.getElementById("lan-http-port").value,
  });
}
