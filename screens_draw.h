#ifndef SCREENS_DRAW_H
#define SCREENS_DRAW_H

#include "config.h"
#include "screens.h"

void drawMenu();
void drawSecurityMenu();
void drawInsightsMenu();
void drawHistoryMenu();
void drawSystemMenu();

void drawAutoWatch();
void drawRFHealth();
void drawMonitor();
void drawAnalyzer();
void drawDeviceMonitor();
void drawDeviceDetail();

void drawApList();
void drawApDetail();
void drawAPWalkTest();
void drawCompare();

void drawBLEScan();
void drawBLEDetail();
void drawBLEWalkTest();

void drawDeauthWatch();
void drawRogueAPWatch();
void drawBLETrackerWatch();
void drawAlertSettings();

void drawWhyIsItSlow();
void drawChannelRecommendation();
void drawEnvironmentChange();

void drawEventLog();
void drawBaselineCompare();

void drawBatteryPower();
void drawDisplaySettings();
void drawRadioControl();
void drawAbout();

void drawHiddenSSID();
void drawStats();
void drawQuickSnapshot();
void drawRSSIMeter();
void drawChannelScorecard();
void drawExport();
void drawPowerMode();

void drawPlaceholder(const char* title, const char* subtitle);

void takeSnapshot(Baseline* snap);

#endif // SCREENS_DRAW_H
