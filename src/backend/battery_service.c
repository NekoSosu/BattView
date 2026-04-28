#include "battery_monitor/backend/battery_service.h"
#include "smc_private.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool BatteryPowerBreakdownRead(double *cpuPower, double *gpuPower, double *otherPower);

typedef struct {
    bool present;
    long long sampledAtSeconds;
    int sourceAgeSeconds;
    bool externalConnected;
    bool isCharging;
    int currentCapacity;
    int maxCapacity;
    int amperageMA;
    double voltageV;
    double temperatureC;
    int timeToEmpty;
    int timeToFull;
} BatteryLiveSnapshot;

typedef struct {
    bool present;
    long long sampledAtSeconds;
    double adapterPowerW;
    double systemPowerW;
    double cpuPowerW;
    double gpuPowerW;
    bool systemPowerLive;
} PowerLiveSnapshot;

typedef struct {
    bool present;
    long long refreshedAtSeconds;
    int designCapacity;
    int cycleCount;
    char mfgDate[16];
} BatteryStaticSnapshot;

struct BatteryService {
    bool initialized;
    long long lastStaticRefresh;
    double staticIntervalSeconds;
    io_connect_t smcConn;
    BatteryStaticSnapshot staticCache;
    BatteryFlowState stableFlow;
    BatteryFlowState candidateFlow;
    int candidateCount;
};

static unsigned short readUInt16BE(const unsigned char *bytes) {
    return (unsigned short)(((unsigned short)bytes[0] << 8) | (unsigned short)bytes[1]);
}

static unsigned short readUInt16LE(const unsigned char *bytes) {
    return (unsigned short)(((unsigned short)bytes[1] << 8) | (unsigned short)bytes[0]);
}

static short readSInt16BE(const unsigned char *bytes) {
    return (short)readUInt16BE(bytes);
}

static short readSInt16LE(const unsigned char *bytes) {
    return (short)readUInt16LE(bytes);
}

static int readSInt32BE(const unsigned char *bytes) {
    return ((int)bytes[0] << 24) | ((int)bytes[1] << 16) | ((int)bytes[2] << 8) | (int)bytes[3];
}

static int readSInt32LE(const unsigned char *bytes) {
    return ((int)bytes[3] << 24) | ((int)bytes[2] << 16) | ((int)bytes[1] << 8) | (int)bytes[0];
}

static long long wallSeconds(void) {
    return (long long)time(NULL);
}

static double getNumberValue(CFDictionaryRef dict, CFStringRef key, bool *ok) {
    if (ok) *ok = false;
    if (!dict) return 0.0;
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return 0.0;
    double result = 0.0;
    if (!CFNumberGetValue((CFNumberRef)value, kCFNumberDoubleType, &result)) return 0.0;
    if (ok) *ok = true;
    return result;
}

static bool getIntValue(CFDictionaryRef dict, CFStringRef key, int *outValue) {
    if (!dict || !outValue) return false;
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return false;
    return CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, outValue);
}

static bool getLongValue(CFDictionaryRef dict, CFStringRef key, long long *outValue) {
    if (!dict || !outValue) return false;
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) return false;
    return CFNumberGetValue((CFNumberRef)value, kCFNumberLongLongType, outValue);
}

static CFDictionaryRef getDictValue(CFDictionaryRef dict, CFStringRef key) {
    if (!dict) return NULL;
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFDictionaryGetTypeID()) return NULL;
    return (CFDictionaryRef)value;
}

static bool getArrayDictIntValue(CFDictionaryRef dict, CFStringRef arrayKey, CFStringRef key, int *outValue) {
    if (!dict || !outValue) return false;
    CFTypeRef value = CFDictionaryGetValue(dict, arrayKey);
    if (!value || CFGetTypeID(value) != CFArrayGetTypeID()) return false;
    CFArrayRef array = (CFArrayRef)value;
    if (CFArrayGetCount(array) < 1) return false;
    CFTypeRef first = CFArrayGetValueAtIndex(array, 0);
    if (!first || CFGetTypeID(first) != CFDictionaryGetTypeID()) return false;
    return getIntValue((CFDictionaryRef)first, key, outValue);
}

