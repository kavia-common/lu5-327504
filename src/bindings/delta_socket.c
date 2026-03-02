/**
 * delta_socket.c - Delta Robot API: Socket communication bindings for lu5
 *
 * Implements SocketClass, SocketServer, CheckAllStatus, SocketVersion as
 * Lua-callable C functions. DeltaSocket instances are Lua userdata backed by
 * a delta_socket_t struct. Member functions (Send, Receive, Close, CheckStatus)
 * are exposed via a metatable named "DeltaSocket".
 *
 * POSIX sockets are used on Linux; on WASM all network calls are stubbed.
 *
 * Flow name: DeltaSocketFlow
 * Entry points (registered in lu5_bindings.c):
 *   delta_SocketClass    -> "SocketClass"
 *   delta_SocketServer   -> "SocketServer"
 *   delta_CheckAllStatus -> "CheckAllStatus"
 *   delta_SocketVersion  -> "SocketVersion"
 * Contracts:
 *   - SocketClass connects synchronously; failure logs warning (not fatal).
 *   - Send writes string/number + delimiter to fd.
 *   - Receive reads until delimiter, splits by Spacing, returns table.
 *   - Close closes the fd; repeated calls are safe (fd == -1 guard).
 *   - CheckStatus returns 3 values: port, status string, errcode.
 *   - CheckAllStatus returns 3 parallel array tables.
 *   - All userdata tracked in "_delta_sockets_registry" global table.
 */

#include "delta_socket.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * Platform-specific socket includes
 * ------------------------------------------------------------------------- */
#ifndef LU5_WASM
#  ifndef _WIN32
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <errno.h>
     typedef int socket_fd_t;
#    define INVALID_SOCKET_FD (-1)
#    define delta_close_fd(fd) close(fd)
#  else
#    include <winsock2.h>
#    pragma comment(lib, "ws2_32.lib")
     typedef SOCKET socket_fd_t;
#    define INVALID_SOCKET_FD INVALID_SOCKET
#    define delta_close_fd(fd) closesocket(fd)
#  endif
#else
     typedef int socket_fd_t;
#    define INVALID_SOCKET_FD (-1)
#    define delta_close_fd(fd) ((void)(fd))
#endif

/* -------------------------------------------------------------------------
 * delta_socket_t - internal socket state stored in Lua userdata
 * ------------------------------------------------------------------------- */
#define DELTA_SPACING_MAX  8
#define DELTA_DELIM_MAX    8
#define DELTA_CMD_MAX      64

typedef struct {
    socket_fd_t fd;                         /* socket file descriptor */
    int         port;                       /* port number */
    int         is_server;                  /* 1=server, 0=client */
    char        spacing[DELTA_SPACING_MAX]; /* receive split char */
    char        delimiter[DELTA_DELIM_MAX]; /* send end-of-msg */
    char        cmd[DELTA_CMD_MAX];         /* default command */
    double      sleeptime;                  /* inter-send interval (s) */
    double      timeout;                    /* receive timeout (s) */
} delta_socket_t;

/* -------------------------------------------------------------------------
 * Helper: retrieve and validate DeltaSocket userdata from stack
 * ------------------------------------------------------------------------- */
static delta_socket_t *delta_socket_check(lua_State *L, int idx)
{
    delta_socket_t *sock = (delta_socket_t *)luaL_checkudata(L, idx, DELTA_SOCKET_MT);
    if (sock == NULL) {
        luaL_error(L, "DeltaSocket: invalid socket object");
    }
    return sock;
}

/* -------------------------------------------------------------------------
 * Helper: create a new delta_socket_t userdata and register it
 * Returns the userdata on top of the Lua stack.
 * ------------------------------------------------------------------------- */
static delta_socket_t *delta_socket_new(lua_State *L)
{
    delta_socket_t *sock = (delta_socket_t *)lua_newuserdata(L, sizeof(delta_socket_t));
    memset(sock, 0, sizeof(delta_socket_t));
    sock->fd         = INVALID_SOCKET_FD;
    sock->sleeptime  = 0.1;
    sock->timeout    = 10.0;
    sock->spacing[0] = ',';
    sock->spacing[1] = '\0';
    strncpy(sock->delimiter, "\r\n", DELTA_DELIM_MAX - 1);

    luaL_getmetatable(L, DELTA_SOCKET_MT);
    lua_setmetatable(L, -2);

    /* Register in global tracking table */
    lua_getglobal(L, DELTA_SOCKET_REGISTRY);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, DELTA_SOCKET_REGISTRY);
    }
    int reg_len = (int)luaL_len(L, -1);
    lua_pushvalue(L, -2);          /* push userdata */
    lua_rawseti(L, -2, reg_len + 1);
    lua_pop(L, 1);                 /* pop registry table */

    return sock;
}

