#include "../lan_engine.h"
extern "C" void* memcpy(void* a,const void* b,size_t n){for(size_t i=0;i<n;++i)((uint8_t*)a)[i]=((const uint8_t*)b)[i];return a;}
extern "C" void* memset(void* a,int b,size_t n){for(size_t i=0;i<n;++i)((uint8_t*)a)[i]=b;return a;}
static LanEngine engine;
static LanPortList list;
static char input[384]; static uint32_t parsedIP;
#define EXPORT(name) __attribute__((export_name(#name)))
extern "C" {
EXPORT(inputBuffer) char* inputBuffer(){return input;}
EXPORT(parseIP) int parseIP(){return parseLanIP(input,parsedIP);}
EXPORT(parsedAddress) uint32_t parsedAddress(){return parsedIP;}
EXPORT(parsePorts) int parsePorts(){return parseLanPorts(input,list);}
EXPORT(portCount) unsigned portCount(){return list.count;}
EXPORT(parsedPort) unsigned parsedPort(unsigned i){return list.ports[i];}
EXPORT(configure) void configure(uint32_t ip,uint32_t mask){engine={};engine.context.ip=ip;engine.context.mask=mask;}
EXPORT(localHost) int localHost(uint32_t target){return engine.context.localHost(target);}
EXPORT(defaultFirst) uint32_t defaultFirst(){uint32_t first,last;engine.context.defaultRange(first,last);return first;}
EXPORT(defaultLast) uint32_t defaultLast(){uint32_t first,last;engine.context.defaultRange(first,last);return last;}
EXPORT(discover) int discover(uint32_t first,uint32_t last){return engine.startDiscovery(first,last);}
EXPORT(startPorts) int startPorts(uint32_t target){return engine.startPorts(target,list);}
EXPORT(neighbor) void neighbor(int found,int error){uint8_t mac[6]={2,0,0,0,0,1};engine.neighbor(found,mac,error);}
EXPORT(portResult) void portResult(unsigned state){engine.portResult((LanPortState)state);}
EXPORT(cancel) void cancel(){engine.cancel();}
EXPORT(progress) unsigned progress(){return (unsigned)engine.progress;}
EXPORT(completed) unsigned completed(){return engine.completed;}
EXPORT(total) unsigned total(){return engine.total;}
EXPORT(hostCount) unsigned hostCount(){return engine.hostCount;}
EXPORT(omitted) unsigned omitted(){return engine.omitted;}
EXPORT(errors) unsigned errors(){return engine.errors;}
EXPORT(resultState) unsigned resultState(unsigned i){return (unsigned)engine.ports[i].state;}
}
