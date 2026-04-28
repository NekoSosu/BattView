#include "battery_monitor/frontend/battery_ui_bindings.h"

#include <stdio.h>
#include <string.h>

static void formatWatts(char *buffer, size_t size, double watts) {
    snprintf(buffer, size, "%.2fW", watts);
}

static const char *modeKey(BatteryFlowState flow) {
    if (flow == BATTERY_FLOW_CHARGING) return "charging";
    if (flow == BATTERY_FLOW_PASSTHROUGH) return "passthrough";
    if (flow == BATTERY_FLOW_ASSIST) return "assist";
    return "battery";
}

static void setText(TlBindings *bindings, const char *key, const char *value) {
    TlBindingsSetText(bindings, key, value ? value : "");
}

void BatteryUiBindingsFromViewModel(const BatteryViewModel *model, const char *timestamp, TlBindings *bindings) {
    if (!bindings) return;
    TlBindingsInit(bindings);
    setText(bindings, "header.title", "BATTVIEW");
    setText(bindings, "header.time", timestamp);
    setText(bindings, "footer", "q: quit");

    if (!model || !model->valid) {
        setText(bindings, "status.empty", "Battery data unavailable.");
        return;
    }

    char percent[16];
    char descriptor[24];
    snprintf(percent, sizeof(percent), " %d%%", model->percent);
    snprintf(descriptor, sizeof(descriptor), "[%d%% des]", model->truePercent);
    setText(bindings, "battery.percent", percent);
    setText(bindings, "battery.descriptor", descriptor);

    double design = model->designCapacity > 0 ? model->designCapacity : (model->maxCapacity > 0 ? model->maxCapacity : 1);
    double charge = model->currentCapacity;
    double usable = model->maxCapacity - model->currentCapacity;
    double degraded = design - model->maxCapacity;
    if (usable < 0.0) usable = 0.0;
    if (degraded < 0.0) degraded = 0.0;
    TlSegment batterySegments[] = {
        {"Charge", "█", charge, TG_STYLE_NORMAL},
        {"Usable", "▒", usable, TG_STYLE_DIM},
        {"Degraded", "░", degraded, TG_STYLE_DIM},
    };
    TlBindingsSetSegments(bindings, "battery.segments", batterySegments, 3);

    TlTableRow rows[STATUS_ROW_COUNT];
    for (int i = 0; i < STATUS_ROW_COUNT; ++i) {
        snprintf(rows[i].label, sizeof(rows[i].label), "%s", model->rows[i].label ? model->rows[i].label : "");
        snprintf(rows[i].value, sizeof(rows[i].value), "%s", model->rows[i].value ? model->rows[i].value : "");
    }
    TlBindingsSetTable(bindings, "status.rows", rows, STATUS_ROW_COUNT);

    char ac[16], batteryIn[16], batteryOut[16], system[16];
    formatWatts(ac, sizeof(ac), model->powerFlow.acInW);
    formatWatts(batteryIn, sizeof(batteryIn), model->powerFlow.batteryInW);
    formatWatts(batteryOut, sizeof(batteryOut), model->powerFlow.batteryOutW);
    formatWatts(system, sizeof(system), model->powerFlow.systemOutW);
    setText(bindings, "power.ac", ac);
    setText(bindings, "power.batteryIn", batteryIn);
    setText(bindings, "power.batteryOut", batteryOut);
    setText(bindings, "power.system", system);
    TlBindingsSetEnum(bindings, "power.mode", modeKey(model->powerFlow.flow));

    TlSegment breakdown[] = {
        {"CPU", "█", model->powerFlow.cpuW, TG_STYLE_NORMAL},
        {"GPU", "▒", model->powerFlow.gpuW, TG_STYLE_NORMAL},
        {"Other", "░", model->powerFlow.otherW, TG_STYLE_NORMAL},
    };
    TlBindingsSetSegments(bindings, "power.breakdown", breakdown, 3);
    double total = model->powerFlow.totalW > 0.0 ? model->powerFlow.totalW : 0.0;
    char legend[64];
    snprintf(legend, sizeof(legend), "\001█\002 CPU:  %6.2fW (%2.0f%%)", model->powerFlow.cpuW, total > 0.0 ? model->powerFlow.cpuW * 100.0 / total : 0.0);
    setText(bindings, "power.cpuLegend", legend);
    snprintf(legend, sizeof(legend), "\001▒\002 GPU:  %6.2fW (%2.0f%%)", model->powerFlow.gpuW, total > 0.0 ? model->powerFlow.gpuW * 100.0 / total : 0.0);
    setText(bindings, "power.gpuLegend", legend);
    snprintf(legend, sizeof(legend), "\001░\002 Other:%6.2fW (%2.0f%%)", model->powerFlow.otherW, total > 0.0 ? model->powerFlow.otherW * 100.0 / total : 0.0);
    setText(bindings, "power.otherLegend", legend);

    char powerText[128];
    if (model->powerFlow.flow == BATTERY_FLOW_ASSIST) {
        snprintf(powerText, sizeof(powerText), "AC %.2fW + BAT %.2fW -> SYS %.2fW", model->powerFlow.acInW, model->powerFlow.batteryOutW, model->powerFlow.systemOutW);
    } else if (model->powerFlow.flow == BATTERY_FLOW_CHARGING) {
        snprintf(powerText, sizeof(powerText), "AC %.2fW -> BAT %.2fW + SYS %.2fW", model->powerFlow.acInW, model->powerFlow.batteryInW, model->powerFlow.systemOutW);
    } else if (model->powerFlow.flow == BATTERY_FLOW_PASSTHROUGH) {
        snprintf(powerText, sizeof(powerText), "AC %.2fW -> SYS %.2fW", model->powerFlow.acInW, model->powerFlow.systemOutW);
    } else {
        snprintf(powerText, sizeof(powerText), "BAT %.2fW -> SYS %.2fW", model->powerFlow.batteryOutW, model->powerFlow.systemOutW);
    }
    setText(bindings, "power.text", powerText);

    char detail[128];
    snprintf(detail, sizeof(detail), "CPU %.2fW GPU %.2fW Oth %.2fW", model->powerFlow.cpuW, model->powerFlow.gpuW, model->powerFlow.otherW);
    setText(bindings, "power.detail", detail);
}
