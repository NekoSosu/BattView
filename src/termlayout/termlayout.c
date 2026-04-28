#include "battery_monitor/termlayout/termlayout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *id;
    TgRect rect;
} AnchorEntry;

typedef struct {
    AnchorEntry entries[64];
    int count;
} AnchorMap;

struct TlNode {
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
    const TlNode *children;
    int childCount;
};

static const TgStyle STYLE_DEFAULT = {false, false};
static const TlPolicySet *activePolicies = NULL;

void TlBindingsInit(TlBindings *bindings) {
    if (!bindings) return;
    memset(bindings, 0, sizeof(*bindings));
}

static TlBinding *bindingSlot(TlBindings *bindings, const char *key, TlBindingKind kind) {
    if (!bindings || !key) return NULL;
    for (int i = 0; i < bindings->count; ++i) {
        if (strcmp(bindings->values[i].key, key) == 0) {
            bindings->values[i].kind = kind;
            return &bindings->values[i];
        }
    }
    if (bindings->count >= TL_MAX_VALUES) return NULL;
    TlBinding *binding = &bindings->values[bindings->count++];
    memset(binding, 0, sizeof(*binding));
    snprintf(binding->key, sizeof(binding->key), "%s", key);
    binding->kind = kind;
    return binding;
}

void TlBindingsSetText(TlBindings *bindings, const char *key, const char *text) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_TEXT);
    if (binding) snprintf(binding->text, sizeof(binding->text), "%s", text ? text : "");
}

void TlBindingsSetNumber(TlBindings *bindings, const char *key, double number) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_NUMBER);
    if (binding) binding->number = number;
}

void TlBindingsSetBool(TlBindings *bindings, const char *key, bool value) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_BOOL);
    if (binding) binding->boolean = value;
}

void TlBindingsSetEnum(TlBindings *bindings, const char *key, const char *value) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_ENUM);
    if (binding) snprintf(binding->text, sizeof(binding->text), "%s", value ? value : "");
}

void TlBindingsSetTable(TlBindings *bindings, const char *key, const TlTableRow *rows, int rowCount) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_TABLE);
    if (!binding) return;
    binding->rowCount = TgClampInt(rowCount, 0, TL_MAX_ARRAY_ITEMS);
    for (int i = 0; i < binding->rowCount; ++i) binding->rows[i] = rows[i];
}

void TlBindingsSetSegments(TlBindings *bindings, const char *key, const TlSegment *segments, int segmentCount) {
    TlBinding *binding = bindingSlot(bindings, key, TL_BIND_SEGMENTS);
    if (!binding) return;
    binding->segmentCount = TgClampInt(segmentCount, 0, TL_MAX_SEGMENTS);
    for (int i = 0; i < binding->segmentCount; ++i) binding->segments[i] = segments[i];
}

const TlBinding *TlBindingsGet(const TlBindings *bindings, const char *key) {
    if (!bindings || !key) return NULL;
    for (int i = 0; i < bindings->count; ++i) {
        if (strcmp(bindings->values[i].key, key) == 0) return &bindings->values[i];
    }
    return NULL;
}

static TgStyle nodeStyle(const TlNode *node) {
    return node ? node->style : STYLE_DEFAULT;
}

static int nodeOuterFixed(TlDirection direction, const TlNode *node) {
    int size = direction == TL_DIR_VERTICAL ? node->height : node->width;
    return size > 0 ? size : 0;
}

static int nodeOuterMin(TlDirection direction, const TlNode *node) {
    int size = direction == TL_DIR_VERTICAL ? node->minHeight : node->minWidth;
    return size > 0 ? size : 1;
}

static bool visibleForRect(const TlNode *node, TgRect rect) {
    if (!node) return false;
    if (node->visibleMinWidth > 0 && rect.width < node->visibleMinWidth) return false;
    if (node->visibleMinHeight > 0 && rect.height < node->visibleMinHeight) return false;
    return true;
}

