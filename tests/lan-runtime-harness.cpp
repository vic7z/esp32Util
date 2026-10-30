// Compile the real hardware orchestration against deterministic SDK substitutes.
// No socket or ARP traffic leaves this WebAssembly test harness.
#include "../lan_network.cpp"
extern "C" void* memcpy(void* a,const void* b,size_t n){for(size_t i=0;i<n;++i)((uint8_t*)a)[i]=((const uint8_t*)b)[i];return a;}
extern "C" void* memset(void* a,int b,size_t n){for(size_t i=0;i<n;++i)((uint8_t*)a)[i]=b;return a;}
extern "C" size_t strlen(const char* a){size_t n=0;while(a[n])++n;return n;}
extern "C" char* strncpy(char* a,const char* b,size_t n){size_t i=0;while(i<n&&b[i]){a[i]=b[i];++i;}while(i<n)a[i++]=0;return a;}
#define EXPORT(name) __attribute__((export_name(#name)))
extern "C" {
EXPORT(reset) void reset(){fakeResponseLength=fakeResponseAt=fakeRequestLength=0;fakeReadChunk=512;fakeSendMode=0;fakeHoldOpen=fakeReadError=false;identityLength=identitySent=identityRequestLength=0;identityStage=0;fakeCallback=nullptr;fakeQueueFail=false;fakeTime=0;fakeOpenSockets=fakeMaxSockets=fakeCloses=fakeConnections=fakeArpRequests=0;fakeSocketMode=0;fakeNeighborFound=true;WiFi=FakeWiFi();socketFD=-1;arpJob={};arpGeneration=0;arpStage=0;status={};}
EXPORT(connect) int connect(){return connectLan("fixture-network","fixture-password");}
EXPORT(setConnected) void setConnected(int value){WiFi.connected=value;}
EXPORT(setAddress) void setAddress(uint32_t ip,uint32_t mask){WiFi.ip=ip;WiFi.mask=mask;}
EXPORT(tick) void tick(uint32_t elapsed,int runCallback){fakeTime+=elapsed;if(runCallback&&fakeCallback){auto fn=fakeCallback;fakeCallback=nullptr;fn(nullptr);}updateLanNetwork();}
EXPORT(setTime) void setTime(uint32_t value){fakeTime=value;}
EXPORT(connectionState) unsigned connectionState(){return (unsigned)getLanStatus().connection;}
EXPORT(discover) int discover(uint32_t first,uint32_t last){return startLanDiscovery(first,last);}
EXPORT(ports) int ports(uint32_t target){LanPortList list;parseLanPorts("22,80,443",list);return startLanPortScan(target,list);}
EXPORT(cancel) void cancel(){cancelLanScan();}
EXPORT(disconnect) void disconnect(){disconnectLan();}
EXPORT(socketMode) void socketMode(int mode){fakeSocketMode=mode;}
EXPORT(queueFailure) void queueFailure(int value){fakeQueueFail=value;}
EXPORT(neighborFound) void neighborFound(int value){fakeNeighborFound=value;}
EXPORT(openSockets) unsigned openSockets(){return fakeOpenSockets;}
EXPORT(maxSockets) unsigned maxSockets(){return fakeMaxSockets;}
EXPORT(connections) unsigned connections(){return fakeConnections;}
EXPORT(lastTarget) uint32_t lastTarget(){return fakeLastTcpTarget;}
EXPORT(lastPort) unsigned lastPort(){return fakeLastPort;}
EXPORT(arpRequests) unsigned arpRequests(){return fakeArpRequests;}
EXPORT(completed) unsigned completed(){return getLanStatus().scan.completed;}
EXPORT(progress) unsigned progress(){return (unsigned)getLanStatus().scan.progress;}
EXPORT(hostCount) unsigned hostCount(){return getLanStatus().scan.hostCount;}
EXPORT(portState) unsigned portState(unsigned index){return (unsigned)getLanStatus().scan.ports[index].state;}
EXPORT(identify) int identify(uint32_t target,unsigned port){return startLanIdentification(target,port);}
EXPORT(parseSample) int parseSample(){return parseLanIdentity(fakeResponse,fakeResponseLength,status.identity);}
EXPORT(identityHeaders) int identityHeaders(){return status.identity.headersComplete;}
EXPORT(identityChunked) int identityChunked(){return status.identity.chunked;}
EXPORT(identityEncoded) int identityEncoded(){return status.identity.encoded;}
EXPORT(responsePtr) char* responsePtr(){return fakeResponse;}
EXPORT(responseLength) void responseLength(unsigned n){fakeResponseLength=n<sizeof(fakeResponse)?n:sizeof(fakeResponse);fakeResponseAt=0;}
EXPORT(readChunk) void readChunk(unsigned n){fakeReadChunk=n;}
EXPORT(holdOpen) void holdOpen(int value){fakeHoldOpen=value;}
EXPORT(sendMode) void sendMode(unsigned value){fakeSendMode=value;}
EXPORT(readError) void readError(int value){fakeReadError=value;}
EXPORT(requestPtr) char* requestPtr(){return fakeRequest;}
EXPORT(savedPortCount) unsigned savedPortCount(){return status.portCount;}
EXPORT(savedPortTarget) uint32_t savedPortTarget(){return status.portTarget;}
EXPORT(identityState) unsigned identityState(){return (unsigned)status.identity.state;}
EXPORT(identityCode) unsigned identityCode(){return status.identity.statusCode;}
EXPORT(identityTitle) const char* identityTitle(){return status.identity.title;}
EXPORT(identityServer) const char* identityServer(){return status.identity.server;}
EXPORT(identityCategory) const char* identityCategory(){return status.identity.category;}
EXPORT(identityLimited) int identityLimited(){return status.identity.limited;}
}
