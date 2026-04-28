#include "battery_monitor/termgfx/termgfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const TgStyle TG_STYLE_NORMAL = {false, false};
const TgStyle TG_STYLE_BOLD = {true, false};
const TgStyle TG_STYLE_DIM = {false, true};

typedef struct {
    char glyph[8];
    TgStyle style;
} TgCell;

struct TgFrame {
    int width;
    int height;
    TgCell *cells;
};

static bool inBounds(const TgFrame *frame, int x, int y) {
    return frame && x >= 0 && y >= 0 && x < frame->width && y < frame->height;
}

static void putCell(TgFrame *frame, int x, int y, const char *glyph, TgStyle style) {
    if (!inBounds(frame, x, y)) return;
    TgCell *cell = &frame->cells[y * frame->width + x];
    snprintf(cell->glyph, sizeof(cell->glyph), "%s", glyph && glyph[0] ? glyph : " ");
    cell->style = style;
}

TgFrame *TgFrameCreate(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    TgFrame *frame = calloc(1, sizeof(*frame));
    if (!frame) return NULL;
    frame->width = width;
    frame->height = height;
    frame->cells = calloc((size_t)width * (size_t)height, sizeof(TgCell));
    if (!frame->cells) {
        free(frame);
        return NULL;
    }
    TgFrameClear(frame);
    return frame;
}

void TgFrameDestroy(TgFrame *frame) {
    if (!frame) return;
    free(frame->cells);
    free(frame);
}

void TgFrameClear(TgFrame *frame) {
    if (!frame) return;
    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) putCell(frame, x, y, " ", TG_STYLE_NORMAL);
    }
}

void TgFramePresent(const TgFrame *frame) {
    if (!frame) return;
    printf("\033[H");
    TgStyle active = TG_STYLE_NORMAL;
    bool hasActive = false;
    for (int y = 0; y < frame->height; ++y) {
        /* Clear current terminal row first to avoid stale edge artifacts. */
        printf("\033[0m\033[2K\r");
        hasActive = false;
        for (int x = 0; x < frame->width; ++x) {
            const TgCell *cell = &frame->cells[y * frame->width + x];
            if (!hasActive || cell->style.bold != active.bold || cell->style.dim != active.dim) {
                printf("\033[0m");
                if (cell->style.bold) printf("\033[1m");
                if (cell->style.dim) printf("\033[2m");
                active = cell->style;
                hasActive = true;
            }
            fputs(cell->glyph, stdout);
        }
        printf("\033[0m");
        if (y + 1 < frame->height) putchar('\n');
    }
    fflush(stdout);
}

int TgFrameWidth(const TgFrame *frame) {
    return frame ? frame->width : 0;
}

int TgFrameHeight(const TgFrame *frame) {
    return frame ? frame->height : 0;
}

int TgTextWidth(const char *text) {
    if (!text) return 0;
    int width = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == 1 || *p == 2) continue;
        if ((*p & 0xC0) != 0x80) width++;
    }
    return width;
}

int TgClampInt(int value, int min, int max) {
    return value < min ? min : (value > max ? max : value);
}

int TgMinInt(int a, int b) {
    return a < b ? a : b;
}

int TgMaxInt(int a, int b) {
    return a > b ? a : b;
}

static int nextGlyph(const char **cursor, char *out, size_t size) {
    if (!cursor || !*cursor || !**cursor || size == 0) return 0;
    const unsigned char *p = (const unsigned char *)*cursor;
    int bytes = 1;
    if ((*p & 0x80) == 0) bytes = 1;
    else if ((*p & 0xE0) == 0xC0) bytes = 2;
    else if ((*p & 0xF0) == 0xE0) bytes = 3;
    else if ((*p & 0xF8) == 0xF0) bytes = 4;
    if ((size_t)bytes >= size) bytes = (int)size - 1;
    memcpy(out, *cursor, (size_t)bytes);
    out[bytes] = '\0';
    *cursor += bytes;
    return 1;
}

