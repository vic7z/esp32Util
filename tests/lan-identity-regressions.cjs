const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const ip=s=>s.split('.').reduce((n,o)=>(n*256+Number(o))>>>0,0);
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname,'../build/lan-runtime-harness.wasm')),{}),a=instance.exports;
 const text=ptr=>{const m=new Uint8Array(a.memory.buffer);let end=ptr;while(m[end])end++;return Buffer.from(m.slice(ptr,end)).toString()};
 const sample=s=>{const b=Buffer.from(s);new Uint8Array(a.memory.buffer).set(b,a.responsePtr());a.responseLength(b.length)};
 const ready=()=>{a.reset();a.connect();a.setConnected(1);a.tick(1,0)};
 const parse=s=>{sample(s);return a.parseSample()};
 const response='HTTP/1.1 200 OK\r\nSERVER: test-device/1.0\r\nContent-Type: text/html; charset=utf-8\r\n\r\n<TITLE class="name"> HP LaserJet &amp; Office &#65; </TITLE>';
 a.reset();assert(parse(response));assert.equal(a.identityCode(),200);assert.equal(text(a.identityTitle()),'HP LaserJet & Office A');assert.equal(text(a.identityServer()),'test-device/1.0');assert.equal(text(a.identityCategory()),'Possible printer');
 assert(parse('HTTP/1.0 401 Unauthorized\r\nServer: Device\r\n\r\n'));assert.equal(a.identityCode(),401);assert.equal(text(a.identityTitle()),'');assert.equal(text(a.identityCategory()),'HTTP service; device unknown','old title bytes cannot leak into classification');
 for(const bad of ['','SSH-2.0-fixture','HTTP/1.1 999 Nope\r\n\r\n','HTTP/1.1 20A Nope\r\n\r\n','HTTP/1.1 2000 Nope\r\n\r\n'])assert(!parse(bad));
 for(let n=0;n<response.length;n++){parse(response.slice(0,n));}
 assert(parse('HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n<title>Fake</title>'));assert.equal(text(a.identityTitle()),'');
 assert(parse('HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nServer: chunked\r\n\r\n<title>Fake</title>'));assert(a.identityChunked());assert.equal(text(a.identityTitle()),'');
 assert(parse('HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n<title>Fake</title>'));assert(a.identityEncoded());assert.equal(text(a.identityTitle()),'');
 assert(parse('HTTP/1.1 200 OK\r\nServer: '+ 'x'.repeat(300)+'\r\n\r\n<title>'+'a'.repeat(300)+'</title>'));assert.equal(text(a.identityServer()).length,96);assert.equal(text(a.identityTitle()).length,96);
 assert(parse('HTTP/1.1 200 OK\r\nX: '+'a'.repeat(2100)+'\r\n\r\n<title>Ignored</title>'));assert(!a.identityHeaders());
 ready();for(const host of ['8.8.8.8','192.168.4.2','192.168.1.42','192.168.1.255'])assert(!a.identify(ip(host),80));for(const p of [0,22,443,8443,65535])assert(!a.identify(ip('192.168.1.1'),p));assert.equal(a.connections(),0);
 ready();sample(response);a.socketMode(1);a.sendMode(1);a.readChunk(7);assert(a.identify(ip('192.168.1.1'),8080));assert(!a.ports(ip('192.168.1.1')));for(let i=0;i<200&&a.identityState()===1;i++)a.tick(1,0);assert.equal(a.identityState(),2);assert.equal(a.identityCode(),200);assert.equal(text(a.identityTitle()),'HP LaserJet & Office A');assert.equal(a.maxSockets(),1);assert.equal(a.openSockets(),0);assert.equal(a.connections(),1);assert(text(a.requestPtr()).startsWith('GET / HTTP/1.0\r\nHost: 192.168.1.1:8080\r\n'));assert(!text(a.requestPtr()).includes('Authorization'));
 ready();sample('HTTP/1.1 302 Found\r\nLocation: http://8.8.8.8/\r\n\r\n');a.socketMode(1);a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(1,0);assert.equal(a.identityCode(),302);assert.equal(a.connections(),1,'redirect never followed');
 ready();sample('HTTP/1.1 200 OK\r\nServer: fixture\r\n\r\n'+ 'a'.repeat(6000));a.socketMode(1);a.identify(ip('192.168.1.1'),80);for(let i=0;i<10;i++)a.tick(1,0);assert.equal(a.identityState(),2);assert(a.identityLimited());assert.equal(a.openSockets(),0);
 ready();a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(350,0);assert.equal(a.identityState(),4);assert.equal(a.openSockets(),0);
 ready();sample(response);a.socketMode(3);a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(1,0);assert.equal(a.identityState(),2);
 for(const mode of [2,5,6,7]){ready();a.socketMode(mode);a.identify(ip('192.168.1.1'),80);a.tick(1,0);assert.equal(a.identityState(),5);assert.equal(a.openSockets(),0);}
 ready();a.socketMode(1);a.sendMode(2);a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(2500,0);assert.equal(a.identityState(),4);assert.equal(a.openSockets(),0);
 for(const mode of [3,4]){ready();a.socketMode(1);a.sendMode(mode);a.identify(ip('192.168.1.1'),80);a.tick(1,0);assert.equal(a.identityState(),5);assert.equal(a.openSockets(),0);}
 ready();a.socketMode(1);sample(response);a.holdOpen(1);a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(2500,0);assert.equal(a.identityState(),4);assert.equal(a.identityCode(),200);assert(a.identityLimited());
 ready();a.socketMode(1);a.readError(1);a.identify(ip('192.168.1.1'),80);a.tick(1,0);assert.equal(a.identityState(),5);assert.equal(a.openSockets(),0);
 ready();a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.cancel();a.tick(1000,0);assert.equal(a.identityState(),3);assert.equal(a.openSockets(),0);assert.equal(a.connections(),1);
 ready();a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.setConnected(0);a.tick(1,0);assert.equal(a.openSockets(),0);assert.equal(a.identityState(),0);
 ready();a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.setAddress(ip('192.168.1.43'),ip('255.255.255.0'));a.tick(1,0);assert.equal(a.openSockets(),0);assert.equal(a.identityState(),0);
 ready();a.setTime(0xfffffff0);a.identify(ip('192.168.1.1'),80);a.tick(1,0);a.tick(350,0);assert.equal(a.identityState(),4);
 ready();a.socketMode(1);a.ports(ip('192.168.1.1'));a.tick(1,0);a.tick(1,0);a.tick(1,0);sample(response);assert(a.identify(ip('192.168.1.20'),80));a.tick(1,0);a.tick(1,0);assert.equal(a.savedPortCount(),3);assert.equal(a.savedPortTarget()>>>0,ip('192.168.1.1'));assert.equal(a.portState(0),1,'identification preserves completed TCP results');
 console.log('HTTP identification regressions passed: bounded parser, title/header evidence, encoding limits, local targets, partial I/O, redirects, deadlines and cleanup.');
})().catch(e=>{console.error(e);process.exitCode=1});
