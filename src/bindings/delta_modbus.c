/**
 * delta_modbus.c - Delta Robot API: Modbus register read/write bindings
 *
 * Implements ReadModbus and WriteModbus as Lua-callable C functions.
 * Both are hardware stubs in lu5: they validate arguments, log warnings,
 * and return safe default values (0 for reads, nothing for writes).
 *
 * Flow name: DeltaModbusFlow
 * Entry points: ReadModbus, WriteModbus (registered in lu5_bindings.c)
 * Contracts:
 *   - Size must be "W" (16-bit) or "DW" (32-bit).
 *   - RegAddress is accepted as-is (hardware-specific range).
 *   - ReadModbus returns 1 value (integer 0 stub).
 *   - WriteModbus returns 0 values.
 */

#include "delta_modbus.h"

#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * delta_validate_modbus_size - validate "W" or "DW" size argument
 * Calls luaL_error on failure (does not return).
 * ------------------------------------------------------------------------- */
static void delta_validate_modbus_size(lua_State *L, int arg_idx, const char *fname)
{
    const char *size = lu5_assert_string(L, arg_idx, fname);
    if (strcmp(size, "W") != 0 && strcmp(size, "DW") != 0) {
        luaL_error(L,
            "%s: size must be \"W\" (16-bit) or \"DW\" (32-bit), got \"%s\"",
            fname, size);
    }
}

/* -------------------------------------------------------------------------
 * ReadModbus - Read a Modbus register
 * Invariant: size in {"W","DW"}; returns 0 (stub).
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int ReadModbus(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "ReadModbus: requires 2 arguments (RegAddress, Size)");
        return 0;
    }

    lua_Number reg_addr = lu5_assert_number(L, 1, "ReadModbus");
    delta_validate_modbus_size(L, 2, "ReadModbus");
    (void)reg_addr;

    LU5_WARN("ReadModbus: hardware not available in lu5, returning 0");
    lua_pushinteger(L, 0);
    return 1;
}

/* -------------------------------------------------------------------------
 * WriteModbus - Write a Modbus register
 * Invariant: size in {"W","DW"}.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int WriteModbus(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 3) {
        luaL_error(L, "WriteModbus: requires 3 arguments (RegAddress, Size, RegValue)");
        return 0;
    }

    lua_Number reg_addr  = lu5_assert_number(L, 1, "WriteModbus");
    delta_validate_modbus_size(L, 2, "WriteModbus");
    lua_Number reg_value = lu5_assert_number(L, 3, "WriteModbus");
    (void)reg_addr;
    (void)reg_value;

    LU5_WARN("WriteModbus: hardware not available in lu5, write ignored");
    return 0;
}
