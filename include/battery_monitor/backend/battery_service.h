#ifndef BATTERY_MONITOR_BACKEND_BATTERY_SERVICE_H
#define BATTERY_MONITOR_BACKEND_BATTERY_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    BATTERY_FLOW_BATTERY = 0,
    BATTERY_FLOW_CHARGING,
    BATTERY_FLOW_PASSTHROUGH,
    BATTERY_FLOW_ASSIST
} BatteryFlowState;

typedef struct {
    double intervalSeconds;
    bool debugSamples;
} BatteryQuery;

typedef struct {
    bool valid;
    bool batteryPresent;
    bool onAC;
    bool isCharging;
    bool isDischarging;
    BatteryFlowState flow;
    int percent;
    int currentCapacity;
    int maxCapacity;
    int designCapacity;
    int cycleCount;
    int timeToEmpty;
    int timeToCharge;
    int sampleAgeSeconds;
    int amperageMA;
    double voltage;
    double amperageA;
    double batteryPowerW;
    double adapterPowerW;
    double systemPowerW;
    double cpuPowerW;
    double gpuPowerW;
    double temperatureC;
    double powerW;
    bool systemPowerLive;
    char health[64];
    char mfgDate[16];
} BatterySnapshot;

typedef struct {
    bool ok;
    BatterySnapshot snapshot;
} BatteryResponse;

typedef struct BatteryService BatteryService;

bool BatteryServiceInit(BatteryService **service);
void BatteryServiceClose(BatteryService *service);
bool BatteryServiceQuery(BatteryService *service, BatteryQuery query, BatteryResponse *response);

bool BatteryServiceBuildMock(const char *mode, BatteryResponse *response);
void BatteryFormatDuration(int minutes, char *out, size_t size);

#endif
