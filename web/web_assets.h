#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

inline String getIndexHtml() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 RF Tool</title>
    <style>
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0a0a0f;color:#fff;line-height:1.6}
        .header{background:#12121a;padding:15px 20px;border-bottom:1px solid #2a2a3a;position:sticky;top:0;z-index:100}
        .header-content{max-width:1400px;margin:0 auto;display:flex;justify-content:space-between;align-items:center}
        .logo{display:flex;align-items:center;gap:10px}
        .logo-icon{width:36px;height:36px;background:linear-gradient(135deg,#00d4ff,#aa66ff);border-radius:8px;display:flex;align-items:center;justify-content:center;font-size:18px}
        .logo-text h1{font-size:1.1rem;background:linear-gradient(90deg,#00d4ff,#00ff88);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
        .status{display:flex;align-items:center;gap:8px;padding:6px 12px;background:#1a1a25;border-radius:20px;font-size:.8rem}
        .status-dot{width:8px;height:8px;border-radius:50%;background:#00ff88;animation:pulse 2s infinite}
        @keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
        .nav{display:flex;gap:5px;padding:10px 20px;background:#12121a;border-bottom:1px solid #2a2a3a;overflow-x:auto}
        .nav button{padding:8px 16px;background:transparent;border:none;color:#a0a0b0;font-size:.85rem;cursor:pointer;border-radius:6px;white-space:nowrap}
        .nav button:hover{background:#252535;color:#fff}
        .nav button.active{background:linear-gradient(135deg,#00d4ff,#aa66ff);color:#000;font-weight:600}
        .main{max-width:1400px;margin:0 auto;padding:20px}
        .tab{display:none}
        .tab.active{display:block}
        .stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px;margin-bottom:20px}
        .stat-card{background:#1a1a25;border-radius:10px;padding:15px;border:1px solid #2a2a3a}
        .stat-label{font-size:.7rem;color:#a0a0b0;text-transform:uppercase}
        .stat-value{font-size:1.6rem;font-weight:700;margin-top:5px}
        .card{background:#1a1a25;border-radius:10px;border:1px solid #2a2a3a;margin-bottom:15px;overflow:hidden}
        .card-header{padding:12px 15px;background:#12121a;border-bottom:1px solid #2a2a3a;display:flex;justify-content:space-between;align-items:center}
        .card-title{font-size:.95rem;font-weight:600}
        .card-body{padding:15px}
        .btn{padding:8px 16px;border:none;border-radius:6px;font-size:.85rem;font-weight:600;cursor:pointer}
        .btn-primary{background:linear-gradient(135deg,#00d4ff,#aa66ff);color:#000}
        .btn-secondary{background:#252535;color:#fff;border:1px solid #2a2a3a}
        .btn-danger{background:#ff4444;color:#fff}
        .btn-sm{padding:5px 10px;font-size:.75rem}
        .btn-group{display:flex;gap:8px;flex-wrap:wrap}
        .table-container{overflow-x:auto;border-radius:6px;background:#12121a;max-height:400px;overflow-y:auto}
        table{width:100%;border-collapse:collapse;font-size:.85rem;table-layout:fixed}
        th{text-align:left;padding:10px 8px;background:#1a1a25;color:#a0a0b0;font-size:.7rem;text-transform:uppercase;position:sticky;top:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
        td{padding:10px 8px;border-bottom:1px solid #2a2a3a;cursor:pointer;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;vertical-align:middle}
        tr:hover td{background:#252535}
        .col-sig{width:40px}
        .col-ssid{width:25%}
        .col-bssid{width:110px}
        .col-ch{width:50px}
        .col-rssi{width:70px}
        .col-sec{width:70px}
        .col-name{width:30%}
        .col-addr{width:130px}
        .col-type{width:80px}
        .mono{font-family:monospace;font-size:.8rem}
        .signal{display:flex;align-items:flex-end;gap:2px;height:16px;width:32px}
        .signal div{width:4px;background:#606070;border-radius:1px;min-height:4px}
        .signal div:nth-child(1){height:25%}.signal div:nth-child(2){height:50%}.signal div:nth-child(3){height:75%}.signal div:nth-child(4){height:100%}
        .signal.good div{background:#00ff88}.signal.fair div:nth-child(-n+2){background:#ffaa00}.signal.poor div:nth-child(1){background:#ff4444}
        .badge{display:inline-block;padding:3px 8px;border-radius:10px;font-size:.65rem;font-weight:600;text-transform:uppercase}
        .badge-open{background:rgba(255,68,68,.2);color:#ff4444}.badge-secure{background:rgba(0,255,136,.2);color:#00ff88}
        .alert{display:flex;align-items:flex-start;gap:12px;padding:12px;background:#12121a;border-radius:8px;margin-bottom:8px;border-left:3px solid #00d4ff}
        .alert.deauth{border-left-color:#ff4444;background:rgba(255,68,68,.1)}.alert.rogue{border-left-color:#ffaa00;background:rgba(255,170,0,.1)}
        .alert-icon{width:32px;height:32px;border-radius:6px;display:flex;align-items:center;justify-content:center;font-size:1rem;background:#252535}
        .alert.deauth .alert-icon{background:rgba(255,68,68,.2)}.alert.rogue .alert-icon{background:rgba(255,170,0,.2)}
        .alert-content{flex:1}.alert-title{font-weight:600;font-size:.85rem}.alert-msg{font-size:.75rem;color:#a0a0b0}
        .form-group{margin-bottom:12px}.form-group label{display:block;margin-bottom:5px;font-size:.8rem;color:#a0a0b0}
        select,input{width:100%;padding:10px;background:#12121a;border:1px solid #2a2a3a;border-radius:6px;color:#fff;font-size:.85rem}
        .range{display:flex;align-items:center;gap:10px}.range input{flex:1}.range-val{min-width:50px;text-align:right;font-family:monospace;color:#00d4ff;font-weight:600;font-size:.85rem}
        .empty{text-align:center;padding:40px;color:#606070}.empty-icon{font-size:3rem;margin-bottom:10px;opacity:.5}
        .grid-2{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:15px}
        .modal{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.9);display:none;align-items:center;justify-content:center;z-index:1000;padding:20px}
        .modal.active{display:flex}.modal-content{background:#1a1a25;border-radius:12px;border:1px solid #2a2a3a;max-width:500px;width:100%;max-height:80vh;overflow-y:auto}
        .modal-header{padding:15px;border-bottom:1px solid #2a2a3a;display:flex;justify-content:space-between;align-items:center;background:#12121a}
        .modal-body{padding:15px}.modal-footer{padding:15px;border-top:1px solid #2a2a3a;display:flex;gap:10px;justify-content:flex-end;background:#12121a}
        .close-btn{background:none;border:none;color:#a0a0b0;font-size:1.5rem;cursor:pointer}
        .detail-row{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #2a2a3a}.detail-row:last-child{border-bottom:none}
        .detail-label{color:#a0a0b0;font-size:.85rem}.detail-value{font-weight:600;font-size:.85rem}
        .rec-card{background:#12121a;padding:12px;border-radius:8px;border:1px solid #2a2a3a;margin-bottom:8px;display:flex;align-items:center;gap:12px}
        .rec-card.best{border-color:#00ff88;background:rgba(0,255,136,.1)}.rec-rank{width:28px;height:28px;border-radius:50%;background:#252535;display:flex;align-items:center;justify-content:center;font-weight:700;font-size:.8rem}
        .rec-card.best .rec-rank{background:#00ff88;color:#000}.rec-info{flex:1}.rec-title{font-weight:600;font-size:.9rem}.rec-sub{font-size:.75rem;color:#606070}
        @media(max-width:768px){.stats{grid-template-columns:repeat(2,1fr)}}
        ::-webkit-scrollbar{width:6px;height:6px}::-webkit-scrollbar-track{background:#12121a}::-webkit-scrollbar-thumb{background:#2a2a3a;border-radius:3px}
        .channels{display:grid;grid-template-columns:repeat(13,1fr);gap:3px;margin-top:10px;height:100px;align-items:end}
        .ch-bar{background:#252535;border-radius:3px 3px 0 0;min-height:4px;transition:all .3s;position:relative;cursor:pointer}
        .ch-bar.active{background:linear-gradient(to top,#00d4ff,#aa66ff)}.ch-bar.best{box-shadow:0 0 10px #00ff88}.ch-bar.hot{background:linear-gradient(to top,#ff4444,#ffaa00)}
        .ch-tooltip{position:absolute;bottom:100%;left:50%;transform:translateX(-50%);background:#000;padding:4px 8px;border-radius:4px;font-size:.7rem;white-space:nowrap;opacity:0;transition:opacity .2s;pointer-events:none}
        .ch-bar:hover .ch-tooltip{opacity:1}
        .pkt-modal .modal-content{max-width:800px}
        .pkt-stats{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:15px}
        .pkt-stat{background:#12121a;padding:12px;border-radius:8px;text-align:center}
        .pkt-stat-value{font-size:1.5rem;font-weight:700;color:#00d4ff}
        .pkt-stat-label{font-size:.7rem;color:#808090;margin-top:4px}
        .pkt-stat.alert .pkt-stat-value{color:#ff4444}
        .pkt-content{background:#0a0a0f;border:1px solid #2a2a3a;border-radius:8px;padding:12px;height:250px;position:relative}
        .pkt-graph{display:none;width:100%;height:100%}
        .pkt-graph.active{display:block}
        .pkt-log{display:none;width:100%;height:100%;overflow-y:auto;font-family:monospace;font-size:.8rem;line-height:1.4}
        .pkt-log.active{display:block}
        .pkt-log .pkt-entry{display:flex;gap:10px;padding:3px 0;border-bottom:1px solid #1a1a25}
        .ch-packets{display:flex;gap:3px;align-items:end;height:60px;background:#0a0a0f;padding:10px;border-radius:8px;border:1px solid #2a2a3a;margin-top:10px}
        .ch-pkt-bar{flex:1;background:#252535;border-radius:3px 3px 0 0;min-height:4px;transition:all .3s}
        .ch-pkt-bar.active{background:linear-gradient(to top,#00d4ff,#aa66ff);box-shadow:0 0 8px rgba(0,212,255,.5)}
        .ch-pkt-bar:hover{background:#3a3a4a}
        .pkt-log.verbose .pkt-entry{display:flex}
        .pkt-log.simple .pkt-entry.simple{display:flex;gap:10px;padding:3px 0;border-bottom:1px solid #1a1a25}
        .pkt-time{color:#606070;min-width:60px}
        .pkt-type{color:#00d4ff;min-width:80px}
        .pkt-ch{color:#aa66ff;min-width:40px}
        .pkt-info{color:#a0a0b0;flex:1}
        .pkt-deauth .pkt-type{color:#ff4444}
        .pkt-beacon .pkt-type{color:#00ff88}
        .toggle-btn{background:#252535;border:1px solid #2a2a3a;padding:6px 12px;border-radius:6px;color:#fff;cursor:pointer;font-size:.8rem}
        .toggle-btn.active{background:linear-gradient(135deg,#00d4ff,#aa66ff);color:#000;border-color:transparent;font-weight:600}
        .ch-packets{display:flex;gap:2px;align-items:end;height:40px;margin-top:8px}
        .ch-pkt-bar{flex:1;background:#252535;border-radius:2px 2px 0 0;min-height:2px}
        .ch-pkt-bar.active{background:linear-gradient(to top,#00d4ff,#aa66ff)}
    </style>
</head>
<body>
    <header class="header">
        <div class="header-content">
            <div class="logo">
                <div class="logo-icon">📡</div>
                <div class="logo-text"><h1>ESP32 RF Tool</h1></div>
            </div>
            <div class="status"><div class="status-dot"></div><span id="status">Connected</span></div>
        </div>
    </header>
    <nav class="nav">
        <button class="active" onclick="showTab('dash',this)">📊 Dashboard</button>
        <button onclick="showTab('wifi',this)">📶 WiFi</button>
        <button onclick="showTab('ble',this)">🔷 BLE</button>
        <button onclick="showTab('analyzer',this)">📡 Analyzer</button>
        <button onclick="showTab('security',this)">🛡️ Security</button>
        <button onclick="showTab('set',this)">⚙️ Settings</button>
    </nav>
    <main class="main">
        <div id="dash" class="tab active">
            <div class="stats">
                <div class="stat-card"><div class="stat-label">WiFi Networks</div><div class="stat-value" id="d-w">0</div></div>
                <div class="stat-card"><div class="stat-label">BLE Devices</div><div class="stat-value" id="d-b">0</div></div>
                <div class="stat-card"><div class="stat-label">Alerts</div><div class="stat-value" id="d-a" style="color:#00ff88">0</div></div>
                <div class="stat-card"><div class="stat-label">Best Channel</div><div class="stat-value" id="d-c">--</div></div>
            </div>
            <div class="grid-2">
                <div class="card"><div class="card-header"><div class="card-title">📡 Signal History</div></div><div class="card-body"><canvas id="chart" width="600" height="150"></canvas></div></div>
                <div class="card"><div class="card-header"><div class="card-title">📊 Channel Distribution</div></div><div class="card-body"><div class="channels" id="ch-bars"></div></div></div>
            </div>
        </div>
        <div id="wifi" class="tab">
            <div class="card">
                <div class="card-header"><div class="card-title">📶 WiFi Networks <span id="w-count" style="background:#252535;padding:3px 10px;border-radius:10px;font-size:.8rem;margin-left:8px">0</span></div><div class="btn-group"><button class="btn btn-primary" onclick="scanWiFi()">🔍 Scan</button><button class="btn btn-danger" onclick="cmd('clear')">🗑️ Clear</button></div></div>
                <div class="card-body">
                    <div class="table-container">
                        <table>
                            <thead>
                                <tr>
                                    <th class="col-sig">Sig</th>
                                    <th class="col-ssid">SSID</th>
                                    <th class="col-bssid">BSSID</th>
                                    <th class="col-ch">Ch</th>
                                    <th class="col-rssi">RSSI</th>
                                    <th class="col-sec">Sec</th>
                                </tr>
                            </thead>
                            <tbody id="w-tbl"></tbody>
                        </table>
                    </div>
                    <p style="color:#606070;font-size:.75rem;margin-top:10px">💡 Click any row for details</p>
                </div>
            </div>
        </div>
        <div id="ble" class="tab">
            <div class="card">
                <div class="card-header"><div class="card-title">🔷 BLE Devices <span id="b-count" style="background:#252535;padding:3px 10px;border-radius:10px;font-size:.8rem;margin-left:8px">0</span></div><div class="btn-group"><button class="btn btn-primary" onclick="scanBLE()">🔍 Scan</button></div></div>
                <div class="card-body">
                    <div class="table-container">
                        <table>
                            <thead>
                                <tr>
                                    <th class="col-sig">Sig</th>
                                    <th class="col-name">Name</th>
                                    <th class="col-addr">Address</th>
                                    <th class="col-rssi">RSSI</th>
                                    <th class="col-type">Type</th>
                                </tr>
                            </thead>
                            <tbody id="b-tbl"></tbody>
                        </table>
                    </div>
                    <p style="color:#606070;font-size:.75rem;margin-top:10px">💡 Click any row for details</p>
                </div>
            </div>
        </div>
        <div id="analyzer" class="tab">
            <div class="grid-2">
                <div class="card">
                    <div class="card-header"><div class="card-title">📊 Channel Usage</div><button class="btn btn-sm btn-primary" onclick="scanWiFi()">🔄 Refresh</button></div>
                    <div class="card-body"><div class="channels" id="analyzer-ch"></div></div>
                </div>
                <div class="card">
                    <div class="card-header"><div class="card-title">⭐ Recommendations</div></div>
                    <div class="card-body" id="ch-rec"><div class="empty"><div class="empty-icon">📊</div><p>Scan to get recommendations</p></div></div>
                </div>
            </div>
        </div>
        <div id="security" class="tab">
            <div class="stats">
                <div class="stat-card"><div class="stat-label">Deauth</div><div class="stat-value" id="s-d" style="color:#00ff88">0</div></div>
                <div class="stat-card"><div class="stat-label">Rogue APs</div><div class="stat-value" id="s-r" style="color:#00ff88">0</div></div>
                <div class="stat-card"><div class="stat-label">Trackers</div><div class="stat-value" id="s-t" style="color:#00ff88">0</div></div>
            </div>
            <div class="grid-2">
                <div class="card"><div class="card-header"><div class="card-title">🚨 Rogue APs</div></div><div class="card-body" id="rogue-list"><div class="empty"><div class="empty-icon">✅</div><p>No rogue APs</p></div></div></div>
                <div class="card"><div class="card-header"><div class="card-title">📋 Security Events</div></div><div class="card-body" id="s-alerts"><div class="empty"><div class="empty-icon">🛡️</div><p>No security events</p></div></div></div>
            </div>
        </div>
        <div id="set" class="tab">
            <div class="card">
                <div class="card-header"><div class="card-title">⚙️ Device Settings</div><button class="btn btn-primary" onclick="save()">💾 Save</button></div>
                <div class="card-body">
                    <div class="grid-2">
                        <div class="form-group"><label>Scan Speed</label><select id="s-speed"><option value="0">Fast</option><option value="1">Normal</option><option value="2">Slow</option></select></div>
                        <div class="form-group"><label>RSSI Threshold</label><div class="range"><input type="range" id="s-rssi" min="-100" max="-40" oninput="this.nextElementSibling.textContent=this.value+'dBm'"><span class="range-val">-70dBm</span></div></div>
                        <div class="form-group"><label>RGB Brightness</label><div class="range"><input type="range" id="s-rgb" min="0" max="100" step="5" oninput="this.nextElementSibling.textContent=this.value+'%'"><span class="range-val">50%</span></div></div>
                        <div class="form-group"><label>Screen Timeout</label><select id="s-to"><option value="0">Never</option><option value="30">30s</option><option value="60">1m</option><option value="120">2m</option><option value="300">5m</option></select></div>
                        <div class="form-group"><label>Deauth Threshold</label><input type="number" id="s-deauth" min="1" max="100" value="10"></div>
                        <div class="form-group"><label>Power Mode</label><select id="s-pwr"><option value="0">Balanced</option><option value="1">Performance</option><option value="2">Power Save</option></select></div>
                    </div>
                </div>
            </div>
            <div class="card">
                <div class="card-header"><div class="card-title">📡 WiFi AP Settings</div><span style="color:#ffaa00;font-size:.75rem">Restart required after change</span></div>
                <div class="card-body">
                    <div class="grid-2">
                        <div class="form-group"><label>AP Name (SSID)</label><input type="text" id="s-ap-ssid" maxlength="32" placeholder="ESP32-Tool"></div>
                        <div class="form-group"><label>AP Password (empty = open)</label><input type="text" id="s-ap-pass" maxlength="64" placeholder="Leave empty for open network"></div>
                    </div>
                    <p style="color:#606070;font-size:.75rem;margin-top:10px">💡 Leave password empty to create an open network (no password required)</p>
                </div>
            </div>
        </div>
    </main>
    <div class="modal" id="ap-modal">
        <div class="modal-content">
            <div class="modal-header"><h3>📶 AP Details</h3><button class="close-btn" onclick="closeModal()">&times;</button></div>
            <div class="modal-body" id="ap-modal-body"></div>
            <div class="modal-footer"><button class="btn btn-secondary" onclick="closeModal()">Close</button></div>
        </div>
    </div>
    <div class="modal" id="ble-modal">
        <div class="modal-content">
            <div class="modal-header"><h3>🔷 BLE Details</h3><button class="close-btn" onclick="closeModal()">&times;</button></div>
            <div class="modal-body" id="ble-modal-body"></div>
            <div class="modal-footer"><button class="btn btn-secondary" onclick="closeModal()">Close</button></div>
        </div>
    </div>
    <div class="modal pkt-modal" id="pkt-modal">
        <div class="modal-content">
            <div class="modal-header"><h3>📡 Packet Monitor - Channel <span id="pkt-ch-num">--</span></h3><button class="close-btn" onclick="closePktModal()">&times;</button></div>
            <div class="modal-body">
                <div class="pkt-stats">
                    <div class="pkt-stat"><div class="pkt-stat-value" id="pkt-total">0</div><div class="pkt-stat-label">Total Packets</div></div>
                    <div class="pkt-stat"><div class="pkt-stat-value" id="pkt-beacon">0</div><div class="pkt-stat-label">Beacons</div></div>
                    <div class="pkt-stat"><div class="pkt-stat-value" id="pkt-data">0</div><div class="pkt-stat-label">Data</div></div>
                    <div class="pkt-stat alert" id="pkt-deauth-stat"><div class="pkt-stat-value" id="pkt-deauth">0</div><div class="pkt-stat-label">Deauth</div></div>
                </div>
                <div style="display:flex;gap:10px;margin-bottom:10px;align-items:center">
                    <button class="toggle-btn active" id="verb-btn" onclick="toggleVerbose()">📊 Graph Mode</button>
                    <button class="btn btn-sm btn-primary" onclick="exportPktLog()">💾 Export</button>
                    <span style="color:#606070;font-size:.75rem;margin-left:auto">PPS: <span id="pkt-pps" style="color:#00d4ff;font-weight:700">0</span></span>
                    <span style="color:#606070;font-size:.75rem">Peak: <span id="pkt-peak" style="color:#00ff88;font-weight:700">0</span></span>
                </div>
                <div class="pkt-content">
                    <canvas class="pkt-graph active" id="pkt-graph" width="700" height="230"></canvas>
                    <div class="pkt-log" id="pkt-log"></div>
                </div>
                <div style="margin-top:10px"><div class="ch-packets" id="ch-pkt-bars"></div></div>
            </div>
            <div class="modal-footer"><button class="btn btn-secondary" onclick="closePktModal()">Close</button></div>
        </div>
    </div>
    <script>
        let data={wifi:[],ble:[],events:[],rogue:[]},rssiHist=[];
        function showTab(t,b){document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.nav button').forEach(x=>x.classList.remove('active'));document.getElementById(t).classList.add('active');b.classList.add('active')}
        function initCH(){['ch-bars','analyzer-ch'].forEach(id=>{const g=document.getElementById(id);g.innerHTML='';for(let i=1;i<=13;i++){const d=document.createElement('div');d.className='ch-bar';d.id=id+'-'+i;d.style.height='4px';d.innerHTML='<div class="ch-tooltip">CH'+i+': 0 APs<br>Click to monitor</div>';d.onclick=()=>showPktMonitor(i);g.appendChild(d)}})}
        function drawChart(){const c=document.getElementById('chart'),x=c.getContext('2d'),w=c.width,h=c.height;x.fillStyle='#000';x.fillRect(0,0,w,h);if(rssiHist.length<2)return;x.strokeStyle='#2a2a3a';for(let i=0;i<5;i++){const y=h-25-i*(h-35)/4;x.beginPath();x.moveTo(40,y);x.lineTo(w,y);x.stroke()}x.fillStyle='#606070';x.font='10px monospace';x.textAlign='right';for(let i=0;i<5;i++){x.fillText(-100+i*20+'',35,h-25-i*(h-35)/4)}x.beginPath();x.strokeStyle='#00d4ff';x.lineWidth=2;const step=(w-50)/(rssiHist.length-1);rssiHist.forEach((v,i)=>{if(v===null)return;const y=h-25-((v+100)/70)*(h-35);if(i===0)x.moveTo(45,y);else x.lineTo(45+i*step,y)});x.stroke()}
        function sigClass(r){return r>-60?'good':r>-75?'fair':'poor'}
        function sigBars(c){return'<div class="signal '+c+'"><div></div><div></div><div></div><div></div></div>'}
        function calcChLoad(w){const cl={};for(let i=1;i<=13;i++)cl[i]=0;w.forEach(ap=>{if(ap.ch>=1&&ap.ch<=13)cl[ap.ch]++});return cl}
        function calcBestCh(cl){let bc=1,ml=999;for(let ch=1;ch<=13;ch++)if(cl[ch]<ml){ml=cl[ch];bc=ch}if(cl[1]<=cl[6]&&cl[1]<=cl[11])bc=1;else if(cl[6]<=cl[1]&&cl[6]<=cl[11])bc=6;else if(cl[11]<=cl[1]&&cl[11]<=cl[6])bc=11;return bc}
        function update(){fetch('/api/data').then(r=>{if(!r.ok)throw new Error('HTTP '+r.status);return r.json()}).then(d=>{data=d;document.getElementById('d-w').textContent=d.wifi?d.wifi.length:0;document.getElementById('d-b').textContent=d.ble?d.ble.length:0;const a=d.events?d.events.length:0;document.getElementById('d-a').textContent=a;document.getElementById('d-a').style.color=a>0?'#ff4444':'#00ff88';const cl=calcChLoad(d.wifi||[]);const bc=calcBestCh(cl);document.getElementById('d-c').textContent='CH'+bc;if(d.wifi&&d.wifi.length>0){const avg=d.wifi.reduce((a,b)=>a+b.rssi,0)/d.wifi.length;rssiHist.push(avg);if(rssiHist.length>40)rssiHist.shift()}drawChart();updateCH(cl,bc);updateWiFi(d.wifi||[]);updateBLE(d.ble||[]);updateAlerts(d.events||[]);updateRogue(d.rogue||[]);updateRec(cl,bc);document.getElementById('status').textContent='Connected'}).catch(e=>{console.error('Update error:',e);document.getElementById('status').textContent='Error: '+e.message})}
        function updateCH(cl,bc){const mx=Math.max(...Object.values(cl),1);for(let i=1;i<=13;i++){const v=cl[i]||0,h=Math.max(4,(v/mx)*100);['ch-bars','analyzer-ch'].forEach(id=>{const b=document.getElementById(id+'-'+i);if(b){b.style.height=h+'px';b.className='ch-bar'+(v?' active':'')+(i===bc?' best':'')+(v>8?' hot':'');const tt=b.querySelector('.ch-tooltip');if(tt)tt.textContent='CH'+i+': '+v+' APs'}})}}
        function updateWiFi(w){document.getElementById('w-count').textContent=w.length;const t=document.getElementById('w-tbl');if(!w.length){t.innerHTML='<tr><td colspan="6" style="text-align:center;color:#606070;padding:30px">No networks found</td></tr>';return}t.innerHTML=w.map((ap,i)=>{const sc=sigClass(ap.rssi),sec=ap.sec===7?'open':'secure',st=ap.sec===7?'Open':'WPA2';return'<tr onclick="showAP('+i+')"><td class="col-sig">'+sigBars(sc)+'</td><td class="col-ssid"><b>'+(ap.ssid||'<i style="color:#606070">Hidden</i>')+'</b></td><td class="col-bssid mono">'+ap.bssid+'</td><td class="col-ch">'+ap.ch+'</td><td class="col-rssi" style="color:'+(ap.rssi>-60?'#00ff88':ap.rssi>-80?'#ffaa00':'#ff4444')+'">'+ap.rssi+'</td><td class="col-sec"><span class="badge badge-'+sec+'">'+st+'</span></td></tr>'}).join('')}
        function updateBLE(b){document.getElementById('b-count').textContent=b.length;const t=document.getElementById('b-tbl');if(!b.length){t.innerHTML='<tr><td colspan="5" style="text-align:center;color:#606070;padding:30px">No devices found</td></tr>';return}t.innerHTML=b.map((d,i)=>{const sc=sigClass(d.rssi),tp=d.type===1?'iBeacon':d.type===2?'Eddystone':'Generic';return'<tr onclick="showBLE('+i+')"><td class="col-sig">'+sigBars(sc)+'</td><td class="col-name"><b>'+(d.name||'<i style="color:#606070">Unknown</i>')+'</b></td><td class="col-addr mono">'+d.addr+'</td><td class="col-rssi" style="color:'+(d.rssi>-70?'#00ff88':'#ffaa00')+'">'+d.rssi+'</td><td class="col-type">'+tp+'</td></tr>'}).join('')}
        function showAP(i){const ap=data.wifi[i];if(!ap)return;const m=document.getElementById('ap-modal'),b=document.getElementById('ap-modal-body');const sec=ap.sec===7?'Open':ap.sec<=4?'WEP':'WPA2',sc=ap.sec===7?'#ff4444':ap.sec<=4?'#ffaa00':'#00ff88';b.innerHTML='<div class="detail-row"><span class="detail-label">SSID</span><span class="detail-value">'+(ap.ssid||'Hidden')+'</span></div><div class="detail-row"><span class="detail-label">BSSID</span><span class="detail-value mono">'+ap.bssid+'</span></div><div class="detail-row"><span class="detail-label">Vendor</span><span class="detail-value">'+(ap.vendor||'Unknown')+'</span></div><div class="detail-row"><span class="detail-label">Channel</span><span class="detail-value">'+ap.ch+' ('+(2400+ap.ch*5)+'MHz)</span></div><div class="detail-row"><span class="detail-label">RSSI</span><span class="detail-value">'+ap.rssi+' dBm</span></div><div class="detail-row"><span class="detail-label">Security</span><span class="detail-value" style="color:'+sc+'">'+sec+'</span></div>';m.classList.add('active')}
        function showBLE(i){const d=data.ble[i];if(!d)return;const m=document.getElementById('ble-modal'),b=document.getElementById('ble-modal-body');const tp=d.type===1?'iBeacon':d.type===2?'Eddystone':'Generic';b.innerHTML='<div class="detail-row"><span class="detail-label">Name</span><span class="detail-value">'+(d.name||'Unknown')+'</span></div><div class="detail-row"><span class="detail-label">Address</span><span class="detail-value mono">'+d.addr+'</span></div><div class="detail-row"><span class="detail-label">Type</span><span class="detail-value">'+tp+'</span></div><div class="detail-row"><span class="detail-label">RSSI</span><span class="detail-value">'+d.rssi+' dBm</span></div><div class="detail-row"><span class="detail-label">Has Name</span><span class="detail-value">'+(d.hasName?'Yes':'No')+'</span></div>';m.classList.add('active')}
        let pktData={total:0,beacon:0,data:0,deauth:0,pps:0,peak:0,channel:0,channels:[],beacons:[],deauths:[]};
        let pktLog=[],pktVerbose=false,pktInterval=null,monitoringCh=0;
        function closeModal(){document.getElementById('ap-modal').classList.remove('active');document.getElementById('ble-modal').classList.remove('active');closePktModal()}
        function closePktModal(){document.getElementById('pkt-modal').classList.remove('active');if(pktInterval){clearInterval(pktInterval);pktInterval=null}pktHistory=[];fetch('/api/action?type=stop_sniffer',{method:'POST'}).then(()=>console.log('Sniffer stopped'))}
        function showPktMonitor(ch){monitoringCh=ch;pktVerbose=false;document.getElementById('pkt-ch-num').textContent=ch;document.getElementById('pkt-modal').classList.add('active');document.getElementById('pkt-log').innerHTML='';document.getElementById('pkt-log').classList.remove('active');document.getElementById('pkt-graph').classList.add('active');document.getElementById('verb-btn').textContent='📊 Graph Mode';document.getElementById('verb-btn').classList.add('active');initChPktBars();fetch('/api/action?type=start_sniffer&channel='+ch,{method:'POST'}).then(()=>{pktInterval=setInterval(fetchPktData,500)})}
        function initChPktBars(){const c=document.getElementById('ch-pkt-bars');if(!c)return;c.innerHTML='';for(let i=1;i<=13;i++){const d=document.createElement('div');d.className='ch-pkt-bar'+(i===monitoringCh?' active':'');d.style.height='10%';d.title='CH'+i;d.style.minHeight='4px';c.appendChild(d)}}
        function fetchPktData(){fetch('/api/packets').then(r=>r.json()).then(d=>{pktData=d;updatePktDisplay();updatePktGraph();if(pktVerbose)updatePktLog();pktHistory=d.ppsHistory||[];if(!pktVerbose)drawPktGraph()}).catch(e=>console.error('Pkt fetch error:',e))}
        function updatePktDisplay(){document.getElementById('pkt-total').textContent=pktData.total;document.getElementById('pkt-beacon').textContent=pktData.beacon;document.getElementById('pkt-data').textContent=pktData.data;document.getElementById('pkt-deauth').textContent=pktData.deauth;document.getElementById('pkt-pps').textContent=pktData.pps;document.getElementById('pkt-peak').textContent=pktData.peak;const deauthStat=document.getElementById('pkt-deauth-stat');if(pktData.deauth>0)deauthStat.classList.add('alert');else deauthStat.classList.remove('alert')}
        function updatePktGraph(){const c=document.getElementById('ch-pkt-bars');if(!c||!pktData.channels)return;const chArr=pktData.channels;const max=Math.max(...chArr.slice(1),1);c.innerHTML='';for(let i=1;i<=13;i++){const count=chArr[i]||0;const h=Math.max(5,(count/max)*100);const d=document.createElement('div');d.className='ch-pkt-bar'+(i===monitoringCh?' active':'');d.style.height=h+'%';d.title='CH'+i+': '+count+' pkts';c.appendChild(d)}}
        function updatePktLog(){if(!pktData.packets)return;const log=document.getElementById('pkt-log');pktData.packets.forEach(p=>{const id=p.ts+'_'+p.type+'_'+p.ch;if(!document.getElementById('pkt-'+id)){const entry=document.createElement('div');entry.id='pkt-'+id;entry.className='pkt-entry '+(p.type==='DEAUTH'?'pkt-deauth':p.type==='BEACON'?'pkt-beacon':'')+(pktVerbose?'':' simple');const time=new Date(p.ts).toLocaleTimeString();let msg=p.type+' | RSSI:'+p.rssi+'dBm';if(p.ssid)msg+=' | SSID:'+p.ssid;if(p.bssid&&p.bssid!=='00:00:00:00:00:00')msg+=' | BSSID:'+p.bssid;entry.innerHTML='<span class="pkt-time">'+time+'</span><span class="pkt-type">'+p.type+'</span><span class="pkt-ch">CH'+p.ch+'</span><span class="pkt-info">'+msg+'</span>';log.appendChild(entry);log.scrollTop=log.scrollHeight;if(log.children.length>100)log.removeChild(log.firstChild)}})}
        let pktHistory=[],maxHistory=60;
        function toggleVerbose(){pktVerbose=!pktVerbose;const btn=document.getElementById('verb-btn');const log=document.getElementById('pkt-log');const graph=document.getElementById('pkt-graph');btn.textContent=pktVerbose?'📋 Verbose Mode':'📊 Graph Mode';btn.classList.toggle('active',!pktVerbose);if(pktVerbose){log.classList.add('active');graph.classList.remove('active')}else{log.classList.remove('active');graph.classList.add('active')}}
        function drawPktGraph(){const c=document.getElementById('pkt-graph');if(!c)return;const x=c.getContext('2d'),w=c.width,h=c.height;x.fillStyle='#0a0a0f';x.fillRect(0,0,w,h);if(pktHistory.length<2)return;const max=Math.max(...pktHistory,10);x.strokeStyle='#2a2a3a';for(let i=0;i<5;i++){const y=h-30-i*(h-50)/4;x.beginPath();x.moveTo(50,y);x.lineTo(w,y);x.stroke()}x.fillStyle='#606070';x.font='10px monospace';x.textAlign='right';for(let i=0;i<5;i++){x.fillText(Math.round(max*i/4)+'',45,h-30-i*(h-50)/4)}x.beginPath();x.strokeStyle='#00d4ff';x.lineWidth=2;const step=(w-60)/(maxHistory-1);pktHistory.forEach((v,i)=>{const y=h-30-(v/max)*(h-50);if(i===0)x.moveTo(50,y);else x.lineTo(50+i*step,y)});x.stroke();x.fillStyle='#00d4ff';pktHistory.forEach((v,i)=>{const y=h-30-(v/max)*(h-50);x.beginPath();x.arc(50+i*step,y,3,0,Math.PI*2);x.fill()})}
        function exportPktLog(){const rows=[['Time','Type','Channel','RSSI','BSSID','SSID/Info']];document.querySelectorAll('.pkt-entry').forEach(e=>{const t=e.querySelector('.pkt-time')?.textContent||'';const ty=e.querySelector('.pkt-type')?.textContent||'';const ch=e.querySelector('.pkt-ch')?.textContent||'';const info=e.querySelector('.pkt-info')?.textContent||'';rows.push([t,ty,ch,'','',info])});const csv=rows.map(r=>r.join(',')).join('\n');const blob=new Blob([csv],{type:'text/csv'});const url=URL.createObjectURL(blob);const a=document.createElement('a');a.href=url;a.download='packet-log-'+new Date().toISOString().slice(0,19).replace(/:/g,'-')+'.csv';a.click();URL.revokeObjectURL(url)}
        function updateAlerts(e){const c=document.getElementById('s-alerts');const a=e?e.length:0;if(!a){c.innerHTML='<div class="empty"><div class="empty-icon">✅</div><p>No security events</p></div>';['s-d','s-r','s-t'].forEach(id=>{document.getElementById(id).textContent=0});return}let nd=0,nr=0;const h=e.slice().reverse().map(ev=>{let t='info',ic='ℹ️';if(ev.type===0){t='deauth';ic='⚠️';nd++}else if(ev.type===1){t='rogue';ic='🚨';nr++}return'<div class="alert '+t+'"><div class="alert-icon">'+ic+'</div><div class="alert-content"><div class="alert-title">'+(ev.type===0?'Deauth Attack':ev.type===1?'Rogue AP':'Tracker')+'</div><div class="alert-msg">'+ev.msg+'</div></div></div>'}).join('');c.innerHTML=h;document.getElementById('s-d').textContent=nd;document.getElementById('s-d').style.color=nd?'#ff4444':'#00ff88';document.getElementById('s-r').textContent=nr;document.getElementById('s-r').style.color=nr?'#ffaa00':'#00ff88';document.getElementById('s-t').textContent=0}
        function updateRogue(r){const c=document.getElementById('rogue-list');if(!r||!r.length){c.innerHTML='<div class="empty"><div class="empty-icon">✅</div><p>No rogue APs detected</p></div>';return}c.innerHTML=r.map(ap=>'<div class="alert rogue"><div class="alert-icon">🚨</div><div class="alert-content"><div class="alert-title">Rogue AP: '+ap.ssid+'</div><div class="alert-msg"><span class="mono">'+ap.bssid1+'</span><br><span class="mono">'+ap.bssid2+'</span></div></div></div>').join('')}
        function updateRec(cl,bc){const c=document.getElementById('ch-rec');if(!cl||Object.values(cl).every(v=>v===0)){c.innerHTML='<div class="empty"><div class="empty-icon">📊</div><p>Scan to get recommendations</p></div>';return}const rec=[1,6,11].map(ch=>({ch,l:cl[ch]||0})).sort((a,b)=>a.l-b.l);c.innerHTML=rec.map((r,i)=>'<div class="rec-card '+(i===0?'best':'')+'"><div class="rec-rank">'+(i+1)+'</div><div class="rec-info"><div class="rec-title">Channel '+r.ch+'</div><div class="rec-sub">'+r.l+' networks</div></div>'+(i===0?'<span style="color:#00ff88">Best</span>':'')+'</div>').join('')}
        function scanWiFi(){document.getElementById('status').textContent='Scanning...';cmd('scan_wifi')}
        function scanBLE(){document.getElementById('status').textContent='Scanning...';cmd('scan_ble')}
        function cmd(t,p){fetch('/api/action?type='+t+p,{method:'POST'}).then(r=>r.text()).then(m=>{document.getElementById('status').textContent=m;setTimeout(()=>document.getElementById('status').textContent='Connected',1500);update()})}
        function save(){const fd=new FormData();fd.append('scanSpeed',document.getElementById('s-speed').value);fd.append('rssiThreshold',document.getElementById('s-rssi').value);fd.append('rgbBrightness',document.getElementById('s-rgb').value);fd.append('deauthThreshold',document.getElementById('s-deauth').value);fd.append('screenTimeout',document.getElementById('s-to').value);fd.append('powerMode',document.getElementById('s-pwr').value);fd.append('apSSID',document.getElementById('s-ap-ssid').value);fd.append('apPassword',document.getElementById('s-ap-pass').value);document.getElementById('status').textContent='Saving...';fetch('/api/settings',{method:'POST',body:fd}).then(()=>{document.getElementById('status').textContent='Saved! Restart device to apply WiFi changes.';setTimeout(()=>document.getElementById('status').textContent='Connected',3000)})}
        function load(){fetch('/api/settings').then(r=>r.json()).then(s=>{document.getElementById('s-speed').value=s.scanSpeed;document.getElementById('s-rssi').value=s.rssiThreshold;document.getElementById('s-rssi').nextElementSibling.textContent=s.rssiThreshold+'dBm';document.getElementById('s-rgb').value=s.rgbBrightness;document.getElementById('s-rgb').nextElementSibling.textContent=s.rgbBrightness+'%';document.getElementById('s-deauth').value=s.deauthThreshold;document.getElementById('s-to').value=s.screenTimeout;document.getElementById('s-pwr').value=s.powerMode;document.getElementById('s-ap-ssid').value=s.apSSID||'';document.getElementById('s-ap-pass').value=s.apPassword||''})}
        initCH();load();update();setInterval(update,2000);
    </script>
</body>
</html>
)rawliteral";
  return html;
}

#endif
