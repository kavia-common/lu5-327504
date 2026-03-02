/**
 * delta_point.c - Delta Robot API: Point Management bindings for lu5
 *
 * Implements SetGlobalPoint and ReadPoint as Lua-callable C functions.
 * Points are stored in the Lua global table "_delta_points" keyed by their
 * integer index. Each point entry is itself a Lua table with named fields.
 *
 * Flow name: DeltaPointFlow
 * Entry points: SetGlobalPoint, ReadPoint (registered in lu5_bindings.c)
 * Contracts:
 *   - SetGlobalPoint: point in [1,1000]; name should begin with "GL_"; X,Y,Z required.
 *   - ReadPoint: item must be one of the documented field names.
 *   - "_delta_points" table is created lazily on first SetGlobalPoint call.
 *   - ReadPoint returns nil (1 value) for unknown points or missing fields.
 *   - Both functions validate types; errors via luaL_error.
 */

#include "delta_point.h"

#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "../lu5_types.h"
#include "../lu5_logger.h"

/* Name of the Lua global registry table for Delta points */
#define DELTA_POINTS_TABLE "_delta_points"

/* -------------------------------------------------------------------------
 * delta_ensure_points_table - create or push the registry table onto stack
 * After return: the table is at the top of the stack.
 * Invariant: "_delta_points" always contains a valid Lua table.
 * ------------------------------------------------------------------------- */
static void delta_ensure_points_table(lua_State *L)
{
    lua_getglobal(L, DELTA_POINTS_TABLE);
    if (!lua_istable(L, -1)) {
        /* Table doesn't exist yet - create it */
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);               /* duplicate for setglobal */
        lua_setglobal(L, DELTA_POINTS_TABLE);
    }
    /* Table is now at top of stack */
}

/* -------------------------------------------------------------------------
 * SetGlobalPoint - Store a named global point
 * Minimum required: point (int), name (string), X, Y, Z (numbers).
 * Optional axis fields stored as they appear; JRC is stored as a sub-table.
 * ------------------------------------------------------------------------- */

/* PUBLIC_INTERFACE */
int SetGlobalPoint(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 5) {
        luaL_error(L,
            "SetGlobalPoint: requires at least 5 arguments "
            "(point, name, X, Y, Z)");
        return 0;
    }

    lua_Integer point = lu5_assert_integer(L, 1, "SetGlobalPoint");
    if (point < 1 || point > 1000) {
        luaL_error(L, "SetGlobalPoint: point number must be 1..1000, got %d", (int)point);
        return 0;
    }

    const char *name = lu5_assert_string(L, 2, "SetGlobalPoint");
    if (strncmp(name, "GL_", 3) != 0) {
        LU5_WARN("SetGlobalPoint: point name \"%s\" does not begin with \"GL_\" "
                 "(controller may reject it)", name);
    }

    lua_Number x = lu5_assert_number(L, 3, "SetGlobalPoint");
    lua_Number y = lu5_assert_number(L, 4, "SetGlobalPoint");
    lua_Number z = lu5_assert_number(L, 5, "SetGlobalPoint");

    /* Ensure registry table exists and is on stack */
    delta_ensure_points_table(L);
    int table_idx = lua_gettop(L);

    /* Build the point entry table */
    lua_newtable(L);

    lua_pushstring(L, name);  lua_setfield(L, -2, "name");
    lua_pushnumber(L, x);     lua_setfield(L, -2, "X");
    lua_pushnumber(L, y);     lua_setfield(L, -2, "Y");
    lua_pushnumber(L, z);     lua_setfield(L, -2, "Z");

    /*
     * Store optional axis-specific fields by argument position.
     * arg 6 onward depends on robot type (4-,5-,6-axis) or JRC table.
     * We map positional numeric args to axis field names; JRC table is detected
     * and stored separately.
     */
    const char *optional_fields[] = {
        "RZ", "RY", "RX", "elbow", "shoulder", "flip", "UF", "TF", "H", "E", "S", "F", NULL
    };
    int optional_idx = 0;

    for (int i = 6; i <= argc; i++) {
        if (lua_istable(L, i)) {
            /* JRC table: copy as sub-table */
            lua_newtable(L);
            int jrc_len = (int)luaL_len(L, i);
            for (int j = 1; j <= jrc_len; j++) {
                lua_rawgeti(L, i, j);
                lua_rawseti(L, -2, j);
            }
            lua_setfield(L, -2, "JRC");
            break; /* JRC is last meaningful argument */
        } else if (lua_isnumber(L, i) && optional_fields[optional_idx] != NULL) {
            lua_Number val = lua_tonumber(L, i);
            lua_pushnumber(L, val);
            lua_setfield(L, -2, optional_fields[optional_idx]);
            optional_idx++;
        } else if (lua_isstring(L, i) && optional_fields[optional_idx] != NULL) {
            const char *s = lua_tostring(L, i);
            lua_pushstring(L, s);
            lua_setfield(L, -2, optional_fields[optional_idx]);
            optional_idx++;
        }
    }

    /* Store entry at point index in registry table */
    lua_rawseti(L, table_idx, (lua_Integer)point);

    /* Pop registry table */
    lua_pop(L, 1);

    return 0;
}