/* -------------------------------------------------------------------------
 * Helper: parse optional spacing/delimiter/cmd/sleeptime/timeout arguments
 * ------------------------------------------------------------------------- */
static void delta_socket_parse_opts(lua_State *L, delta_socket_t *sock,
                                    int spacing_idx, int delim_idx,
                                    int cmd_idx, int sleep_idx, int timeout_idx,
                                    int argc)
{
    if (spacing_idx > 0 && spacing_idx <= argc &&
        !lua_isnil(L, spacing_idx) && lua_isstring(L, spacing_idx)) {
        const char *s = lua_tostring(L, spacing_idx);
        strncpy(sock->spacing, s, DELTA_SPACING_MAX - 1);
        sock->spacing[DELTA_SPACING_MAX - 1] = '\0';
    }
    if (delim_idx > 0 && delim_idx <= argc &&
        !lua_isnil(L, delim_idx) && lua_isstring(L, delim_idx)) {
        const char *s = lua_tostring(L, delim_idx);
        strncpy(sock->delimiter, s, DELTA_DELIM_MAX - 1);
        sock->delimiter[DELTA_DELIM_MAX - 1] = '\0';
    }
    if (cmd_idx > 0 && cmd_idx <= argc &&
        !lua_isnil(L, cmd_idx) && lua_isstring(L, cmd_idx)) {
        const char *s = lua_tostring(L, cmd_idx);
        strncpy(sock->cmd, s, DELTA_CMD_MAX - 1);
        sock->cmd[DELTA_CMD_MAX - 1] = '\0';
    }
    if (sleep_idx > 0 && sleep_idx <= argc && lua_isnumber(L, sleep_idx)) {
        sock->sleeptime = lua_tonumber(L, sleep_idx);
    }
    if (timeout_idx > 0 && timeout_idx <= argc && lua_isnumber(L, timeout_idx)) {
        double t = lua_tonumber(L, timeout_idx);
        sock->timeout = (t > 0) ? t : 10.0;
    }
}

/* -------------------------------------------------------------------------
 * Member function: Send(Cmd)
 * ------------------------------------------------------------------------- */
static int delta_socket_Send(lua_State *L)
{
    delta_socket_t *sock = delta_socket_check(L, 1);
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "Socket:Send requires 1 argument (Cmd)");
        return 0;
    }

    char buf[512];
    buf[0] = '\0';
    if (lua_isnumber(L, 2)) {
        snprintf(buf, sizeof(buf), "%g", lua_tonumber(L, 2));
    } else if (lua_isstring(L, 2)) {
        snprintf(buf, sizeof(buf), "%s", lua_tostring(L, 2));
    } else {
        luaL_error(L, "Socket:Send: Cmd must be a string or number");
        return 0;
    }

#ifndef LU5_WASM
    if (sock->fd != INVALID_SOCKET_FD) {
        send(sock->fd, buf, (int)strlen(buf), 0);
        if (strlen(sock->delimiter) > 0) {
            send(sock->fd, sock->delimiter, (int)strlen(sock->delimiter), 0);
        }
    } else {
        LU5_WARN("Socket:Send: socket is not connected");
    }
#else
    LU5_WARN("Socket:Send: sockets not available in lu5-wasm");
    (void)sock;
#endif

    return 0;
}

/* -------------------------------------------------------------------------
 * Member function: Receive()
 * ------------------------------------------------------------------------- */