static TgRect applyMargin(TgRect rect, const TlNode *node) {
    rect.x += node->marginLeft;
    rect.y += node->marginTop;
    rect.width -= node->marginLeft + node->marginRight;
    rect.height -= node->marginTop + node->marginBottom;
    if (rect.width < 0) rect.width = 0;
    if (rect.height < 0) rect.height = 0;
    return rect;
}

static const char *boundText(const TlBindings *bindings, const char *key, const char *literal) {
    const TlBinding *binding = TlBindingsGet(bindings, key);
    if (binding && (binding->kind == TL_BIND_TEXT || binding->kind == TL_BIND_ENUM)) return binding->text;
    return literal ? literal : "";
}

static void anchorsPut(AnchorMap *anchors, const char *id, TgRect rect) {
    if (!anchors || !id || !id[0] || anchors->count >= (int)(sizeof(anchors->entries) / sizeof(anchors->entries[0]))) return;
    anchors->entries[anchors->count++] = (AnchorEntry){id, rect};
}

static bool anchorsGet(const AnchorMap *anchors, const char *id, TgRect *rect) {
    if (!anchors || !id) return false;
    for (int i = anchors->count - 1; i >= 0; --i) {
        if (strcmp(anchors->entries[i].id, id) == 0) {
            if (rect) *rect = anchors->entries[i].rect;
            return true;
        }
    }
    return false;
}

static int anchorX(TgRect rect, TlAnchor anchor) {
    if (anchor == TL_ANCHOR_RIGHT) return rect.x + rect.width;
    if (anchor == TL_ANCHOR_CENTER || anchor == TL_ANCHOR_TOP || anchor == TL_ANCHOR_BOTTOM) return rect.x + rect.width / 2;
    return rect.x;
}

static int anchorY(TgRect rect, TlAnchor anchor) {
    if (anchor == TL_ANCHOR_BOTTOM) return rect.y + rect.height - 1;
    if (anchor == TL_ANCHOR_CENTER || anchor == TL_ANCHOR_LEFT || anchor == TL_ANCHOR_RIGHT) return rect.y + rect.height / 2;
    return rect.y;
}

static int preferredNodeHeight(const TlBindings *bindings, const TlNode *node, int width);
static int preferredNodeWidth(const TlBindings *bindings, const TlNode *node);

static TgRect nodeRectInLocalCanvas(const TlNode *node, int width, int height, const AnchorMap *anchors) {
    int x = node->x;
    int y = node->y;
    if (node->anchorToId && node->anchorToId[0]) {
        TgRect anchorRect;
        if (anchorsGet(anchors, node->anchorToId, &anchorRect)) {
            int ax = anchorX(anchorRect, node->anchorTo);
            int ay = anchorY(anchorRect, node->anchorTo);
            if (node->align == TG_ALIGN_CENTER) x = ax - width / 2 + node->x;
            else if (node->align == TG_ALIGN_RIGHT) x = ax - width + node->x;
            else x = ax + node->x;
            y = ay + node->y;
        }
    }
    return (TgRect){x, y, width, height};
}

static TgRect canvasContentBounds(const TlNode *node, const TlBindings *bindings) {
    TgRect bounds = {0, 0, 0, 0};
    if (!node || node->childCount <= 0) return bounds;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    bool hasBounds = false;
    AnchorMap localAnchors = {0};
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        if (child->kind == TL_NODE_CONNECTOR) continue;
        int width = child->width > 0 ? child->width : preferredNodeWidth(bindings, child);
        int height = child->height > 0 ? child->height : preferredNodeHeight(bindings, child, width);
        if (width <= 0) width = 1;
        if (height <= 0) height = 1;
        TgRect childRect = nodeRectInLocalCanvas(child, width, height, &localAnchors);
        anchorsPut(&localAnchors, child->id, childRect);
        if (!hasBounds) {
            minX = childRect.x;
            minY = childRect.y;
            maxX = childRect.x + childRect.width;
            maxY = childRect.y + childRect.height;
            hasBounds = true;
        } else {
            minX = TgMinInt(minX, childRect.x);
            minY = TgMinInt(minY, childRect.y);
            maxX = TgMaxInt(maxX, childRect.x + childRect.width);
            maxY = TgMaxInt(maxY, childRect.y + childRect.height);
        }
    }
    return hasBounds ? (TgRect){minX, minY, maxX - minX, maxY - minY} : bounds;
}

