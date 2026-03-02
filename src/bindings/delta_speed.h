#ifndef __LU5_DELTA_SPEED_BINDINGS__
#define __LU5_DELTA_SPEED_BINDINGS__

#include <lua.h>

/**
 * @brief SpdJ - Set Joint Maximum Speed
 *
 * Set the default maximum speed for joint movement.
 *
 * @param Speed  Speed as percentage (0.001-100)
 * @return nothing
 *
 * @example
 * SpdJ(50)
 * @example
 */
int SpdJ(lua_State *L);

/**
 * @brief AccJ - Set Joint Acceleration
 *
 * Set the preset acceleration during joint motion.
 *
 * @param Acceleration  Acceleration as percentage (0.001-100)
 * @return nothing
 *
 * @example
 * AccJ(30)
 * @example
 */
int AccJ(lua_State *L);

/**
 * @brief DecJ - Set Joint Deceleration
 *
 * Set the preset deceleration during joint motion.
 *
 * @param Deceleration  Deceleration as percentage (0.001-100)
 * @return nothing
 *
 * @example
 * DecJ(30)
 * @example
 */
int DecJ(lua_State *L);

/**
 * @brief SpdL - Set Linear Maximum Speed
 *
 * Set the default maximum speed for linear movement.
 *
 * @param Speed  Speed in mm/sec (1-2000)
 * @return nothing
 *
 * @example
 * SpdL(500)
 * @example
 */
int SpdL(lua_State *L);

/**
 * @brief AccL - Set Linear Acceleration
 *
 * Set the default acceleration during linear motion.
 *
 * @param Acceleration  Acceleration in mm/sec^2 (1-25000)
 * @return nothing
 *
 * @example
 * AccL(1000)
 * @example
 */
int AccL(lua_State *L);

/**
 * @brief DecL - Set Linear Deceleration
 *
 * Set the default deceleration during linear motion.
 *
 * @param Deceleration  Deceleration in mm/sec^2 (1-25000)
 * @return nothing
 *
 * @example
 * DecL(1000)
 * @example
 */
int DecL(lua_State *L);

/**
 * @brief Accur - Set In-Place Accuracy Mode
 *
 * Set the robot's in-place accuracy mode.
 * Valid modes: "HIGH", "STANDARD", "MEDIUM", "ROUGH", "MAXROUGH"
 *
 * @param Mode     Accuracy mode string
 * @param ["CART"] Optional Cartesian qualifier
 * @return nothing
 *
 * @example
 * Accur("HIGH")
 * Accur("MEDIUM", "CART")
 * @example
 */
int Accur(lua_State *L);

#endif /* __LU5_DELTA_SPEED_BINDINGS__ */
