const fs=require('fs');
const extract=(file,name)=>{const s=fs.readFileSync(file,'utf8'),a=s.indexOf('void '+name+'('); if(a<0)throw name;let b=s.indexOf('{',a),level=1;while(level){++b;if(s[b]==='{')level++;if(s[b]==='}')level--;}return s.slice(a,b+1)};
const header=`#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "u8g2.h"
template<class T> T min(T a,T b){return a<b?a:b;}
template<class T> T max(T a,T b){return a>b?a:b;}
int overflow = 0;
struct OLED {
 u8g2_t g{}; uint8_t pixels[1024]{};
 void firstPage(){memset(pixels,0,sizeof(pixels));overflow=0;}
 bool nextPage(){return false;}
 void setFont(const uint8_t* f){u8g2_SetFont(&g,f);u8g2_SetFontMode(&g,1);}
 uint16_t getStrWidth(const char* t){return u8g2_GetStrWidth(&g,t);}
 void setDrawColor(uint8_t c){u8g2_SetDrawColor(&g,c);}
 void drawStr(int x,int y,const char* t){if(x<0 || x+getStrWidth(t)>128 || y>64 || y-u8g2_GetAscent(&g)<0)++overflow;u8g2_DrawStr(&g,x,y,t);}
 void drawHLine(int x,int y,int w){u8g2_DrawHLine(&g,x,y,w);}
 void drawPixel(int x,int y){u8g2_DrawPixel(&g,x,y);}
 void drawBox(int x,int y,int w,int h){u8g2_DrawBox(&g,x,y,w,h);}
 int cx=0,cy=0;
 void setCursor(int x,int y){cx=x;cy=y;}
 void printf(const char* fmt,...){char t[80];va_list a;va_start(a,fmt);vsnprintf(t,sizeof(t),fmt,a);va_end(a);drawStr(cx,cy,t);}
 void drawDisc(int x,int y,int r){u8g2_DrawDisc(&g,x,y,r,U8G2_DRAW_ALL);}
 void drawFrame(int x,int y,int w,int h){u8g2_DrawFrame(&g,x,y,w,h);}
 void drawRFrame(int x,int y,int w,int h,int r){u8g2_DrawRFrame(&g,x,y,w,h,r);}
 void drawRBox(int x,int y,int w,int h,int r){u8g2_DrawRBox(&g,x,y,w,h,r);}
 void drawVLine(int x,int y,int h){u8g2_DrawVLine(&g,x,y,h);}
 void drawLine(int x,int y,int xx,int yy){u8g2_DrawLine(&g,x,y,xx,yy);}
} oled;
void drawFooter(const char*,const char* = nullptr);
void drawFooterPill(uint8_t,const char*,bool = false);
struct {uint8_t currentChannel=6,selectedChannel=6;bool frozen=false;} scanState;
struct CaptureSummary {uint32_t eapol;uint8_t probeCount;};
CaptureSummary getCaptureSummary(){return {12,24};}
uint32_t pktBeacon=48,pktData=123,pktProbe=24,pktDeauth=0;
int liveLoad(){return 32;}
template<class T> T constrain(T v,T a,T b){return v<a?a:v>b?b:v;}
uint32_t pps=1234;float avgRssi=-62;uint32_t history[128];uint8_t histIdx=0;
#define HISTORY_SIZE 128
#define AP_VISIBLE 3
#define BLE_VISIBLE 3
struct wifi_ap_record_t {uint8_t ssid[33]; int rssi;uint8_t primary;int authmode;uint8_t bssid[6];};
wifi_ap_record_t apList[3] = {{{'H','o','m','e',' ','W','i','F','i',0},-42,6,0,{0}},{{'A',' ','v','e','r','y',' ','l','o','n','g',' ','n','e','t','w','o','r','k',' ','n','a','m','e',0},-100,13,0,{0}},{{0},-78,1,0,{0}}};
uint16_t apCount=3;uint8_t apCursor=1,apScroll=0,apSelectedIndex=1;
struct BLEDeviceInfo {char name[40];int rssi;bool isActive;uint8_t advType;char address[18];};
BLEDeviceInfo bleDevices[3]={{"Desk sensor",-51,true,0,"AA:BB:CC:DD:EE:01"},{"Very long BLE device name",-100,true,1,"AA:BB:CC:DD:EE:02"},{"",-89,false,0,"AA:BB:CC:DD:EE:03"}};
uint16_t bleDeviceCount=3;uint8_t bleCursor=1,bleScroll=0,bleSelectedIndex=1;
const char* authStr(int){return "WPA2 / WPA3";}
const char* getVendor(uint8_t*){return "Very long manufacturer name";}
struct {uint8_t viewMode=0;uint16_t totalAPs=32,totalBLE=12;} autoWatch;
uint32_t deauthPerSecond=0;
#define MAX_CHANNEL 13
uint8_t bestChannel(){return 1;}uint8_t worstChannel(){return 6;}uint8_t channelLoad(int ch){return (ch*17)%101;}int countOverlappingAPs(int ch){return ch==6?4:1;}
#define MAX_MONITORED_DEVICES 3
struct MonitoredDevice {bool active=true; char name[40]; int rssi=-50;bool isPresent=true;uint8_t type=0;uint16_t seenCount=65535;uint8_t bssid[6]{};char bleAddr[18]="AA:BB:CC:DD:EE:01";};
MonitoredDevice monitoredDevices[3]={{true,"Desk client"},{true,"Very long client device name"},{true,""}};uint8_t deviceScroll=0,deviceCursor=1,deviceSelectedIndex=1;uint16_t monitoredDeviceCount=3;
void drawPlaceholder(const char*,const char*){}
struct {int rssiThreshold=-40;char apSSID[33]="My configured WiFi network";char apPassword[65]="fixture only";} settings;
`;
const functions=['drawTextFit','drawHeader','drawGraphHeader','drawFooterPill','drawFooter','drawScrollDots','drawGrid','drawPatternBar'].map(n=>extract('display.cpp',n)).join('\n')+'\n'+['drawGenericMenu','drawMonitorFooter','drawMonitor','drawApList','drawApDetail','drawBLEScan','drawBLEDetail','drawAutoWatch','drawDeviceMonitor','drawDeviceDetail','drawWebServer','drawAnalyzer'].map(n=>extract('screens_draw.cpp',n)).join('\n');
const footer=`extern "C" __attribute__((export_name("render"))) uint8_t* render(int screen) {
 static u8x8_display_info_t info{}; info.tile_width=16;info.tile_height=8;info.pixel_width=128;info.pixel_height=64;
 memset(&oled.g,0,sizeof(oled.g));oled.g.u8x8.display_info=&info;
 u8g2_SetupBuffer(&oled.g,oled.pixels,8,u8g2_ll_hvline_vertical_top_lsb,U8G2_R0);
 for(int i=0;i<128;++i)history[i]=(i*7)%40;
 const char* items[]={"Auto Watch","RF Health","Live Monitor","Channel Analyzer","Device Monitor","AP Scanner","BLE Monitor","Security","Insights","History","Web Server","System"};uint8_t cursor=3;
 if(screen==0)drawGenericMenu("POCKET RF TOOL",items,12,cursor);
 if(screen==1)drawApList();if(screen==2)drawApDetail();if(screen==3)drawBLEScan();if(screen==4)drawBLEDetail();if(screen==5)drawMonitor();if(screen==6)drawAutoWatch();
 if(screen==7){apCount=0;drawApList();apCount=3;}
 if(screen==8){scanState.frozen=true;drawMonitor();scanState.frozen=false;}
 if(screen==9)drawDeviceMonitor();if(screen==10)drawDeviceDetail();if(screen==11)drawAnalyzer();
 if(screen==12){pps=0xffffffff;avgRssi=-100;drawMonitor();pps=1234;}
 if(screen==13){cursor=11;drawGenericMenu("SYSTEM MENU WITH LONG TITLE",items,12,cursor);}
 if(screen==14){cursor=0;drawGenericMenu("EMPTY",nullptr,0,cursor);}
 if(screen==15){oled.firstPage();oled.setFont(u8g2_font_6x10_tf);drawTextFit(120,26,"Too long",8);drawTextFit(0,39,"Control\\nname",60);}
 return oled.pixels;
}
`;
fs.writeFileSync('build/oled-render.cpp',header+functions+footer+'\nextern \"C\" __attribute__((export_name(\"overflow\"))) int getOverflow(){return overflow;}\n');
