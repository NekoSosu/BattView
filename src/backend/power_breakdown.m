#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct IOReportSubscriptionRef *IOReportSubscriptionRef;
typedef CFDictionaryRef IOReportSampleRef;
typedef int (^IOReportIterateBlock)(IOReportSampleRef ch);

extern CFDictionaryRef IOReportCopyChannelsInGroup(CFStringRef group, CFStringRef subgroup, uint64_t a, uint64_t b, uint64_t c);
extern void IOReportMergeChannels(CFDictionaryRef a, CFDictionaryRef b, CFTypeRef unused);
extern IOReportSubscriptionRef IOReportCreateSubscription(void *a, CFMutableDictionaryRef channels, CFMutableDictionaryRef *out, uint64_t d, CFTypeRef e);
extern CFDictionaryRef IOReportCreateSamples(IOReportSubscriptionRef sub, CFMutableDictionaryRef channels, CFTypeRef unused);
extern CFDictionaryRef IOReportCreateSamplesDelta(CFDictionaryRef a, CFDictionaryRef b, CFTypeRef unused);
extern void IOReportIterate(CFDictionaryRef samples, IOReportIterateBlock block);
extern CFStringRef IOReportChannelGetChannelName(CFDictionaryRef item);
extern CFStringRef IOReportChannelGetGroup(CFDictionaryRef item);
extern CFStringRef IOReportChannelGetUnitLabel(CFDictionaryRef item);
extern int IOReportChannelGetFormat(CFDictionaryRef item);
extern int64_t IOReportSimpleGetIntegerValue(CFDictionaryRef item, int32_t idx);

enum { kIOReportFormatSimple = 1 };

static IOReportSubscriptionRef gSubscription = NULL;
static CFMutableDictionaryRef gChannels = NULL;
static CFDictionaryRef gPreviousSample = NULL;
static double gPreviousTime = 0.0;
static bool gInitialized = false;
static bool gHasLast = false;
static double gLastCpu = 0.0;
static double gLastGpu = 0.0;
static double gLastOther = 0.0;

static double monotonicSeconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
    return 0.0;
}

static CFMutableDictionaryRef buildChannels(void) {
    CFMutableDictionaryRef channels = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!channels) return NULL;
    CFDictionaryRef energy = IOReportCopyChannelsInGroup(CFSTR("Energy Model"), NULL, 0, 0, 0);
    if (energy) {
        IOReportMergeChannels(channels, energy, NULL);
        CFRelease(energy);
    }
    return channels;
}

bool BatteryPowerBreakdownRead(double *cpuPower, double *gpuPower, double *otherPower) {
    if (!cpuPower || !gpuPower || !otherPower) return false;
    *cpuPower = 0.0;
    *gpuPower = 0.0;
    *otherPower = 0.0;

    if (!gInitialized) {
        gChannels = buildChannels();
        if (!gChannels) return false;
        gSubscription = IOReportCreateSubscription(NULL, gChannels, &gChannels, 0, NULL);
        if (!gSubscription || !gChannels) return false;
        gPreviousSample = IOReportCreateSamples(gSubscription, gChannels, NULL);
        gPreviousTime = monotonicSeconds();
        gInitialized = true;
        return false;
    }

    CFDictionaryRef current = IOReportCreateSamples(gSubscription, gChannels, NULL);
    double currentTime = monotonicSeconds();
    if (!current || !gPreviousSample) return false;
    CFDictionaryRef delta = IOReportCreateSamplesDelta(gPreviousSample, current, NULL);
    double elapsed = currentTime - gPreviousTime;
    CFRelease(gPreviousSample);
    gPreviousSample = current;
    gPreviousTime = currentTime;
    if (!delta || elapsed <= 0.0) {
        if (delta) CFRelease(delta);
        return false;
    }

    __block double cpu = 0.0;
    __block double gpu = 0.0;
    __block double system = 0.0;
    __block double other = 0.0;
    IOReportIterate(delta, ^int(IOReportSampleRef ch) {
        if (!ch || IOReportChannelGetFormat(ch) != kIOReportFormatSimple) return 0;
        CFStringRef name = IOReportChannelGetChannelName(ch);
        CFStringRef group = IOReportChannelGetGroup(ch);
        if (!name || !group) return 0;
        NSString *channelName = (__bridge NSString *)name;
        NSString *groupName = (__bridge NSString *)group;
        if (![groupName isEqualToString:@"Energy Model"]) return 0;

        int64_t raw = IOReportSimpleGetIntegerValue(ch, 0);
        double joules = (double)raw;
        CFStringRef unit = IOReportChannelGetUnitLabel(ch);
        if (unit) {
            NSString *unitName = (__bridge NSString *)unit;
            if ([unitName isEqualToString:@"mJ"]) joules /= 1000.0;
            else if ([unitName isEqualToString:@"uJ"]) joules /= 1000000.0;
            else if ([unitName isEqualToString:@"nJ"]) joules /= 1000000000.0;
        }
        double watts = joules / elapsed;
        if ([channelName isEqualToString:@"CPU Energy"]) cpu = watts;
        else if ([channelName isEqualToString:@"GPU Energy"]) gpu = watts;
        else if ([channelName isEqualToString:@"System Energy"]) system = watts;
        else if ([channelName isEqualToString:@"ANE Energy"] || [channelName isEqualToString:@"DRAM Energy"] || [channelName isEqualToString:@"GPU SRAM Energy"]) other += watts;
        return 0;
    });
    CFRelease(delta);

    if (cpu > 0.0) *cpuPower = cpu;
    if (gpu > 0.0) *gpuPower = gpu;
    if (system > 0.0) *otherPower = system;
    else if (other > 0.0) *otherPower = other;
    if (*cpuPower > 0.0 || *gpuPower > 0.0 || *otherPower > 0.0) {
        gLastCpu = *cpuPower;
        gLastGpu = *gpuPower;
        gLastOther = *otherPower;
        gHasLast = true;
        return true;
    }
    if (gHasLast) {
        *cpuPower = gLastCpu;
        *gpuPower = gLastGpu;
        *otherPower = gLastOther;
        return true;
    }
    return false;
}
