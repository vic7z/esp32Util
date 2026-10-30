const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const ip=text=>text.split('.').reduce((value,octet)=>(value*256+Number(octet))>>>0,0);
(async()=>{
 const {instance}=await WebAssembly.instantiate(fs.readFileSync(path.join(__dirname,'../build/lan-harness.wasm')),{});
 const a=instance.exports;
 function text(value){const bytes=new Uint8Array(a.memory.buffer,a.inputBuffer(),384);bytes.fill(0);bytes.set(Buffer.from(value));}
 for(const value of ['192.168.1.2','10.0.0.1','255.255.255.255','0.0.0.0']){text(value);assert.equal(a.parseIP(),1);assert.equal(a.parsedAddress()>>>0,ip(value))}
 for(const value of ['', '1.2.3', '1.2.3.4x','1.2.3.4.5','256.1.1.1','-1.2.3.4','1..2.3','1.2.3.4444']){text(value);assert.equal(a.parseIP(),0,value)}
 text('22, 80,443,8000-8002,22');assert.equal(a.parsePorts(),1);assert.deepEqual(Array.from({length:a.portCount()},(_,i)=>a.parsedPort(i)),[22,80,443,8000,8001,8002]);
 for(const value of ['', '0','65536','-1','22,','1-33','90-80','22,,23','2x','99999999999','1-2-3']){text(value);assert.equal(a.parsePorts(),0,value);assert.equal(a.portCount(),6,'invalid parses do not mutate previous list')}
 text('65535');assert.equal(a.parsePorts(),1);assert.equal(a.parsedPort(0),65535);
 text('1-32');assert.equal(a.parsePorts(),1);assert.equal(a.portCount(),32);
 a.configure(ip('192.168.1.42'),ip('255.255.255.0'));
 assert(a.localHost(ip('192.168.1.1')));for(const target of ['192.168.1.0','192.168.1.255','192.168.1.42','192.168.2.1','127.0.0.1','8.8.8.8'])assert(!a.localHost(ip(target)),target);
 assert.equal(a.defaultFirst()>>>0,ip('192.168.1.1'));assert.equal(a.defaultLast()>>>0,ip('192.168.1.254'));
 assert(a.discover(ip('192.168.1.1'),ip('192.168.1.254')));assert(!a.startPorts(ip('192.168.1.1')),'one job at a time');
 for(let i=0;i<254;i++)a.neighbor(1,0);
 assert.equal(a.completed(),254);assert.equal(a.progress(),2);assert.equal(a.hostCount(),32);assert.equal(a.omitted(),222);
 text('22,80,443');assert(a.parsePorts());assert(a.startPorts(ip('192.168.1.1')));
 a.portResult(1);a.cancel();a.portResult(2);assert.equal(a.progress(),3);assert.equal(a.completed(),1);assert.equal(a.resultState(1),0,'cancelled ports remain untested');
 assert(a.startPorts(ip('192.168.1.2')));a.portResult(2);a.portResult(3);a.portResult(4);assert.equal(a.errors(),1);assert.equal(a.progress(),2);
 a.configure(ip('10.20.30.40'),ip('255.255.0.0'));assert.equal(a.defaultFirst()>>>0,ip('10.20.30.1'));assert.equal(a.defaultLast()>>>0,ip('10.20.30.254'));
 assert(!a.discover(ip('10.20.0.1'),ip('10.20.1.1')),'255+ addresses rejected');
 a.configure(ip('192.168.1.42'),ip('255.255.0.0'));assert(a.localHost(ip('192.168.2.1')));assert(!a.localHost(ip('192.168.4.20')));assert(!a.discover(ip('192.168.4.1'),ip('192.168.4.200')));
 a.configure(ip('192.168.4.42'),ip('255.255.255.0'));assert(!a.localHost(ip('192.168.4.1')));assert(!a.discover(ip('192.168.4.1'),ip('192.168.4.200')));
 a.configure(ip('10.0.0.1'),ip('255.255.255.252'));assert(a.localHost(ip('10.0.0.2')));assert.equal(a.defaultLast()>>>0,ip('10.0.0.2'));
 for(const mask of ['0.0.0.0','255.0.255.0','255.255.255.255','255.255.255.254']){a.configure(ip('10.0.0.1'),ip(mask));assert(!a.localHost(ip('10.0.0.2')),mask)}
 console.log('LAN policy/engine regressions passed: IPv4, subnet/AP boundaries, bounded ranges and ports, cancellation, progress, and host overflow.');
})().catch(error=>{console.error(error);process.exitCode=1});
