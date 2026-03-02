/**
 * delta_tasks.c - Delta Robot API: Multi-task cooperative execution bindings
 *
 * Implements AuxTasksAdd and AuxTasks as Lua-callable C functions.
 *
 * In the Delta controller, AuxTasks runs each sub-function for 15 ms in a
 * round-robin slice. In lu5 (single-threaded), each registered function is
 * called once per AuxTasks() invocation via lua_pcall, simulating cooperative
 * multitasking semantics without threads.
 *
 * Flow name: DeltaTasksFlow
 * Entry points: AuxTasksAdd, AuxTasks (registered in lu5_bindings.c)
 * Contracts:
 *   - AuxTasksAdd: 1-10 Lua function arguments. Resets previous refs.
 *   - AuxTasks: calls each stored function once; pcall errors are logged.
 *   - State: module-level arrays (delta_task_refs, delta_task_count).
 *   - AuxTasksAdd always clears stale refs before storing new ones.
 */

#include "delta_tasks.h"

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * Module-level task state
 * Invariant: delta_task_count in [0, DELTA_MAX_AUX_TASKS].
 *            delta_task_refs[i] == LUA_NOREF means slot is unused.
 * ------------------------------------------------------------------------- */

static int delta_task_refs[DELTA_MAX_AUX_TASKS];
static int delta_task_count = 0;

/* -------------------------------------------------------------------------
 * AuxTasksAdd - Register sub-functions
 * Clears all existing refs before storing new ones to prevent stale state.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int AuxTasksAdd(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 1) {
        luaL_error(L, "AuxTasksAdd: requires at least 1 function argument");
        return 0;
    }
    if (argc > DELTA_MAX_AUX_TASKS) {
        luaL_error(L,
            "AuxTasksAdd: maximum %d sub-functions allowed, got %d",
            DELTA_MAX_AUX_TASKS, argc);
        return 0;
    }

    /* Validate all arguments are functions before storing anything */
    for (int i = 1; i <= argc; i++) {
        if (!lua_isfunction(L, i)) {
            luaL_error(L,
                "AuxTasksAdd: argument %d must be a function, got %s",
                i, lua_typename(L, lua_type(L, i)));
            return 0;
        }
    }

    /* Release all previously stored references (reset invariant) */
    for (int i = 0; i < delta_task_count; i++) {
        if (delta_task_refs[i] != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, delta_task_refs[i]);
            delta_task_refs[i] = LUA_NOREF;
        }
    }
    delta_task_count = 0;

    /* Store new references */
    for (int i = 1; i <= argc; i++) {
        lua_pushvalue(L, i);
        delta_task_refs[delta_task_count++] = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * AuxTasks - Execute each registered sub-function once
 * Errors from sub-functions are logged but do not stop execution.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int AuxTasks(lua_State *L)
{
    if (delta_task_count == 0) {
        LU5_WARN("AuxTasks: no tasks registered; call AuxTasksAdd first");
        return 0;
    }

    for (int i = 0; i < delta_task_count; i++) {
        if (delta_task_refs[i] == LUA_NOREF) continue;

        /* Push the function from the registry */
        lua_rawgeti(L, LUA_REGISTRYINDEX, delta_task_refs[i]);

        /* Call with 0 arguments, 0 results; use pcall for error safety */
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            LU5_WARN("AuxTasks: sub-function %d error: %s", i + 1,
                     err ? err : "(unknown)");
            lua_pop(L, 1); /* pop error message */
        }
    }

    return 0;
}
