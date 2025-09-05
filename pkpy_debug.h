#pragma once
#include "pocketpy.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

typedef void (*pkpy_debug_session_cb_t)(int);

#define PKPY_DEBUG_EVENT_CONTINUE   0
#define PKPY_DEBUG_EVENT_QUIT       1

void pkpy_debug_init(int port, const char *breakpointFnName, pkpy_debug_session_cb_t cb);
void pkpy_debug_poll(void);
void pkpy_debug_shutdown(void);

// -----------------------------------------------------------------------------
// Implementation (header-only, define PKPY_DEBUG_IMPLEMENTATION before including)
// -----------------------------------------------------------------------------
#ifdef PKPY_DEBUG_IMPLEMENTATION

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

static int pkpy_debug_server = -1;
static int pkpy_debug_client = -1;
static pkpy_debug_session_cb_t pkpy_debug_break_cb = NULL;
static char pkpy_debug_breakpoint_name[64] = "breakpoint";

// Forward decl
static bool pkpy_debug_breakpoint(int argc, py_StackRef argv);

void pkpy_debug_init(int port, const char *breakpointFnName, pkpy_debug_session_cb_t cb) {
    if (breakpointFnName && strlen(breakpointFnName) < sizeof(pkpy_debug_breakpoint_name))
        strcpy(pkpy_debug_breakpoint_name, breakpointFnName);

    if (port > 0) {
        pkpy_debug_break_cb = cb;
        SOCK_INIT();
        pkpy_debug_server = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        if (pkpy_debug_server == INVALID_SOCKET) {
            fprintf(stderr, "socket error: %d\n", SOCKET_ERRNO);
            return;
        }
#else
        if (pkpy_debug_server < 0) {
            perror("socket");
            return;
        }
#endif
        int opt = 1;
        setsockopt(pkpy_debug_server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(pkpy_debug_server, (struct sockaddr*)&addr, sizeof(addr))
#ifdef _WIN32
            == SOCKET_ERROR
#else
            < 0
#endif
        ) {
            fprintf(stderr, "bind error: %d\n", SOCKET_ERRNO);
            CLOSESOCKET(pkpy_debug_server);
            pkpy_debug_server = -1;
            SOCK_CLEANUP();
            return;
        }
        listen(pkpy_debug_server, 1);
        SOCK_NONBLOCK(pkpy_debug_server);

        printf("[pkpy_debug] Listening on port %d\n", port);
    }

    // Register breakpoint() in Python
    py_bindfunc(py_getmodule("__main__"), pkpy_debug_breakpoint_name, pkpy_debug_breakpoint);
}

void pkpy_debug_poll(void) {
    if (pkpy_debug_server >= 0 && pkpy_debug_client < 0) {
        struct sockaddr_in cliaddr;
#ifdef _WIN32
        int len = sizeof(cliaddr);
        int fd = accept(pkpy_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd != INVALID_SOCKET) {
            SOCK_NONBLOCK(fd);
            pkpy_debug_client = fd;
            printf("[pkpy_debug] Client connected\n");
        }
#else
        socklen_t len = sizeof(cliaddr);
        int fd = accept(pkpy_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd >= 0) {
            SOCK_NONBLOCK(fd);
            pkpy_debug_client = fd;
            printf("[pkpy_debug] Client connected\n");
        }
#endif
    }
    if (pkpy_debug_client >= 0) {
        char buf[1];
#ifdef _WIN32
        int n = recv(pkpy_debug_client, buf, 1, MSG_PEEK);
#else
        ssize_t n = recv(pkpy_debug_client, buf, 1, MSG_PEEK);
#endif
        if (n == 0) {
            printf("[pkpy_debug] Client disconnected\n");
            CLOSESOCKET(pkpy_debug_client);
            pkpy_debug_client = -1;
        }
    }
}

void pkpy_debug_shutdown(void) {
    if (pkpy_debug_client >= 0) { CLOSESOCKET(pkpy_debug_client); pkpy_debug_client = -1; }
    if (pkpy_debug_server >= 0) { CLOSESOCKET(pkpy_debug_server); pkpy_debug_server = -1; }
    SOCK_CLEANUP();
}

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