/* -------------------------------------------------------------------------
 * ReadPoint - Read one field from a stored point
 * Returns nil if point or field is not found.
 * ------------------------------------------------------------------------- */

/* Valid readable items */
static const char *valid_point_items[] = {
    "X","Y","Z","RX","RY","RZ","UF","TF","H","E","S","F","JRC","name", NULL
};

/* PUBLIC_INTERFACE */
int ReadPoint(lua_State *L)
{
    int argc = lua_gettop(L);
    if (argc < 2) {
        luaL_error(L, "ReadPoint: requires 2 arguments (point, item)");
        return 0;
    }

    const char *item = lu5_assert_string(L, 2, "ReadPoint");

    /* Validate item name */
    int valid_item = 0;
    for (int i = 0; valid_point_items[i] != NULL; i++) {
        if (strcmp(item, valid_point_items[i]) == 0) {
            valid_item = 1;
            break;
        }
    }
    if (!valid_item) {
        luaL_error(L,
            "ReadPoint: unknown item \"%s\". Valid items: "
            "X,Y,Z,RX,RY,RZ,UF,TF,H,E,S,F,JRC",
            item);
        return 0;
    }

    /* Look up the registry table */
    lua_getglobal(L, DELTA_POINTS_TABLE);
    if (!lua_istable(L, -1)) {
        /* No points stored yet */
        lua_pop(L, 1);
        lua_pushnil(L);
        return 1;
    }

    /* Resolve point: integer index or string name */
    if (lua_isnumber(L, 1)) {
        lua_Integer point = lua_tointeger(L, 1);
        lua_rawgeti(L, -1, (lua_Integer)point);
    } else if (lua_isstring(L, 1)) {
        /* Search by name - iterate the table */
        const char *search_name = lua_tostring(L, 1);
        lua_pushnil(L); /* first key for next() */
        int found = 0;
        while (lua_next(L, -2) != 0) {
            /* value at -1, key at -2 */
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "name");
                const char *entry_name = lua_tostring(L, -1);
                lua_pop(L, 1); /* pop name field */
                if (entry_name && strcmp(entry_name, search_name) == 0) {
                    found = 1;
                    break;
                }
            }
            lua_pop(L, 1); /* pop value, keep key for next() */
        }
        if (!found) {
            lua_pop(L, 1); /* pop registry table */
            lua_pushnil(L);
            return 1;
        }
        /* Entry table is at top; registry table is below */
    } else {
        luaL_error(L, "ReadPoint: argument 1 must be a point number or name string");
        return 0;
    }

    if (!lua_istable(L, -1)) {
        /* Point not found at that index */
        lua_pop(L, 2); /* pop result + registry table */
        lua_pushnil(L);
        return 1;
    }

    /* Fetch the requested field from the point entry */
    lua_getfield(L, -1, item);

    /* Stack now: ... registry_table point_entry field_value */
    /* Move field_value to position before registry_table then clean up */
    lua_replace(L, -3);  /* replace registry table slot with field value */
    lua_pop(L, 1);       /* pop remaining point entry table */

    return 1;
}
