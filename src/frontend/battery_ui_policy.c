#include "battery_monitor/frontend/battery_ui_policy.h"

#include <stdio.h>

static void batteryTablePlan(const TlPolicyCtx *ctx, int *outCols, int *outRowsPerCol) {
    if (!ctx || !outCols || !outRowsPerCol) return;
    *outCols = (ctx->rect.width >= 48 && ctx->rect.height >= 6) ? 2 : 1;
    *outRowsPerCol = *outCols == 2 ? (ctx->rowCount + 1) / 2 : ctx->rowCount;
}

static bool batteryTableRow(const TlPolicyCtx *ctx, TlPolicyTableRowView *outRow) {
    if (!ctx || !ctx->binding || !outRow || ctx->rowIndex < 0 || ctx->rowIndex >= ctx->binding->rowCount) return false;
    outRow->label = ctx->binding->rows[ctx->rowIndex].label;
    outRow->value = ctx->binding->rows[ctx->rowIndex].value;
    return true;
}

static bool batteryRepeatLine(const TlPolicyCtx *ctx, char *out, int outSize, TgStyle *outStyle) {
    if (!ctx || !ctx->binding || !out || outSize <= 0 || ctx->binding->kind != TL_BIND_SEGMENTS) return false;
    if (ctx->rowIndex < 0 || ctx->rowIndex >= ctx->binding->segmentCount) return false;
    double total = 0.0;
    for (int i = 0; i < ctx->binding->segmentCount; ++i) total += ctx->binding->segments[i].value;
    const TlSegment *segment = &ctx->binding->segments[ctx->rowIndex];
    double pct = total > 0.0 ? segment->value * 100.0 / total : 0.0;
    snprintf(out, (size_t)outSize, "%s %s: %6.2fW (%2.0f%%)", segment->glyph, segment->label, segment->value, pct);
    if (outStyle) *outStyle = TG_STYLE_DIM;
    return true;
}

TlPolicySet BatteryUiPolicies(void) {
    static const TlPolicy policies[] = {
        {
            .name = "battery.table",
            .vtable =
                {
                    .table_plan = batteryTablePlan,
                    .table_row = batteryTableRow,
                    .repeat_line = NULL,
                },
            .user = NULL,
        },
        {
            .name = "battery.repeat",
            .vtable =
                {
                    .table_plan = NULL,
                    .table_row = NULL,
                    .repeat_line = batteryRepeatLine,
                },
            .user = NULL,
        },
    };
    return (TlPolicySet){.items = policies, .count = (int)(sizeof(policies) / sizeof(policies[0]))};
}
