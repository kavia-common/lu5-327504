#ifndef __LU5_DELTA_SOCKET_BINDINGS__
#define __LU5_DELTA_SOCKET_BINDINGS__

#include <lua.h>

/** Metatable name for the DeltaSocket userdata */
#define DELTA_SOCKET_MT "DeltaSocket"

/** Registry key for the socket instance tracking table */
#define DELTA_SOCKET_REGISTRY "_delta_sockets_registry"

/**
 * @brief SocketClass - Create a TCP client socket connection
 *
 * Set up Socket connection details and establish the connection to a server.
 *
 * @param Host_IP    IP address string of the server
 * @param Port       Port number (integer, 1-65535)
 * @param Spacing    Delimiter char for splitting received data (or nil for ',')
 * @param Delimiter  End-of-message char appended to sent data (or nil for CRLF)
 * @param Cmd        Default command string to send (or nil)
 * @param Sleeptime  Interval between sends in seconds (or nil for 0.1)
 * @param Timeout    Receive timeout in seconds (> 0, default 10)
 * @return  DeltaSocket userdata with Send, Receive, Close, CheckStatus methods
 *
 * @example
 * sock = SocketClass("192.168.1.99", 7000, nil, nil, nil, 0.3, 5)
 * sock:Send("hello")
 * data = sock:Receive()
 * sock:Close()
 * @example
 */
int delta_SocketClass(lua_State *L);

/**
 * @brief SocketServer - Create a TCP server socket
 *
 * Set the lu5 process as a free-communication protocol slave station.
 *
 * @param Port       Listening port number (integer, 3000-10000)
 * @param Spacing    Receive delimiter char (or nil for ';')
 * @param Delimiter  Send end-of-message char (or nil for CRLF)
 * @param Cmd        Default command (or nil)
 * @param Timeout    Receive timeout in seconds (default 10)
 * @return  DeltaSocket userdata
 *
 * @example
 * server = SocketServer(7000, nil, nil, nil, 10)
 * @example
 */
int delta_SocketServer(lua_State *L);

/**
 * @brief CheckAllStatus - Read all socket connection statuses
 *
 * @return  retPort[], retStatus[], retErr[] (three parallel arrays)
 *
 * @example
 * ports, statuses, errs = CheckAllStatus()
 * @example
 */
int delta_CheckAllStatus(lua_State *L);

/**
 * @brief SocketVersion - Print the socket implementation version
 *
 * @return nothing
 *
 * @example
 * SocketVersion()
 * @example
 */
int delta_SocketVersion(lua_State *L);

/**
 * Register the DeltaSocket metatable (called once during binding setup).
 *
 * @param L  Lua state
 */
void delta_socket_register_metatable(lua_State *L);

#endif /* __LU5_DELTA_SOCKET_BINDINGS__ */