static bool formatManufactureDate(long long rawDate, char *out, size_t size) {
    if (rawDate <= 0 || !out || size == 0) return false;
    if (rawDate <= 0xFFFF) {
        int day = (int)rawDate & 0x1F;
        int month = ((int)rawDate >> 5) & 0x0F;
        int year = ((int)rawDate >> 9) + 1980;
        if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            snprintf(out, size, "%04d-%02d-%02d", year, month, day);
            return true;
        }
    }
    if (rawDate <= 0xFFFFFFFFFFFFLL) {
        char encoded[7];
        long long value = rawDate;
        for (int i = 5; i >= 0; --i) {
            int byte = (int)(value & 0xFF);
            if (byte < '0' || byte > '9') return false;
            encoded[i] = (char)byte;
            value >>= 8;
        }
        encoded[6] = '\0';
        int year = 1992 + (encoded[5] - '0') * 10 + (encoded[4] - '0');
        int month = (encoded[3] - '0') * 10 + (encoded[2] - '0');
        int day = (encoded[1] - '0') * 10 + (encoded[0] - '0');
        if (year >= 1980 && year <= 2107 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            snprintf(out, size, "%04d-%02d-%02d", year, month, day);
            return true;
        }
    }
    return false;
}

static void readIOPSFallback(BatteryLiveSnapshot *live) {
    CFTypeRef info = IOPSCopyPowerSourcesInfo();
    if (!info) return;
    CFArrayRef list = IOPSCopyPowerSourcesList(info);
    if (!list) {
        CFRelease(info);
        return;
    }
    CFIndex count = CFArrayGetCount(list);
    for (CFIndex i = 0; i < count; ++i) {
        CFTypeRef ps = CFArrayGetValueAtIndex(list, i);
        CFDictionaryRef desc = IOPSGetPowerSourceDescription(info, ps);
        if (!desc) continue;
        CFStringRef type = CFDictionaryGetValue(desc, CFSTR("Type"));
        if (!type || CFStringCompare(type, CFSTR("InternalBattery"), 0) != kCFCompareEqualTo) continue;
        int value = 0;
        if (!live->present && getIntValue(desc, CFSTR("Current Capacity"), &value)) {
            live->currentCapacity = value;
            live->present = true;
        }
        if (live->maxCapacity <= 0 && getIntValue(desc, CFSTR("Max Capacity"), &value)) live->maxCapacity = value;
        if (live->timeToEmpty < 0 && getIntValue(desc, CFSTR("Time to Empty"), &value) && value >= 0) live->timeToEmpty = value;
        if (live->timeToFull < 0 && getIntValue(desc, CFSTR("Time to Full Charge"), &value) && value >= 0 && value < 10000) live->timeToFull = value;
        CFTypeRef charging = CFDictionaryGetValue(desc, CFSTR("Is Charging"));
        if (charging && CFGetTypeID(charging) == CFBooleanGetTypeID()) live->isCharging = CFBooleanGetValue((CFBooleanRef)charging);
        CFTypeRef state = CFDictionaryGetValue(desc, CFSTR("Power Source State"));
        if (state && CFGetTypeID(state) == CFStringGetTypeID()) {
            live->externalConnected = CFStringCompare((CFStringRef)state, CFSTR("AC Power"), 0) == kCFCompareEqualTo;
        }
    }
    CFRelease(list);
    CFRelease(info);
}

static void readStaticFields(CFDictionaryRef props, BatteryStaticSnapshot *staticData) {
    int value = 0;
    if (getIntValue(props, CFSTR("DesignCapacity"), &value)) staticData->designCapacity = value;
    if (getIntValue(props, CFSTR("CycleCount"), &value)) staticData->cycleCount = value;
    long long mfg = 0;
    if (getLongValue(props, CFSTR("ManufactureDate"), &mfg)) formatManufactureDate(mfg, staticData->mfgDate, sizeof(staticData->mfgDate));
    if (!staticData->mfgDate[0]) {
        CFDictionaryRef batteryData = getDictValue(props, CFSTR("BatteryData"));
        if (batteryData && getLongValue(batteryData, CFSTR("ManufactureDate"), &mfg)) {
            formatManufactureDate(mfg, staticData->mfgDate, sizeof(staticData->mfgDate));
        }
    }
    staticData->present = true;
    staticData->refreshedAtSeconds = wallSeconds();
}

static bool readSMCVoltage(io_connect_t conn, const char *key, double registryVoltageV, double *outVoltageV) {
    SMCKeyData val;
    if (BatterySMCReadKey(conn, key, &val) != kIOReturnSuccess || val.keyInfo.dataSize < 2) return false;
    double be = readUInt16BE((const unsigned char *)val.bytes) / 1000.0;
    double le = readUInt16LE((const unsigned char *)val.bytes) / 1000.0;
    bool beOk = be >= 5.0 && be <= 20.0;
    bool leOk = le >= 5.0 && le <= 20.0;
    if (!beOk && !leOk) return false;
    if (beOk && !leOk) *outVoltageV = be;
    else if (!beOk && leOk) *outVoltageV = le;
    else if (registryVoltageV > 0.0 && fabs(be - registryVoltageV) <= fabs(le - registryVoltageV)) *outVoltageV = be;
    else *outVoltageV = le;
    return true;
}

