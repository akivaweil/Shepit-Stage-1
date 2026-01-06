#ifndef WEBDASHBOARD_H
#define WEBDASHBOARD_H

void setupWebDashboard();
void handleWebDashboard();
void logToDashboard(const String& message);
void logToDashboard(const char* message);
void serialPrintln(const String& message);
void serialPrintln(const char* message);

#endif