static int delta_socket_Receive(lua_State *L)
{
    delta_socket_t *sock = delta_socket_check(L, 1);

#ifndef LU5_WASM
    if (sock->fd == INVALID_SOCKET_FD) {
        LU5_WARN("Socket:Receive: socket is not connected");
        lua_pushnil(L);
        return 1;
    }

    char recv_buf[4096];
    int  total     = 0;
    int  delim_len = (int)strlen(sock->delimiter);

    while (total < (int)(sizeof(recv_buf) - 1)) {
        char ch;
        int  n = (int)recv(sock->fd, &ch, 1, 0);
        if (n <= 0) break;
        recv_buf[total++] = ch;
        if (delim_len > 0 && total >= delim_len) {
            if (memcmp(recv_buf + total - delim_len,
                       sock->delimiter, (size_t)delim_len) == 0) {
                total -= delim_len; /* strip delimiter */
                break;
            }
        }
    }
    recv_buf[total] = '\0';

    /* Split by spacing character and build Lua table */
    lua_newtable(L);
    int  idx = 1;
    char sp  = sock->spacing[0];

    if (sp == '\0') {
        lua_pushstring(L, recv_buf);
        lua_rawseti(L, -2, idx);
    } else {
        char *ptr   = recv_buf;
        char *token = ptr;
        while (*ptr != '\0') {
            if (*ptr == sp) {
                *ptr = '\0';
                lua_pushstring(L, token);
                lua_rawseti(L, -2, idx++);
                token = ptr + 1;
            }
            ptr++;
        }
        lua_pushstring(L, token);
        lua_rawseti(L, -2, idx);
    }
#else
    LU5_WARN("Socket:Receive: sockets not available in lu5-wasm");
    (void)sock;
    lua_newtable(L);
#endif

    return 1;
}

/* -------------------------------------------------------------------------
 * Member function: Close()
 * ------------------------------------------------------------------------- */