static bool readSMCCurrent(io_connect_t conn, const char *key, double registryCurrentA, double *outCurrentA) {
    SMCKeyData val;
    if (BatterySMCReadKey(conn, key, &val) != kIOReturnSuccess || val.keyInfo.dataSize < 2) return false;
    double be = readSInt16BE((const unsigned char *)val.bytes) / 1000.0;
    double le = readSInt16LE((const unsigned char *)val.bytes) / 1000.0;
    bool beOk = fabs(be) <= 15.0;
    bool leOk = fabs(le) <= 15.0;
    if (!beOk && !leOk) return false;
    if (beOk && !leOk) *outCurrentA = be;
    else if (!beOk && leOk) *outCurrentA = le;
    else if (fabs(registryCurrentA) > 0.0 && fabs(be - registryCurrentA) <= fabs(le - registryCurrentA)) *outCurrentA = be;
    else *outCurrentA = le;
    return true;
}

static bool readSMCBatteryPower(io_connect_t conn, const char *key, double *outPowerW) {
    SMCKeyData val;
    if (BatterySMCReadKey(conn, key, &val) != kIOReturnSuccess || val.keyInfo.dataSize < 4) return false;
    double be = readSInt32BE((const unsigned char *)val.bytes) / 1000.0;
    double le = readSInt32LE((const unsigned char *)val.bytes) / 1000.0;
    bool beOk = fabs(be) <= 200.0;
    bool leOk = fabs(le) <= 200.0;
    if (!beOk && !leOk) return false;
    if (beOk && !leOk) *outPowerW = be;
    else if (!beOk && leOk) *outPowerW = le;
    else *outPowerW = fabs(be) >= fabs(le) ? be : le;
    return true;
}

static void overlaySMCLive(BatteryService *service, BatteryLiveSnapshot *live, PowerLiveSnapshot *power) {
    if (!service->smcConn) return;
    double systemPower = BatterySMCGetFloatValue(service->smcConn, "PSTR");
    if (systemPower > 0.0 && systemPower <= 400.0) {
        power->systemPowerW = systemPower;
        power->systemPowerLive = true;
    }
    double voltage = 0.0;
    if (readSMCVoltage(service->smcConn, "B0AV", live->voltageV, &voltage) && voltage > 0.0) {
        live->voltageV = voltage;
    }
    double currentA = 0.0;
    if (readSMCCurrent(service->smcConn, "B0AC", live->amperageMA / 1000.0, &currentA)) {
        live->amperageMA = (int)(currentA * 1000.0);
    }
    double batteryPower = 0.0;
    if (readSMCBatteryPower(service->smcConn, "B0AP", &batteryPower) && fabs(batteryPower) > 0.05 && live->voltageV > 0.0) {
        double magnitudeA = fabs(batteryPower) / live->voltageV;
        int sign = live->amperageMA < 0 ? -1 : 1;
        if (live->externalConnected && live->isCharging) sign = 1;
        live->amperageMA = (int)(sign * magnitudeA * 1000.0);
    }
    double temperature = BatterySMCGetFloatValue(service->smcConn, "B0CT");
    if (temperature > 0.0 && temperature < 100.0) live->temperatureC = temperature;
}

