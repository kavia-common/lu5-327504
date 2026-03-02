#include "deltaapi2.h"

/*
DeltaAPI-2 bindings for lu5.

Design goals (Reusable Flow, Maintainable, Debuggable, Non-Patchy):
- One canonical entrypoints for parsing common DeltaAPI-2 types:
  - Pin index (number or string)
  - ON/OFF word
  - W/DW size
  - timeouts (ms / seconds)
- One canonical backend adapter (currently stubbed) so real hardware integration
  can be added without rewriting the Lua-visible API.

REQ traceability:
- REQ: DELTAAPI2-REQ-001 DI
- REQ: DELTAAPI2-REQ-002 DO
- REQ: DELTAAPI2-REQ-003 ExtDI/ExtDO
- REQ: DELTAAPI2-REQ-004 WAIT (IO + Modbus forms)
- REQ: DELTAAPI2-REQ-005 DELAY
- REQ: DELTAAPI2-REQ-006 ReadModbus/WriteModbus (W/DW + DW even address)
- REQ: DELTAAPI2-REQ-007 SocketClass (Send/Receive/Close)
- REQ: DELTAAPI2-REQ-008 SocketServer + CheckAllStatus + CheckStatus + SocketVersion + SocketErrorCode
- REQ: DELTAAPI2-REQ-009 AuxTasksAdd/AuxTasks (15ms time slice)
- REQ: DELTAAPI2-REQ-010 Motion commands + globals (stubbed but API-present)
*/

#include <lauxlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifdef __unix__
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

// -------------------------
// Shared helpers (core flow)
// -------------------------

typedef enum {
	LU5_DELTA_SIZE_W = 0,
	LU5_DELTA_SIZE_DW = 1,
} lu5_delta_size;

typedef struct {
	bool is_name;
	int pin_number;          // valid if !is_name
	char pin_name[17];       // valid if is_name; always nul-terminated
} lu5_delta_pin_index;

static void lu5_delta_sleep_ms(int ms)
{
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	// usleep in microseconds
	usleep((useconds_t)(ms * 1000));
#endif
}

static lua_Number lu5_delta_now_monotonic_seconds(void)
{
#ifdef _WIN32
	// Fallback to clock() resolution on Windows builds
	return (lua_Number)clock() / (lua_Number)CLOCKS_PER_SEC;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (lua_Number)ts.tv_sec + (lua_Number)ts.tv_nsec / 1e9;
#endif
}

static void lu5_delta_arg_error(lua_State *L, const char *fname, int arg, const char *msg)
{
	luaL_error(L, "Function '%s': invalid argument %d: %s", fname, arg, msg);
}

// parse "ON"/"OFF" (case-insensitive), optionally accept boolean.
// Returns true for ON, false for OFF.
static bool lu5_delta_parse_on_off(lua_State *L, int index, const char *fname)
{
	if (lua_isboolean(L, index)) {
		return lua_toboolean(L, index) ? true : false;
	}

	if (!lua_isstring(L, index)) {
		lu5_delta_arg_error(L, fname, index, "expected 'ON'/'OFF' (string) or boolean");
		return false;
	}

	const char *s = lua_tostring(L, index);
	if (s == NULL) {
		lu5_delta_arg_error(L, fname, index, "expected 'ON'/'OFF' string");
		return false;
	}

	if (strcasecmp(s, "ON") == 0) return true;
	if (strcasecmp(s, "OFF") == 0) return false;

	lu5_delta_arg_error(L, fname, index, "expected 'ON' or 'OFF'");
	return false;
}

static void lu5_delta_push_on_off(lua_State *L, bool on)
{
	lua_pushstring(L, on ? "ON" : "OFF");
}

static lu5_delta_pin_index lu5_delta_parse_pin_index(lua_State *L, int index, const char *fname, int min_pin, int max_pin, bool allow_string_name)
{
	lu5_delta_pin_index out;
	memset(&out, 0, sizeof(out));

	if (lua_isinteger(L, index)) {
		lua_Integer pin = lua_tointeger(L, index);
		if (pin < min_pin || pin > max_pin) {
			char buf[128];
			snprintf(buf, sizeof(buf), "pin out of range (%d..%d)", min_pin, max_pin);
			lu5_delta_arg_error(L, fname, index, buf);
		}
		out.is_name = false;
		out.pin_number = (int)pin;
		return out;
	}

	if (allow_string_name && lua_isstring(L, index)) {
		const char *name = lua_tostring(L, index);
		if (!name) {
			lu5_delta_arg_error(L, fname, index, "expected pin name string");
		}
		size_t n = strlen(name);
		if (n == 0 || n > 16) {
			lu5_delta_arg_error(L, fname, index, "pin name must be 1..16 characters");
		}
		out.is_name = true;
		snprintf(out.pin_name, sizeof(out.pin_name), "%s", name);
		return out;
	}

	lu5_delta_arg_error(L, fname, index, allow_string_name ? "expected pin index (integer) or pin name (string)" : "expected pin index (integer)");
	return out;
}

static lu5_delta_size lu5_delta_parse_size(lua_State *L, int index, const char *fname)
{
	if (!lua_isstring(L, index)) {
		lu5_delta_arg_error(L, fname, index, "expected size string 'W' or 'DW'");
		return LU5_DELTA_SIZE_W;
	}
	const char *s = lua_tostring(L, index);
	if (!s) {
		lu5_delta_arg_error(L, fname, index, "expected size string 'W' or 'DW'");
		return LU5_DELTA_SIZE_W;
	}
	if (strcmp(s, "W") == 0) return LU5_DELTA_SIZE_W;
	if (strcmp(s, "DW") == 0) return LU5_DELTA_SIZE_DW;
	lu5_delta_arg_error(L, fname, index, "expected size 'W' or 'DW'");
	return LU5_DELTA_SIZE_W;
}