static TgRect childRectInCanvas(TgRect parent, const TlNode *canvas, const TlNode *node, const TlBindings *bindings, const AnchorMap *anchors) {
    int width = node->width > 0 ? node->width : preferredNodeWidth(bindings, node);
    int height = node->height > 0 ? node->height : preferredNodeHeight(bindings, node, width);
    if (width <= 0) width = parent.width;
    if (height <= 0) height = parent.height;
    TgRect content = canvasContentBounds(canvas, bindings);
    int offsetX = 0;
    int offsetY = 0;
    if (canvas && canvas->align == TG_ALIGN_CENTER && content.width > 0 && parent.width > content.width) {
        offsetX = (parent.width - content.width) / 2 - content.x;
    } else if (canvas && canvas->align == TG_ALIGN_RIGHT && content.width > 0 && parent.width > content.width) {
        offsetX = parent.width - content.width - content.x;
    }
    if (canvas && canvas->valign == TL_VALIGN_CENTER && content.height > 0 && parent.height > content.height) {
        offsetY = (parent.height - content.height) / 2 - content.y;
    } else if (canvas && canvas->valign == TL_VALIGN_BOTTOM && content.height > 0 && parent.height > content.height) {
        offsetY = parent.height - content.height - content.y;
    }
    int x = parent.x + offsetX + node->x;
    int y = parent.y + offsetY + node->y;
    if (node->anchorToId && node->anchorToId[0]) {
        TgRect anchorRect;
        if (anchorsGet(anchors, node->anchorToId, &anchorRect)) {
            int ax = anchorX(anchorRect, node->anchorTo);
            int ay = anchorY(anchorRect, node->anchorTo);
            if (node->align == TG_ALIGN_CENTER) x = ax - width / 2 + node->x;
            else if (node->align == TG_ALIGN_RIGHT) x = ax - width + node->x;
            else x = ax + node->x;
            y = ay + node->y;
        }
    }
    return (TgRect){x, y, width, height};
}

static int preferredTableHeight(const TlBindings *bindings, const TlNode *node, int width) {
    const TlBinding *table = TlBindingsGet(bindings, node->bind);
    int rowCount = table && table->kind == TL_BIND_TABLE ? table->rowCount : 0;
    bool twoCols = width >= 48;
    return twoCols ? (rowCount + 1) / 2 : rowCount;
}

static int preferredNodeHeight(const TlBindings *bindings, const TlNode *node, int width) {
    if (node->height > 0) return node->height;
    if (node->kind == TL_NODE_TABLE) return TgMaxInt(node->minHeight, preferredTableHeight(bindings, node, width));
    if (node->kind == TL_NODE_STACK && node->direction == TL_DIR_VERTICAL) {
        int total = 0;
        for (int i = 0; i < node->childCount; ++i) {
            const TlNode *child = &node->children[i];
            if (child->dockBottom) continue;
            total += child->marginTop + preferredNodeHeight(bindings, child, width) + child->marginBottom;
            if (i > 0) total += node->gap;
        }
        return TgMaxInt(node->minHeight, total);
    }
    return TgMaxInt(node->minHeight, 1);
}

