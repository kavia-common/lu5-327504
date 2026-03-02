#ifndef __LU5_DELTA_TASKS_BINDINGS__
#define __LU5_DELTA_TASKS_BINDINGS__

#include <lua.h>

/** Maximum number of sub-functions supported by AuxTasksAdd */
#define DELTA_MAX_AUX_TASKS 10

/**
 * @brief AuxTasksAdd - Register sub-functions for multi-task execution
 *
 * Store up to 10 Lua function references for cooperative execution by AuxTasks.
 * The first function may include motion commands; subsequent functions may not.
 * Calling AuxTasksAdd again resets and replaces all previously stored functions.
 *
 * @param function1  First function (may include motion commands)
 * @param [function2..function10]  Additional functions (no motion commands)
 * @return nothing
 *
 * @example
 * function Motion() MovP(1) end
 * function Out1()   DO(1, "ON") end
 * AuxTasksAdd(Motion, Out1)
 * while true do AuxTasks() end
 * @example
 */
int AuxTasksAdd(lua_State *L);

/**
 * @brief AuxTasks - Execute registered sub-functions cooperatively
 *
 * Call each function registered with AuxTasksAdd once per invocation.
 * In lu5 (single-threaded), functions are called sequentially via lua_pcall.
 *
 * @return nothing
 *
 * @example
 * AuxTasksAdd(Motion, Output1, Output2)
 * while true do
 *   AuxTasks()
 * end
 * @example
 */
int AuxTasks(lua_State *L);

#endif /* __LU5_DELTA_TASKS_BINDINGS__ */
