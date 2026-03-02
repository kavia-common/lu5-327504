#ifndef __LU5_DELTA_MOTION_BINDINGS__
#define __LU5_DELTA_MOTION_BINDINGS__

#include <lua.h>

/**
 * @brief MovP - Point-to-Point Motion
 *
 * Move the robot to the specified point using point-to-point motion.
 *
 * @param Point  Target point name (string) or point number (integer)
 * @param [...]  Optional speed/acceleration/pass modifiers (ignored in lu5 stub)
 * @return nothing
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * MovP("P1")
 * MovP(1)
 * @example
 */
int MovP(lua_State *L);

/**
 * @brief MovL - Linear Motion
 *
 * Move the robot to the specified point in a straight line.
 *
 * @param Point  Target point name (string) or point number (integer)
 * @param [...]  Optional offset and speed/acceleration/pass modifiers
 * @return nothing
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * MovL("P2")
 * MovL(2)
 * @example
 */
int MovL(lua_State *L);

/**
 * @brief MovJ - Single-Axis Joint Motion
 *
 * Rotate a single axis of the robot to the specified angle.
 *
 * @param Joint   Axis number (integer, 1-6)
 * @param Degree  Target angle in degrees (number, -360 to 360)
 * @param [...]   Optional speed/acceleration modifiers
 * @return nothing
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * MovJ(1, 90)
 * MovJ(3, -45.5)
 * @example
 */
int MovJ(lua_State *L);

#endif /* __LU5_DELTA_MOTION_BINDINGS__ */