static int preferredNodeWidth(const TlBindings *bindings, const TlNode *node) {
    if (!node) return 1;
    if (node->width > 0) return node->width;
    if (node->kind == TL_NODE_TEXT) {
        const char *text = boundText(bindings, node->bind, node->literal);
        return TgMaxInt(node->minWidth, TgTextWidth(text));
    }
    if (node->kind == TL_NODE_SEGMENTED_BAR) return TgMaxInt(node->minWidth, 1);
    if (node->kind == TL_NODE_LINE) return TgMaxInt(node->minWidth, 1);
    if (node->kind == TL_NODE_TABLE) return TgMaxInt(node->minWidth, 12);
    if (node->kind == TL_NODE_ROW) {
        int total = 0;
        int visibleCount = 0;
        for (int i = 0; i < node->childCount; ++i) {
            const TlNode *child = &node->children[i];
            int childWidth = preferredNodeWidth(bindings, child);
            total += child->marginLeft + childWidth + child->marginRight;
            visibleCount++;
        }
        if (visibleCount > 1) total += (visibleCount - 1) * node->gap;
        return TgMaxInt(node->minWidth, total);
    }
    if (node->kind == TL_NODE_STACK) {
        int maxWidth = 0;
        for (int i = 0; i < node->childCount; ++i) {
            const TlNode *child = &node->children[i];
            int childWidth = child->marginLeft + preferredNodeWidth(bindings, child) + child->marginRight;
            maxWidth = TgMaxInt(maxWidth, childWidth);
        }
        return TgMaxInt(node->minWidth, maxWidth);
    }
    if (node->kind == TL_NODE_BOX) {
        int innerWidth = 0;
        if (node->childCount > 0) {
            TlNode inner = *node;
            inner.kind = TL_NODE_ROW;
            inner.width = 0;
            inner.minWidth = 0;
            inner.border = false;
            innerWidth = preferredNodeWidth(bindings, &inner);
        }
        return TgMaxInt(node->minWidth, innerWidth + 2);
    }
    if (node->kind == TL_NODE_CANVAS) {
        int maxRight = 0;
        int minLeft = 0;
        bool has = false;
        for (int i = 0; i < node->childCount; ++i) {
            const TlNode *child = &node->children[i];
            if (child->kind == TL_NODE_CONNECTOR) continue;
            int childWidth = preferredNodeWidth(bindings, child);
            int left = child->x;
            int right = child->x + childWidth;
            if (!has) {
                minLeft = left;
                maxRight = right;
                has = true;
            } else {
                minLeft = TgMinInt(minLeft, left);
                maxRight = TgMaxInt(maxRight, right);
            }
        }
        return has ? TgMaxInt(node->minWidth, maxRight - minLeft) : TgMaxInt(node->minWidth, 1);
    }
    return TgMaxInt(node->minWidth, 1);
}

static void renderNode(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors);

static void renderChildrenFlow(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors) {
    if (node->childCount <= 0) return;
    TlDirection direction = node->direction;
    int available = direction == TL_DIR_VERTICAL ? rect.height : rect.width;
    int dockHeight = 0;
    if (direction == TL_DIR_VERTICAL) {
        for (int i = 0; i < node->childCount; ++i) {
            const TlNode *child = &node->children[i];
            if (child->dockBottom && visibleForRect(child, rect)) dockHeight += child->height > 0 ? child->height : nodeOuterMin(TL_DIR_VERTICAL, child);
        }
        available = TgMaxInt(0, available - dockHeight);
    }
    int fixed = 0;
    int flex = 0;
    int visibleCount = 0;
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        TgRect test = rect;
        if (child->dockBottom || !visibleForRect(child, test)) continue;
        visibleCount++;
        if (child->flex > 0) flex += child->flex;
        else {
            int fixedSize = nodeOuterFixed(direction, child);
            if (fixedSize <= 0) fixedSize = direction == TL_DIR_VERTICAL ? preferredNodeHeight(bindings, child, rect.width) : preferredNodeWidth(bindings, child);
            fixed += fixedSize;
        }
    }
    int gaps = visibleCount > 1 ? (visibleCount - 1) * node->gap : 0;
    int flexSpace = TgMaxInt(0, available - fixed - gaps);
    int cursor = direction == TL_DIR_VERTICAL ? rect.y : rect.x;
    int rendered = 0;
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        if (child->dockBottom || !visibleForRect(child, rect)) continue;
        if (rendered++ > 0) cursor += node->gap;
        int main = nodeOuterFixed(direction, child);
        if (child->flex > 0) main = flex > 0 ? flexSpace * child->flex / flex : nodeOuterMin(direction, child);
        if (main <= 0) main = direction == TL_DIR_VERTICAL ? preferredNodeHeight(bindings, child, rect.width) : preferredNodeWidth(bindings, child);
        if (main <= 0) main = 1;
        TgRect childRect = direction == TL_DIR_VERTICAL
                               ? (TgRect){rect.x, cursor, rect.width, main}
                               : (TgRect){cursor, rect.y, main, rect.height};
        renderNode(frame, child, childRect, bindings, anchors);
        cursor += main;
    }
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        if (!child->dockBottom || !visibleForRect(child, rect)) continue;
        int height = child->height > 0 ? child->height : nodeOuterMin(TL_DIR_VERTICAL, child);
        TgRect childRect = {rect.x, rect.y + rect.height - height, rect.width, height};
        renderNode(frame, child, childRect, bindings, anchors);
    }
}