void TgDrawText(TgFrame *frame, int x, int y, const char *text, TgStyle style) {
    if (!frame || !text) return;
    char glyph[8];
    const char *cursor = text;
    int col = x;
    TgStyle currentStyle = style;
    while (*cursor) {
        unsigned char marker = (unsigned char)*cursor;
        if (marker == 1) {
            currentStyle = TG_STYLE_NORMAL;
            cursor++;
            continue;
        }
        if (marker == 2) {
            currentStyle = TG_STYLE_DIM;
            cursor++;
            continue;
        }
        if (!nextGlyph(&cursor, glyph, sizeof(glyph))) break;
        putCell(frame, col++, y, glyph, currentStyle);
    }
}

void TgDrawTextInRect(TgFrame *frame, TgRect rect, const char *text, TgStyle style) {
    if (!frame || !text || rect.width <= 0 || rect.height <= 0) return;
    char glyph[8];
    const char *cursor = text;
    int col = rect.x;
    int end = rect.x + rect.width;
    TgStyle currentStyle = style;
    while (col < end && *cursor) {
        unsigned char marker = (unsigned char)*cursor;
        if (marker == 1) {
            currentStyle = TG_STYLE_NORMAL;
            cursor++;
            continue;
        }
        if (marker == 2) {
            currentStyle = TG_STYLE_DIM;
            cursor++;
            continue;
        }
        if (!nextGlyph(&cursor, glyph, sizeof(glyph))) break;
        putCell(frame, col++, rect.y, glyph, currentStyle);
    }
}

void TgDrawTextAligned(TgFrame *frame, TgRect rect, const char *text, TgAlign align, TgStyle style) {
    if (!text) text = "";
    int width = TgTextWidth(text);
    int x = rect.x;
    if (align == TG_ALIGN_RIGHT) x = rect.x + rect.width - width;
    else if (align == TG_ALIGN_CENTER) x = rect.x + (rect.width - width) / 2;
    if (x < rect.x) x = rect.x;
    TgDrawTextInRect(frame, (TgRect){x, rect.y, rect.width - (x - rect.x), rect.height}, text, style);
}

void TgDrawHLine(TgFrame *frame, int x, int y, int width, const char *glyph, TgStyle style) {
    for (int i = 0; i < width; ++i) putCell(frame, x + i, y, glyph, style);
}

void TgDrawVLine(TgFrame *frame, int x, int y, int height, const char *glyph, TgStyle style) {
    for (int i = 0; i < height; ++i) putCell(frame, x, y + i, glyph, style);
}

void TgDrawBox(TgFrame *frame, TgRect rect, TgStyle style) {
    if (!frame || rect.width <= 1 || rect.height <= 1) return;
    TgDrawHLine(frame, rect.x + 1, rect.y, rect.width - 2, "─", style);
    TgDrawHLine(frame, rect.x + 1, rect.y + rect.height - 1, rect.width - 2, "─", style);
    TgDrawVLine(frame, rect.x, rect.y + 1, rect.height - 2, "│", style);
    TgDrawVLine(frame, rect.x + rect.width - 1, rect.y + 1, rect.height - 2, "│", style);
    putCell(frame, rect.x, rect.y, "┌", style);
    putCell(frame, rect.x + rect.width - 1, rect.y, "┐", style);
    putCell(frame, rect.x, rect.y + rect.height - 1, "└", style);
    putCell(frame, rect.x + rect.width - 1, rect.y + rect.height - 1, "┘", style);
}

void TgDrawConnector(TgFrame *frame, int x1, int y1, int x2, int y2, const char *joinGlyph, const char *endGlyph, TgStyle style) {
    if (!frame) return;
    if (y1 == y2) {
        int start = TgMinInt(x1, x2);
        int end = TgMaxInt(x1, x2);
        TgDrawHLine(frame, start, y1, end - start, "─", style);
        putCell(frame, x2, y2, endGlyph ? endGlyph : "▶", style);
        return;
    }
    TgDrawHLine(frame, x1, y1, TgMaxInt(0, x2 - x1), "─", style);
    int top = TgMinInt(y1, y2);
    int bottom = TgMaxInt(y1, y2);
    TgDrawVLine(frame, x2, top, bottom - top + 1, "│", style);
    putCell(frame, x2, y1, joinGlyph ? joinGlyph : "┤", style);
    putCell(frame, x2, y2, endGlyph ? endGlyph : "▶", style);
}
