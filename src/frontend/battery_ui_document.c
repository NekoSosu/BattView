#include "battery_monitor/frontend/battery_ui_document.h"

#define ST_NORMAL ((TgStyle){false, false})
#define ST_BOLD ((TgStyle){true, false})
#define ST_DIM ((TgStyle){false, true})

#define TEXT_BIND_NODE(key, styleValue, alignValue) \
    {.kind = TL_NODE_TEXT, .bind = key, .style = styleValue, .align = alignValue, .height = 1, .minHeight = 1}

#define TEXT_LIT_NODE(text, styleValue, alignValue) \
    {.kind = TL_NODE_TEXT, .literal = text, .style = styleValue, .align = alignValue, .height = 1, .minHeight = 1}

#define HLINE_NODE() \
    {.kind = TL_NODE_LINE, .literal = "─", .style = ST_DIM, .height = 1, .minHeight = 1}

#define ROW_NODE(kids) \
    {.kind = TL_NODE_ROW, .direction = TL_DIR_HORIZONTAL, .height = 1, .minHeight = 1, .children = kids, .childCount = (int)(sizeof(kids) / sizeof((kids)[0]))}

#define STACK_NODE(kids) \
    {.kind = TL_NODE_STACK, .direction = TL_DIR_VERTICAL, .children = kids, .childCount = (int)(sizeof(kids) / sizeof((kids)[0]))}

#define BOX_NODE(idValue, xValue, yValue, wValue, hValue, kids) \
    {.kind = TL_NODE_BOX, .id = idValue, .direction = TL_DIR_HORIZONTAL, .x = xValue, .y = yValue, .width = wValue, .height = hValue, .minWidth = wValue, .minHeight = hValue, .border = true, .children = kids, .childCount = (int)(sizeof(kids) / sizeof((kids)[0]))}

#define BOX_AUTO_NODE(idValue, xValue, yValue, minWValue, hValue, kids) \
    {.kind = TL_NODE_BOX, .id = idValue, .direction = TL_DIR_HORIZONTAL, .x = xValue, .y = yValue, .width = 0, .height = hValue, .minWidth = minWValue, .minHeight = hValue, .border = true, .children = kids, .childCount = (int)(sizeof(kids) / sizeof((kids)[0]))}

#define CPU_LEGEND_NODE(yValue) \
    {.kind = TL_NODE_TEXT, .bind = "power.cpuLegend", .style = ST_DIM, .align = TG_ALIGN_CENTER, .anchorToId = "sys", .anchorTo = TL_ANCHOR_BOTTOM, .x = 0, .y = yValue, .width = 28, .height = 1, .minHeight = 1}

#define GPU_LEGEND_NODE(yValue) \
    {.kind = TL_NODE_TEXT, .bind = "power.gpuLegend", .style = ST_DIM, .align = TG_ALIGN_CENTER, .anchorToId = "sys", .anchorTo = TL_ANCHOR_BOTTOM, .x = 0, .y = yValue, .width = 28, .height = 1, .minHeight = 1}

#define OTHER_LEGEND_NODE(yValue) \
    {.kind = TL_NODE_TEXT, .bind = "power.otherLegend", .style = ST_DIM, .align = TG_ALIGN_CENTER, .anchorToId = "sys", .anchorTo = TL_ANCHOR_BOTTOM, .x = 0, .y = yValue, .width = 28, .height = 1, .minHeight = 1}

static const TlNodeDecl headingCells[] = {
    {.kind = TL_NODE_TEXT, .bind = "header.title", .style = ST_BOLD, .align = TG_ALIGN_LEFT, .height = 1, .minHeight = 1, .flex = 1},
    {.kind = TL_NODE_TEXT, .bind = "header.time", .style = ST_DIM, .align = TG_ALIGN_RIGHT, .width = 8, .height = 1, .minWidth = 8, .minHeight = 1},
};

static const TlNodeDecl heading[] = {
    ROW_NODE(headingCells),
    HLINE_NODE(),
};

static const TlNodeDecl batteryCells[] = {
    {.kind = TL_NODE_TEXT, .literal = "Battery ", .style = ST_BOLD, .width = 8, .height = 1, .minWidth = 8, .minHeight = 1},
    {.kind = TL_NODE_SEGMENTED_BAR, .bind = "battery.segments", .height = 1, .minWidth = 1, .minHeight = 1, .flex = 1},
    {.kind = TL_NODE_TEXT, .bind = "battery.percent", .style = ST_BOLD, .width = 4, .height = 1, .minWidth = 4, .minHeight = 1},
    {.kind = TL_NODE_TEXT, .bind = "battery.descriptor", .style = ST_DIM, .width = 9, .height = 1, .minHeight = 1},
};

static const TlNodeDecl batteryLegendCells[] = {
    {.kind = TL_NODE_SPACER, .width = 9, .height = 1},
    {.kind = TL_NODE_TEXT, .literal = "\002\001█\002 Charge  ▒ Usable  ░ Degraded", .style = ST_DIM, .height = 1, .minHeight = 1, .flex = 1},
};