static void renderText(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings) {
    TgDrawTextAligned(frame, rect, boundText(bindings, node->bind, node->literal), node->align, nodeStyle(node));
}

static void renderLine(TgFrame *frame, const TlNode *node, TgRect rect) {
    if (node->line == TL_LINE_VERTICAL) TgDrawVLine(frame, rect.x, rect.y, rect.height, node->literal ? node->literal : "│", nodeStyle(node));
    else TgDrawHLine(frame, rect.x, rect.y, rect.width, node->literal ? node->literal : "─", nodeStyle(node));
}

static void renderBar(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings) {
    const TlBinding *binding = TlBindingsGet(bindings, node->bind);
    if (!binding || binding->kind != TL_BIND_SEGMENTS || rect.width <= 0) return;
    double total = 0.0;
    for (int i = 0; i < binding->segmentCount; ++i) total += binding->segments[i].value;
    int used = 0;
    for (int i = 0; i < binding->segmentCount; ++i) {
        int width = i + 1 == binding->segmentCount ? rect.width - used : (total > 0.0 ? TgClampInt((int)(binding->segments[i].value * rect.width / total + 0.5), 0, rect.width - used) : 0);
        TgDrawHLine(frame, rect.x + used, rect.y, width, binding->segments[i].glyph, binding->segments[i].style);
        used += width;
    }
}

static const TlPolicy *findPolicy(const char *name) {
    if (!name || !name[0] || !activePolicies || !activePolicies->items || activePolicies->count <= 0) return NULL;
    for (int i = 0; i < activePolicies->count; ++i) {
        const TlPolicy *policy = &activePolicies->items[i];
        if (policy->name && strcmp(policy->name, name) == 0) return policy;
    }
    return NULL;
}

static void renderTable(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings) {
    const TlBinding *binding = TlBindingsGet(bindings, node->bind);
    if (!binding || binding->kind != TL_BIND_TABLE) return;
    int cols = rect.width >= 48 && rect.height >= 6 ? 2 : 1;
    int perCol = cols == 2 ? (binding->rowCount + 1) / 2 : binding->rowCount;
    const TlPolicy *policy = findPolicy(node->policy);
    if (policy && policy->vtable.table_plan) {
        TlPolicyCtx ctx = {.binding = binding, .rect = rect, .rowCount = binding->rowCount, .user = policy->user};
        policy->vtable.table_plan(&ctx, &cols, &perCol);
        if (cols < 1) cols = 1;
        if (perCol < 1) perCol = binding->rowCount > 0 ? binding->rowCount : 1;
    }
    int twoCols = cols >= 2;
    int colW = twoCols ? (rect.width - 2) / 2 : rect.width;
    for (int i = 0; i < binding->rowCount; ++i) {
        int col = twoCols ? i / perCol : 0;
        int rowIndex = twoCols ? i % perCol : i;
        if (rowIndex >= rect.height) continue;
        int baseX = rect.x + (twoCols ? col * (colW + 2) : 0);
        TgRect row = {baseX, rect.y + rowIndex, colW, 1};
        const char *label = binding->rows[i].label;
        const char *value = binding->rows[i].value;
        if (policy && policy->vtable.table_row) {
            TlPolicyTableRowView rowView = {0};
            TlPolicyCtx ctx = {.binding = binding, .rect = row, .rowIndex = i, .rowCount = binding->rowCount, .user = policy->user};
            if (policy->vtable.table_row(&ctx, &rowView)) {
                label = rowView.label ? rowView.label : "";
                value = rowView.value ? rowView.value : "";
            }
        }
        TgDrawTextInRect(frame, (TgRect){row.x, row.y, 12, 1}, label, TG_STYLE_DIM);
        TgDrawTextInRect(frame, (TgRect){row.x + 13, row.y, TgMaxInt(0, row.width - 13), 1}, value, TG_STYLE_NORMAL);
    }
}

