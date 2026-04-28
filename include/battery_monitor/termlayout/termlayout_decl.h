#ifndef BATTERY_MONITOR_TERMLAYOUT_DECL_H
#define BATTERY_MONITOR_TERMLAYOUT_DECL_H

#include "battery_monitor/termgfx/termgfx.h"

#include <stdbool.h>

typedef enum {
    TL_NODE_STACK = 0,
    TL_NODE_ROW,
    TL_NODE_TEXT,
    TL_NODE_BOX,
    TL_NODE_LINE,
    TL_NODE_SEGMENTED_BAR,
    TL_NODE_TABLE,
    TL_NODE_CONNECTOR,
    TL_NODE_SPACER,
    TL_NODE_CONDITIONAL,
    TL_NODE_REPEAT,
    TL_NODE_FALLBACK,
    TL_NODE_CANVAS
} TlNodeKind;

typedef enum {
    TL_DIR_VERTICAL = 0,
    TL_DIR_HORIZONTAL
} TlDirection;

typedef enum {
    TL_LINE_HORIZONTAL = 0,
    TL_LINE_VERTICAL
} TlLineKind;

typedef enum {
    TL_ANCHOR_LEFT = 0,
    TL_ANCHOR_RIGHT,
    TL_ANCHOR_TOP,
    TL_ANCHOR_BOTTOM,
    TL_ANCHOR_CENTER
} TlAnchor;

typedef enum {
    TL_VALIGN_TOP = 0,
    TL_VALIGN_CENTER,
    TL_VALIGN_BOTTOM
} TlVAlign;

typedef struct TlNodeDecl TlNodeDecl;

struct TlNodeDecl {
    TlNodeKind kind;
    const char *id;
    const char *bind;
    const char *literal;
    const char *policy;
    TgStyle style;
    TgAlign align;
    TlVAlign valign;
    TlDirection direction;
    TlLineKind line;
    int width;
    int height;
    int minWidth;
    int minHeight;
    int flex;
    int gap;
    int marginTop;
    int marginRight;
    int marginBottom;
    int marginLeft;
    int x;
    int y;
    int visibleMinWidth;
    int visibleMinHeight;
    bool dockBottom;
    bool border;
    bool showLegend;
    const char *whenKey;
    const char *whenValue;
    const char *fromId;
    const char *toId;
    TlAnchor fromAnchor;
    TlAnchor toAnchor;
    const char *jointGlyph;
    const char *endGlyph;
    const char *anchorToId;
    TlAnchor anchorTo;
    const TlNodeDecl *children;
    int childCount;
};

#endif