static const TlNodeDecl battery[] = {
    ROW_NODE(batteryCells),
    ROW_NODE(batteryLegendCells),
};

static const TlNodeDecl acBoxCells[] = {
    {.kind = TL_NODE_TEXT, .literal = "AC", .style = ST_BOLD, .width = 3, .height = 1, .minWidth = 2, .minHeight = 1},
    {.kind = TL_NODE_TEXT, .bind = "power.ac", .align = TG_ALIGN_RIGHT, .width = 7, .height = 1, .minWidth = 4, .minHeight = 1, .flex = 1},
};

static const TlNodeDecl batOutBoxCells[] = {
    {.kind = TL_NODE_TEXT, .literal = "BAT", .style = ST_BOLD, .width = 3, .height = 1, .minWidth = 3, .minHeight = 1},
    {.kind = TL_NODE_TEXT, .bind = "power.batteryOut", .align = TG_ALIGN_RIGHT, .width = 7, .height = 1, .minWidth = 4, .minHeight = 1, .flex = 1},
};

static const TlNodeDecl batInBoxCells[] = {
    {.kind = TL_NODE_TEXT, .literal = "BAT", .style = ST_BOLD, .width = 3, .height = 1, .minWidth = 3, .minHeight = 1},
    {.kind = TL_NODE_TEXT, .bind = "power.batteryIn", .align = TG_ALIGN_RIGHT, .width = 7, .height = 1, .minWidth = 4, .minHeight = 1, .flex = 1},
};

static const TlNodeDecl sysBoxCells[] = {
    {.kind = TL_NODE_TEXT, .literal = "SYS", .style = ST_BOLD, .width = 3, .height = 1, .minWidth = 3, .minHeight = 1},
    {.kind = TL_NODE_SEGMENTED_BAR, .bind = "power.breakdown", .height = 1, .minWidth = 1, .minHeight = 1, .flex = 1, .marginLeft = 2, .marginRight = 2},
    {.kind = TL_NODE_TEXT, .bind = "power.system", .align = TG_ALIGN_RIGHT, .width = 7, .height = 1, .minWidth = 4, .minHeight = 1},
};

static const TlNodeDecl singleGraph[] = {
    BOX_AUTO_NODE("src", 0, 0, 12, 3, batOutBoxCells),
    BOX_AUTO_NODE("sys", 16, 0, 26, 3, sysBoxCells),
    {.kind = TL_NODE_CONNECTOR, .fromId = "src", .toId = "sys", .fromAnchor = TL_ANCHOR_RIGHT, .toAnchor = TL_ANCHOR_LEFT, .endGlyph = "▶"},
    CPU_LEGEND_NODE(1),
    GPU_LEGEND_NODE(2),
    OTHER_LEGEND_NODE(3),
};

static const TlNodeDecl passGraph[] = {
    BOX_AUTO_NODE("src", 0, 0, 12, 3, acBoxCells),
    BOX_AUTO_NODE("sys", 16, 0, 26, 3, sysBoxCells),
    {.kind = TL_NODE_CONNECTOR, .fromId = "src", .toId = "sys", .fromAnchor = TL_ANCHOR_RIGHT, .toAnchor = TL_ANCHOR_LEFT, .endGlyph = "▶"},
    CPU_LEGEND_NODE(1),
    GPU_LEGEND_NODE(2),
    OTHER_LEGEND_NODE(3),
};

static const TlNodeDecl assistGraph[] = {
    BOX_AUTO_NODE("ac", 0, 0, 12, 3, acBoxCells),
    BOX_AUTO_NODE("bat", 0, 4, 12, 3, batOutBoxCells),
    BOX_AUTO_NODE("sys", 17, 2, 26, 3, sysBoxCells),
    {.kind = TL_NODE_LINE, .literal = "─", .x = 12, .y = 1, .width = 2, .height = 1},
    {.kind = TL_NODE_LINE, .literal = "─", .x = 12, .y = 5, .width = 2, .height = 1},
    {.kind = TL_NODE_LINE, .literal = "│", .line = TL_LINE_VERTICAL, .x = 14, .y = 1, .width = 1, .height = 5},
    {.kind = TL_NODE_TEXT, .literal = "┐", .x = 14, .y = 1, .width = 1, .height = 1},
    {.kind = TL_NODE_TEXT, .literal = "├─▶", .x = 14, .y = 3, .width = 3, .height = 1},
    {.kind = TL_NODE_TEXT, .literal = "┘", .x = 14, .y = 5, .width = 1, .height = 1},
    CPU_LEGEND_NODE(1),
    GPU_LEGEND_NODE(2),
    OTHER_LEGEND_NODE(3),
};