static void lu5_delta_validate_modbus_value(lua_State *L, const char *fname, lu5_delta_size size, lua_Integer value, int arg_index)
{
	if (size == LU5_DELTA_SIZE_W) {
		if (value < -32767 || value > 32767) {
			lu5_delta_arg_error(L, fname, arg_index, "W value out of range (-32767..32767)");
		}
	} else {
		if (value < (lua_Integer)INT32_MIN || value > (lua_Integer)INT32_MAX) {
			lu5_delta_arg_error(L, fname, arg_index, "DW value out of range (-2147483648..2147483647)");
		}
	}
}

static void lu5_delta_validate_modbus_addr(lua_State *L, const char *fname, lua_Integer addr, lu5_delta_size size, int arg_index)
{
	// DeltaAPI-2: DW must be even
	if (size == LU5_DELTA_SIZE_DW && (addr % 2) != 0) {
		lu5_delta_arg_error(L, fname, arg_index, "DW requires an even RegAddress (0,2,4,...)");
	}
}

// -------------------------
// Backend adapter (stub)
// -------------------------

typedef struct {
	bool simulation_mode; // future extension point; for now always true
} lu5_delta_backend;

static lu5_delta_backend g_backend = {
	.simulation_mode = true,
};

static bool backend_di_read(const lu5_delta_pin_index *pin, int length, lua_Integer *packed_out, bool *single_on_out)
{
	(void)pin;
	(void)length;
	// deterministic stub: always OFF / 0
	if (packed_out) *packed_out = 0;
	if (single_on_out) *single_on_out = false;
	return true;
}

static bool backend_do_write(const lu5_delta_pin_index *pin, int length, bool has_word_status, bool word_status_on, lua_Integer packed_status, lua_Number delay_seconds)
{
	(void)pin;
	(void)length;
	(void)has_word_status;
	(void)word_status_on;
	(void)packed_status;

	// Stub semantics:
	// - If delay_seconds > 0: block for delay and then "reverse signal" (no-op in stub).
	// This keeps sketch behavior deterministic without needing hardware.
	if (delay_seconds > 0) {
		int ms = (int)(delay_seconds * 1000.0);
		if (ms < 0) ms = 0;
		lu5_delta_sleep_ms(ms);
	}
	return true;
}

static bool backend_extdi_read(lua_Integer address_index, lua_Integer pin_index, bool *on_out)
{
	(void)address_index;
	(void)pin_index;
	*on_out = false;
	return true;
}

static bool backend_extdo_write(lua_Integer address_index, lua_Integer pin_index, bool on, lua_Number delay_seconds)
{
	(void)address_index;
	(void)pin_index;
	(void)on;
	if (delay_seconds > 0) {
		int ms = (int)(delay_seconds * 1000.0);
		if (ms < 0) ms = 0;
		lu5_delta_sleep_ms(ms);
	}
	return true;
}

static bool backend_modbus_read(lua_Integer addr, lu5_delta_size size, lua_Integer *value_out)
{
	(void)addr;
	(void)size;
	*value_out = 0;
	return true;
}

static bool backend_modbus_write(lua_Integer addr, lu5_delta_size size, lua_Integer value)
{
	(void)addr;
	(void)size;
	(void)value;
	return true;
}

// -------------------------
// split(str, pat) utility
// -------------------------

static int l_delta_split(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	const char *fname = "split";
	const char *str = luaL_checkstring(L, 1);
	const char *pat = luaL_checkstring(L, 2);
	if (!str || !pat) {
		lu5_delta_arg_error(L, fname, 1, "expected split(str, pat)");
	}
	if (strlen(pat) == 0) {
		lu5_delta_arg_error(L, fname, 2, "pattern/delimiter cannot be empty");
	}

	lua_newtable(L);

	const char *start = str;
	int idx = 1;

	const char *found;
	size_t pat_len = strlen(pat);
	while ((found = strstr(start, pat)) != NULL) {
		lua_pushlstring(L, start, (size_t)(found - start));
		lua_rawseti(L, -2, idx++);
		start = found + pat_len;
	}

	// tail
	lua_pushstring(L, start);
	lua_rawseti(L, -2, idx++);

	return 1;
}

// -------------------------
// Digital IO endpoints
// -------------------------

static int l_delta_DI(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-001
	const char *fname = "DI";
	int argc = lua_gettop(L);
	if (argc != 1 && argc != 2) {
		luaL_error(L, "Function '%s' expects 1 or 2 arguments (Pin_Index[, Length])", fname);
		return 0;
	}

	lu5_delta_pin_index pin = lu5_delta_parse_pin_index(L, 1, fname, 1, 24, true);

	if (argc == 1) {
		bool on = false;
		if (!backend_di_read(&pin, 1, NULL, &on)) {
			luaL_error(L, "Function '%s': backend DI read failed", fname);
			return 0;
		}
		lu5_delta_push_on_off(L, on);
		return 1;
	}

	lua_Integer length = lua_tointeger(L, 2);
	if (!lua_isinteger(L, 2) || length < 1 || length > 24) {
		lu5_delta_arg_error(L, fname, 2, "Length must be integer in range 1..24");
	}

	lua_Integer packed = 0;
	if (!backend_di_read(&pin, (int)length, &packed, NULL)) {
		luaL_error(L, "Function '%s': backend DI read failed", fname);
		return 0;
	}

	lua_pushinteger(L, packed);
	return 1;
}