static void readAppleSmartBattery(BatteryService *service, BatteryLiveSnapshot *live, PowerLiveSnapshot *power, bool refreshStatic) {
    io_service_t battery = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));
    if (battery == MACH_PORT_NULL) return;
    CFMutableDictionaryRef props = NULL;
    if (IORegistryEntryCreateCFProperties(battery, &props, kCFAllocatorDefault, 0) != KERN_SUCCESS || !props) {
        IOObjectRelease(battery);
        return;
    }

    live->present = true;
    live->sampledAtSeconds = wallSeconds();
    power->present = true;
    power->sampledAtSeconds = live->sampledAtSeconds;

    int intValue = 0;
    if (getIntValue(props, CFSTR("UpdateTime"), &intValue) && intValue > 0) {
        live->sourceAgeSeconds = (int)fmax(0.0, difftime(time(NULL), (time_t)intValue));
    }
    CFTypeRef ext = CFDictionaryGetValue(props, CFSTR("ExternalConnected"));
    if (ext && CFGetTypeID(ext) == CFBooleanGetTypeID()) live->externalConnected = CFBooleanGetValue((CFBooleanRef)ext);
    CFTypeRef charging = CFDictionaryGetValue(props, CFSTR("IsCharging"));
    if (!charging) charging = CFDictionaryGetValue(props, CFSTR("Is Charging"));
    if (charging && CFGetTypeID(charging) == CFBooleanGetTypeID()) live->isCharging = CFBooleanGetValue((CFBooleanRef)charging);

    if (getIntValue(props, CFSTR("AppleRawCurrentCapacity"), &intValue)) live->currentCapacity = intValue;
    if (getIntValue(props, CFSTR("AppleRawMaxCapacity"), &intValue)) live->maxCapacity = intValue;
    if (getIntValue(props, CFSTR("Amperage"), &intValue) || getIntValue(props, CFSTR("InstantAmperage"), &intValue)) {
        live->amperageMA = intValue;
    }
    bool ok = false;
    double voltage = getNumberValue(props, CFSTR("AppleRawBatteryVoltage"), &ok);
    if (!ok) voltage = getNumberValue(props, CFSTR("Voltage"), &ok);
    if (ok && voltage > 0.0) live->voltageV = voltage / 1000.0;
    if (getIntValue(props, CFSTR("Temperature"), &intValue)) {
        live->temperatureC = intValue > 2000 ? (intValue / 10.0) - 273.15 : intValue / 100.0;
    }
    live->timeToEmpty = -1;
    live->timeToFull = -1;
    if (getIntValue(props, CFSTR("AvgTimeToEmpty"), &intValue) && intValue >= 0 && intValue < 10000) live->timeToEmpty = intValue;
    if (getIntValue(props, CFSTR("AvgTimeToFull"), &intValue) && intValue >= 0 && intValue < 10000) live->timeToFull = intValue;

    int watts = 0, millivolts = 0, milliamps = 0;
    if (getArrayDictIntValue(props, CFSTR("AppleRawAdapterDetails"), CFSTR("Watts"), &watts) && watts > 0) {
        power->adapterPowerW = watts;
    } else if (getArrayDictIntValue(props, CFSTR("AppleRawAdapterDetails"), CFSTR("Voltage"), &millivolts) &&
               getArrayDictIntValue(props, CFSTR("AppleRawAdapterDetails"), CFSTR("Current"), &milliamps) &&
               millivolts > 0 && milliamps > 0) {
        power->adapterPowerW = (millivolts / 1000.0) * (milliamps / 1000.0);
    }

    CFDictionaryRef batteryData = getDictValue(props, CFSTR("BatteryData"));
    if (batteryData) {
        bool systemOk = false;
        double systemPower = getNumberValue(batteryData, CFSTR("SystemPower"), &systemOk);
        if (systemOk && systemPower > 0.0) {
            power->systemPowerW = systemPower;
            power->systemPowerLive = true;
        } else if (getIntValue(batteryData, CFSTR("SystemLoad"), &intValue) && intValue > 0) {
            power->systemPowerW = intValue / 1000.0;
        }
    }
    if (power->systemPowerW <= 0.0) {
        CFDictionaryRef telemetry = getDictValue(props, CFSTR("PowerTelemetryData"));
        if (telemetry && getIntValue(telemetry, CFSTR("SystemLoad"), &intValue) && intValue > 0) {
            power->systemPowerW = intValue / 1000.0;
        }
    }
    double cpu = 0.0, gpu = 0.0, other = 0.0;
    if (BatteryPowerBreakdownRead(&cpu, &gpu, &other)) {
        if (cpu > 0.0 && cpu <= 100.0) power->cpuPowerW = cpu;
        if (gpu > 0.0 && gpu <= 100.0) power->gpuPowerW = gpu;
        if (other > 0.0 && other <= 400.0 && power->systemPowerW <= 0.0) power->systemPowerW = other;
    }
    overlaySMCLive(service, live, power);

    if (refreshStatic) readStaticFields(props, &service->staticCache);
    CFRelease(props);
    IOObjectRelease(battery);
}