static void pkpy_debug_dump_exception() {
    char* msg = py_formatexc();
    if (msg) {
        dprintf(pkpy_debug_client, "Exception: %s\n", msg);
        free(msg);
    }
}

// Minimal REPL loop (blocking until client disconnects)
static int pkpy_debug_repl(int argc, py_StackRef argv) {
    int ret = PKPY_DEBUG_EVENT_CONTINUE;
    // Print and expose args to REPL loop:
    py_newlist(py_r0());
    for (int i = 0; i < argc; ++i) {
        py_list_append(py_r0(), py_arg(i));
        py_str(py_arg(i));
        const char* cstr = py_tostr(py_retval());
        dprintf(pkpy_debug_client, "args[%d]: %s\n", i, cstr ? cstr : "(unprintable)");
    }
    py_Ref prev_args = py_getglobal(py_name("args"));
    if(prev_args)
        py_setglobal(py_name("__debug_prev_args__"), prev_args); //store for later restoration
    py_setglobal(py_name("args"), py_r0());

    char line[1024];
    while (pkpy_debug_client >= 0) {
        dprintf(pkpy_debug_client, "py> ");

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(pkpy_debug_client, &readfds);
#ifdef _WIN32
        int rv = select(0, &readfds, NULL, NULL, NULL);
#else
        const int rv = select(pkpy_debug_client + 1, &readfds, NULL, NULL, NULL);
#endif
        if (rv <= 0) {
            fprintf(stderr, "Debugger select() error\n");
            break;
        }

#ifdef _WIN32
        int n = recv(pkpy_debug_client, line, sizeof(line)-1, 0);
#else
        const ssize_t n = recv(pkpy_debug_client, line, sizeof(line)-1, 0);
#endif
        if (n <= 0) {
            fprintf(stderr, "Debugger disconnected — resuming.\n");
            break;
        }
        if (n >= sizeof(line) - 1) {
            dprintf(pkpy_debug_client, "Input too long\n");
            continue;
        }
        line[n] = 0;
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (line[0] == 0) break;
        if (strncmp(line, ".cont", 5) == 0) break;
        if (strncmp(line, ".quit", 5) == 0) {
            ret = PKPY_DEBUG_EVENT_QUIT;
            break;
        }

        bool ok = py_exec(line, "<repl>", EVAL_MODE, NULL);
        if (!ok) {
            pkpy_debug_dump_exception();
        } else {
            py_str(py_retval());
            const char* cstr = py_tostr(py_retval());
            dprintf(pkpy_debug_client, "%s\n", cstr ? cstr : "(unprintable)");
        }
    }
    if(prev_args) { // restore original args
        py_setglobal(py_name("args"), py_getglobal(py_name("__debug_prev_args__")));
        py_setglobal(py_name("__debug_prev_args__"), py_NIL());
    }
    return ret;
}

static bool pkpy_debug_breakpoint(int argc, py_StackRef argv) {
    if (pkpy_debug_client >= 0) {
        dprintf(pkpy_debug_client, "\n[pkpy_debug] Breakpoint hit!\n");
        py_Frame* frame = py_inspect_currentframe();
        if(frame) { // print local context
            int lineno;
            const char* sloc = py_Frame_sourceloc(frame, &lineno);
            dprintf(pkpy_debug_client, "  at %s:%d\n", sloc ? sloc : "(unknown)", lineno);
            py_Frame_newlocals(frame, py_r0());
            py_json_dumps(py_r0(), 0);
            dprintf(pkpy_debug_client, "locals: %s\n", py_tostr(py_retval()));
        }

        int ret = pkpy_debug_repl(argc, argv);
        if (pkpy_debug_break_cb)
            pkpy_debug_break_cb(ret);
    }
    py_newnone(py_retval());
    return true;
}


#endif // PKPY_DEBUG_IMPLEMENTATION

#ifdef __cplusplus
}
#endif