static int l_delta_DO(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-002
	const char *fname = "DO";
	int argc = lua_gettop(L);

	// Overloads:
	// DO(pin, status[, delay])
	// DO(pin, length, status_num[, delay])
	if (argc < 2 || argc > 4) {
		luaL_error(L, "Function '%s' expects 2..4 arguments", fname);
		return 0;
	}

	lu5_delta_pin_index pin = lu5_delta_parse_pin_index(L, 1, fname, 1, 12, true);

	lua_Number delay = 0.0;
	bool has_delay = false;
	if (argc == 3 || argc == 4) {
		// delay is last arg when used
		if (argc == 3 && (lua_isstring(L, 2) || lua_isboolean(L, 2))) {
			// DO(pin, status, delay)
			if (!lua_isnumber(L, 3)) lu5_delta_arg_error(L, fname, 3, "Delay_Time must be a number (seconds)");
			delay = lua_tonumber(L, 3);
			has_delay = true;
		} else if (argc == 4) {
			if (!lua_isnumber(L, 4)) lu5_delta_arg_error(L, fname, 4, "Delay_Time must be a number (seconds)");
			delay = lua_tonumber(L, 4);
			has_delay = true;
		}
		if (has_delay && delay < 0) {
			lu5_delta_arg_error(L, fname, argc, "Delay_Time must be >= 0");
		}
	}

	if (lua_isstring(L, 2) || lua_isboolean(L, 2)) {
		// DO(pin, status[, delay])
		bool on = lu5_delta_parse_on_off(L, 2, fname);
		if (!backend_do_write(&pin, 1, true, on, 0, has_delay ? delay : 0.0)) {
			luaL_error(L, "Function '%s': backend DO write failed", fname);
		}
		return 0;
	}

	// DO(pin, length, status_num[, delay])
	if (!lua_isinteger(L, 2)) lu5_delta_arg_error(L, fname, 2, "Length must be integer in range 1..12");
	lua_Integer length = lua_tointeger(L, 2);
	if (length < 1 || length > 12) {
		lu5_delta_arg_error(L, fname, 2, "Length must be integer in range 1..12");
	}
	if (!lua_isinteger(L, 3)) lu5_delta_arg_error(L, fname, 3, "Status_num must be integer");
	lua_Integer status_num = lua_tointeger(L, 3);

	if (!backend_do_write(&pin, (int)length, false, false, status_num, has_delay ? delay : 0.0)) {
		luaL_error(L, "Function '%s': backend DO write failed", fname);
	}
	return 0;
}

static int l_delta_ExtDI(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-003
	const char *fname = "ExtDI";
	if (lua_gettop(L) != 2) {
		luaL_error(L, "Function '%s' expects 2 arguments (Address_Index, Pin_Index)", fname);
		return 0;
	}
	if (!lua_isinteger(L, 1) || lua_tointeger(L, 1) < 1) {
		lu5_delta_arg_error(L, fname, 1, "Address_Index must be a positive integer");
	}
	if (!lua_isinteger(L, 2) || lua_tointeger(L, 2) < 1) {
		lu5_delta_arg_error(L, fname, 2, "Pin_Index must be an integer >= 1");
	}

	lua_Integer addr = lua_tointeger(L, 1);
	lua_Integer pin = lua_tointeger(L, 2);

	bool on = false;
	if (!backend_extdi_read(addr, pin, &on)) {
		luaL_error(L, "Function '%s': backend ExtDI read failed", fname);
		return 0;
	}
	lu5_delta_push_on_off(L, on);
	return 1;
}

static int l_delta_ExtDO(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-003
	const char *fname = "ExtDO";
	int argc = lua_gettop(L);
	if (argc != 3 && argc != 4) {
		luaL_error(L, "Function '%s' expects 3 or 4 arguments (Address_Index, Pin_Index, Status[, Delay_Time])", fname);
		return 0;
	}
	if (!lua_isinteger(L, 1) || lua_tointeger(L, 1) < 1) {
		lu5_delta_arg_error(L, fname, 1, "Address_Index must be a positive integer");
	}
	if (!lua_isinteger(L, 2) || lua_tointeger(L, 2) < 1) {
		lu5_delta_arg_error(L, fname, 2, "Pin_Index must be an integer >= 1");
	}
	bool on = lu5_delta_parse_on_off(L, 3, fname);

	lua_Number delay = 0.0;
	if (argc == 4) {
		if (!lua_isnumber(L, 4)) lu5_delta_arg_error(L, fname, 4, "Delay_Time must be a number (seconds)");
		delay = lua_tonumber(L, 4);
		if (delay < 0) lu5_delta_arg_error(L, fname, 4, "Delay_Time must be >= 0");
	}

	if (!backend_extdo_write(lua_tointeger(L, 1), lua_tointeger(L, 2), on, delay)) {
		luaL_error(L, "Function '%s': backend ExtDO write failed", fname);
	}
	return 0;
}

// -------------------------
// Modbus endpoints
// -------------------------

static int l_delta_ReadModbus(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-006
	const char *fname = "ReadModbus";
	if (lua_gettop(L) != 2) {
		luaL_error(L, "Function '%s' expects 2 arguments (RegAddress, Size)", fname);
		return 0;
	}
	if (!lua_isinteger(L, 1)) lu5_delta_arg_error(L, fname, 1, "RegAddress must be an integer");
	lua_Integer addr = lua_tointeger(L, 1);
	lu5_delta_size size = lu5_delta_parse_size(L, 2, fname);
	lu5_delta_validate_modbus_addr(L, fname, addr, size, 1);

	lua_Integer value = 0;
	if (!backend_modbus_read(addr, size, &value)) {
		luaL_error(L, "Function '%s': backend modbus read failed", fname);
		return 0;
	}
	lua_pushinteger(L, value);
	return 1;
}

static int l_delta_WriteModbus(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-006
	const char *fname = "WriteModbus";
	if (lua_gettop(L) != 3) {
		luaL_error(L, "Function '%s' expects 3 arguments (RegAddress, Size, RegValue)", fname);
		return 0;
	}
	if (!lua_isinteger(L, 1)) lu5_delta_arg_error(L, fname, 1, "RegAddress must be an integer");
	lua_Integer addr = lua_tointeger(L, 1);
	lu5_delta_size size = lu5_delta_parse_size(L, 2, fname);
	lu5_delta_validate_modbus_addr(L, fname, addr, size, 1);

	if (!lua_isinteger(L, 3)) lu5_delta_arg_error(L, fname, 3, "RegValue must be an integer");
	lua_Integer value = lua_tointeger(L, 3);
	lu5_delta_validate_modbus_value(L, fname, size, value, 3);

	if (!backend_modbus_write(addr, size, value)) {
		luaL_error(L, "Function '%s': backend modbus write failed", fname);
	}
	return 0;
}

// -------------------------
// DELAY
// -------------------------

