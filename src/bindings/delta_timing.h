#ifndef __LU5_DELTA_TIMING_BINDINGS__
#define __LU5_DELTA_TIMING_BINDINGS__

#include <lua.h>

/**
 * @brief DELAY - Pause execution
 *
 * Pause program execution for the specified number of seconds.
 *
 * @param Delay_Time  Duration in seconds (number, minimum 0.001)
 * @return nothing
 *
 * @example
 * DELAY(0.5)
 * DELAY(2)
 * @example
 */
int DELAY(lua_State *L);

/**
 * @brief WAIT - Wait for a condition
 *
 * Wait for DI/DO signals or a Modbus register value to meet a condition.
 * In lu5 (no hardware), the condition is assumed met immediately.
 *
 * DI/DO form:
 *   WAIT("DI", pin, "ON" [, timeout_ms])
 *   WAIT("DO", pin, "OFF" [, timeout_ms])
 *   WAIT("DI", {1,2,3}, {"ON","OFF","ON"} [, timeout_ms])
 *
 * Modbus form:
 *   WAIT(var, address, "W"|"DW", value)
 *
 * @return nothing
 *
 * @note Hardware stub in lu5: returns immediately after logging intent.
 *
 * @example
 * WAIT("DI", 1, "ON")
 * WAIT("DO", 2, "OFF", 1000)
 * @example
 */
int WAIT(lua_State *L);

#endif /* __LU5_DELTA_TIMING_BINDINGS__ */