static const TlNodeDecl chargeGraph[] = {
    BOX_AUTO_NODE("ac", 0, 2, 12, 3, acBoxCells),
    BOX_AUTO_NODE("bat", 17, 0, 12, 3, batInBoxCells),
    BOX_AUTO_NODE("sys", 17, 4, 26, 3, sysBoxCells),
    {.kind = TL_NODE_LINE, .literal = "─", .x = 12, .y = 3, .width = 2, .height = 1},
    {.kind = TL_NODE_LINE, .literal = "│", .line = TL_LINE_VERTICAL, .x = 14, .y = 1, .width = 1, .height = 5},
    {.kind = TL_NODE_TEXT, .literal = "┤", .x = 14, .y = 3, .width = 1, .height = 1},
    {.kind = TL_NODE_TEXT, .literal = "┌─▶", .x = 14, .y = 1, .width = 3, .height = 1},
    {.kind = TL_NODE_TEXT, .literal = "└─▶", .x = 14, .y = 5, .width = 3, .height = 1},
    CPU_LEGEND_NODE(1),
    GPU_LEGEND_NODE(2),
    OTHER_LEGEND_NODE(3),
};

static const TlNodeDecl graphVariants[] = {
    {.kind = TL_NODE_CANVAS, .whenKey = "power.mode", .whenValue = "battery", .align = TG_ALIGN_CENTER, .valign = TL_VALIGN_CENTER, .minWidth = 41, .minHeight = 8, .height = 11, .children = singleGraph, .childCount = (int)(sizeof(singleGraph) / sizeof(singleGraph[0]))},
    {.kind = TL_NODE_CANVAS, .whenKey = "power.mode", .whenValue = "passthrough", .align = TG_ALIGN_CENTER, .valign = TL_VALIGN_CENTER, .minWidth = 41, .minHeight = 8, .height = 11, .children = passGraph, .childCount = (int)(sizeof(passGraph) / sizeof(passGraph[0]))},
    {.kind = TL_NODE_CANVAS, .whenKey = "power.mode", .whenValue = "charging", .align = TG_ALIGN_CENTER, .valign = TL_VALIGN_CENTER, .minWidth = 42, .minHeight = 8, .height = 11, .children = chargeGraph, .childCount = (int)(sizeof(chargeGraph) / sizeof(chargeGraph[0]))},
    {.kind = TL_NODE_CANVAS, .whenKey = "power.mode", .whenValue = "assist", .align = TG_ALIGN_CENTER, .valign = TL_VALIGN_CENTER, .minWidth = 42, .minHeight = 8, .height = 11, .children = assistGraph, .childCount = (int)(sizeof(assistGraph) / sizeof(assistGraph[0]))},
};

static const TlNodeDecl textFallback[] = {
    TEXT_BIND_NODE("power.text", ST_NORMAL, TG_ALIGN_CENTER),
    TEXT_BIND_NODE("power.detail", ST_DIM, TG_ALIGN_CENTER),
};

static const TlNodeDecl graphFallback[] = {
    {.kind = TL_NODE_CONDITIONAL, .minWidth = 41, .minHeight = 8, .height = 11, .children = graphVariants, .childCount = (int)(sizeof(graphVariants) / sizeof(graphVariants[0]))},
    {.kind = TL_NODE_STACK, .direction = TL_DIR_VERTICAL, .minHeight = 2, .height = 2, .children = textFallback, .childCount = 2},
};

static const TlNodeDecl power[] = {
    HLINE_NODE(),
    {.kind = TL_NODE_FALLBACK, .valign = TL_VALIGN_CENTER, .minHeight = 2, .flex = 1, .children = graphFallback, .childCount = 2},
};

static const TlNodeDecl rootChildren[] = {
    {.kind = TL_NODE_STACK, .direction = TL_DIR_VERTICAL, .height = 2, .minHeight = 2, .children = heading, .childCount = 2},
    {.kind = TL_NODE_STACK, .direction = TL_DIR_VERTICAL, .height = 2, .minHeight = 2, .children = battery, .childCount = 2},
    HLINE_NODE(),
    {.kind = TL_NODE_TABLE, .bind = "status.rows", .policy = "battery.table", .minHeight = 1},
    {.kind = TL_NODE_STACK, .direction = TL_DIR_VERTICAL, .minHeight = 2, .flex = 1, .children = power, .childCount = 2},
    {.kind = TL_NODE_TEXT, .bind = "footer", .style = ST_DIM, .height = 1, .minHeight = 1, .dockBottom = true},
};

static const TlNodeDecl rootNode = {
    .kind = TL_NODE_STACK,
    .id = "root",
    .direction = TL_DIR_VERTICAL,
    .children = rootChildren,
    .childCount = (int)(sizeof(rootChildren) / sizeof(rootChildren[0])),
};

bool BatteryUiDocumentBuild(TlCompiledDocument *out, char *errBuf, int errBufLen) {
    return TlCompileDocument(&rootNode, out, errBuf, errBufLen);
}
