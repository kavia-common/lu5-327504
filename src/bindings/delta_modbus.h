#ifndef __LU5_DELTA_MODBUS_BINDINGS__
#define __LU5_DELTA_MODBUS_BINDINGS__

#include <lua.h>

/**
 * @brief ReadModbus - Read a Modbus register value
 *
 * Read the value from a memory/register address.
 *
 * @param RegAddress  Register address (number)
 * @param Size        Data size: "W" (16-bit) or "DW" (32-bit)
 * @return  Register value (integer), or 0 (stub in lu5)
 *
 * @note Hardware stub in lu5: returns 0 with a warning.
 *
 * @example
 * val = ReadModbus(0x1000, "W")
 * val = ReadModbus(0x3000, "DW")
 * @example
 */
int ReadModbus(lua_State *L);

/**
 * @brief WriteModbus - Write a value to a Modbus register
 *
 * Write a value to a register address.
 *
 * @param RegAddress  Register address (number)
 * @param Size        Data size: "W" (16-bit) or "DW" (32-bit)
 * @param RegValue    Value to write (number)
 * @return nothing
 *
 * @note Hardware stub in lu5.
 *
 * @example
 * WriteModbus(0x1000, "W", 42)
 * @example
 */
int WriteModbus(lua_State *L);

#endif /* __LU5_DELTA_MODBUS_BINDINGS__ */