static int l_delta_DELAY(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-005
	const char *fname = "DELAY";
	if (lua_gettop(L) != 1) {
		luaL_error(L, "Function '%s' expects 1 argument (Delay_Time_seconds)", fname);
		return 0;
	}
	if (!lua_isnumber(L, 1)) lu5_delta_arg_error(L, fname, 1, "Delay_Time must be a number (seconds)");
	lua_Number seconds = lua_tonumber(L, 1);
	if (seconds < 0.001) {
		lu5_delta_arg_error(L, fname, 1, "Delay_Time minimum is 0.001 seconds");
	}
	int ms = (int)(seconds * 1000.0);
	if (ms < 1) ms = 1;
	lu5_delta_sleep_ms(ms);
	return 0;
}

// -------------------------
// WAIT
// -------------------------

static bool lu5_delta_is_valid_modbus_wait_range(lua_Integer addr)
{
	return (addr >= 0x1000 && addr <= 0x1FFF) || (addr >= 0x3000 && addr <= 0x3FFF);
}

static int l_delta_WAIT(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-004
	const char *fname = "WAIT";
	int argc = lua_gettop(L);
	if (argc < 3 || argc > 4) {
		// Modbus form is exactly 4 args, IO form is 3 or 4
		luaL_error(L, "Function '%s' expects 3..4 arguments", fname);
		return 0;
	}

	// Decide form:
	// IO form: WAIT("DI"/"DO", index_or_table, status_or_table, [timeout_ms])
	// Modbus form: WAIT(var, addr_or_table, "W"/"DW", value)
	if (lua_isstring(L, 1) && (strcasecmp(lua_tostring(L, 1), "DI") == 0 || strcasecmp(lua_tostring(L, 1), "DO") == 0)) {
		// IO form
		const char *io_type = lua_tostring(L, 1);

		// Normalize timeout
		lua_Integer timeout_ms = -1;
		if (argc == 4) {
			if (!lua_isnumber(L, 4)) lu5_delta_arg_error(L, fname, 4, "Timeout must be a number (ms)");
			timeout_ms = (lua_Integer)lua_tointeger(L, 4);
			if (timeout_ms < 0) lu5_delta_arg_error(L, fname, 4, "Timeout must be >= 0 (ms)");
		}

		// Normalize to tables to share polling flow
		bool multi = lua_istable(L, 2);
		if (multi != lua_istable(L, 3)) {
			luaL_error(L, "Function '%s': IO index and status must both be single values or both be tables", fname);
			return 0;
		}

		const lua_Number start = lu5_delta_now_monotonic_seconds();
		while (true) {
			bool met = true;

			if (!multi) {
				// Single
				if (!lua_isinteger(L, 2)) lu5_delta_arg_error(L, fname, 2, "IO index must be integer or table");
				bool want_on = lu5_delta_parse_on_off(L, 3, fname);

				// Stub behavior: DI/DO always OFF => met only if want OFF
				bool actual_on = false;
				met = (actual_on == want_on);
			} else {
				// Multiple
				size_t len_idx = lua_rawlen(L, 2);
				size_t len_st = lua_rawlen(L, 3);
				if (len_idx != len_st) {
					luaL_error(L, "Function '%s': IO index table and status table must have the same length", fname);
					return 0;
				}
				for (size_t i = 1; i <= len_idx; i++) {
					lua_rawgeti(L, 2, (lua_Integer)i);
					if (!lua_isinteger(L, -1)) {
						lua_pop(L, 1);
						lu5_delta_arg_error(L, fname, 2, "IO index table must contain integers");
					}
					lua_pop(L, 1);

					lua_rawgeti(L, 3, (lua_Integer)i);
					bool want_on = lu5_delta_parse_on_off(L, -1, fname);
					lua_pop(L, 1);

					bool actual_on = false;
					if (actual_on != want_on) {
						met = false;
						break;
					}
				}
			}

			if (met) break;

			if (timeout_ms >= 0) {
				lua_Number elapsed_ms = (lu5_delta_now_monotonic_seconds() - start) * 1000.0;
				if (elapsed_ms >= (lua_Number)timeout_ms) break;
			}

			(void)io_type; // reserved for real backend
			lu5_delta_sleep_ms(5);
		}

		return 0;
	}

	// Modbus form: WAIT(var, addr_or_table, "W"/"DW", value)
	if (argc != 4) {
		luaL_error(L, "Function '%s': Modbus WAIT expects 4 arguments", fname);
		return 0;
	}

	// arg1 variable is unused (word)
	(void)lua_tostring(L, 1);

	bool multi = lua_istable(L, 2);

	lu5_delta_size size = lu5_delta_parse_size(L, 3, fname);
	if (!lua_isinteger(L, 4)) lu5_delta_arg_error(L, fname, 4, "Modbus data must be an integer");
	lua_Integer want = lua_tointeger(L, 4);
	lu5_delta_validate_modbus_value(L, fname, size, want, 4);

	// In stub: modbus always returns 0, so condition met iff want == 0 for all addresses.
	if (!multi) {
		if (!lua_isinteger(L, 2)) lu5_delta_arg_error(L, fname, 2, "Modbus address must be integer or table");
		lua_Integer addr = lua_tointeger(L, 2);
		if (!lu5_delta_is_valid_modbus_wait_range(addr)) {
			lu5_delta_arg_error(L, fname, 2, "Modbus address must be within 0x1000..0x1FFF or 0x3000..0x3FFF");
		}
		lu5_delta_validate_modbus_addr(L, fname, addr, size, 2);
		// block until met: deterministic stub => either immediate if want==0, or wait "forever"
		if (want != 0) {
			// Avoid infinite tight loop; provide a clear error boundary.
			luaL_error(L, "Function '%s': stub backend never changes Modbus value; WAIT would block indefinitely for non-zero target", fname);
		}
		return 0;
	}

	// table addresses: ensure all in range and even if DW
	size_t len = lua_rawlen(L, 2);
	for (size_t i = 1; i <= len; i++) {
		lua_rawgeti(L, 2, (lua_Integer)i);
		if (!lua_isinteger(L, -1)) {
			lua_pop(L, 1);
			lu5_delta_arg_error(L, fname, 2, "Modbus address table must contain integers");
		}
		lua_Integer addr = lua_tointeger(L, -1);
		lua_pop(L, 1);

		if (!lu5_delta_is_valid_modbus_wait_range(addr)) {
			lu5_delta_arg_error(L, fname, 2, "Modbus address must be within 0x1000..0x1FFF or 0x3000..0x3FFF");
		}
		lu5_delta_validate_modbus_addr(L, fname, addr, size, 2);
	}

	if (want != 0) {
		luaL_error(L, "Function '%s': stub backend never changes Modbus value; WAIT would block indefinitely for non-zero target", fname);
	}
	return 0;
}

