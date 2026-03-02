/**
 * delta_timing.c - Delta Robot API: Timing and Flow Control bindings for lu5
 *
 * Implements DELAY and WAIT as Lua-callable C functions.
 *
 * DELAY performs an actual sleep so scripts that rely on timing still work.
 * WAIT is a hardware stub that logs the condition and returns immediately.
 *
 * Flow name: DeltaTimingFlow
 * Entry points: DELAY, WAIT (registered in lu5_bindings.c)
 * Contracts:
 *   - DELAY: delay >= 0.001 seconds; uses nanosleep (POSIX) or Sleep (Win32).
 *   - WAIT: validates first argument type; logs intent; always proceeds.
 *   - Both return 0 (no Lua return values).
 */

#include "delta_timing.h"

#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* Platform-appropriate sleep implementation */
#ifndef LU5_WASM
#  ifdef _WIN32
#    include <windows.h>
     static void delta_sleep_seconds(double secs) {
         DWORD ms = (DWORD)(secs * 1000.0);
         if (ms < 1) ms = 1;
         Sleep(ms);
     }
#  else
#    include <time.h>
     static void delta_sleep_seconds(double secs) {
         struct timespec ts;
         ts.tv_sec  = (time_t)secs;
         ts.tv_nsec = (long)((secs - (double)ts.tv_sec) * 1.0e9);
         nanosleep(&ts, NULL);
     }
#  endif
#else
     static void delta_sleep_seconds(double secs) {
         (void)secs;
         LU5_WARN("DELAY: sleep not available in lu5-wasm");
     }
#endif

/* -------------------------------------------------------------------------
 * DELAY - Pause execution
 * Invariant: delay >= 0.001 seconds.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int DELAY(lua_State *L)
{
    lua_Number delay = lu5_assert_number(L, 1, "DELAY");
    if (delay < 0.001) {
        luaL_error(L, "DELAY: delay_time must be >= 0.001 seconds, got %f", delay);
        return 0;
    }
    delta_sleep_seconds((double)delay);
    return 0;
}

/* -------------------------------------------------------------------------
 * WAIT - Wait for a condition
 * In lu5, hardware conditions are assumed to be met immediately.
 * Invariant: at least 3 arguments required for DI/DO forms.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int WAIT(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 1) {
        luaL_error(L, "WAIT: requires at least 1 argument");
        return 0;
    }

    /* Detect form by inspecting first argument */
    if (lua_isstring(L, 1)) {
        const char *io_type = lua_tostring(L, 1);
        if (strcmp(io_type, "DI") == 0 || strcmp(io_type, "DO") == 0) {
            /* DI/DO form: WAIT(io_type, index_or_table, status [, timeout]) */
            if (argc < 3) {
                luaL_error(L, "WAIT: DI/DO form requires at least 3 arguments");
                return 0;
            }
            LU5_WARN(
                "WAIT: no hardware in lu5; WAIT(%s, ...) condition assumed met immediately",
                io_type);
        } else {
            /* Modbus form: WAIT(var, address, "W"|"DW", value) */
            LU5_WARN(
                "WAIT: no hardware in lu5; Modbus WAIT condition assumed met immediately");
        }
    } else {
        LU5_WARN("WAIT: no hardware in lu5; condition assumed met immediately");
    }

    return 0;
}
