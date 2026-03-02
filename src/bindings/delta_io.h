#ifndef __LU5_DELTA_IO_BINDINGS__
#define __LU5_DELTA_IO_BINDINGS__

#include <lua.h>

/**
 * @brief DI - Digital Input
 *
 * Read the pin status of a standard digital input.
 *
 * @param Pin_Index  Pin number (1-24) or pin name string (up to 16 chars)
 * @param [Length]   Optional: number of pins to read continuously (1-24)
 * @return  "ON"/"OFF" (single-pin form) or integer bitmask (multi-pin form)
 *
 * @note Hardware stub in lu5: always returns "OFF" or 0.
 *
 * @example
 * status = DI(1)
 * mask   = DI(1, 4)
 * @example
 */
int DI(lua_State *L);

/**
 * @brief DO - Digital Output
 *
 * Set the pin status of a standard digital output.
 * Overloads:
 *   DO(pin, status)
 *   DO(pin, status, delay_time)
 *   DO(pin, length, status_num)
 *   DO(pin, length, status_num, delay_time)
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * DO(1, "ON")
 * DO(1, "OFF", 0.5)
 * @example
 */
int DO(lua_State *L);

/**
 * @brief ExtDI - External Board Digital Input
 *
 * Read the status of a digital input pin on an external board.
 *
 * @param Address_Index  Station number of the external board
 * @param Pin_Index      Pin number on the external board
 * @return "ON" or "OFF"
 *
 * @note Hardware stub in lu5: always returns "OFF".
 *
 * @example
 * status = ExtDI(4, 1)
 * @example
 */
int ExtDI(lua_State *L);

/**
 * @brief ExtDO - External Board Digital Output
 *
 * Set the status of a digital output pin on an external board.
 * Overloads:
 *   ExtDO(address, pin, status)
 *   ExtDO(address, pin, status, delay_time)
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * ExtDO(4, 1, "ON")
 * @example
 */
int ExtDO(lua_State *L);

#endif /* __LU5_DELTA_IO_BINDINGS__ */
