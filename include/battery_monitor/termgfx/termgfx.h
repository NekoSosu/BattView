#ifndef BATTERY_MONITOR_TERMGFX_H
#define BATTERY_MONITOR_TERMGFX_H

#include <stdbool.h>

typedef struct TgFrame TgFrame;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} TgRect;

typedef struct {
    bool bold;
    bool dim;
} TgStyle;

typedef enum {
    TG_ALIGN_LEFT = 0,
    TG_ALIGN_CENTER,
    TG_ALIGN_RIGHT
} TgAlign;

extern const TgStyle TG_STYLE_NORMAL;
extern const TgStyle TG_STYLE_BOLD;
extern const TgStyle TG_STYLE_DIM;

TgFrame *TgFrameCreate(int width, int height);
void TgFrameDestroy(TgFrame *frame);
void TgFrameClear(TgFrame *frame);
void TgFramePresent(const TgFrame *frame);
int TgFrameWidth(const TgFrame *frame);
int TgFrameHeight(const TgFrame *frame);

int TgTextWidth(const char *text);
int TgClampInt(int value, int min, int max);
int TgMinInt(int a, int b);
int TgMaxInt(int a, int b);

void TgDrawText(TgFrame *frame, int x, int y, const char *text, TgStyle style);
void TgDrawTextInRect(TgFrame *frame, TgRect rect, const char *text, TgStyle style);
void TgDrawTextAligned(TgFrame *frame, TgRect rect, const char *text, TgAlign align, TgStyle style);
void TgDrawHLine(TgFrame *frame, int x, int y, int width, const char *glyph, TgStyle style);
void TgDrawVLine(TgFrame *frame, int x, int y, int height, const char *glyph, TgStyle style);
void TgDrawBox(TgFrame *frame, TgRect rect, TgStyle style);
void TgDrawConnector(TgFrame *frame, int x1, int y1, int x2, int y2, const char *joinGlyph, const char *endGlyph, TgStyle style);

#endif
