#ifndef BATTERY_MONITOR_APP_VIEW_MODEL_H
#define BATTERY_MONITOR_APP_VIEW_MODEL_H

#include "battery_monitor/backend/battery_service.h"

#define STATUS_ROW_COUNT 12

typedef struct {
    const char *label;
    const char *value;
} StatusRow;

typedef struct {
    double acInW;
    double batteryInW;
    double batteryOutW;
    double systemOutW;
    double cpuW;
    double gpuW;
    double otherW;
    double totalW;
    BatteryFlowState flow;
} PowerFlowViewModel;

typedef struct {
    bool valid;
    int percent;
    int truePercent;
    int currentCapacity;
    int maxCapacity;
    int designCapacity;
    int healthRatio;
    char healthText[64];
    char healthFull[96];
    char timeText[32];
    char powerText[32];
    char voltageText[32];
    char amperageText[32];
    char temperatureText[32];
    char chargeText[32];
    char maxCapacityText[32];
    char designCapacityText[32];
    char cyclesText[32];
    char mfgDateText[32];
    char powerModeText[32];
    const char *timeLabel;
    const char *powerLabel;
    StatusRow rows[STATUS_ROW_COUNT];
    PowerFlowViewModel powerFlow;
} BatteryViewModel;

void BatteryViewModelBuild(const BatterySnapshot *snapshot, BatteryViewModel *model);

#endif