// -------------------------
// AuxTasks scheduler
// -------------------------

#define LU5_DELTA_AUX_MAX_TASKS 10
#define LU5_DELTA_AUX_REGKEY "lu5.deltaapi2.auxtasks"

typedef struct {
	int task_count;
	int refs[LU5_DELTA_AUX_MAX_TASKS]; // registry references to Lua functions
	int current_task;                  // next task index to run
	lua_Number slice_seconds;          // 0.015 by spec
} lu5_delta_auxtasks_state;

static lu5_delta_auxtasks_state *lu5_delta_get_or_create_auxtasks(lua_State *L)
{
	lua_getfield(L, LUA_REGISTRYINDEX, LU5_DELTA_AUX_REGKEY);
	if (lua_islightuserdata(L, -1)) {
		lu5_delta_auxtasks_state *st = (lu5_delta_auxtasks_state *)lua_touserdata(L, -1);
		lua_pop(L, 1);
		return st;
	}
	lua_pop(L, 1);

	lu5_delta_auxtasks_state *st = (lu5_delta_auxtasks_state *)lua_newuserdatauv(L, sizeof(lu5_delta_auxtasks_state), 0);
	memset(st, 0, sizeof(*st));
	st->slice_seconds = 0.015;

	lua_setfield(L, LUA_REGISTRYINDEX, LU5_DELTA_AUX_REGKEY);

	// fetch it back as lightuserdata for stable pointer storage
	lua_getfield(L, LUA_REGISTRYINDEX, LU5_DELTA_AUX_REGKEY);
	lu5_delta_auxtasks_state *out = (lu5_delta_auxtasks_state *)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return out;
}

static void lu5_delta_auxtasks_clear(lua_State *L, lu5_delta_auxtasks_state *st)
{
	for (int i = 0; i < st->task_count; i++) {
		if (st->refs[i] != LUA_NOREF) {
			luaL_unref(L, LUA_REGISTRYINDEX, st->refs[i]);
			st->refs[i] = LUA_NOREF;
		}
	}
	st->task_count = 0;
	st->current_task = 0;
}

static int l_delta_AuxTasksAdd(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-009
	const char *fname = "AuxTasksAdd";
	int argc = lua_gettop(L);
	if (argc < 1 || argc > LU5_DELTA_AUX_MAX_TASKS) {
		luaL_error(L, "Function '%s' expects 1..10 function arguments", fname);
		return 0;
	}

	lu5_delta_auxtasks_state *st = lu5_delta_get_or_create_auxtasks(L);
	lu5_delta_auxtasks_clear(L, st);

	for (int i = 1; i <= argc; i++) {
		if (!lua_isfunction(L, i)) {
			lu5_delta_arg_error(L, fname, i, "expected function");
		}
		lua_pushvalue(L, i);
		st->refs[i - 1] = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	st->task_count = argc;
	st->current_task = 0;
	return 0;
}

static int l_delta_AuxTasks(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-009
	const char *fname = "AuxTasks";
	if (lua_gettop(L) != 0) {
		luaL_error(L, "Function '%s' expects 0 arguments", fname);
		return 0;
	}

	lu5_delta_auxtasks_state *st = lu5_delta_get_or_create_auxtasks(L);
	if (st->task_count == 0) return 0;

	lua_Number slice_end = lu5_delta_now_monotonic_seconds() + st->slice_seconds;

	// Contract note:
	// DeltaAPI-2 says: "Execute each subfunction for 15ms; resume where left off next iteration".
	// In plain Lua without coroutines, we cannot preempt a running function safely.
	// We implement cooperative time slicing: run one function per AuxTasks() call, but keep
	// the 15ms budget for future coroutine extension.
	(void)slice_end;

	int idx = st->current_task % st->task_count;
	int ref = st->refs[idx];

	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
		luaL_error(L, "Function '%s': task %d failed: %s", fname, idx + 1, lua_tostring(L, -1));
		return 0;
	}
	st->current_task = (idx + 1) % st->task_count;
	return 0;
}

// -------------------------
// Motion/global commands (API-present; stubbed)
// -------------------------

static int l_delta_MovP(lua_State *L) { (void)L; return luaL_error(L, "MovP is not supported (stub backend)"); }
static int l_delta_MovL(lua_State *L) { (void)L; return luaL_error(L, "MovL is not supported (stub backend)"); }
static int l_delta_MovJ(lua_State *L) { (void)L; return luaL_error(L, "MovJ is not supported (stub backend)"); }
static int l_delta_SetGlobalPoint(lua_State *L) { (void)L; return luaL_error(L, "SetGlobalPoint is not supported (stub backend)"); }
static int l_delta_ReadPoint(lua_State *L) { (void)L; return luaL_error(L, "ReadPoint is not supported (stub backend)"); }

static int l_delta_SpdJ(lua_State *L) { (void)L; return luaL_error(L, "SpdJ is not supported (stub backend)"); }
static int l_delta_AccJ(lua_State *L) { (void)L; return luaL_error(L, "AccJ is not supported (stub backend)"); }
static int l_delta_DecJ(lua_State *L) { (void)L; return luaL_error(L, "DecJ is not supported (stub backend)"); }
static int l_delta_SpdL(lua_State *L) { (void)L; return luaL_error(L, "SpdL is not supported (stub backend)"); }
static int l_delta_AccL(lua_State *L) { (void)L; return luaL_error(L, "AccL is not supported (stub backend)"); }
static int l_delta_DecL(lua_State *L) { (void)L; return luaL_error(L, "DecL is not supported (stub backend)"); }
static int l_delta_Accur(lua_State *L) { (void)L; return luaL_error(L, "Accur is not supported (stub backend)"); }

