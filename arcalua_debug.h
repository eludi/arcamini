#pragma once

#include "minilua.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

typedef void (*lua_debug_session_cb_t)(int);

/// debug session events passed to the debug session callback
#define ARCALUA_DEBUG_EVENT_CONTINUE   0
#define ARCALUA_DEBUG_EVENT_QUIT       1

/**
 * Initialize the Lua debugger.
 * 
 * @param L Lua state
 * @param port TCP port for debugger connection (0 = disabled)
 * @param breakpointFnName name of the global Lua function to register (e.g. "breakpoint")
 * @param cb callback invoked when a debug session has ended (can be NULL)
 */
void arcalua_debug_init(void* vm, int port, const char *breakpointFnName, lua_debug_session_cb_t cb);

/** Poll for connections and handle disconnections. Call once per frame. */
void arcalua_debug_poll(void);

/** Shutdown debugger, close sockets. */
void arcalua_debug_shutdown(void);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// Implementation (header-only, define ARCALUA_DEBUG_IMPLEMENTATION before including)
// -----------------------------------------------------------------------------
#ifdef ARCALUA_DEBUG_IMPLEMENTATION


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

// Evaluate Lua source. 
// On success: returns number of results pushed on the stack (>=0).
// On error: returns -1 and pushes error message on the stack.
static int arcalua_eval(lua_State *L, const char *src) {
    int status;

    // 1) Try as an expression: "return <src>"
    luaL_Buffer b;
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, "return ", 7);
    luaL_addstring(&b, src);
    luaL_pushresult(&b);                  // stack: [ "return <src>" ]
    const char *wrapped = lua_tostring(L, -1);

    status = luaL_loadstring(L, wrapped); // stack: [ "return <src>", <func or err> ]
    lua_remove(L, -2);                    // remove the temp string

    if (status != LUA_OK) {
        // 2) Fallback: try as a statement chunk
        lua_pop(L, 1); // pop error msg from loadstring
        status = luaL_loadstring(L, src); // stack: [ <func or err> ]
        if (status != LUA_OK) {
            // Leave the error message on stack for caller
            return -1;
        }
    }

    // bind _ENV of this chunk to the real global table:
    lua_pushglobaltable(L);
    lua_setupvalue(L, -2, 1);

    // Call the chunk:
    int base = lua_gettop(L) - 1; // index of the function before call
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        // error message is on top, return -1
        return -1;
    }

    // Number of results = current top - base
    int nresults = lua_gettop(L) - base;
    return nresults;
}


// Internal state
static int arcalua_debug_server = -1;
static int arcalua_debug_client = -1;
static lua_debug_session_cb_t arcalua_debug_break_cb = NULL;

// Forward declaration
static int arcalua_debug_breakpoint(lua_State *L);

void arcalua_debug_init(void* vm, int port, const char *breakpointFnName, lua_debug_session_cb_t cb) {
    lua_State *L = (lua_State*)vm;
    if (port > 0) {
        arcalua_debug_break_cb = cb;
        SOCK_INIT();
        arcalua_debug_server = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
        if (arcalua_debug_server == INVALID_SOCKET) {
            fprintf(stderr, "socket error: %d\n", SOCKET_ERRNO);
            return;
        }
#else
        if (arcalua_debug_server < 0) {
            perror("socket");
            return;
        }
#endif
        int opt = 1;
        setsockopt(arcalua_debug_server, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(arcalua_debug_server, (struct sockaddr*)&addr, sizeof(addr))
#ifdef _WIN32
            == SOCKET_ERROR
#else
            < 0
#endif
        ) {
            fprintf(stderr, "bind error: %d\n", SOCKET_ERRNO);
            CLOSESOCKET(arcalua_debug_server);
            arcalua_debug_server = -1;
            SOCK_CLEANUP();
            return;
        }
        listen(arcalua_debug_server, 1);
        SOCK_NONBLOCK(arcalua_debug_server);

        printf("[arcalua_debug] Listening on port %d\n", port);
    }

    // Register breakpoint() in Lua
    lua_pushcfunction(L, arcalua_debug_breakpoint);
    lua_setglobal(L, breakpointFnName);
}

