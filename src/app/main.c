#include "battery_monitor/app/view_model.h"
#include "battery_monitor/backend/battery_service.h"
#include "battery_monitor/frontend/terminal_ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double parsePositiveDouble(const char *value, double fallback) {
    char *end = NULL;
    double parsed = value ? strtod(value, &end) : 0.0;
    if (!value || end == value || parsed <= 0.0) return fallback;
    return parsed;
}

static void runMock(const char *mode) {
    BatteryResponse response;
    BatteryServiceBuildMock(mode, &response);
    BatteryViewModel model;
    BatteryViewModelBuild(&response.snapshot, &model);
    TerminalUISetup();
    TerminalUIRender(&model);
    printf("\n[MOCK MODE: %s] Press q or Ctrl+C to exit\n", mode);
    while (!TerminalUISleepOrQuit(1.0)) {}
    TerminalUIRestore();
}

int main(int argc, char **argv) {
    double intervalSeconds = 1.0;
    bool debugSamples = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            intervalSeconds = parsePositiveDouble(argv[++i], intervalSeconds);
        } else if (strcmp(argv[i], "--debug-samples") == 0) {
            debugSamples = true;
        } else if (strstr(argv[i], "assist")) {
            runMock("assist");
            return 0;
        } else if (strstr(argv[i], "bat")) {
            runMock("bat");
            return 0;
        } else if (strstr(argv[i], "ac")) {
            runMock("ac");
            return 0;
        }
    }

    BatteryService *service = NULL;
    if (!BatteryServiceInit(&service)) {
        fprintf(stderr, "failed to initialize battery service\n");
        return 1;
    }
    TerminalUISetup();
    while (true) {
        BatteryResponse response;
        BatteryQuery query = {.intervalSeconds = intervalSeconds, .debugSamples = debugSamples};
        BatteryServiceQuery(service, query, &response);
        BatteryViewModel model;
        BatteryViewModelBuild(&response.snapshot, &model);
        TerminalUIRender(&model);
        if (TerminalUISleepOrQuit(intervalSeconds)) break;
    }
    TerminalUIRestore();
    BatteryServiceClose(service);
    return 0;
}