static BatteryFlowState rawFlowFromLive(const BatteryLiveSnapshot *live, double signedBatteryPowerW) {
    const double deadband = 0.10;
    if (!live->externalConnected) return BATTERY_FLOW_BATTERY;
    if (signedBatteryPowerW > deadband || live->isCharging) return BATTERY_FLOW_CHARGING;
    if (signedBatteryPowerW < -deadband) return BATTERY_FLOW_ASSIST;
    return BATTERY_FLOW_PASSTHROUGH;
}

static BatteryFlowState stabilizeFlow(BatteryService *service, BatteryFlowState candidate) {
    if (candidate == BATTERY_FLOW_BATTERY || service->stableFlow == BATTERY_FLOW_BATTERY) {
        service->stableFlow = candidate;
        service->candidateFlow = candidate;
        service->candidateCount = 0;
        return service->stableFlow;
    }
    if (candidate == BATTERY_FLOW_CHARGING) {
        service->stableFlow = candidate;
        service->candidateFlow = candidate;
        service->candidateCount = 0;
        return service->stableFlow;
    }
    if (candidate != service->candidateFlow) {
        service->candidateFlow = candidate;
        service->candidateCount = 1;
    } else {
        service->candidateCount++;
    }
    if (service->candidateCount >= 2) service->stableFlow = candidate;
    return service->stableFlow;
}

static void setHealth(const BatterySnapshot *snapshot, char *out, size_t size) {
    if (snapshot->maxCapacity <= 0 || snapshot->designCapacity <= 0) {
        snprintf(out, size, "Unknown");
        return;
    }
    int ratio = (int)((double)snapshot->maxCapacity / snapshot->designCapacity * 100.0 + 0.5);
    if (ratio >= 90) snprintf(out, size, "Good");
    else if (ratio >= 75) snprintf(out, size, "Fair");
    else snprintf(out, size, "Svc Req.");
}

static void deriveSnapshot(BatteryService *service, const BatteryLiveSnapshot *live, const PowerLiveSnapshot *power, BatterySnapshot *out) {
    memset(out, 0, sizeof(*out));
    out->valid = live->present;
    out->batteryPresent = live->present;
    out->onAC = live->externalConnected;
    out->currentCapacity = live->currentCapacity;
    out->maxCapacity = live->maxCapacity;
    out->designCapacity = service->staticCache.designCapacity;
    out->cycleCount = service->staticCache.cycleCount;
    out->timeToEmpty = live->timeToEmpty;
    out->timeToCharge = live->timeToFull;
    out->sampleAgeSeconds = live->sourceAgeSeconds;
    out->amperageMA = live->amperageMA;
    out->amperageA = live->amperageMA / 1000.0;
    out->voltage = live->voltageV;
    out->temperatureC = live->temperatureC;
    out->adapterPowerW = power->adapterPowerW;
    out->systemPowerW = power->systemPowerW;
    out->cpuPowerW = power->cpuPowerW;
    out->gpuPowerW = power->gpuPowerW;
    out->systemPowerLive = power->systemPowerLive;
    if (out->maxCapacity > 0) out->percent = (int)((double)out->currentCapacity / out->maxCapacity * 100.0 + 0.5);
    double signedBatteryPowerW = live->voltageV * (live->amperageMA / 1000.0);
    out->powerW = fabs(signedBatteryPowerW);
    out->flow = stabilizeFlow(service, rawFlowFromLive(live, signedBatteryPowerW));
    out->isCharging = out->flow == BATTERY_FLOW_CHARGING;
    out->isDischarging = out->flow == BATTERY_FLOW_BATTERY || out->flow == BATTERY_FLOW_ASSIST;
    if (out->isCharging) out->batteryPowerW = fabs(signedBatteryPowerW);
    else if (out->isDischarging) out->batteryPowerW = fabs(signedBatteryPowerW);
    else out->batteryPowerW = 0.0;
    if (out->systemPowerW <= 0.0) {
        if (!out->onAC) out->systemPowerW = out->batteryPowerW;
        else if (out->adapterPowerW > 0.0 && out->isCharging) out->systemPowerW = fmax(0.0, out->adapterPowerW - out->batteryPowerW);
        else out->systemPowerW = out->adapterPowerW;
    }
    if (out->onAC && out->systemPowerW > 0.0) {
        if (out->isCharging) out->adapterPowerW = out->systemPowerW + out->batteryPowerW;
        else if (out->isDischarging) out->adapterPowerW = fmax(0.0, out->systemPowerW - out->batteryPowerW);
        else out->adapterPowerW = out->systemPowerW;
    } else if (out->onAC && out->adapterPowerW <= 0.0) {
        out->adapterPowerW = 0.01;
    }
    if (!out->onAC) out->adapterPowerW = 0.0;
    setHealth(out, out->health, sizeof(out->health));
    snprintf(out->mfgDate, sizeof(out->mfgDate), "%s", service->staticCache.mfgDate[0] ? service->staticCache.mfgDate : "Unknown");
}