static bool conditionMatches(const TlNode *node, const TlBindings *bindings) {
    if (!node->whenKey || !node->whenKey[0]) return true;
    const TlBinding *binding = TlBindingsGet(bindings, node->whenKey);
    if (!binding) return false;
    if (binding->kind == TL_BIND_BOOL) return binding->boolean;
    if (binding->kind == TL_BIND_ENUM || binding->kind == TL_BIND_TEXT) return node->whenValue && strcmp(binding->text, node->whenValue) == 0;
    return false;
}

static void renderConditional(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors) {
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        if (conditionMatches(child, bindings) && rect.width >= child->minWidth && rect.height >= child->minHeight) {
            renderNode(frame, child, rect, bindings, anchors);
            return;
        }
    }
}

static bool conditionalCanRender(const TlNode *node, TgRect rect, const TlBindings *bindings) {
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        if (conditionMatches(child, bindings) && rect.width >= child->minWidth && rect.height >= child->minHeight) return true;
    }
    return false;
}

static void renderFallback(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors) {
    for (int i = 0; i < node->childCount; ++i) {
        const TlNode *child = &node->children[i];
        bool canRender = child->kind == TL_NODE_CONDITIONAL ? conditionalCanRender(child, rect, bindings) : true;
        if (canRender && visibleForRect(child, rect) && rect.width >= child->minWidth && rect.height >= child->minHeight) {
            TgRect childRect = rect;
            int childHeight = child->height > 0 ? child->height : preferredNodeHeight(bindings, child, rect.width);
            if (childHeight > 0 && rect.height > childHeight) {
                if (node->valign == TL_VALIGN_CENTER) childRect.y += (rect.height - childHeight) / 2;
                else if (node->valign == TL_VALIGN_BOTTOM) childRect.y += rect.height - childHeight;
                childRect.height = childHeight;
            }
            renderNode(frame, child, childRect, bindings, anchors);
            return;
        }
    }
}

static void renderRepeat(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors) {
    const TlBinding *binding = TlBindingsGet(bindings, node->bind);
    if (!binding || binding->kind != TL_BIND_SEGMENTS) return;
    const TlPolicy *policy = findPolicy(node->policy);
    int rows = TgMinInt(binding->segmentCount, rect.height);
    for (int i = 0; i < rows; ++i) {
        char line[128];
        TgStyle style = TG_STYLE_DIM;
        bool custom = false;
        if (policy && policy->vtable.repeat_line) {
            TlPolicyCtx ctx = {.binding = binding, .rect = (TgRect){rect.x, rect.y + i, rect.width, 1}, .rowIndex = i, .rowCount = binding->segmentCount, .user = policy->user};
            custom = policy->vtable.repeat_line(&ctx, line, (int)sizeof(line), &style);
        }
        if (!custom) {
            double total = 0.0;
            for (int s = 0; s < binding->segmentCount; ++s) total += binding->segments[s].value;
            double pct = total > 0.0 ? binding->segments[i].value * 100.0 / total : 0.0;
            snprintf(line, sizeof(line), "%s %s: %6.2fW (%2.0f%%)", binding->segments[i].glyph, binding->segments[i].label, binding->segments[i].value, pct);
        }
        TgDrawTextInRect(frame, (TgRect){rect.x, rect.y + i, rect.width, 1}, line, style);
    }
    (void)anchors;
}

static void renderConnector(TgFrame *frame, const TlNode *node, const AnchorMap *anchors) {
    TgRect from, to;
    if (!anchorsGet(anchors, node->fromId, &from) || !anchorsGet(anchors, node->toId, &to)) return;
    TgDrawConnector(frame, anchorX(from, node->fromAnchor), anchorY(from, node->fromAnchor),
                    anchorX(to, node->toAnchor) - 1, anchorY(to, node->toAnchor),
                    node->jointGlyph, node->endGlyph ? node->endGlyph : "▶", nodeStyle(node));
}

