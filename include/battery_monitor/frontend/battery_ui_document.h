#ifndef BATTERY_MONITOR_FRONTEND_BATTERY_UI_DOCUMENT_H
#define BATTERY_MONITOR_FRONTEND_BATTERY_UI_DOCUMENT_H

#include "battery_monitor/termlayout/termlayout.h"

bool BatteryUiDocumentBuild(TlCompiledDocument *out, char *errBuf, int errBufLen);

#endif
