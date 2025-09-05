#pragma once
#include "quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

typedef void (*qjs_debug_session_cb_t)(int);

/// debug session events passed to the debug session callback
#define QJS_DEBUG_EVENT_CONTINUE   0
#define QJS_DEBUG_EVENT_QUIT       1

/**
 * Initialize the QuickJS debugger.
 * 
 * @param vm QuickJS context
 * @param port TCP port for debugger connection (0 = disabled)
 * @param breakpointFnName name of the global JS function to register (e.g. "breakpoint")
 * @param cb callback invoked when a debug session has ended (can be NULL)
 */
void qjs_debug_init(void* vm, int port, const char *breakpointFnName, qjs_debug_session_cb_t cb);

/** Poll for connections and handle disconnections. Call once per frame. */
void qjs_debug_poll(void);

/** Shutdown debugger, close sockets. */
void qjs_debug_shutdown(void);

// -----------------------------------------------------------------------------
// Implementation (header-only, define QJS_DEBUG_IMPLEMENTATION before including)
// -----------------------------------------------------------------------------
#ifdef QJS_DEBUG_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define CLOSESOCKET(s) closesocket(s)
#define SOCKET_ERRNO WSAGetLastError()
#define SOCK_INIT() do { \
    WSADATA wsaData; \
    WSAStartup(MAKEWORD(2,2), &wsaData); \
} while(0)
#define SOCK_CLEANUP() WSACleanup()
#define SOCK_NONBLOCK(s) do { \
    u_long mode = 1; \
    ioctlsocket(s, FIONBIO, &mode); \
} while(0)

// Portable socket printf for both POSIX and Windows
static int dprintf(int sock, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len < 0) return len;
    if (len > (int)sizeof(buf)) len = (int)sizeof(buf);
    return send(sock, buf, len, 0);
}

#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#define CLOSESOCKET(s) close(s)
#define SOCKET_ERRNO errno
#define SOCK_INIT() ((void)0)
#define SOCK_CLEANUP() ((void)0)
#undef SOCK_NONBLOCK
#define SOCK_NONBLOCK(s) fcntl(s, F_SETFL, O_NONBLOCK)
#endif

static int qjs_debug_server = -1;
static int qjs_debug_client = -1;
static qjs_debug_session_cb_t qjs_debug_break_cb = NULL;

// Forward decl
static JSValue qjs_debug_breakpoint(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv);

void qjs_debug_init(void* vm, int port, const char *breakpointFnName, qjs_debug_session_cb_t cb) {
    JSContext* ctx = (JSContext*)vm;
    if (port > 0) {
        qjs_debug_break_cb = cb;
        SOCK_INIT();
        qjs_debug_server = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        if (qjs_debug_server == INVALID_SOCKET) {
            fprintf(stderr, "socket error: %d\n", SOCKET_ERRNO);
            return;
        }
#else
        if (qjs_debug_server < 0) {
            perror("socket");
            return;
        }
#endif
        int opt = 1;
        setsockopt(qjs_debug_server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(qjs_debug_server, (struct sockaddr*)&addr, sizeof(addr))
#ifdef _WIN32
            == SOCKET_ERROR
#else
            < 0
#endif
        ) {
            fprintf(stderr, "bind error: %d\n", SOCKET_ERRNO);
            CLOSESOCKET(qjs_debug_server);
            qjs_debug_server = -1;
            SOCK_CLEANUP();
            return;
        }
        listen(qjs_debug_server, 1);
        SOCK_NONBLOCK(qjs_debug_server);

        printf("[qjs_debug] Listening on port %d\n", port);
    }

    // register breakpoint() in JS
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, breakpointFnName,
                      JS_NewCFunction(ctx, qjs_debug_breakpoint,
                                      breakpointFnName, 1));
    JS_FreeValue(ctx, global);
}

void qjs_debug_poll(void) {
    if (qjs_debug_server >= 0 && qjs_debug_client < 0) {
        struct sockaddr_in cliaddr;
#ifdef _WIN32
        int len = sizeof(cliaddr);
        int fd = accept(qjs_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd != INVALID_SOCKET) {
            SOCK_NONBLOCK(fd);
            qjs_debug_client = fd;
            printf("[qjs_debug] Client connected\n");
        }
#else
        socklen_t len = sizeof(cliaddr);
        int fd = accept(qjs_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd >= 0) {
            SOCK_NONBLOCK(fd);
            qjs_debug_client = fd;
            printf("[qjs_debug] Client connected\n");
        }
#endif
    }
    if (qjs_debug_client >= 0) {
        char buf[1];
#ifdef _WIN32
        int n = recv(qjs_debug_client, buf, 1, MSG_PEEK);
#else
        ssize_t n = recv(qjs_debug_client, buf, 1, MSG_PEEK);
#endif
        if (n == 0) {
            printf("[qjs_debug] Client disconnected\n");
            CLOSESOCKET(qjs_debug_client);
            qjs_debug_client = -1;
        }
    }
}

void qjs_debug_shutdown(void) {
    if (qjs_debug_client >= 0) { CLOSESOCKET(qjs_debug_client); qjs_debug_client = -1; }
    if (qjs_debug_server >= 0) { CLOSESOCKET(qjs_debug_server); qjs_debug_server = -1; }
    SOCK_CLEANUP();
}

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