static void renderNode(TgFrame *frame, const TlNode *node, TgRect rect, const TlBindings *bindings, AnchorMap *anchors) {
    if (!node || !visibleForRect(node, rect)) return;
    rect = applyMargin(rect, node);
    if (node->width > 0 && node->kind != TL_NODE_STACK && node->kind != TL_NODE_ROW) rect.width = TgMinInt(rect.width, node->width);
    if (node->height > 0 && node->kind != TL_NODE_STACK && node->kind != TL_NODE_ROW) rect.height = TgMinInt(rect.height, node->height);
    if (rect.width <= 0 || rect.height <= 0) return;
    anchorsPut(anchors, node->id, rect);

    if (node->kind == TL_NODE_STACK || node->kind == TL_NODE_ROW) {
        renderChildrenFlow(frame, node, rect, bindings, anchors);
    } else if (node->kind == TL_NODE_BOX) {
        TgDrawBox(frame, rect, nodeStyle(node));
        TgRect inner = {rect.x + 1, rect.y + 1, TgMaxInt(0, rect.width - 2), TgMaxInt(0, rect.height - 2)};
        renderChildrenFlow(frame, node, inner, bindings, anchors);
    } else if (node->kind == TL_NODE_TEXT) {
        renderText(frame, node, rect, bindings);
    } else if (node->kind == TL_NODE_LINE) {
        renderLine(frame, node, rect);
    } else if (node->kind == TL_NODE_SEGMENTED_BAR) {
        renderBar(frame, node, rect, bindings);
    } else if (node->kind == TL_NODE_TABLE) {
        renderTable(frame, node, rect, bindings);
    } else if (node->kind == TL_NODE_CONNECTOR) {
        renderConnector(frame, node, anchors);
    } else if (node->kind == TL_NODE_CONDITIONAL) {
        renderConditional(frame, node, rect, bindings, anchors);
    } else if (node->kind == TL_NODE_REPEAT) {
        renderRepeat(frame, node, rect, bindings, anchors);
    } else if (node->kind == TL_NODE_FALLBACK) {
        renderFallback(frame, node, rect, bindings, anchors);
    } else if (node->kind == TL_NODE_CANVAS) {
        for (int i = 0; i < node->childCount; ++i) renderNode(frame, &node->children[i], childRectInCanvas(rect, node, &node->children[i], bindings, anchors), bindings, anchors);
    }
}

static void writeError(char *errBuf, int errBufLen, const char *message) {
    if (!errBuf || errBufLen <= 0) return;
    snprintf(errBuf, (size_t)errBufLen, "%s", message ? message : "unknown error");
}

static int countDeclNodes(const TlNodeDecl *node) {
    if (!node) return 0;
    int total = 1;
    for (int i = 0; i < node->childCount; ++i) {
        total += countDeclNodes(&node->children[i]);
    }
    return total;
}

