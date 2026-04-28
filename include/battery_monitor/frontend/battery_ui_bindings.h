#ifndef BATTERY_MONITOR_FRONTEND_BATTERY_UI_BINDINGS_H
#define BATTERY_MONITOR_FRONTEND_BATTERY_UI_BINDINGS_H

#include "battery_monitor/app/view_model.h"
#include "battery_monitor/termlayout/termlayout.h"

void BatteryUiBindingsFromViewModel(const BatteryViewModel *model, const char *timestamp, TlBindings *bindings);

#endif
