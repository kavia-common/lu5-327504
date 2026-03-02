#ifndef __LU5_DELTA_POINT_BINDINGS__
#define __LU5_DELTA_POINT_BINDINGS__

#include <lua.h>

/**
 * @brief SetGlobalPoint - Store a named global point
 *
 * Modify global point information. Points are stored in the Lua global
 * registry table "_delta_points" at the given point index.
 *
 * Minimum form (lu5 sim):
 *   SetGlobalPoint(point, name, X, Y, Z)
 *
 * Full six-axis form:
 *   SetGlobalPoint(point, name, X, Y, Z, RX, RY, RZ, elbow, shoulder, flip, UF, TF, JRC)
 *
 * @param point  Point number (integer, 1-1000)
 * @param name   Point name string; should begin with "GL_"
 * @param X      X coordinate in mm
 * @param Y      Y coordinate in mm
 * @param Z      Z coordinate in mm
 * @return nothing
 *
 * @example
 * SetGlobalPoint(1, "GL_P1", 200, 100, -50, 0, 0, 0, 0, {0,0,0,1,0,0,0,4})
 * @example
 */
int SetGlobalPoint(lua_State *L);

/**
 * @brief ReadPoint - Read a single field from a stored point
 *
 * Read one item from a previously stored global point.
 *
 * @param Point  Point number (integer) or point name (string)
 * @param Item   Field name: "X","Y","Z","RX","RY","RZ","UF","TF","H","E","S","F","JRC"
 * @return  Field value (number), or nil if not found
 *
 * @example
 * x = ReadPoint(1, "X")
 * @example
 */
int ReadPoint(lua_State *L);

#endif /* __LU5_DELTA_POINT_BINDINGS__ */