// -------------------------
// SocketClass / SocketServer (POSIX only; gated)
// -------------------------

#define LU5_DELTA_SOCKETCLASS_MT "lu5.deltaapi2.SocketClass"
#define LU5_DELTA_SOCKETSERVER_MT "lu5.deltaapi2.SocketServer"

typedef struct {
	bool is_server;
	int port;
	char spacing[8];    // delimiter for receive split; default "," for client, ";" for server
	char delimiter[8];  // send line delimiter; default "\r\n"
	lua_Number timeout_seconds;
#ifdef __unix__
	int fd;
	int client_fd; // for server: accepted client
#endif
} lu5_delta_socket;

static void lu5_delta_socket_set_default_client(lu5_delta_socket *s)
{
	memset(s, 0, sizeof(*s));
	s->is_server = false;
	s->port = 0;
	snprintf(s->spacing, sizeof(s->spacing), "%s", ",");
	snprintf(s->delimiter, sizeof(s->delimiter), "%s", "\r\n");
	s->timeout_seconds = 10.0;
#ifdef __unix__
	s->fd = -1;
	s->client_fd = -1;
#endif
}

static void lu5_delta_socket_set_default_server(lu5_delta_socket *s)
{
	lu5_delta_socket_set_default_client(s);
	s->is_server = true;
	snprintf(s->spacing, sizeof(s->spacing), "%s", ";");
}

static int l_socket_error_code_table(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	lua_newtable(L);

	struct { int code; const char *msg; } codes[] = {
		{0x0000, "No error(No error)"},
		{0x0001, "SessionID is invalid/ conflict(SessionID is invalid or conflict)"},
		{0x0002, "Busy(Busy)"},
		{0x0003, "Send fail, disconnect (Send fail, disconnect)"},
		{0x0006, "Sent successfully while SByte or EByte were set (Sent successfully while SByte orEByte were set.)"},
		{0x0007, "Length of sent packet is too short (Length of sent packet is too short)"},
		{0x0008, "Freeport Role is incorrect (Freeport Role is incorrect)"},
		{0x0009, "Freeport Connection is full (Freeport Connection is full)"},
		{0x000A, "Freeport Channel is error (Freeport Channel is error)"},
		{0x000B, "Client IP or Port cannot be zero (Client IP or Port cannot be zero)"},
		{0x000C, "Server Port is out of range!(3000~10000) (Server Port is out of range!(3000~10000))"},
		{0x000D, "Socket can not be created (Socket can not be created)"},
		{0x000E, "Network Device can not be bound (Network Device can not be bound)"},
		{0x000F, "Server setsockopt SO_REUSEADDR error (Server setsockopt SO_REUSEADDRerror)"},
		{0x0010, "Server bind failed (Server bind failed)"},
		{0x0011, "Server listen failed (Server listen failed)"},
		{0x0012, "Client connection refused ! Please check the network connection. (Client connectionrefused)"},
		{0x0031, "Server returns data without ending code (Server returns data without ending code)"},
	};

	for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
		lua_pushinteger(L, codes[i].code);
		lua_pushstring(L, codes[i].msg);
		lua_settable(L, -3);
	}

	return 1;
}

static lu5_delta_socket *lu5_delta_check_socket(lua_State *L, int index, const char *mt)
{
	return (lu5_delta_socket *)luaL_checkudata(L, index, mt);
}

static int l_socket_CheckStatus(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	lu5_delta_socket *s = lu5_delta_check_socket(L, 1, s->is_server ? LU5_DELTA_SOCKETSERVER_MT : LU5_DELTA_SOCKETCLASS_MT);

	lua_pushinteger(L, s->port);
#ifdef __unix__
	bool connected = false;
	if (!s->is_server) {
		connected = (s->fd >= 0);
	} else {
		connected = (s->client_fd >= 0);
	}
	lua_pushstring(L, connected ? "Connected" : "DisConnected");
	lua_pushinteger(L, 0x0000);
#else
	lua_pushstring(L, "DisConnected");
	lua_pushinteger(L, 0x000D);
#endif
	return 3;
}

static int l_socket_Close(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-007
	lu5_delta_socket *s = (lu5_delta_socket *)lua_touserdata(L, 1);
	if (!s) return 0;

#ifdef __unix__
	if (s->client_fd >= 0) {
		close(s->client_fd);
		s->client_fd = -1;
	}
	if (s->fd >= 0) {
		close(s->fd);
		s->fd = -1;
	}
#endif
	return 0;
}

static int l_socket_Send(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-007
	lu5_delta_socket *s = (lu5_delta_socket *)lua_touserdata(L, 1);
	if (!s) return luaL_error(L, "Socket Send: invalid self");

	const char *cmd = NULL;
	if (lua_isstring(L, 2)) cmd = lua_tostring(L, 2);
	else if (lua_isnumber(L, 2)) {
		lua_pushvalue(L, 2);
		lua_tostring(L, -1);
		cmd = lua_tostring(L, -1);
		lua_pop(L, 1);
	} else {
		return luaL_error(L, "Send expects Cmd as string or number");
	}
	if (!cmd) return luaL_error(L, "Send: invalid Cmd");

#ifdef __unix__
	int fd = s->is_server ? s->client_fd : s->fd;
	if (fd < 0) return luaL_error(L, "Send: DisConnected");

	char buf[2048];
	snprintf(buf, sizeof(buf), "%s%s", cmd, s->delimiter);

	ssize_t n = send(fd, buf, strlen(buf), 0);
	if (n < 0) return luaL_error(L, "Send failed: %s", strerror(errno));
#else
	return luaL_error(L, "Socket features are not supported on this platform build");
#endif
	return 0;
}

