/**
 * delta_motion.c - Delta Robot API: Motion command bindings for lu5
 *
 * Implements MovP, MovL, MovJ as Lua-callable C functions.
 * All are hardware stubs that validate arguments, log a warning, return 0.
 * Scripts that target both the physical Delta controller and lu5 simulation
 * run without modification.
 *
 * Flow name: DeltaMotionFlow
 * Entry points: MovP, MovL, MovJ (registered in lu5_bindings.c)
 * Contracts:
 *   - Argument 1 must be a string (point name) or integer (point number).
 *   - Additional modifier arguments are accepted but ignored in stubs.
 *   - All stubs emit LU5_WARN.
 *   - Return 0 (no Lua return values).
 */

#include "delta_motion.h"

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * MovP - Point-to-Point Motion
 * Invariant: arg1 is a string (name) or integer (point index >= 1).
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int MovP(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 1) {
        luaL_error(L, "MovP: requires at least 1 argument (Point name or number)");
        return 0;
    }

    /* Accept string name or integer point number */
    if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) {
        luaL_error(L, "MovP: argument 1 must be a point name (string) or point number (integer)");
        return 0;
    }

    /* Additional modifier arguments (SPD, ACC, DEC, PASS, Offset) are ignored */
    LU5_WARN("MovP: hardware not available in lu5, motion command ignored");
    return 0;
}

/* -------------------------------------------------------------------------
 * MovL - Linear Motion
 * Invariant: same as MovP for arg1.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int MovL(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 1) {
        luaL_error(L, "MovL: requires at least 1 argument (Point name or number)");
        return 0;
    }

    if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) {
        luaL_error(L, "MovL: argument 1 must be a point name (string) or point number (integer)");
        return 0;
    }

    LU5_WARN("MovL: hardware not available in lu5, motion command ignored");
    return 0;
}

/* -------------------------------------------------------------------------
 * MovJ - Single-Axis Joint Motion
 * Invariant: joint in [1,6]; degree in [-360, 360].
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int MovJ(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "MovJ: requires at least 2 arguments (joint, degree)");
        return 0;
    }

    lua_Integer joint  = lu5_assert_integer(L, 1, "MovJ");
    lua_Number  degree = lu5_assert_number(L, 2, "MovJ");

    if (joint < 1 || joint > 6) {
        luaL_error(L, "MovJ: joint must be in range 1..6, got %d", (int)joint);
        return 0;
    }
    if (degree < -360.0 || degree > 360.0) {
        luaL_error(L, "MovJ: degree must be in range -360..360, got %f", degree);
        return 0;
    }

    /* Additional modifier arguments (SPD, ACC, DEC) are ignored in stub */
    LU5_WARN("MovJ: hardware not available in lu5, motion command ignored");
    return 0;
}
