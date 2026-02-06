#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

void startWebServer();
void stopWebServer();
void handleWebServerLoop();

#endif