static int l_socket_Receive(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-007
	lu5_delta_socket *s = (lu5_delta_socket *)lua_touserdata(L, 1);
	if (!s) return luaL_error(L, "Socket Receive: invalid self");

#ifdef __unix__
	int fd = s->is_server ? s->client_fd : s->fd;
	if (fd < 0) return luaL_error(L, "Receive: DisConnected");

	// simple recv with timeout
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	struct timeval tv;
	tv.tv_sec = (int)s->timeout_seconds;
	tv.tv_usec = (int)((s->timeout_seconds - (lua_Number)tv.tv_sec) * 1e6);

	int r = select(fd + 1, &readfds, NULL, NULL, &tv);
	if (r == 0) {
		return luaL_error(L, "Receive timeout");
	}
	if (r < 0) {
		return luaL_error(L, "Receive select error: %s", strerror(errno));
	}

	char buf[2048];
	ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0) return luaL_error(L, "Receive failed/disconnected");

	buf[n] = '\0';

	// If spacing configured, split into table if delimiter present
	if (strlen(s->spacing) > 0 && strstr(buf, s->spacing) != NULL) {
		lua_pushcfunction(L, l_delta_split);
		lua_pushstring(L, buf);
		lua_pushstring(L, s->spacing);
		lua_call(L, 2, 1);
		return 1;
	}

	lua_pushstring(L, buf);
	return 1;
#else
	return luaL_error(L, "Socket features are not supported on this platform build");
#endif
}

static int l_SocketVersion(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	// DeltaAPI-2: prints version directly
	printf("lu5 SocketVersion (deltaapi2): 0.1\n");
	fflush(stdout);
	return 0;
}

static int l_CheckAllStatus(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	// Stub: no global registry of sockets; return empty arrays.
	lua_newtable(L); // retPort
	lua_newtable(L); // retStatus
	lua_newtable(L); // retErr
	return 3;
}

static int l_SocketClass(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-007
	const char *fname = "SocketClass";
	int argc = lua_gettop(L);
	if (argc < 2 || argc > 7) {
		return luaL_error(L, "Function '%s' expects 2..7 args (HostIP, Port, [Spacing], [Delimiter], [Cmd], [Sleeptime], [Timeout])", fname);
	}
	const char *host = luaL_checkstring(L, 1);
	if (!lua_isinteger(L, 2)) lu5_delta_arg_error(L, fname, 2, "Port must be integer");
	lua_Integer port = lua_tointeger(L, 2);

	lu5_delta_socket *s = (lu5_delta_socket *)lua_newuserdatauv(L, sizeof(lu5_delta_socket), 0);
	lu5_delta_socket_set_default_client(s);
	s->port = (int)port;

	// spacing char or nil
	if (argc >= 3 && !lua_isnil(L, 3)) {
		const char *sp = luaL_checkstring(L, 3);
		snprintf(s->spacing, sizeof(s->spacing), "%s", sp);
	}
	// delimiter char or nil
	if (argc >= 4 && !lua_isnil(L, 4)) {
		const char *dl = luaL_checkstring(L, 4);
		snprintf(s->delimiter, sizeof(s->delimiter), "%s", dl);
	}
	// Cmd + Sleeptime ignored in current binding (Delta controller feature)
	// Timeout seconds
	if (argc >= 7 && !lua_isnil(L, 7)) {
		if (!lua_isnumber(L, 7)) lu5_delta_arg_error(L, fname, 7, "Timeout must be number (seconds)");
		s->timeout_seconds = lua_tonumber(L, 7);
		if (s->timeout_seconds <= 0) lu5_delta_arg_error(L, fname, 7, "Timeout must be > 0");
	}

#ifdef __unix__
	// Connect immediately
	struct hostent *server = gethostbyname(host);
	if (!server) {
		return luaL_error(L, "SocketClass: unknown host");
	}
	s->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s->fd < 0) {
		return luaL_error(L, "SocketClass: socket create failed: %s", strerror(errno));
	}
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, (size_t)server->h_length);
	serv_addr.sin_port = htons((uint16_t)port);

	if (connect(s->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		close(s->fd);
		s->fd = -1;
		return luaL_error(L, "SocketClass: connect failed: %s", strerror(errno));
	}
#else
	(void)host;
	return luaL_error(L, "Socket features are not supported on this platform build");
#endif

	luaL_getmetatable(L, LU5_DELTA_SOCKETCLASS_MT);
	lua_setmetatable(L, -2);
	return 1;
}