bool BatteryServiceInit(BatteryService **service) {
    if (!service) return false;
    BatteryService *created = calloc(1, sizeof(*created));
    if (!created) return false;
    created->initialized = true;
    created->staticIntervalSeconds = 60.0;
    created->lastStaticRefresh = -1000000;
    created->smcConn = BatterySMCOpen();
    created->stableFlow = BATTERY_FLOW_BATTERY;
    created->candidateFlow = BATTERY_FLOW_BATTERY;
    *service = created;
    return true;
}

void BatteryServiceClose(BatteryService *service) {
    if (service && service->smcConn) BatterySMCClose(service->smcConn);
    free(service);
}

bool BatteryServiceQuery(BatteryService *service, BatteryQuery query, BatteryResponse *response) {
    if (!service || !response) return false;
    memset(response, 0, sizeof(*response));
    long long now = wallSeconds();
    bool refreshStatic = (now - service->lastStaticRefresh) >= (long long)(service->staticIntervalSeconds + 0.5) ||
                         service->staticCache.designCapacity <= 0;
    BatteryLiveSnapshot live = {.timeToEmpty = -1, .timeToFull = -1};
    PowerLiveSnapshot power = {0};
    readAppleSmartBattery(service, &live, &power, refreshStatic);
    if (!live.present) readIOPSFallback(&live);
    if (refreshStatic && service->staticCache.present) service->lastStaticRefresh = now;
    deriveSnapshot(service, &live, &power, &response->snapshot);
    response->ok = response->snapshot.valid;
    if (query.debugSamples) {
        fprintf(stderr, "flow=%d ac=%d chg=%d ma=%d v=%.2f bat=%.2f adapter=%.2f sys=%.2f\n",
                response->snapshot.flow, response->snapshot.onAC, response->snapshot.isCharging,
                response->snapshot.amperageMA, response->snapshot.voltage, response->snapshot.batteryPowerW,
                response->snapshot.adapterPowerW, response->snapshot.systemPowerW);
    }
    return response->ok;
}

bool BatteryServiceBuildMock(const char *mode, BatteryResponse *response) {
    if (!mode || !response) return false;
    memset(response, 0, sizeof(*response));
    BatterySnapshot *s = &response->snapshot;
    s->valid = true;
    s->batteryPresent = true;
    s->currentCapacity = 4500;
    s->maxCapacity = 5000;
    s->designCapacity = 5500;
    s->percent = 90;
    s->cycleCount = 123;
    s->timeToEmpty = 180;
    s->timeToCharge = 60;
    s->voltage = 12.10;
    s->temperatureC = 32.4;
    s->amperageMA = 826;
    s->amperageA = 0.826;
    s->powerW = 10.0;
    s->cpuPowerW = 15.0;
    s->gpuPowerW = 5.0;
    snprintf(s->health, sizeof(s->health), "Good");
    snprintf(s->mfgDate, sizeof(s->mfgDate), "2024-09-25");
    if (strcmp(mode, "ac") == 0) {
        s->onAC = true;
        s->isCharging = true;
        s->flow = BATTERY_FLOW_CHARGING;
        s->batteryPowerW = 10.0;
        s->adapterPowerW = 40.0;
        s->systemPowerW = 30.0;
    } else if (strcmp(mode, "assist") == 0) {
        s->onAC = true;
        s->isDischarging = true;
        s->flow = BATTERY_FLOW_ASSIST;
        s->batteryPowerW = 15.0;
        s->adapterPowerW = 45.0;
        s->systemPowerW = 60.0;
    } else {
        s->onAC = false;
        s->isDischarging = true;
        s->flow = BATTERY_FLOW_BATTERY;
        s->batteryPowerW = 20.0;
        s->adapterPowerW = 0.0;
        s->systemPowerW = 20.0;
    }
    response->ok = true;
    return true;
}

void BatteryFormatDuration(int minutes, char *out, size_t size) {
    if (!out || size == 0) return;
    if (minutes < 0) {
        snprintf(out, size, "--:--");
        return;
    }
    int hours = minutes / 60;
    int mins = minutes % 60;
    if (hours > 99) snprintf(out, size, ">99h");
    else snprintf(out, size, "%dh %02dm", hours, mins);
}
