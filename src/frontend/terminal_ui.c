#include "battery_monitor/frontend/terminal_ui.h"
#include "battery_monitor/frontend/battery_ui_bindings.h"
#include "battery_monitor/frontend/battery_ui_document.h"
#include "battery_monitor/frontend/battery_ui_policy.h"
#include "battery_monitor/termgfx/termgfx.h"

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_UI_WIDTH 62
#define MIN_UI_WIDTH 32

static volatile sig_atomic_t terminalSizeDirty = 1;
static int cachedTerminalWidth = DEFAULT_UI_WIDTH;
static int cachedTerminalHeight = 24;
static time_t lastTerminalSizeCheck = 0;
static struct termios originalTermios;
static bool hasOriginalTermios = false;
static bool terminalActive = false;
static TlCompiledDocument compiledDocument = {0};
static bool hasCompiledDocument = false;
static TlPolicySet renderPolicies = {0};

static void handleSignal(int signo) {
    (void)signo;
    TerminalUIRestore();
    _exit(0);
}

static void handleResize(int signo) {
    (void)signo;
    terminalSizeDirty = 1;
}

void TerminalUISetup(void) {
    if (tcgetattr(STDIN_FILENO, &originalTermios) == 0) {
        struct termios raw = originalTermios;
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
        raw.c_lflag |= ISIG;
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) hasOriginalTermios = true;
    }
    printf("\033[?1049h\033[?25l\033[?1000h\033[?1006h\033[H\033[2J");
    fflush(stdout);
    terminalActive = true;
    atexit(TerminalUIRestore);
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    signal(SIGWINCH, handleResize);
}

void TerminalUIRestore(void) {
    if (!terminalActive) return;
    if (hasCompiledDocument) {
        TlFreeCompiledDocument(&compiledDocument);
        hasCompiledDocument = false;
    }
    if (hasOriginalTermios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
        hasOriginalTermios = false;
    }
    printf("\033[?1006l\033[?1000l\033[?25h\033[?1049l");
    fflush(stdout);
    terminalActive = false;
}

bool TerminalUIShouldQuit(void) {
    fd_set readfds;
    bool quit = false;
    while (true) {
        struct timeval timeout = {0, 0};
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout) <= 0) break;
        unsigned char buffer[64];
        ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (bytes <= 0) break;
        for (ssize_t i = 0; i < bytes; ++i) {
            if (tolower(buffer[i]) == 'q' || buffer[i] == 3) quit = true;
        }
    }
    return quit;
}

bool TerminalUISleepOrQuit(double seconds) {
    if (seconds <= 0.0) return TerminalUIShouldQuit();
    int slices = (int)(seconds * 10.0);
    if (slices < 1) slices = 1;
    useconds_t sliceUsec = (useconds_t)(seconds * 1000000.0 / slices);
    for (int i = 0; i < slices; ++i) {
        if (TerminalUIShouldQuit()) return true;
        usleep(sliceUsec);
    }
    return TerminalUIShouldQuit();
}

static void updateTerminalSizeIfNeeded(void) {
    time_t now = time(NULL);
    if (!terminalSizeDirty && now == lastTerminalSizeCheck) return;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        cachedTerminalWidth = ws.ws_col;
        if (ws.ws_row > 0) cachedTerminalHeight = ws.ws_row;
    }
    terminalSizeDirty = 0;
    lastTerminalSizeCheck = now;
}

static void currentTimestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *local = localtime(&now);
    snprintf(buffer, size, "%s", "unknown");
    if (local) strftime(buffer, size, "%H:%M:%S", local);
}

static bool sameRenderInput(const BatteryViewModel *model, const BatteryViewModel *lastModel, bool hadLastModel) {
    if (!model) return !hadLastModel;
    return hadLastModel && memcmp(model, lastModel, sizeof(*model)) == 0;
}

void TerminalUIRender(const BatteryViewModel *model) {
    static BatteryViewModel lastModel;
    static char lastTimestamp[32] = "";
    static bool hasLast = false;
    static bool hadLastModel = false;
    char timestamp[32];
    currentTimestamp(timestamp, sizeof(timestamp));
    if (hasLast && !terminalSizeDirty && strcmp(timestamp, lastTimestamp) == 0 && sameRenderInput(model, &lastModel, hadLastModel)) return;

    updateTerminalSizeIfNeeded();
    int width = cachedTerminalWidth > 0 ? cachedTerminalWidth : DEFAULT_UI_WIDTH;
    if (width < MIN_UI_WIDTH) width = MIN_UI_WIDTH;
    int height = cachedTerminalHeight > 0 ? cachedTerminalHeight : 24;
    TgFrame *frame = TgFrameCreate(width, height);
    if (!frame) return;

    if (!hasCompiledDocument) {
        char error[256];
        if (!BatteryUiDocumentBuild(&compiledDocument, error, (int)sizeof(error))) {
            TgFrameDestroy(frame);
            fprintf(stderr, "failed to compile battery UI document: %s\n", error);
            return;
        }
        renderPolicies = BatteryUiPolicies();
        hasCompiledDocument = true;
    }
    TlBindings bindings;
    BatteryUiBindingsFromViewModel(model, timestamp, &bindings);
    TlRenderCompiledWithPolicies(frame, &compiledDocument, &bindings, &renderPolicies);
    TgFramePresent(frame);
    TgFrameDestroy(frame);

    if (model) lastModel = *model;
    hadLastModel = model != NULL;
    snprintf(lastTimestamp, sizeof(lastTimestamp), "%s", timestamp);
    hasLast = true;
}
