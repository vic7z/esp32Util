// Synthetic browser fixture only. No WiFi, ARP or port scans are performed.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path');
const root=path.join(__dirname,'..');
const lan={portTarget:'192.168.1.1',connection:'connected',ssid:'Fixture LAN',message:'Synthetic preview data.',ip:'192.168.1.42',mask:'255.255.255.0',first:'192.168.1.1',last:'192.168.1.254',target:'192.168.1.1',job:'ports',progress:'done',completed:3,total:3,errors:0,omitted:0,hosts:[{ip:'192.168.1.1',mac:'02:00:00:00:00:01',vendor:'Router <test>'},{ip:'192.168.1.20',mac:'02:00:00:00:00:02',vendor:'Printer'}],ports:[{port:80,state:'open'},{port:22,state:'closed'},{port:443,state:'timed out'}]};
lan.identity={target:'192.168.1.20',port:80,state:'done',status:200,category:'Possible printer',title:'HP LaserJet & Office <fixture>',server:'Embedded HTTP Server / fixture',contentType:'text/html; charset=utf-8',limited:false,encoded:false,chunked:false,headersComplete:true};
const capture={listening:false,scanning:false,recording:false,frames:0,capacity:32,snaplen:783,dropped:0,truncated:0,eapol:0,probeRequests:0,evicted:0,eapolByChannel:[],probes:[]};
http.createServer((req,res)=>{
 const url=new URL(req.url,'http://127.0.0.1');
 if(url.pathname==='/api/lan/action') {
  let body='';req.on('data',chunk=>{body+=chunk;if(body.length>8192)req.destroy()});
  req.on('end',()=>{
   const field=name=>{const escaped=name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');return (body.match(new RegExp('name="'+escaped+'"\\r\\n\\r\\n([^\\r\\n]*)'))||[])[1]||''};
   const action=field('type');
   if(action==='identify'){lan.job='identify';lan.target=field('target');lan.identity.target=lan.target;lan.identity.port=Number(field('port'));lan.progress='done';lan.completed=lan.total=1;}
   if(action==='disconnect'){lan.connection='disconnected';lan.job='none';lan.progress='idle';lan.hosts=[];lan.ports=[]}
   if(action==='connect'){lan.connection='connected';lan.ssid=field('ssid');lan.message='Fixture WiFi connected.'}
   if(action==='cancel')lan.progress='cancelled';
   if(action==='discover'){lan.job='discovery';lan.progress='done';lan.completed=3;lan.total=3;lan.hosts=[{ip:'192.168.1.1',mac:'02:00:00:00:00:01',vendor:'Router <test>'}];lan.ports=[]}
   if(action==='ports'){lan.job='ports';lan.progress='done';lan.target=field('target');lan.completed=lan.total=3;lan.ports=[{port:80,state:'open'},{port:22,state:'closed'},{port:443,state:'timed out'}]}
   res.end('Fixture action complete');
  });return;
 }
 if(url.pathname.startsWith('/api/')){
  res.setHeader('Content-Type','application/json');
  if(url.pathname==='/api/lan')return res.end(JSON.stringify(lan));
  if(url.pathname==='/api/capture')return res.end(JSON.stringify(capture));
  if(url.pathname==='/api/settings')return res.end(JSON.stringify({scanSpeed:1,rssiThreshold:-70,rgbBrightness:50,deauthThreshold:10,screenTimeout:60,powerMode:0,apSSID:'ESP32-Tool',apPassword:''}));
  return res.end(JSON.stringify({wifi:[],ble:[],events:[],rogue:[],hidden:[],graph:[],walkActive:false}));
 }
 const file=url.pathname==='/'?'index.html':url.pathname.slice(1);
 if(!['index.html','app.js','style.css'].includes(file)){res.statusCode=404;return res.end()}
 res.setHeader('Content-Type',file.endsWith('.js')?'application/javascript':file.endsWith('.css')?'text/css':'text/html');
 res.end(fs.readFileSync(path.join(root,'data',file)));
}).listen(8765,'127.0.0.1',()=>console.log('LAN fixture: http://127.0.0.1:8765 (synthetic data only)'));
