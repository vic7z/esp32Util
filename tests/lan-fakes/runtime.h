#ifndef LAN_FAKE_RUNTIME_H
#define LAN_FAKE_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static uint32_t fakeTime = 0;
inline uint32_t millis(){return fakeTime;}
class IPAddress {
  uint8_t bytes[4];
public:
  IPAddress(uint32_t ip=0):bytes{uint8_t(ip>>24),uint8_t(ip>>16),uint8_t(ip>>8),uint8_t(ip)}{}
  uint8_t operator[](size_t i) const {return bytes[i];}
};
enum { WL_CONNECTED=3, ESP_OK=0, WIFI_STORAGE_RAM=0, WIFI_IF_STA=0 };
struct wifi_config_t {uint8_t bytes[128];};
static unsigned fakeConfigClears=0;
inline int esp_wifi_set_storage(int){return 0;}
inline int esp_wifi_set_config(int,const wifi_config_t*){++fakeConfigClears;return 0;}
struct FakeWiFi {
  bool connected=false;
  uint32_t ip=0xc0a8012a, mask=0xffffff00;
  int status(){return connected?WL_CONNECTED:0;}
  void setAutoReconnect(bool){}
  void persistent(bool){}
  void disconnect(bool,bool){connected=false;}
  void begin(const char*,const char*){}
  IPAddress localIP(){return IPAddress(ip);}
  IPAddress subnetMask(){return IPAddress(mask);}
  IPAddress softAPIP(){return IPAddress(0xc0a80401);}
};
static FakeWiFi WiFi;
using err_t=int;
constexpr int ERR_OK=0, ERR_IF=-1, ERR_MEM=-2;
struct netif {};
using esp_netif_t=netif;
struct ip4_addr_t {uint32_t addr;};
struct eth_addr {uint8_t addr[6];};
static netif fakeNet;
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*){return &fakeNet;}
inline void* esp_netif_get_netif_impl(esp_netif_t* n){return n;}
inline bool netif_is_up(netif*){return WiFi.connected;}
inline uint32_t lwip_htonl(uint32_t n){return ((n&255)<<24)|((n&0xff00)<<8)|((n>>8)&0xff00)|(n>>24);}
inline uint16_t lwip_htons(uint16_t n){return (n<<8)|(n>>8);}
static void(*fakeCallback)(void*)=nullptr;
static bool fakeQueueFail=false;
static uint32_t fakeArpRequests=0, fakeLastArpTarget=0;
static bool fakeNeighborFound=true;
inline err_t tcpip_try_callback(void(*fn)(void*),void*){
  if(fakeQueueFail||fakeCallback)return ERR_MEM;
  fakeCallback=fn;return ERR_OK;
}
inline err_t etharp_request(netif*,const ip4_addr_t* ip){++fakeArpRequests;fakeLastArpTarget=lwip_htonl(ip->addr);return ERR_OK;}
inline int etharp_find_addr(netif*,const ip4_addr_t*,eth_addr** mac,const ip4_addr_t** ip){
  static eth_addr value={{2,0,0,0,0,1}};static ip4_addr_t address={};
  *mac=fakeNeighborFound?&value:nullptr;*ip=&address;return fakeNeighborFound?0:-1;
}
enum {AF_INET=2,SOCK_STREAM=1,IPPROTO_TCP=6,F_GETFL=1,F_SETFL=2,O_NONBLOCK=4,SOL_SOCKET=1,SO_ERROR=2};
enum {EINPROGRESS=115,EWOULDBLOCK=11,ECONNREFUSED=111};
static int fakeErrno=0;
#define errno fakeErrno
using socklen_t=unsigned;
struct in_addr{uint32_t s_addr;};
struct sockaddr_in {unsigned char sin_len,sin_family;uint16_t sin_port;in_addr sin_addr;};
struct sockaddr{};
struct FakeFdSet{uint32_t bits;};
#define fd_set FakeFdSet
#define FD_ZERO(set) ((set)->bits=0)
#define FD_SET(fd,set) ((set)->bits|=(1u<<(fd)))
struct timeval{long tv_sec,tv_usec;};
// mode: 0=pending, 1=immediate open, 2=refused, 3=async open,
// 4=async refusal, 5=socket exhaustion, 6=fcntl failure, 7=select failure.
static int fakeSocketMode=0;
static unsigned fakeOpenSockets=0,fakeMaxSockets=0,fakeCloses=0,fakeConnections=0;
static uint32_t fakeLastTcpTarget=0;
static uint16_t fakeLastPort=0;
inline int lwip_socket(int,int,int){
  if(fakeSocketMode==5)return -1;
  ++fakeOpenSockets;if(fakeOpenSockets>fakeMaxSockets)fakeMaxSockets=fakeOpenSockets;return 7;
}
inline int lwip_close(int){if(fakeOpenSockets)--fakeOpenSockets;++fakeCloses;return 0;}
inline int lwip_fcntl(int,int,int){return fakeSocketMode==6?-1:0;}
inline int lwip_connect(int,const sockaddr* raw,size_t){
  const sockaddr_in* address=(const sockaddr_in*)raw;
  fakeLastTcpTarget=lwip_htonl(address->sin_addr.s_addr);fakeLastPort=lwip_htons(address->sin_port);++fakeConnections;
  fakeErrno=fakeSocketMode==2?ECONNREFUSED:EINPROGRESS;
  return fakeSocketMode==1?0:-1;
}
inline int lwip_select(int,void*,fd_set*,fd_set*,timeval*){return fakeSocketMode==7?-1:(fakeSocketMode==3||fakeSocketMode==4)?1:0;}
inline int lwip_getsockopt(int,int,int,void* value,socklen_t*){*(int*)value=fakeSocketMode==4?ECONNREFUSED:0;return 0;}
static char fakeResponse[8192], fakeRequest[256];
static size_t fakeResponseLength=0,fakeResponseAt=0,fakeRequestLength=0;
static unsigned fakeReadChunk=512, fakeSendMode=0;
static bool fakeHoldOpen=false, fakeReadError=false;
inline int lwip_send(int,const void* bytes,size_t length,int){
 if(fakeSendMode==2){fakeErrno=EWOULDBLOCK;return -1;}
 if(fakeSendMode==3){fakeErrno=99;return -1;}if(fakeSendMode==4)return 0;
 if(fakeSendMode==1&&length>7)length=7;
 for(size_t i=0;i<length&&fakeRequestLength<sizeof(fakeRequest)-1;++i)fakeRequest[fakeRequestLength++]=((const char*)bytes)[i];
 fakeRequest[fakeRequestLength]=0;return length;
}
inline int lwip_recv(int,void* bytes,size_t length,int){
 if(fakeReadError){fakeErrno=99;return -1;}
 if(fakeResponseAt==fakeResponseLength){if(fakeHoldOpen){fakeErrno=EWOULDBLOCK;return -1;}return 0;}
 if(length>fakeReadChunk)length=fakeReadChunk;
 if(length>fakeResponseLength-fakeResponseAt)length=fakeResponseLength-fakeResponseAt;
 memcpy(bytes,fakeResponse+fakeResponseAt,length);fakeResponseAt+=length;return length;
}
#endif
