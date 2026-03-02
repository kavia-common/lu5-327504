/**
 * delta_io.c - Delta Robot API: Digital I/O bindings for lu5
 *
 * Implements DI, DO, ExtDI, ExtDO as Lua-callable C functions.
 * Because lu5 has no physical hardware, all functions are stubs that log a
 * warning and return safe default values.
 *
 * Flow name: DeltaIOFlow
 * Entry points: DI, DO, ExtDI, ExtDO (registered in lu5_bindings.c)
 * Contracts:
 *   - Inputs validated (type + range); invalid inputs call luaL_error.
 *   - All stubs emit LU5_WARN so the caller knows hardware is not available.
 *   - Return counts match lua_push* call counts exactly.
 */

#include "delta_io.h"

#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * DI - Digital Input
 * Invariant: pin in [1,24] (when integer) or string name <= 16 chars.
 * Two forms:
 *   DI(pin)         -> push "OFF" (stub)
 *   DI(pin, length) -> push 0    (stub bitmask)
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int DI(lua_State *L)
{
    int argc = lua_gettop(L);

    /* Validate pin argument: accepts integer or string */
    if (!lua_isnumber(L, 1) && !lua_isstring(L, 1)) {
        luaL_error(L, "DI: argument 1 must be a pin number (1-24) or pin name string");
        return 0;
    }

    if (argc == 1) {
        /* Single-pin form: return "ON" or "OFF" */
        LU5_WARN("DI: hardware not available in lu5, returning \"OFF\"");
        lua_pushstring(L, "OFF");
        return 1;
    }

    /* Multi-pin form: validate length */
    lua_Integer length = lu5_assert_integer(L, 2, "DI");
    if (length < 1 || length > 24) {
        luaL_error(L, "DI: length must be in range 1..24, got %d", (int)length);
        return 0;
    }

    LU5_WARN("DI: hardware not available in lu5, returning 0");
    lua_pushinteger(L, 0);
    return 1;
}

/* -------------------------------------------------------------------------
 * DO - Digital Output
 * Invariant: pin in [1,12]; status in {"ON","OFF"}; delay >= 0.
 * Forms:
 *   DO(pin, status)
 *   DO(pin, status, delay_time)
 *   DO(pin, length, status_num)
 *   DO(pin, length, status_num, delay_time)
 * Discrimination: arg2 is string -> single-pin; arg2 is number -> multi.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int DO(lua_State *L)
{
    int argc = lua_gettop(L);

    if (argc < 2) {
        luaL_error(L, "DO: requires at least 2 arguments (pin, status)");
        return 0;
    }

    /* Validate pin: integer or string */
    if (!lua_isnumber(L, 1) && !lua_isstring(L, 1)) {
        luaL_error(L, "DO: argument 1 must be a pin number (1-12) or pin name string");
        return 0;
    }

    if (lua_isstring(L, 2) && !lua_isnumber(L, 2)) {
        /* Single-pin form: DO(pin, status [, delay]) */
        const char *status = lu5_assert_string(L, 2, "DO");
        if (strcmp(status, "ON") != 0 && strcmp(status, "OFF") != 0) {
            luaL_error(L, "DO: status must be \"ON\" or \"OFF\", got \"%s\"", status);
            return 0;
        }
        if (argc >= 3) {
            lua_Number delay = lu5_assert_number(L, 3, "DO");
            if (delay < 0.0) {
                luaL_error(L, "DO: delay_time must be >= 0, got %f", delay);
                return 0;
            }
            (void)delay; /* stub - no hardware */
        }
    } else {
        /* Multi-pin form: DO(pin, length, status_num [, delay]) */
        lua_Integer length = lu5_assert_integer(L, 2, "DO");
        if (length < 1 || length > 12) {
            luaL_error(L, "DO: length must be in range 1..12, got %d", (int)length);
            return 0;
        }
        if (argc < 3) {
            luaL_error(L, "DO: multi-pin form requires status_num as argument 3");
            return 0;
        }
        lua_Integer status_num = lu5_assert_integer(L, 3, "DO");
        (void)status_num; /* stub */
        if (argc >= 4) {
            lua_Number delay = lu5_assert_number(L, 4, "DO");
            if (delay < 0.0) {
                luaL_error(L, "DO: delay_time must be >= 0, got %f", delay);
                return 0;
            }
            (void)delay; /* stub */
        }
    }

    LU5_WARN("DO: hardware not available in lu5");
    return 0;
}

/* -------------------------------------------------------------------------
 * ExtDI - External Board Digital Input
 * Invariant: address and pin are positive integers.
 * Returns "ON" or "OFF" (stub: always "OFF").
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int ExtDI(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "ExtDI: requires 2 arguments (address_index, pin_index)");
        return 0;
    }

    lua_Integer address = lu5_assert_integer(L, 1, "ExtDI");
    lua_Integer pin     = lu5_assert_integer(L, 2, "ExtDI");

    if (address <= 0) {
        luaL_error(L, "ExtDI: address_index must be positive, got %d", (int)address);
        return 0;
    }
    if (pin <= 0) {
        luaL_error(L, "ExtDI: pin_index must be positive, got %d", (int)pin);
        return 0;
    }

    LU5_WARN("ExtDI: hardware not available in lu5, returning \"OFF\"");
    lua_pushstring(L, "OFF");
    return 1;
}

/* -------------------------------------------------------------------------
 * ExtDO - External Board Digital Output
 * Invariant: address and pin are positive integers; status in {"ON","OFF"}.
 * Forms:
 *   ExtDO(address, pin, status)
 *   ExtDO(address, pin, status, delay_time)
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int ExtDO(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 3) {
        luaL_error(L, "ExtDO: requires at least 3 arguments (address, pin, status)");
        return 0;
    }

    lua_Integer address = lu5_assert_integer(L, 1, "ExtDO");
    lua_Integer pin     = lu5_assert_integer(L, 2, "ExtDO");
    const char *status  = lu5_assert_string(L, 3, "ExtDO");

    if (address <= 0) {
        luaL_error(L, "ExtDO: address_index must be positive, got %d", (int)address);
        return 0;
    }
    if (pin <= 0) {
        luaL_error(L, "ExtDO: pin_index must be positive, got %d", (int)pin);
        return 0;
    }
    if (strcmp(status, "ON") != 0 && strcmp(status, "OFF") != 0) {
        luaL_error(L, "ExtDO: status must be \"ON\" or \"OFF\", got \"%s\"", status);
        return 0;
    }
    if (argc >= 4) {
        lua_Number delay = lu5_assert_number(L, 4, "ExtDO");
        if (delay < 0.0) {
            luaL_error(L, "ExtDO: delay_time must be >= 0, got %f", delay);
            return 0;
        }
        (void)delay; /* stub */
    }

    LU5_WARN("ExtDO: hardware not available in lu5");
    return 0;
}
