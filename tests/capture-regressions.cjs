const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.join(__dirname, '..');

function probe(name, source = 1) {
  const ssid = Buffer.isBuffer(name) ? name : Buffer.from(name);
  const frame = Buffer.alloc(26 + ssid.length);
  frame[0] = 0x40; frame[10] = 2; frame[15] = source;
  frame[25] = ssid.length; ssid.copy(frame, 26);
  return frame;
}
function eapol({qos = false, wds = false, ht = false} = {}) {
  const header = 24 + (wds ? 6 : 0) + (qos ? 2 : 0) + (qos && ht ? 4 : 0);
  const frame = Buffer.alloc(header + 12);
  frame[0] = qos ? 0x88 : 0x08; frame[1] = (wds ? 3 : 0) | (ht ? 0x80 : 0);
  Buffer.from([0xaa,0xaa,3,0,0,0,0x88,0x8e,2,1,0,0]).copy(frame, header);
  return frame;
}
(async () => {
  const {instance} = await WebAssembly.instantiate(fs.readFileSync(path.join(root,'build/capture-harness.wasm')), {});
  const api = instance.exports;
  function load(frame) {
    const input = new Uint8Array(api.memory.buffer, api.inputBuffer(), 4096);
    input.fill(0); input.set(frame);
  }
  function observe(frame, seconds=1, channel=6, rssi=-42, usec=123) {
    load(frame); api.observeFrame(frame.length,seconds,usec,channel,rssi);
  }
  function detect(frame, length=frame.length) { load(frame); return !!api.detectEapol(length); }
  for (const options of [{},{qos:true},{wds:true},{qos:true,wds:true},{qos:true,ht:true},{qos:true,wds:true,ht:true}]) {
    const frame=eapol(options);
    assert(detect(frame),JSON.stringify(options));
    for (let length=0;length<frame.length;length++) assert(!detect(frame,length),'truncated EAPOL '+length);
  }
  let invalid=eapol(); invalid[1]|=0x40; assert(!detect(invalid),'encrypted data');
  invalid=eapol(); invalid[1]|=4; assert(!detect(invalid),'fragmented data');
  invalid=eapol(); invalid[22]=1; assert(!detect(invalid),'later fragment');
  invalid=eapol(); invalid[0]|=1; assert(!detect(invalid),'unsupported frame version');
  invalid=eapol(); invalid[0]|=0x40; assert(!detect(invalid),'null data');
  invalid=eapol({qos:true}); invalid[24]=0x80; assert(!detect(invalid),'A-MSDU is not plain LLC');
  invalid=eapol(); invalid[35]=100; assert(!detect(invalid),'declared body exceeds received bytes');
  invalid=eapol(); invalid[30]=0x08; assert(!detect(invalid),'different EtherType');
  api.resetCapture();
  observe(probe('lab'),100); observe(probe('lab'),200);
  assert.equal(api.probeCount(),1); assert.equal(api.probeRequests(),2);
  assert.equal(api.probeSeen(0),2); assert.equal(api.probeFirstSeconds(0),100); assert.equal(api.probeLastSeconds(0),200);
  for(let i=1;i<24;i++) observe(probe('network'+i,i+1),100+i);
  observe(probe('new',99),300);
  assert.equal(api.probeCount(),24); assert.equal(api.probeEvictions(),1);
  assert.equal(api.probeFirstSeconds(0),100,'recently refreshed record retained');
  api.resetCapture(); observe(probe(Buffer.from([97,0,98]))); observe(probe('a')); observe(probe(''));
  assert.equal(api.probeCount(),3,'binary and wildcard names remain distinct');
  let response=probe('response'); response[0]=0x50; observe(response);
  assert.equal(api.probeRequests(),3,'responses are not requests');
  let malformed=probe('short'); malformed[25]=32; observe(malformed);
  assert.equal(api.probeRequests(),3,'malformed names ignored');
  assert.equal(api.rawCount(),0,'raw collection is opt-in');
  api.startRaw(); const first=eapol({qos:true}); observe(first,5000,6,-42,456789);
  const large=Buffer.alloc(900,0x5a); large[0]=0x08; observe(large,5001,11,-75);
  for(let i=2;i<42;i++) observe(eapol(),5002+i,1);
  assert.equal(api.rawCount(),32); assert.equal(api.rawDropped(),10); assert.equal(api.rawTruncated(),1);
  assert.equal(api.eapolCount(),41,'EAPOL still counted after raw buffer fills');
  assert.equal(api.channelEapol(6),1); assert.equal(api.channelEapol(1),40);
  api.stopRaw(); observe(eapol(),6000); assert.equal(api.rawCount(),32);
  const length=api.encodePcap();
  const pcap=Buffer.from(new Uint8Array(api.memory.buffer,api.pcapBuffer(),length));
  assert.equal(pcap.readUInt32LE(0),0xa1b2c3d4); assert.equal(pcap.readUInt16LE(4),2); assert.equal(pcap.readUInt16LE(6),4);
  assert.equal(pcap.readUInt32LE(16),783); assert.equal(pcap.readUInt32LE(20),127);
  let offset=24, count=0;
  while(offset<pcap.length) {
    const seconds=pcap.readUInt32LE(offset), micros=pcap.readUInt32LE(offset+4);
    const included=pcap.readUInt32LE(offset+8), original=pcap.readUInt32LE(offset+12);
    assert(micros<1000000); assert(included<=783); assert(included<=original); assert(offset+16+included<=pcap.length);
    const radio=offset+16;
    assert.equal(pcap[radio],0); assert.equal(pcap.readUInt16LE(radio+2),15); assert.equal(pcap.readUInt32LE(radio+4),0x2a);
    assert.equal(pcap[radio+8]&0x10,0,'FCS absent, as declared');
    if(count===0) {
      assert.equal(seconds,5000,'64-bit uptime survives 32-bit micros wrap'); assert.equal(micros,456789);
      assert.equal(pcap.readUInt16LE(radio+10),2437); assert.equal(pcap.readInt8(radio+14),-42);
      assert.deepEqual(pcap.subarray(radio+15,radio+included),first);
    }
    if(count===1) { assert.equal(included,783); assert.equal(original,915); assert.equal(pcap.readUInt16LE(radio+10),2462); }
    offset+=16+included; ++count;
  }
  assert.equal(offset,pcap.length); assert.equal(count,32);
  fs.writeFileSync(path.join(root,'build/capture-test.pcap'),pcap);
  const countedEapol=api.eapolCount();
  api.startRaw();
  assert.equal(api.rawCount(),0); assert.equal(api.rawDropped(),0); assert.equal(api.rawTruncated(),0);
  assert.equal(api.eapolCount(),countedEapol,'new PCAP preserves authentication history');
  assert.equal(api.probeCount(),3,'new PCAP preserves probe history');
  api.resetCapture(); assert.equal(api.encodePcap(),24); assert.equal(api.probeCount(),0); assert.equal(api.eapolCount(),0);
  observe(probe('x'.repeat(32))); assert.equal(api.probeCount(),1,'maximum SSID length accepted');
  observe(probe('x'.repeat(33))); assert.equal(api.probeCount(),1,'oversize SSID rejected');
  observe(probe('x'.repeat(32),2)); assert.equal(api.probeCount(),2,'same name, different source retained');
  observe(eapol(),1,0); observe(eapol(),1,14); assert.equal(api.eapolCount(),0,'unsupported channels rejected');
  // Deterministic hostile inputs exercise all lengths without overrunning buffers.
  for(let i=0;i<500;i++) { const bytes=Buffer.alloc(i%128); for(let j=0;j<bytes.length;j++) bytes[j]=(i*71+j*37)&255; observe(bytes); }
  assert(api.probeCount()<=24); assert(api.rawCount()<=32);
  console.log('Capture regressions passed: probe LRU/identity, EAPOL header variants and malformed frames, bounded buffers, and production PCAP encoding/independent decoding.');
})().catch(error=>{console.error(error);process.exitCode=1});
