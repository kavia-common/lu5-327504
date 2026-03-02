/**
 * delta_speed.c - Delta Robot API: Speed, Acceleration, and Accuracy bindings
 *
 * Implements SpdJ, AccJ, DecJ, SpdL, AccL, DecL, Accur as Lua-callable C
 * functions. Values are stored in module-level statics so motion commands can
 * later read them.
 *
 * Flow name: DeltaSpeedFlow
 * Entry points: SpdJ, AccJ, DecJ, SpdL, AccL, DecL, Accur
 *               (registered in lu5_bindings.c)
 * Contracts:
 *   - Each setter validates its range and stores the value.
 *   - Invalid range -> luaL_error with clear context.
 *   - Return 0 (setters have no Lua return value).
 *   - Accur validates against the fixed set of five mode strings.
 */

#include "delta_speed.h"

#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* -------------------------------------------------------------------------
 * Module-level state
 * Defaults match the Delta robot controller manual defaults.
 * Invariant: each value stays within its documented range after being set.
 * ------------------------------------------------------------------------- */

static lua_Number delta_spdJ = 10.0;   /* joint speed  %, default 10 */
static lua_Number delta_accJ = 10.0;   /* joint accel  %, default 10 */
static lua_Number delta_decJ = 10.0;   /* joint decel  %, default 10 */
static lua_Number delta_spdL = 100.0;  /* linear speed mm/s, default 100 */
static lua_Number delta_accL = 10.0;   /* linear accel mm/s^2, default 10 */
static lua_Number delta_decL = 10.0;   /* linear decel mm/s^2, default 10 */

/* Valid accuracy mode strings (NULL-terminated sentinel) */
static const char *valid_accur_modes[] = {
    "HIGH", "STANDARD", "MEDIUM", "ROUGH", "MAXROUGH", NULL
};

/* -------------------------------------------------------------------------
 * SpdJ - Joint Maximum Speed
 * Invariant: speed in [0.001, 100] (percentage)
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int SpdJ(lua_State *L)
{
    lua_Number speed = lu5_assert_number(L, 1, "SpdJ");
    if (speed < 0.001 || speed > 100.0) {
        luaL_error(L, "SpdJ: speed must be in range 0.001..100, got %f", speed);
        return 0;
    }
    delta_spdJ = speed;
    return 0;
}

/* -------------------------------------------------------------------------
 * AccJ - Joint Acceleration
 * Invariant: accel in [0.001, 100] (percentage)
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int AccJ(lua_State *L)
{
    lua_Number accel = lu5_assert_number(L, 1, "AccJ");
    if (accel < 0.001 || accel > 100.0) {
        luaL_error(L, "AccJ: acceleration must be in range 0.001..100, got %f", accel);
        return 0;
    }
    delta_accJ = accel;
    return 0;
}

/* -------------------------------------------------------------------------
 * DecJ - Joint Deceleration
 * Invariant: decel in [0.001, 100] (percentage)
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int DecJ(lua_State *L)
{
    lua_Number decel = lu5_assert_number(L, 1, "DecJ");
    if (decel < 0.001 || decel > 100.0) {
        luaL_error(L, "DecJ: deceleration must be in range 0.001..100, got %f", decel);
        return 0;
    }
    delta_decJ = decel;
    return 0;
}

/* -------------------------------------------------------------------------
 * SpdL - Linear Maximum Speed
 * Invariant: speed in [1, 2000] mm/s
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int SpdL(lua_State *L)
{
    lua_Number speed = lu5_assert_number(L, 1, "SpdL");
    if (speed < 1.0 || speed > 2000.0) {
        luaL_error(L, "SpdL: speed must be in range 1..2000 mm/s, got %f", speed);
        return 0;
    }
    delta_spdL = speed;
    return 0;
}

/* -------------------------------------------------------------------------
 * AccL - Linear Acceleration
 * Invariant: accel in [1, 25000] mm/s^2
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int AccL(lua_State *L)
{
    lua_Number accel = lu5_assert_number(L, 1, "AccL");
    if (accel < 1.0 || accel > 25000.0) {
        luaL_error(L, "AccL: acceleration must be in range 1..25000 mm/s^2, got %f", accel);
        return 0;
    }
    delta_accL = accel;
    return 0;
}

/* -------------------------------------------------------------------------
 * DecL - Linear Deceleration
 * Invariant: decel in [1, 25000] mm/s^2
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int DecL(lua_State *L)
{
    lua_Number decel = lu5_assert_number(L, 1, "DecL");
    if (decel < 1.0 || decel > 25000.0) {
        luaL_error(L, "DecL: deceleration must be in range 1..25000 mm/s^2, got %f", decel);
        return 0;
    }
    delta_decL = decel;
    return 0;
}

/* -------------------------------------------------------------------------
 * Accur - In-Place Accuracy Mode
 * Invariant: mode in {"HIGH","STANDARD","MEDIUM","ROUGH","MAXROUGH"}.
 *            optional arg2 must be "CART" if provided.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int Accur(lua_State *L)
{
    int argc = lua_gettop(L);
    const char *mode = lu5_assert_string(L, 1, "Accur");

    /* Validate against known modes */
    int valid = 0;
    for (int i = 0; valid_accur_modes[i] != NULL; i++) {
        if (strcmp(mode, valid_accur_modes[i]) == 0) {
            valid = 1;
            break;
        }
    }
    if (!valid) {
        luaL_error(L,
            "Accur: mode must be one of HIGH, STANDARD, MEDIUM, ROUGH, MAXROUGH; got \"%s\"",
            mode);
        return 0;
    }

    /* Optional second argument: "CART" */
    if (argc >= 2) {
        const char *qualifier = lu5_assert_string(L, 2, "Accur");
        if (strcmp(qualifier, "CART") != 0) {
            luaL_error(L, "Accur: optional argument 2 must be \"CART\", got \"%s\"", qualifier);
            return 0;
        }
    }

    /* Stub - no hardware effect; value accepted and logged */
    LU5_WARN("Accur: hardware not available in lu5, accuracy mode \"%s\" recorded but not applied", mode);
    return 0;
}
