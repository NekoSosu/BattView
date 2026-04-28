#include "battery_monitor/app/view_model.h"

#include <stdio.h>
#include <string.h>

static const char *powerModeText(const BatterySnapshot *s) {
    if (!s->onAC) return "Battery";
    if (s->flow == BATTERY_FLOW_ASSIST) return "AC + Battery";
    if (s->flow == BATTERY_FLOW_CHARGING) return "AC / Charging";
    return "AC / Passthrough";
}

static const char *powerLabel(const BatterySnapshot *s) {
    if (s->onAC && s->isCharging) return "Power In";
    if (s->isDischarging) return "Power Out";
    return "Power";
}

void BatteryViewModelBuild(const BatterySnapshot *s, BatteryViewModel *m) {
    memset(m, 0, sizeof(*m));
    if (!s || !s->valid) return;

    m->valid = true;
    m->percent = s->percent;
    m->currentCapacity = s->currentCapacity;
    m->maxCapacity = s->maxCapacity;
    m->designCapacity = s->designCapacity;
    m->truePercent = s->designCapacity > 0 ? (int)((double)s->currentCapacity / s->designCapacity * 100.0 + 0.5) : s->percent;
    m->healthRatio = s->designCapacity > 0 ? (s->maxCapacity * 100 / s->designCapacity) : 0;
    snprintf(m->healthText, sizeof(m->healthText), "%s", s->health);
    snprintf(m->healthFull, sizeof(m->healthFull), "%s (%d%%)", m->healthText, m->healthRatio);

    m->timeLabel = "Estimate";
    snprintf(m->timeText, sizeof(m->timeText), "--");
    if (s->isDischarging && s->timeToEmpty >= 0) {
        BatteryFormatDuration(s->timeToEmpty, m->timeText, sizeof(m->timeText));
        m->timeLabel = "To Empty";
    } else if (s->onAC && s->isCharging && s->timeToCharge >= 0) {
        BatteryFormatDuration(s->timeToCharge, m->timeText, sizeof(m->timeText));
        m->timeLabel = "To Full";
    }

    m->powerLabel = powerLabel(s);
    snprintf(m->powerModeText, sizeof(m->powerModeText), "%s", powerModeText(s));
    snprintf(m->powerText, sizeof(m->powerText), "%.2f W", s->powerW);
    snprintf(m->voltageText, sizeof(m->voltageText), "%.2f V", s->voltage);
    snprintf(m->amperageText, sizeof(m->amperageText), "%d mA", s->amperageMA);
    snprintf(m->temperatureText, sizeof(m->temperatureText), "%.1f °C", s->temperatureC);
    snprintf(m->chargeText, sizeof(m->chargeText), "%d mAh", s->currentCapacity);
    snprintf(m->maxCapacityText, sizeof(m->maxCapacityText), "%d mAh", s->maxCapacity);
    snprintf(m->designCapacityText, sizeof(m->designCapacityText), "%d mAh", s->designCapacity);
    snprintf(m->cyclesText, sizeof(m->cyclesText), "%d", s->cycleCount);
    snprintf(m->mfgDateText, sizeof(m->mfgDateText), "%s", s->mfgDate);

    m->rows[0] = (StatusRow){"Power Mode", m->powerModeText};
    m->rows[1] = (StatusRow){m->timeLabel, m->timeText};
    m->rows[2] = (StatusRow){m->powerLabel, m->powerText};
    m->rows[3] = (StatusRow){"Voltage", m->voltageText};
    m->rows[4] = (StatusRow){"Amperage", m->amperageText};
    m->rows[5] = (StatusRow){"Temp", m->temperatureText};
    m->rows[6] = (StatusRow){"Health", m->healthFull};
    m->rows[7] = (StatusRow){"Cycles", m->cyclesText};
    m->rows[8] = (StatusRow){"Charge", m->chargeText};
    m->rows[9] = (StatusRow){"Max Cap", m->maxCapacityText};
    m->rows[10] = (StatusRow){"Design Cap", m->designCapacityText};
    m->rows[11] = (StatusRow){"Mfg Date", m->mfgDateText};

    m->powerFlow.flow = s->flow;
    m->powerFlow.acInW = s->adapterPowerW;
    m->powerFlow.systemOutW = s->systemPowerW;
    m->powerFlow.cpuW = s->cpuPowerW;
    m->powerFlow.gpuW = s->gpuPowerW;
    m->powerFlow.totalW = s->systemPowerW > 0.0 ? s->systemPowerW : s->cpuPowerW + s->gpuPowerW;
    if (m->powerFlow.totalW < s->cpuPowerW + s->gpuPowerW) m->powerFlow.totalW = s->cpuPowerW + s->gpuPowerW;
    m->powerFlow.otherW = m->powerFlow.totalW - s->cpuPowerW - s->gpuPowerW;
    if (m->powerFlow.otherW < 0.0) m->powerFlow.otherW = 0.0;
    if (s->flow == BATTERY_FLOW_CHARGING) m->powerFlow.batteryInW = s->batteryPowerW;
    else if (s->flow == BATTERY_FLOW_BATTERY || s->flow == BATTERY_FLOW_ASSIST) m->powerFlow.batteryOutW = s->batteryPowerW;
}
