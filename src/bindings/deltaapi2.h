#ifndef __LU5_DELTAAPI2_BINDINGS__
#define __LU5_DELTAAPI2_BINDINGS__

#include <lua.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Register DeltaAPI-2 endpoints into the given Lua state.
 *
 * Contract:
 * - Inputs: valid lua_State* L
 * - Outputs: none (registers globals + userdata metatables)
 * - Errors: raises Lua error only on programmer misuse (e.g., allocation failures)
 * - Side effects: creates globals DI/DO/ExtDI/ExtDO/WAIT/DELAY/ReadModbus/WriteModbus/
 *   AuxTasksAdd/AuxTasks/SocketClass/SocketServer/CheckAllStatus/SocketVersion/SocketErrorCode
 */
void lu5_bind_deltaapi2(lua_State *L);

#ifdef __cplusplus
}
#endif

#endif