static int l_SocketServer(lua_State *L)
{
	// REQ: DELTAAPI2-REQ-008
	const char *fname = "SocketServer";
	int argc = lua_gettop(L);
	if (argc < 1 || argc > 5) {
		return luaL_error(L, "Function '%s' expects 1..5 args (port, [spacing], [delimiter], [cmd], [timeout])", fname);
	}
	if (!lua_isinteger(L, 1)) lu5_delta_arg_error(L, fname, 1, "Port must be integer");
	lua_Integer port = lua_tointeger(L, 1);

	lu5_delta_socket *s = (lu5_delta_socket *)lua_newuserdatauv(L, sizeof(lu5_delta_socket), 0);
	lu5_delta_socket_set_default_server(s);
	s->port = (int)port;

	// spacing
	if (argc >= 2 && !lua_isnil(L, 2)) {
		const char *sp = luaL_checkstring(L, 2);
		snprintf(s->spacing, sizeof(s->spacing), "%s", sp);
	}
	// delimiter
	if (argc >= 3 && !lua_isnil(L, 3)) {
		const char *dl = luaL_checkstring(L, 3);
		snprintf(s->delimiter, sizeof(s->delimiter), "%s", dl);
	}

	// cmd ignored
	if (argc >= 5 && !lua_isnil(L, 5)) {
		if (!lua_isnumber(L, 5)) lu5_delta_arg_error(L, fname, 5, "Timeout must be number (seconds)");
		s->timeout_seconds = lua_tonumber(L, 5);
		if (s->timeout_seconds <= 0) lu5_delta_arg_error(L, fname, 5, "Timeout must be > 0");
	}

	// Cmd and Delimiter cannot be same
	// (Delta spec note; enforce deterministically)
	// Here "Cmd" isn't stored; we only validate delimiter is not empty.
	if (strcmp(s->delimiter, s->spacing) == 0) {
		return luaL_error(L, "SocketServer: Cmd parameter and Delimiter cannot be the same value (Cmd is not implemented in lu5 stub)");
	}

#ifdef __unix__
	s->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s->fd < 0) return luaL_error(L, "SocketServer: socket create failed: %s", strerror(errno));

	int opt = 1;
	if (setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		return luaL_error(L, "SocketServer: setsockopt failed: %s", strerror(errno));
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((uint16_t)port);

	if (bind(s->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(s->fd);
		s->fd = -1;
		return luaL_error(L, "SocketServer: bind failed: %s", strerror(errno));
	}

	if (listen(s->fd, 1) < 0) {
		close(s->fd);
		s->fd = -1;
		return luaL_error(L, "SocketServer: listen failed: %s", strerror(errno));
	}

	// Accept once (blocking up to timeout)
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(s->fd, &readfds);
	struct timeval tv;
	tv.tv_sec = (int)s->timeout_seconds;
	tv.tv_usec = (int)((s->timeout_seconds - (lua_Number)tv.tv_sec) * 1e6);

	int rr = select(s->fd + 1, &readfds, NULL, NULL, &tv);
	if (rr <= 0) {
		// no connection yet -> keep server open; client_fd stays -1
	} else {
		s->client_fd = accept(s->fd, NULL, NULL);
	}
#else
	return luaL_error(L, "Socket features are not supported on this platform build");
#endif

	luaL_getmetatable(L, LU5_DELTA_SOCKETSERVER_MT);
	lua_setmetatable(L, -2);
	return 1;
}

static void lu5_delta_register_socket_mts(lua_State *L)
{
	luaL_newmetatable(L, LU5_DELTA_SOCKETCLASS_MT);
	lua_pushcfunction(L, l_socket_Send);
	lua_setfield(L, -2, "Send");
	lua_pushcfunction(L, l_socket_Receive);
	lua_setfield(L, -2, "Receive");
	lua_pushcfunction(L, l_socket_Close);
	lua_setfield(L, -2, "Close");
	lua_pushcfunction(L, l_socket_CheckStatus);
	lua_setfield(L, -2, "CheckStatus");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	luaL_newmetatable(L, LU5_DELTA_SOCKETSERVER_MT);
	lua_pushcfunction(L, l_socket_Send);
	lua_setfield(L, -2, "Send");
	lua_pushcfunction(L, l_socket_Receive);
	lua_setfield(L, -2, "Receive");
	lua_pushcfunction(L, l_socket_Close);
	lua_setfield(L, -2, "Close");
	lua_pushcfunction(L, l_socket_CheckStatus);
	lua_setfield(L, -2, "CheckStatus");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
}

// -------------------------
// Registration
// -------------------------

void lu5_bind_deltaapi2(lua_State *L)
{
	// Globals (functions)
	lua_pushcfunction(L, l_delta_DI); lua_setglobal(L, "DI");
	lua_pushcfunction(L, l_delta_DO); lua_setglobal(L, "DO");
	lua_pushcfunction(L, l_delta_ExtDI); lua_setglobal(L, "ExtDI");
	lua_pushcfunction(L, l_delta_ExtDO); lua_setglobal(L, "ExtDO");

	lua_pushcfunction(L, l_delta_WAIT); lua_setglobal(L, "WAIT");
	lua_pushcfunction(L, l_delta_DELAY); lua_setglobal(L, "DELAY");

	lua_pushcfunction(L, l_delta_ReadModbus); lua_setglobal(L, "ReadModbus");
	lua_pushcfunction(L, l_delta_WriteModbus); lua_setglobal(L, "WriteModbus");

	lua_pushcfunction(L, l_delta_AuxTasksAdd); lua_setglobal(L, "AuxTasksAdd");
	lua_pushcfunction(L, l_delta_AuxTasks); lua_setglobal(L, "AuxTasks");

	// Motion/global (API-present, stubbed)
	lua_pushcfunction(L, l_delta_MovP); lua_setglobal(L, "MovP");
	lua_pushcfunction(L, l_delta_MovL); lua_setglobal(L, "MovL");
	lua_pushcfunction(L, l_delta_MovJ); lua_setglobal(L, "MovJ");
	lua_pushcfunction(L, l_delta_SetGlobalPoint); lua_setglobal(L, "SetGlobalPoint");
	lua_pushcfunction(L, l_delta_ReadPoint); lua_setglobal(L, "ReadPoint");

	lua_pushcfunction(L, l_delta_SpdJ); lua_setglobal(L, "SpdJ");
	lua_pushcfunction(L, l_delta_AccJ); lua_setglobal(L, "AccJ");
	lua_pushcfunction(L, l_delta_DecJ); lua_setglobal(L, "DecJ");
	lua_pushcfunction(L, l_delta_SpdL); lua_setglobal(L, "SpdL");
	lua_pushcfunction(L, l_delta_AccL); lua_setglobal(L, "AccL");
	lua_pushcfunction(L, l_delta_DecL); lua_setglobal(L, "DecL");
	lua_pushcfunction(L, l_delta_Accur); lua_setglobal(L, "Accur");

	// Utilities
	lua_pushcfunction(L, l_delta_split); lua_setglobal(L, "split");

	// Socket
	lu5_delta_register_socket_mts(L);
	lua_pushcfunction(L, l_SocketClass); lua_setglobal(L, "SocketClass");
	lua_pushcfunction(L, l_SocketServer); lua_setglobal(L, "SocketServer");
	lua_pushcfunction(L, l_CheckAllStatus); lua_setglobal(L, "CheckAllStatus");
	lua_pushcfunction(L, l_SocketVersion); lua_setglobal(L, "SocketVersion");

	// SocketErrorCode table
	lua_pushcfunction(L, l_socket_error_code_table);
	lua_call(L, 0, 1);
	lua_setglobal(L, "SocketErrorCode");
}