static int delta_socket_Close(lua_State *L)
{
    delta_socket_t *sock = delta_socket_check(L, 1);
    if (sock->fd != INVALID_SOCKET_FD) {
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * Member function: CheckStatus()
 * Returns: port (int), status string, errcode (int)
 * ------------------------------------------------------------------------- */
static int delta_socket_CheckStatus(lua_State *L)
{
    delta_socket_t *sock = delta_socket_check(L, 1);

    lua_pushinteger(L, sock->port);
    lua_pushstring(L, (sock->fd != INVALID_SOCKET_FD) ? "Connected" : "DisConnected");
    lua_pushinteger(L, 0); /* error code: 0 = no error */
    return 3;
}

/* -------------------------------------------------------------------------
 * Metatable __index dispatch for member functions
 * ------------------------------------------------------------------------- */
static int delta_socket_index(lua_State *L)
{
    const char *key = lua_tostring(L, 2);
    if (key == NULL) { lua_pushnil(L); return 1; }

    if      (strcmp(key, "Send")        == 0) lua_pushcfunction(L, delta_socket_Send);
    else if (strcmp(key, "Receive")     == 0) lua_pushcfunction(L, delta_socket_Receive);
    else if (strcmp(key, "Close")       == 0) lua_pushcfunction(L, delta_socket_Close);
    else if (strcmp(key, "CheckStatus") == 0) lua_pushcfunction(L, delta_socket_CheckStatus);
    else                                       lua_pushnil(L);

    return 1;
}

/* -------------------------------------------------------------------------
 * Metatable __gc: close fd on garbage collection
 * ------------------------------------------------------------------------- */
static int delta_socket_gc(lua_State *L)
{
    delta_socket_t *sock = (delta_socket_t *)lua_touserdata(L, 1);
    if (sock && sock->fd != INVALID_SOCKET_FD) {
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * delta_socket_register_metatable - register "DeltaSocket" metatable once
 * Must be called before any SocketClass/SocketServer call.
 * ------------------------------------------------------------------------- */
void delta_socket_register_metatable(lua_State *L)
{
    if (luaL_newmetatable(L, DELTA_SOCKET_MT)) {
        lua_pushcfunction(L, delta_socket_index);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, delta_socket_gc);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1); /* pop metatable */
}

/* -------------------------------------------------------------------------
 * SocketClass - Create a TCP client socket
 * Invariant: port in [1,65535]; connects synchronously.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int delta_SocketClass(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "SocketClass: requires at least 2 arguments (Host_IP, Port)");
        return 0;
    }

    const char *host = lu5_assert_string(L, 1, "SocketClass");
    lua_Integer port  = lu5_assert_integer(L, 2, "SocketClass");

    if (port <= 0 || port > 65535) {
        luaL_error(L, "SocketClass: port must be 1..65535, got %d", (int)port);
        return 0;
    }

    delta_socket_t *sock = delta_socket_new(L);
    sock->port      = (int)port;
    sock->is_server = 0;
    /* Parse optional: spacing=3, delim=4, cmd=5, sleep=6, timeout=7 */
    delta_socket_parse_opts(L, sock, 3, 4, 5, 6, 7, argc);

#ifndef LU5_WASM
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == INVALID_SOCKET_FD) {
        luaL_error(L, "SocketClass: failed to create socket");
        return 0;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons((unsigned short)port);
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
        luaL_error(L, "SocketClass: invalid host IP address \"%s\"", host);
        return 0;
    }

    if (connect(sock->fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        LU5_WARN("SocketClass: connect to %s:%d failed, socket DisConnected", host, (int)port);
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
        /* Return the userdata anyway; CheckStatus will report DisConnected */
    }
#else
    LU5_WARN("SocketClass: sockets not available in lu5-wasm");
#endif

    return 1;
}

/* -------------------------------------------------------------------------
 * SocketServer - Create a TCP server (listening) socket
 * Invariant: port in [3000, 10000] per Delta spec.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int delta_SocketServer(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 1) {
        luaL_error(L, "SocketServer: requires at least 1 argument (Port)");
        return 0;
    }

    lua_Integer port = lu5_assert_integer(L, 1, "SocketServer");
    if (port < 3000 || port > 10000) {
        luaL_error(L, "SocketServer: port must be 3000..10000, got %d", (int)port);
        return 0;
    }

    delta_socket_t *sock = delta_socket_new(L);
    sock->port      = (int)port;
    sock->is_server = 1;

    /* Default server spacing is ';' */
    sock->spacing[0] = ';';
    sock->spacing[1] = '\0';
    /* Parse optional: spacing=2, delim=3, cmd=4, sleep=-1 (none), timeout=5 */
    delta_socket_parse_opts(L, sock, 2, 3, 4, -1, 5, argc);

#ifndef LU5_WASM
    sock->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->fd == INVALID_SOCKET_FD) {
        luaL_error(L, "SocketServer: failed to create socket");
        return 0;
    }

    int opt = 1;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons((unsigned short)port);

    if (bind(sock->fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
        luaL_error(L, "SocketServer: bind to port %d failed", (int)port);
        return 0;
    }

    if (listen(sock->fd, 5) < 0) {
        delta_close_fd(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
        luaL_error(L, "SocketServer: listen on port %d failed", (int)port);
        return 0;
    }

    /* Accept one client connection */
    socket_fd_t client_fd = accept(sock->fd, NULL, NULL);
    if (client_fd == INVALID_SOCKET_FD) {
        LU5_WARN("SocketServer: accept failed; server socket ready but not connected");
    } else {
        delta_close_fd(sock->fd);
        sock->fd = client_fd;
    }
#else
    LU5_WARN("SocketServer: sockets not available in lu5-wasm");
#endif

    return 1;
}

/* -------------------------------------------------------------------------
 * CheckAllStatus - return status of all tracked sockets
 * Returns 3 parallel array tables: ports, statuses, errcodes
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int delta_CheckAllStatus(lua_State *L)
{
    lua_newtable(L); /* ports table    (index -3) */
    lua_newtable(L); /* statuses table (index -2) */
    lua_newtable(L); /* errcodes table (index -1) */

    lua_getglobal(L, DELTA_SOCKET_REGISTRY);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 3; /* return three empty tables */
    }

    int reg_len = (int)luaL_len(L, -1);
    for (int i = 1; i <= reg_len; i++) {
        lua_rawgeti(L, -1, i); /* push socket userdata */
        if (lua_isuserdata(L, -1)) {
            delta_socket_t *sock = (delta_socket_t *)lua_touserdata(L, -1);

            /* ports table is at stack index (top - 4) from bottom of these 4 */
            lua_pushinteger(L, sock->port);
            lua_rawseti(L, -6, i);

            lua_pushstring(L, (sock->fd != INVALID_SOCKET_FD) ? "Connected" : "DisConnected");
            lua_rawseti(L, -5, i);

            lua_pushinteger(L, 0);
            lua_rawseti(L, -4, i);
        }
        lua_pop(L, 1); /* pop userdata */
    }

    lua_pop(L, 1); /* pop registry table */
    return 3;
}

/* -------------------------------------------------------------------------
 * SocketVersion - print implementation version
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int delta_SocketVersion(lua_State *L)
{
    lua_getglobal(L, "print");
    lua_pushstring(L, "DeltaSocket v1.0 (lu5 implementation)");
    lua_call(L, 1, 0);
    return 0;
}
