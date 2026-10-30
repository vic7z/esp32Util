// Local browser fixture only; no ESP32 or radio access.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const root=path.join(__dirname,'..');
const capture={listening:true,scanning:false,recording:false,frames:32,capacity:32,snaplen:783,dropped:10,truncated:1,eapol:41,probeRequests:5,evicted:0,eapolByChannel:[40,0,0,0,0,1,0,0,0,0,0,0,0],probes:[{ssid:'Cafe <test> & lab',source:'02:00:00:00:00:01',wildcard:false,ch:6,rssi:-42,count:4,firstMs:1000,lastMs:5000},{ssid:'',source:'02:00:00:00:00:02',wildcard:true,ch:6,rssi:-70,count:1,firstMs:2000,lastMs:2000}]};
const data={wifi:[],ble:[],events:[],rogue:[],hidden:[],graph:[],walkActive:false};
http.createServer((req,res)=>{
  const url=new URL(req.url,'http://127.0.0.1');
  if(url.pathname==='/api/action') {
    const type=url.searchParams.get('type');
    if(type==='start_sniffer')capture.listening=true;
    if(type==='stop_capture'){capture.listening=false;capture.recording=false}
    if(type==='start_pcap'){capture.recording=true;capture.listening=true;capture.frames=capture.dropped=capture.truncated=0}
    if(type==='stop_pcap')capture.recording=false;
    if(type==='clear_capture'){capture.probes=[];capture.frames=capture.eapol=capture.probeRequests=capture.dropped=capture.truncated=0;capture.recording=false;capture.eapolByChannel.fill(0)}
    return res.end('Fixture action complete');
  }
  if(url.pathname==='/api/capture.pcap') {
    if(!capture.frames){res.statusCode=409;return res.end('No packets yet')}
    capture.recording=false;res.setHeader('Content-Type','application/vnd.tcpdump.pcap');res.setHeader('Content-Disposition','attachment; filename=esp32-capture.pcap');
    return res.end(fs.readFileSync(path.join(root,'build/capture-test.pcap')));
  }
  if(url.pathname.startsWith('/api/')) {
    res.setHeader('Content-Type','application/json');
    if(url.pathname==='/api/capture')return res.end(JSON.stringify(capture));
    if(url.pathname==='/api/settings')return res.end(JSON.stringify({scanSpeed:1,rssiThreshold:-70,rgbBrightness:50,deauthThreshold:10,screenTimeout:60,powerMode:0,apSSID:'ESP32-Tool',apPassword:''}));
    return res.end(JSON.stringify(data));
  }
  const file=url.pathname==='/'?'index.html':url.pathname.slice(1);
  if(!['index.html','app.js','style.css'].includes(file)){res.statusCode=404;return res.end()}
  res.setHeader('Content-Type',file.endsWith('.js')?'application/javascript':file.endsWith('.css')?'text/css':'text/html');
  res.end(fs.readFileSync(path.join(root,'data',file)));
}).listen(8765,'127.0.0.1',()=>console.log('Capture fixture preview: http://127.0.0.1:8765'));
