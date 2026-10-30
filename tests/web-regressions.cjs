const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const elements = new Map();
function element(id) {
  if (!elements.has(id)) elements.set(id, {
    innerHTML:'', textContent:'', value:'', style:{}, children:[],
    classList:{add(){},remove(){},toggle(){},contains(){return false}}, setAttribute(){},
    appendChild(child){this.children.push(child)}, remove(){},click(){},
    getContext(){return new Proxy({}, {get:(_,key)=>key==='createLinearGradient'?()=>({addColorStop(){}}):()=>{}})}
  });
  return elements.get(id);
}
const context = vm.createContext({console, Set, Blob, URL, Promise, FormData,
  localStorage:{getItem(){return null},setItem(){}},
  document:{getElementById:element, createElement:()=>element(Math.random()),
    querySelectorAll:()=>[],documentElement:{setAttribute(){}},body:element('body')},
  setTimeout(){},setInterval(){return 1},clearInterval(){},
  fetch:async()=>({ok:true,json:async()=>({wifi:[],ble:[]}),text:async()=>''})
});
let source=fs.readFileSync(require('node:path').join(__dirname,'../data/app.js'),'utf8');
// Skip startup polling; exercise real rendering and command functions below.
source=source.replace(/initCH\(\);\s*initOverlapBars\(\);\s*load\(\);\s*update\(\);\s*setInterval\(update,\s*2000\);?/, '');
vm.runInContext(source,context);
function run(code){return vm.runInContext(code,context)}
assert.equal(run('authLabel(0)'),'Open');
assert.equal(run('authLabel(1)'),'WEP');
assert.equal(run('authLabel(3)'),'WPA2');
assert.equal(run('authLabel(6)'),'WPA3');
assert.equal(run('authLabel(7)'),'WPA2/WPA3');
assert.equal(run('authClass(0)'),'open');
const attack=`<img src=x onerror="alert(1)">O'Hare\\name`;
context.attack=attack;
run(`data.wifi=[{ssid:attack,bssid:'AA:BB:CC:DD:EE:FF',rssi:-55,ch:6,sec:0,vendor:attack,grade:'B'}];updateWiFi(data.wifi);showAP(0)`);
assert(!element('w-tbl').innerHTML.includes('<img'));
assert(element('w-tbl').innerHTML.includes('&lt;img'));
assert(!element('ap-modal-body').innerHTML.includes('<img'));
assert(!element('ap-modal-body').innerHTML.includes('onclick="startWalk'));
assert.equal(typeof element('start-walk').onclick,'function');
run(`updateBLE([{name:attack,addr:'AA',rssi:-55,type:0}]);updateHidden([{ssid:attack,ch:6,rssi:-55}]);updateAlerts([{type:0,msg:attack}]);updateRogue([{ssid:attack,bssid1:'AA'}])`);
for(const id of ['b-tbl','h-tbl','s-alerts','rogue-list']) assert(!element(id).innerHTML.includes('<img'),id);
assert.equal(run('csvCell(\'a"b,c\')'),'"a""b,c"');
assert.equal(run('csvCell("=1+1")'),'"\'=1+1"');
assert.equal(run('csvCell("-42")'),'"\'-42"');
assert.equal(run('csvCell("normal")'),'"normal"');
assert.equal(run('csvCell("test")'),'"test"');
assert.equal(run('csvCell("radio")'),'"radio"');
assert.equal(run('csvCell("\\t=1")'),'"\'\t=1"');
context.captureState={listening:true,scanning:false,recording:true,frames:1,capacity:32,dropped:2,truncated:1,eapol:3,probeRequests:4,evicted:0,eapolByChannel:[0,0,0,0,0,3],probes:[{ssid:attack,source:attack,ch:6,rssi:-42,count:2,firstMs:1000,lastMs:5000}]};
run('updateCapture(captureState)');
assert(!element('capture-probes').innerHTML.includes('<img'));
assert(element('capture-probes').innerHTML.includes('&lt;img'));
assert(element('capture-eapol').textContent.includes('CH6: 3'));
assert(element('capture-buffer').textContent.includes('2 dropped'));
assert.equal(element('capture-record').disabled,true);
assert.equal(element('capture-download').disabled,false);
context.lanState={connection:'connected',ssid:attack,message:attack,ip:'192.168.1.42',mask:'255.255.255.0',first:'192.168.1.1',last:'192.168.1.254',target:'192.168.1.1',job:'ports',progress:'done',completed:3,total:3,errors:0,omitted:0,hosts:[{ip:'192.168.1.1',mac:'02:00:00:00:00:01',vendor:attack}],ports:[{port:80,state:'open'},{port:22,state:'closed'},{port:443,state:'timed out'}]};
run('updateLAN(lanState)');
assert(!element('lan-hosts').innerHTML.includes('<img'));
assert(element('lan-hosts').innerHTML.includes('&lt;img'));
assert.equal(element('lan-first').value,'192.168.1.1');
element('lan-first').value='192.168.1.20';run('updateLAN(lanState)');
assert.equal(element('lan-first').value,'192.168.1.20','polling does not overwrite an edited range');
run('selectLanHost("192.168.1.10")');assert.equal(element('lan-target').value,'192.168.1.10');
assert(element('lan-port-results').innerHTML.includes('timed out'));
assert.equal(element('lan-cancel').disabled,true);
context.lanState.portTarget='192.168.1.1';
context.lanState.identity={target:'192.168.1.1',port:80,state:'done',status:200,category:'Possible printer',title:attack,server:attack,contentType:'text/html',limited:true,chunked:false,encoded:false};
run('updateLAN(lanState)');
assert.equal(element('lan-identity-title').textContent,attack,'HTTP title is rendered as inert text');
assert.equal(element('lan-identity-server').textContent,attack);
assert.equal(element('lan-port-caption').textContent,'TCP ports · 192.168.1.1');
assert.equal(element('lan-identity-details').hidden,false);
assert(element('lan-identity-status').textContent.includes('partial'));
assert.equal(element('lan-identify').disabled,false);
context.lanState.progress='running';run('updateLAN(lanState)');assert.equal(element('lan-identify').disabled,true);assert.equal(element('lan-cancel').disabled,false);
context.lanState.progress='done';run('updateLAN(lanState)');
(async()=>{
  const blobs=[];
  context.Blob=class extends Blob {constructor(parts,options){super(parts,options);blobs.push(this)}};
  context.URL={createObjectURL:()=> 'blob:test',revokeObjectURL(){}};
  context.fetch=async()=>({ok:true,json:async()=>({packets:[{ts:1000,type:'PROBE',ssid:'a"b,c',bssid:'AA',ch:6,rssi:-55}]})});
  await run('exportPktLog()');
  assert((await blobs[0].text()).includes('"a""b,c"'),'Export must use CSV escaping, not HTML escaping');
  context.captureState.recording=false;
  context.fetch=async url=>url==='/api/capture.pcap' ? {ok:true,blob:async()=>new Blob([new Uint8Array([0xd4,0xc3,0xb2,0xa1])],{type:'application/vnd.tcpdump.pcap'})} : {ok:true,json:async()=>context.captureState};
  await run('downloadPCAP()');
  assert([...elements.values()].some(item=>item.download==='esp32-capture.pcap'),'PCAP download uses a binary attachment');
  assert.equal(run('captureBusy'),false);
  context.fetch=async url=>url==='/api/capture' ? {ok:true,json:async()=>context.captureState} : {ok:false,text:async()=> 'Wait for scan'};
  await run('captureAction("start_pcap")');
  assert.equal(run('captureBusy'),false,'Rejected capture actions release controls');
  assert.equal(element('capture-stop-pcap').disabled,true,'Server state wins after a rejected action');
  const lanRequests=[];
  context.fetch=async(url,options)=>{if(url==='/api/lan')return{ok:true,json:async()=>context.lanState};lanRequests.push({url,options});return{ok:true,text:async()=> 'Connecting'};};
  element('lan-ssid').value='fixture-network';element('lan-password').value='fixture-password';
  await run('connectLAN({preventDefault(){}})');
  assert.equal(lanRequests[0].url,'/api/lan/action');
  assert.equal(lanRequests[0].options.body.get('password'),'fixture-password');
  assert.equal(element('lan-password').value,'','accepted credentials are cleared from the form');
  assert.equal(run('lanBusy'),false);
  element('lan-target').value='192.168.1.20';element('lan-http-port').value='8080';
  await run('identifyLAN({preventDefault(){}})');
  assert.equal(lanRequests[1].options.body.get('type'),'identify');
  assert.equal(lanRequests[1].options.body.get('target'),'192.168.1.20');
  assert.equal(lanRequests[1].options.body.get('port'),'8080');
  context.fetch=async url=>url==='/api/lan'?{ok:true,json:async()=>context.lanState}:{ok:false,text:async()=> 'Off-LAN target rejected'};
  await run('lanAction("ports",{target:"8.8.8.8",ports:"80"})');
  assert.equal(run('lanBusy'),false,'rejected requests release LAN controls');
  assert.equal(element('lan-scan-ports').disabled,false);
  context.fetch=async()=>({ok:false,status:400,text:async()=> 'Invalid setting value'});
  await assert.rejects(run('checkedFetch("/api/settings")'),/Invalid setting/);
  assert.equal(run('monitorActive'),false);
  await run('toggleMonitor()');
  assert.equal(run('monitorActive'),false,'A rejected start must not toggle monitor state');
  assert.equal(element('mon-toggle').disabled,false);
  console.log('Web regressions passed: security labels, hostile names, safe walk handler, CSV escaping, HTTP failures, monitor state.');
})().catch(error=>{console.error(error);process.exitCode=1});
