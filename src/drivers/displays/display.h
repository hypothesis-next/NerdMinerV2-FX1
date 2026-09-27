#ifndef DISPLAY_H
#define DISPLAY_H

#include "displayDriver.h"

extern DisplayDriver *currentDisplayDriver;

void initDisplay();
void alternateScreenState();
void alternateScreenRotation();
void switchToNextScreen();
void resetToFirstScreen();
void drawLoadingScreen();
void drawSetupScreen();
void drawCurrentScreen(unsigned long mElapsed);
void animateCurrentScreen(unsigned long frame);
void doLedStuff(unsigned long frame);
// Temporarily lend the off-screen scratch buffer to secure statistics work.
// The LCD keeps its last complete frame; no mining task depends on this lock.
void beginStatsDisplayMemoryWindow();
void endStatsDisplayMemoryWindow();

#endif // DISPLAY_H
