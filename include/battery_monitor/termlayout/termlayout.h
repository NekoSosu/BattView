#ifndef BATTERY_MONITOR_TERMLAYOUT_H
#define BATTERY_MONITOR_TERMLAYOUT_H

#include "battery_monitor/termlayout/termlayout_decl.h"

#include <stdbool.h>

#define TL_MAX_CHILDREN 12
#define TL_MAX_VALUES 96
#define TL_MAX_ARRAY_ITEMS 24
#define TL_MAX_SEGMENTS 8

typedef enum {
    TL_BIND_TEXT = 0,
    TL_BIND_NUMBER,
    TL_BIND_BOOL,
    TL_BIND_ENUM,
    TL_BIND_TABLE,
    TL_BIND_SEGMENTS
} TlBindingKind;

typedef struct {
    char label[64];
    char value[64];
} TlTableRow;

typedef struct {
    char label[64];
    char glyph[8];
    double value;
    TgStyle style;
} TlSegment;

typedef struct {
    char key[64];
    TlBindingKind kind;
    char text[128];
    double number;
    bool boolean;
    TlTableRow rows[TL_MAX_ARRAY_ITEMS];
    int rowCount;
    TlSegment segments[TL_MAX_SEGMENTS];
    int segmentCount;
} TlBinding;

typedef struct {
    TlBinding values[TL_MAX_VALUES];
    int count;
} TlBindings;

typedef struct TlNode TlNode;

typedef struct {
    const TlNode *root;
} TlDocument;

typedef struct {
    const TlNode *root;
    TlNode *storage;
    int nodeCount;
} TlCompiledDocument;

typedef struct {
    const TlBinding *binding;
    TgRect rect;
    int rowIndex;
    int rowCount;
    void *user;
} TlPolicyCtx;

typedef struct {
    const char *label;
    const char *value;
} TlPolicyTableRowView;

typedef struct {
    void (*table_plan)(const TlPolicyCtx *ctx, int *outCols, int *outRowsPerCol);
    bool (*table_row)(const TlPolicyCtx *ctx, TlPolicyTableRowView *outRow);
    bool (*repeat_line)(const TlPolicyCtx *ctx, char *out, int outSize, TgStyle *outStyle);
} TlPolicyVTable;

typedef struct {
    const char *name;
    TlPolicyVTable vtable;
    void *user;
} TlPolicy;

typedef struct {
    const TlPolicy *items;
    int count;
} TlPolicySet;

void TlBindingsInit(TlBindings *bindings);
void TlBindingsSetText(TlBindings *bindings, const char *key, const char *text);
void TlBindingsSetNumber(TlBindings *bindings, const char *key, double number);
void TlBindingsSetBool(TlBindings *bindings, const char *key, bool value);
void TlBindingsSetEnum(TlBindings *bindings, const char *key, const char *value);
void TlBindingsSetTable(TlBindings *bindings, const char *key, const TlTableRow *rows, int rowCount);
void TlBindingsSetSegments(TlBindings *bindings, const char *key, const TlSegment *segments, int segmentCount);
const TlBinding *TlBindingsGet(const TlBindings *bindings, const char *key);

bool TlCompileDocument(const TlNodeDecl *root, TlCompiledDocument *out, char *errBuf, int errBufLen);
void TlFreeCompiledDocument(TlCompiledDocument *document);
void TlRenderCompiled(TgFrame *frame, const TlCompiledDocument *document, const TlBindings *bindings);
void TlRenderCompiledWithPolicies(TgFrame *frame, const TlCompiledDocument *document, const TlBindings *bindings, const TlPolicySet *policies);
void TlRenderDocument(TgFrame *frame, const TlDocument *document, const TlBindings *bindings);
void TlRenderDocumentWithPolicies(TgFrame *frame, const TlDocument *document, const TlBindings *bindings, const TlPolicySet *policies);

#endif