static bool compileDeclIntoNode(const TlNodeDecl *decl, TlNode *node, TlNode *storage, int nodeCount, int *nextIndex) {
    if (!decl || !node || !storage || !nextIndex) return false;
    memset(node, 0, sizeof(*node));
    node->kind = decl->kind;
    node->id = decl->id;
    node->bind = decl->bind;
    node->literal = decl->literal;
    node->policy = decl->policy;
    node->style = decl->style;
    node->align = decl->align;
    node->valign = decl->valign;
    node->direction = decl->direction;
    node->line = decl->line;
    node->width = decl->width;
    node->height = decl->height;
    node->minWidth = decl->minWidth;
    node->minHeight = decl->minHeight;
    node->flex = decl->flex;
    node->gap = decl->gap;
    node->marginTop = decl->marginTop;
    node->marginRight = decl->marginRight;
    node->marginBottom = decl->marginBottom;
    node->marginLeft = decl->marginLeft;
    node->x = decl->x;
    node->y = decl->y;
    node->visibleMinWidth = decl->visibleMinWidth;
    node->visibleMinHeight = decl->visibleMinHeight;
    node->dockBottom = decl->dockBottom;
    node->border = decl->border;
    node->showLegend = decl->showLegend;
    node->whenKey = decl->whenKey;
    node->whenValue = decl->whenValue;
    node->fromId = decl->fromId;
    node->toId = decl->toId;
    node->fromAnchor = decl->fromAnchor;
    node->toAnchor = decl->toAnchor;
    node->jointGlyph = decl->jointGlyph;
    node->endGlyph = decl->endGlyph;
    node->anchorToId = decl->anchorToId;
    node->anchorTo = decl->anchorTo;
    node->childCount = decl->childCount;
    if (decl->childCount > 0) {
        if (*nextIndex + decl->childCount > nodeCount) return false;
        int childStart = *nextIndex;
        *nextIndex += decl->childCount;
        node->children = &storage[childStart];
        for (int i = 0; i < decl->childCount; ++i) {
            if (!compileDeclIntoNode(&decl->children[i], &storage[childStart + i], storage, nodeCount, nextIndex)) return false;
        }
    }
    return true;
}

bool TlCompileDocument(const TlNodeDecl *root, TlCompiledDocument *out, char *errBuf, int errBufLen) {
    if (!out) {
        writeError(errBuf, errBufLen, "missing output document");
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!root) {
        writeError(errBuf, errBufLen, "missing root declaration");
        return false;
    }
    int nodeCount = countDeclNodes(root);
    if (nodeCount <= 0) {
        writeError(errBuf, errBufLen, "empty document");
        return false;
    }
    TlNode *storage = calloc((size_t)nodeCount, sizeof(*storage));
    if (!storage) {
        writeError(errBuf, errBufLen, "failed to allocate compiled document");
        return false;
    }
    int nextIndex = 1;
    if (!compileDeclIntoNode(root, &storage[0], storage, nodeCount, &nextIndex) || nextIndex != nodeCount) {
        free(storage);
        writeError(errBuf, errBufLen, "failed to compile declaration tree");
        return false;
    }
    out->root = &storage[0];
    out->storage = storage;
    out->nodeCount = nodeCount;
    return true;
}

void TlFreeCompiledDocument(TlCompiledDocument *document) {
    if (!document) return;
    free(document->storage);
    memset(document, 0, sizeof(*document));
}

void TlRenderCompiledWithPolicies(TgFrame *frame, const TlCompiledDocument *document, const TlBindings *bindings, const TlPolicySet *policies) {
    if (!frame || !document || !document->root) return;
    const TlPolicySet *previousPolicies = activePolicies;
    activePolicies = policies;
    AnchorMap anchors = {0};
    TgRect viewport = {0, 0, TgFrameWidth(frame), TgFrameHeight(frame)};
    renderNode(frame, document->root, viewport, bindings, &anchors);
    activePolicies = previousPolicies;
}

void TlRenderCompiled(TgFrame *frame, const TlCompiledDocument *document, const TlBindings *bindings) {
    TlRenderCompiledWithPolicies(frame, document, bindings, NULL);
}

void TlRenderDocument(TgFrame *frame, const TlDocument *document, const TlBindings *bindings) {
    if (!frame || !document || !document->root) return;
    const TlPolicySet *previousPolicies = activePolicies;
    activePolicies = NULL;
    AnchorMap anchors = {0};
    TgRect viewport = {0, 0, TgFrameWidth(frame), TgFrameHeight(frame)};
    renderNode(frame, document->root, viewport, bindings, &anchors);
    activePolicies = previousPolicies;
}

void TlRenderDocumentWithPolicies(TgFrame *frame, const TlDocument *document, const TlBindings *bindings, const TlPolicySet *policies) {
    if (!frame || !document || !document->root) return;
    const TlPolicySet *previousPolicies = activePolicies;
    activePolicies = policies;
    AnchorMap anchors = {0};
    TgRect viewport = {0, 0, TgFrameWidth(frame), TgFrameHeight(frame)};
    renderNode(frame, document->root, viewport, bindings, &anchors);
    activePolicies = previousPolicies;
}
