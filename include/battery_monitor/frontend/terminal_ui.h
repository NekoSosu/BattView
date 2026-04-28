#ifndef BATTERY_MONITOR_FRONTEND_TERMINAL_UI_H
#define BATTERY_MONITOR_FRONTEND_TERMINAL_UI_H

#include "battery_monitor/app/view_model.h"

void TerminalUISetup(void);
void TerminalUIRestore(void);
void TerminalUIRender(const BatteryViewModel *model);
bool TerminalUIShouldQuit(void);
bool TerminalUISleepOrQuit(double seconds);

#endif