static void qjs_debug_dump_exception(JSContext *ctx, JSValueConst val) {
    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(ctx);

        // Try to extract stack trace
        JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
        const char *stack_str = JS_ToCString(ctx, stack);
        if (stack_str) {
            dprintf(qjs_debug_client, "Exception: %s\n", stack_str);
            JS_FreeCString(ctx, stack_str);
        } else {
            const char *exc_str = JS_ToCString(ctx, exc);
            dprintf(qjs_debug_client, "Exception: %s\n",
                    exc_str ? exc_str : "(unprintable)");
            JS_FreeCString(ctx, exc_str);
        }
        JS_FreeValue(ctx, stack);
        JS_FreeValue(ctx, exc);
    }
}

// Minimal REPL loop (blocking until client disconnects)
static int qjs_debug_repl(JSContext* ctx, int argc, JSValueConst *argv) {
    // print and expose args to REPL loop:
    for (int i = 0; i < argc; ++i) {
        JSValue json = JS_JSONStringify(ctx, argv[i], JS_UNDEFINED, JS_UNDEFINED);
        if (JS_IsException(json)) {
            dprintf(qjs_debug_client, "  args[%d]: (unprintable)\n", i);
        } else {
            const char *cstr = JS_ToCString(ctx, json);
            dprintf(qjs_debug_client, "  args[%d]: %s\n", i, cstr ? cstr : "(unprintable)");
            JS_FreeCString(ctx, cstr);
        }
        JS_FreeValue(ctx, json);
    }
    JSValue args_array = JS_NewArray(ctx);
    for (int i = 0; i < argc; ++i) {
        JS_SetPropertyUint32(ctx, args_array, i, JS_DupValue(ctx, argv[i]));
    }
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue prev_args = JS_GetPropertyStr(ctx, global, "args"); // save previous 'args'
    JS_SetPropertyStr(ctx, global, "args", args_array);

    char line[1024];
    int ret = QJS_DEBUG_EVENT_CONTINUE;
    while (qjs_debug_client >= 0) {
        dprintf(qjs_debug_client, "js> ");

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(qjs_debug_client, &readfds);
        // Wait for input, no timeout → block until debugger sends or disconnects
#ifdef _WIN32
        int rv = select(0, &readfds, NULL, NULL, NULL);
#else
        const int rv = select(qjs_debug_client + 1, &readfds, NULL, NULL, NULL);
#endif
        if (rv <= 0) {
            fprintf(stderr, "Debugger select() error\n");
            break;
        }

#ifdef _WIN32
        int n = recv(qjs_debug_client, line, sizeof(line)-1, 0);
#else
        const ssize_t n = recv(qjs_debug_client, line, sizeof(line)-1, 0);
#endif
        if (n <= 0) {
            fprintf(stderr, "Debugger disconnected — resuming.\n");
            break;
        }
        if (n >= sizeof(line) - 1) {
            dprintf(qjs_debug_client, "Input too long\n");
            continue;
        }
        line[n] = 0;
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (line[0] == 0) break; // Empty line: exit REPL
        if (strncmp(line, ".cont", 5) == 0) break;
        if (strncmp(line, ".quit", 5) == 0) {
            ret = QJS_DEBUG_EVENT_QUIT;
            break;
        }

        JSValue result = JS_Eval(ctx, line, strlen(line),
                                 "<repl>", JS_EVAL_TYPE_GLOBAL);

        if (JS_IsException(result)) {
            qjs_debug_dump_exception(ctx, result);
        } else {
            JSValue json = JS_JSONStringify(ctx, result, JS_UNDEFINED, JS_UNDEFINED);
            if (!JS_IsException(json)) {
                const char *cstr = JS_ToCString(ctx, json);
                dprintf(qjs_debug_client, "%s\n", cstr ? cstr : "(unprintable)");
                JS_FreeCString(ctx, cstr);
            }
            JS_FreeValue(ctx, json);
        }
        JS_FreeValue(ctx, result);
    }
    // restore global args
    JS_SetPropertyStr(ctx, global, "args", prev_args);
    JS_FreeValue(ctx, global);
    return ret;
}

static JSValue qjs_debug_breakpoint(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    if (qjs_debug_client >= 0) {
        dprintf(qjs_debug_client, "\n[qjs_debug] Breakpoint hit!\n");
        // Evaluate 'new Error().stack' to get a stack trace at this point
        JSValue stackVal = JS_Eval(ctx, "new Error().stack", strlen("new Error().stack"), "<breakpoint>", 0);
        const char *stack_str = JS_ToCString(ctx, stackVal);
        if (stack_str) {
            // Skip the first two lines
            const char *p = stack_str;
            int lines_skipped = 0;
            while (*p && lines_skipped < 2) {
                if (*p == '\n') lines_skipped++;
                p++;
            }
            dprintf(qjs_debug_client, "%s\n", p);
            JS_FreeCString(ctx, stack_str);
        }
        JS_FreeValue(ctx, stackVal);

        const int ret = qjs_debug_repl(ctx, argc, argv);
        if (qjs_debug_break_cb)
            qjs_debug_break_cb(ret);
    }
    return JS_UNDEFINED;
}

#endif // QJS_DEBUG_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
