const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const ip=text=>text.split('.').reduce((n,o)=>(n*256+Number(o))>>>0,0);
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname,'../build/lan-runtime-harness.wasm')),{});const a=instance.exports;
 const ready=()=>{a.reset();assert(a.connect());a.setConnected(1);a.tick(1,0);assert.equal(a.connectionState(),2)};
 a.reset();assert(!a.ports(ip('192.168.1.1')));assert(!a.discover(ip('192.168.1.1'),ip('192.168.1.2')));assert.equal(a.connections(),0);
 a.reset();assert(a.connect());a.tick(19999,0);assert.equal(a.connectionState(),1);a.tick(1,0);assert.equal(a.connectionState(),3,'connection timeout');
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(1,0);assert.equal(a.openSockets(),1);assert.equal(a.lastTarget()>>>0,ip('192.168.1.1'));assert.equal(a.lastPort(),22);
 a.tick(349,0);assert.equal(a.completed(),0);a.tick(1,0);assert.equal(a.portState(0),3);assert.equal(a.openSockets(),0,'timeout closes socket');
 a.socketMode(1);a.tick(1,0);assert.equal(a.portState(1),1);a.socketMode(2);a.tick(1,0);assert.equal(a.portState(2),2);assert.equal(a.progress(),2);assert.equal(a.maxSockets(),1);
 for(const [mode,state] of [[3,1],[4,2],[5,4],[6,4],[7,4]]){ready();a.socketMode(mode);assert(a.ports(ip('192.168.1.1')));a.tick(1,0);assert.equal(a.portState(0),state,'socket mode '+mode);assert.equal(a.openSockets(),0)}
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(1,0);a.cancel();assert.equal(a.openSockets(),0);assert.equal(a.progress(),3);const before=a.connections();a.tick(1000,1);assert.equal(a.connections(),before,'no new connections after cancellation');
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(1,0);a.setConnected(0);a.tick(1,0);assert.equal(a.openSockets(),0);assert.equal(a.connectionState(),3,'connection loss aborts scan');
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(1,0);a.setAddress(ip('10.0.0.20'),ip('255.255.255.0'));a.tick(1,0);assert.equal(a.openSockets(),0);assert.equal(a.progress(),0,'DHCP address change clears stale scan');assert(!a.ports(ip('192.168.1.1')),'old subnet rejected');
 ready();a.setTime(0xffffff9b);assert(a.ports(ip('192.168.1.1')));a.tick(0,0);a.tick(350,0);assert.equal(a.portState(0),3,'timeout handles millis rollover');
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(0,0);a.socketMode(3);a.tick(351,0);assert.equal(a.portState(0),3,'late polling cannot exceed the scan deadline');
 ready();assert(!a.ports(ip('8.8.8.8')));assert.equal(a.connections(),0,'off-LAN requests never reach transport');
 ready();assert(a.discover(ip('192.168.1.42'),ip('192.168.1.42')));a.tick(1,0);assert.equal(a.arpRequests(),0);assert.equal(a.hostCount(),0);assert.equal(a.progress(),2,'self address is skipped');
 ready();assert(a.discover(ip('192.168.1.1'),ip('192.168.1.2')));for(let i=0;i<10;i++)a.tick(201,1);assert.equal(a.progress(),2);assert.equal(a.completed(),2);assert.equal(a.hostCount(),2);assert.equal(a.arpRequests(),2);
 ready();a.neighborFound(0);assert(a.discover(ip('192.168.1.1'),ip('192.168.1.1')));for(let i=0;i<5;i++)a.tick(201,1);assert.equal(a.hostCount(),0);assert.equal(a.progress(),2,'nonresponsive host is not invented');
 ready();assert(a.discover(ip('192.168.1.1'),ip('192.168.1.2')));a.tick(1,0);a.cancel();assert(!a.discover(ip('192.168.1.1'),ip('192.168.1.2')),'pending callback cannot be reused');a.tick(1,1);assert.equal(a.arpRequests(),0,'cancelled queued request is suppressed');assert(a.discover(ip('192.168.1.1'),ip('192.168.1.2')));
 ready();assert(a.discover(ip('192.168.1.1'),ip('192.168.1.2')));a.tick(1,0);a.tick(2001,0);assert.equal(a.progress(),4,'stalled TCP/IP worker fails boundedly');a.tick(1,1);assert.equal(a.arpRequests(),0);
 ready();a.queueFailure(1);assert(a.discover(ip('192.168.1.1'),ip('192.168.1.2')));a.tick(1,0);a.tick(1,0);assert.equal(a.completed(),2);assert.equal(a.progress(),2,'queue exhaustion cannot hang');
 ready();assert(a.ports(ip('192.168.1.1')));a.tick(1,0);a.disconnect();assert.equal(a.openSockets(),0);assert.equal(a.connectionState(),0);assert.equal(a.progress(),0);
 console.log('LAN runtime regressions passed: actual orchestration with simulated ARP/socket responses, deadlines, socket cleanup, cancellation, connection loss, DHCP changes, and timer rollover.');
})().catch(error=>{console.error(error);process.exitCode=1});