void arcalua_debug_poll(void) {
    if (arcalua_debug_server >= 0 && arcalua_debug_client < 0) {
        struct sockaddr_in cliaddr;
#ifdef _WIN32
        int len = sizeof(cliaddr);
        int fd = accept(arcalua_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd != INVALID_SOCKET) {
            SOCK_NONBLOCK(fd);
            arcalua_debug_client = fd;
            printf("[arcalua_debug] Client connected\n");
        }
#else
        socklen_t len = sizeof(cliaddr);
        int fd = accept(arcalua_debug_server, (struct sockaddr*)&cliaddr, &len);
        if (fd >= 0) {
            SOCK_NONBLOCK(fd);
            arcalua_debug_client = fd;
            printf("[arcalua_debug] Client connected\n");
        }
#endif
    }
    if (arcalua_debug_client >= 0) {
        char buf[1];
#ifdef _WIN32
        int n = recv(arcalua_debug_client, buf, 1, MSG_PEEK);
#else
        ssize_t n = recv(arcalua_debug_client, buf, 1, MSG_PEEK);
#endif
        if (n == 0) {
            printf("[arcalua_debug] Client disconnected\n");
            CLOSESOCKET(arcalua_debug_client);
            arcalua_debug_client = -1;
        }
    }
}

void arcalua_debug_shutdown(void) {
    if (arcalua_debug_client >= 0) { CLOSESOCKET(arcalua_debug_client); arcalua_debug_client = -1; }
    if (arcalua_debug_server >= 0) { CLOSESOCKET(arcalua_debug_server); arcalua_debug_server = -1; }
    SOCK_CLEANUP();
}

// Minimal REPL loop (blocking until client disconnects)
static int arcalua_debug_repl(lua_State *L) {
    char line[1024];
    int ret = ARCALUA_DEBUG_EVENT_CONTINUE;
    while (arcalua_debug_client >= 0) {
        dprintf(arcalua_debug_client, "\nlua> ");

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(arcalua_debug_client, &readfds);
#ifdef _WIN32
        int rv = select(0, &readfds, NULL, NULL, NULL);
#else
        const int rv = select(arcalua_debug_client + 1, &readfds, NULL, NULL, NULL);
#endif
        if (rv <= 0) {
            fprintf(stderr, "Debugger select() error\n");
            break;
        }

#ifdef _WIN32
        int n = recv(arcalua_debug_client, line, sizeof(line)-1, 0);
#else
        const ssize_t n = recv(arcalua_debug_client, line, sizeof(line)-1, 0);
#endif
        if (n <= 0) {
            fprintf(stderr, "Debugger disconnected — resuming.\n");
            break;
        }
        if (n >= sizeof(line) - 1) {
            dprintf(arcalua_debug_client, "Input too long\n");
            continue;
        }
        line[n] = 0;
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
        if (line[0] == 0) break; // Empty line: exit REPL
        if (strncmp(line, ".cont", 5) == 0) break;
        if (strncmp(line, ".quit", 5) == 0) {
            ret = ARCALUA_DEBUG_EVENT_QUIT;
            break;
        }

        int numRet = arcalua_eval(L, line);
        if (numRet < 0) {
            dprintf(arcalua_debug_client, "Error: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        else for (int i = -numRet; i <= -1; ++i) { // send multiple return values
            size_t len;
            const char *s = luaL_tolstring(L, i, &len);
            dprintf(arcalua_debug_client, "  ret[%d]:\"%s\"\n", i + numRet + 1, s);
            lua_pop(L, 1);
        }
        lua_settop(L, 0); // clear stack
    }
    return ret;
}

static int arcalua_debug_breakpoint(lua_State *L) {
    if (arcalua_debug_client >= 0) {
        dprintf(arcalua_debug_client, "\n[arcalua_debug] Breakpoint hit!\n");
        // print current stack trace:
        lua_Debug ar;
        for (int i = 1; lua_getstack(L, i, &ar); i++) {
            lua_getinfo(L, "Sl", &ar);
            dprintf(arcalua_debug_client, "  at %s:%d\n", ar.source, ar.currentline);
        }
        lua_getglobal(L, "args");
        lua_setfield(L, LUA_REGISTRYINDEX, "arcalua_args"); // save global args before overwriting

        // collect arguments in a local table called args and print them serialized as string:
        int nArgs = lua_gettop(L);
        lua_newtable(L);
        for (int i = 1; i <= nArgs; i++) {
            lua_pushinteger(L, i);
            lua_pushvalue(L, i);
            lua_settable(L, -3);

            if (lua_isstring(L, i)) {
                dprintf(arcalua_debug_client, "  args[%d]:\"%s\"\n", i, lua_tostring(L, i));
            } else {
                dprintf(arcalua_debug_client, "  args[%d]:(%s)\n", i, luaL_typename(L, i));
            }
        }
        lua_setglobal(L, "args"); // set args table as global variable
        lua_settop(L, 0); // clear stack

        const int ret = arcalua_debug_repl(L);

        lua_getfield(L, LUA_REGISTRYINDEX, "arcalua_args"); // restore previously saved global args
        lua_setglobal(L, "args");

        if (arcalua_debug_break_cb)
            arcalua_debug_break_cb(ret);
    }
    return 0;
}

#endif // LUA_DEBUG_IMPLEMENTATION

