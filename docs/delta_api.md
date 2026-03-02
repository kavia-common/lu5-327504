# DeltaAPI Reference for lu5

This document describes the Delta Robot API (DeltaAPI) functions available as global Lua functions inside every lu5 sketch. These bindings mirror the Delta controller scripting language so that Delta robot programs can be developed, tested, and simulated on any desktop machine running lu5 — without requiring physical hardware.

> **Stub Behaviour Note:** Because lu5 runs on a desktop with no robot hardware, every function that would interact with the physical controller is a _stub_. Stubs validate all arguments exactly as the real controller would, then emit a `[WARN]` message via `LU5_WARN` and return a safe default value (usually `0`, `"OFF"`, or nothing). This lets Delta programs run end-to-end in simulation while making it obvious which hardware calls were triggered.

---

## Table of Contents

1. [Digital I/O](#digital-io)
2. [Motion Commands](#motion-commands)
3. [Speed and Acceleration](#speed-and-acceleration)
4. [Timing and Flow Control](#timing-and-flow-control)
5. [Point Management](#point-management)
6. [Modbus Register Access](#modbus-register-access)
7. [Socket Communication](#socket-communication)
8. [Multi-task Cooperative Execution](#multi-task-cooperative-execution)
9. [Platform Caveats](#platform-caveats)
10. [Quick-start Example](#quick-start-example)

---

## Digital I/O

These functions read digital input pins and set digital output pins on the Delta robot controller. All are stubs in lu5 — inputs always return `"OFF"` or `0`, and outputs do nothing but log a warning.

### DI — Digital Input

Reads the state of one or more digital input pins.

```lua
-- Single-pin form: returns "ON" or "OFF"
local state = DI(pin)

-- Multi-pin bitmask form: returns an integer bitmask
local mask = DI(startPin, length)
```

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `pin` | integer or string | 1–24 | Pin number or pin name |
| `length` | integer | 1–24 | Number of consecutive pins to read as a bitmask |

**Stub return:** `"OFF"` (single-pin) or `0` (multi-pin bitmask).

```lua
-- Example
local valve = DI(3)         -- read pin 3, returns "OFF" in lu5
local bits  = DI(1, 8)     -- read 8 pins starting at 1, returns 0
print(valve, bits)
```

### DO — Digital Output

Sets the state of one or more digital output pins, optionally after a delay.

```lua
-- Single-pin form
DO(pin, "ON"|"OFF")
DO(pin, "ON"|"OFF", delay_seconds)

-- Multi-pin bitmask form
DO(pin, length, status_bitmask)
DO(pin, length, status_bitmask, delay_seconds)
```

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `pin` | integer or string | 1–12 | Pin number or pin name |
| `status` | string | `"ON"` or `"OFF"` | Desired output state |
| `delay_seconds` | number | ≥ 0 | Optional delay before applying state |
| `length` | integer | 1–12 | Number of consecutive output pins |
| `status_bitmask` | integer | — | Bit-field representing pin states |

**Stub:** No hardware effect; logs a warning.

```lua
-- Example
DO(1, "ON")           -- turn output pin 1 on
DO(2, "OFF", 0.5)     -- turn pin 2 off after 0.5 s delay
DO(1, 4, 0b0101)      -- set 4 pins with bitmask
```

### ExtDI — External Board Digital Input

Reads a digital input pin on an external I/O expansion board.

```lua
local state = ExtDI(address_index, pin_index)
```

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `address_index` | integer | > 0 | Address of the external board |
| `pin_index` | integer | > 0 | Pin number on that board |

**Stub return:** `"OFF"`.

```lua
local ext = ExtDI(1, 3)   -- board 1, pin 3
print(ext)                 -- "OFF" in lu5
```

### ExtDO — External Board Digital Output

Sets a digital output pin on an external I/O expansion board.

```lua
ExtDO(address_index, pin_index, "ON"|"OFF")
ExtDO(address_index, pin_index, "ON"|"OFF", delay_seconds)
```

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `address_index` | integer | > 0 | Address of the external board |
| `pin_index` | integer | > 0 | Pin number on that board |
| `status` | string | `"ON"` or `"OFF"` | Desired state |
| `delay_seconds` | number | ≥ 0 | Optional delay |

**Stub:** No hardware effect.

```lua
ExtDO(1, 5, "ON", 0.2)   -- board 1, pin 5, ON after 0.2 s
```

---

## Motion Commands

These functions command the Delta robot to move its arm. In lu5 all motion is silently accepted and logged as a warning because no arm simulation is performed.

### MovP — Point-to-Point Motion

Moves the robot from the current position to a named or numbered target point using the fastest available path (PTP motion).

```lua
MovP(point_name_or_number [, modifiers...])
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `point_name_or_number` | string or integer | Target point: a global variable name (string) or registered point number (integer ≥ 1) |
| `modifiers` | any (optional) | SPD, ACC, DEC, PASS, Offset values accepted and ignored in stubs |

**Stub:** Logs warning, no movement.

```lua
MovP("GL_Home")    -- move to global point named GL_Home
MovP(5)            -- move to point #5
```

### MovL — Linear Motion

Moves the robot along a straight line in Cartesian space to the target point.

```lua
MovL(point_name_or_number [, modifiers...])
```

Same signature as `MovP`. The distinction between the two is the trajectory type on physical hardware.

```lua
MovL("GL_Pick")    -- straight-line move to pick position
MovL(3)            -- straight-line move to point #3
```

### MovJ — Single-Axis Joint Motion

Rotates a single joint of the robot to the specified angle.

```lua
MovJ(joint, degree [, modifiers...])
```

| Parameter | Type | Range | Description |
|-----------|------|-------|-------------|
| `joint` | integer | 1–6 | Joint number |
| `degree` | number | -360 to 360 | Target angle in degrees |
| `modifiers` | any (optional) | SPD, ACC, DEC values accepted and ignored |

**Stub:** Validates range, then logs warning.

```lua
MovJ(1, 45.0)     -- rotate joint 1 to 45 degrees
MovJ(3, -90.0)    -- rotate joint 3 to -90 degrees
```

---

## Speed and Acceleration

These setters configure the speed and acceleration profiles used by subsequent motion commands. Values are stored in module-level statics and validated against the documented ranges on each call.

### SpdJ — Joint Maximum Speed

Sets the maximum joint speed as a percentage of the maximum rated speed.

```lua
SpdJ(speed_percent)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `speed_percent` | number | 0.001–100 | 10 |

```lua
SpdJ(50)    -- 50 % of maximum joint speed
```

### AccJ — Joint Acceleration

Sets the joint acceleration as a percentage.

```lua
AccJ(accel_percent)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `accel_percent` | number | 0.001–100 | 10 |

```lua
AccJ(30)
```

### DecJ — Joint Deceleration

Sets the joint deceleration as a percentage.

```lua
DecJ(decel_percent)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `decel_percent` | number | 0.001–100 | 10 |

```lua
DecJ(30)
```

### SpdL — Linear Maximum Speed

Sets the maximum linear (Cartesian) speed in mm/s.

```lua
SpdL(speed_mm_per_s)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `speed_mm_per_s` | number | 1–2000 | 100 |

```lua
SpdL(500)   -- 500 mm/s
```

### AccL — Linear Acceleration

Sets the linear acceleration in mm/s².

```lua
AccL(accel_mm_per_s2)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `accel_mm_per_s2` | number | 1–25000 | 10 |

```lua
AccL(2000)
```

### DecL — Linear Deceleration

Sets the linear deceleration in mm/s².

```lua
DecL(decel_mm_per_s2)
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `decel_mm_per_s2` | number | 1–25000 | 10 |

```lua
DecL(2000)
```

### Accur — In-Place Accuracy Mode

Selects the positioning accuracy level for subsequent motion commands.

```lua
Accur(mode)
Accur(mode, "CART")   -- Cartesian qualifier
```

| Parameter | Type | Allowed values |
|-----------|------|----------------|
| `mode` | string | `"HIGH"`, `"STANDARD"`, `"MEDIUM"`, `"ROUGH"`, `"MAXROUGH"` |
| `"CART"` | string (optional) | Adds Cartesian qualifier |

**Stub:** Validates mode, logs warning, no hardware effect.

```lua
Accur("HIGH")
Accur("MEDIUM", "CART")
```

---

## Timing and Flow Control

### DELAY — Pause Execution

Pauses execution for the given number of seconds. Unlike the motion and I/O stubs, `DELAY` performs an **actual sleep** so time-sensitive Delta scripts still exhibit correct pacing in lu5.

```lua
DELAY(seconds)
```

| Parameter | Type | Range |
|-----------|------|-------|
| `seconds` | number | ≥ 0.001 |

**Platform notes:**
- **Linux/macOS:** Uses `nanosleep` (POSIX).
- **Windows:** Uses `Sleep` (Win32), minimum resolution 1 ms.
- **WASM:** Sleep is not available; a warning is logged and execution continues immediately.

```lua
DELAY(1.5)    -- pause for 1.5 seconds
DELAY(0.05)   -- 50 ms delay between operations
```

### WAIT — Wait for a Condition

Blocks until a hardware condition is met. In lu5, all hardware conditions are assumed to be met immediately; the function returns at once after logging a warning.

```lua
-- Digital I/O form
WAIT("DI", pin, "ON"|"OFF")
WAIT("DI", pin, "ON"|"OFF", timeout)

-- Modbus condition form
WAIT(variable, address, "W"|"DW", value)
```

**Stub:** Condition is always considered satisfied; execution proceeds immediately.

```lua
WAIT("DI", 3, "ON")              -- wait for DI pin 3 to go ON
WAIT("DI", 3, "ON", 5.0)         -- same, with 5 s timeout
WAIT("DO", 1, "OFF")             -- wait for DO pin 1 to go OFF
```

---

## Point Management

Point management functions store and retrieve robot coordinate data. Points are persisted in the Lua global table `_delta_points` (keyed by integer index) and survive across function calls within a session.

### SetGlobalPoint — Define a Named Global Point

Stores a robot pose at the given point number. Point names should begin with `"GL_"` to match the Delta controller convention; a warning is issued if the prefix is absent.

```lua
SetGlobalPoint(point_number, name, X, Y, Z [, RZ [, RY [, RX [, ...]]]] [, JRC_table])
```

| Parameter | Type | Range / Notes |
|-----------|------|---------------|
| `point_number` | integer | 1–1000 |
| `name` | string | Should start with `"GL_"` |
| `X`, `Y`, `Z` | number | Cartesian coordinates (mm) |
| `RZ`, `RY`, `RX` | number (optional) | Rotation angles (degrees) |
| `JRC_table` | table (optional) | Joint Reference Configuration array |

```lua
-- 3-axis point (X, Y, Z only)
SetGlobalPoint(1, "GL_Home", 0, 0, 100)

-- 6-axis point
SetGlobalPoint(2, "GL_Pick", 300.5, -120.0, 50.0, 0.0, 0.0, 0.0)

-- Point with JRC table
SetGlobalPoint(3, "GL_Place", 200.0, 150.0, 80.0, 0, 0, 0, {0, 0, 0, 0, 0, 0})
```

### ReadPoint — Read a Field from a Stored Point

Retrieves one field from a previously stored global point. Returns `nil` if the point or field does not exist.

```lua
local value = ReadPoint(point_number_or_name, item)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `point_number_or_name` | integer or string | Point index or name |
| `item` | string | Field name: `"X"`, `"Y"`, `"Z"`, `"RX"`, `"RY"`, `"RZ"`, `"UF"`, `"TF"`, `"H"`, `"E"`, `"S"`, `"F"`, `"JRC"`, `"name"` |

```lua
local x = ReadPoint(1, "X")
local y = ReadPoint("GL_Home", "Y")
local z = ReadPoint(1, "Z")
print(x, y, z)
```

---

## Modbus Register Access

Modbus register functions read and write 16-bit (`"W"`) or 32-bit (`"DW"`) registers on the Delta controller's internal memory bus. Both are stubs in lu5.

### ReadModbus — Read a Modbus Register

```lua
local value = ReadModbus(reg_address, size)
```

| Parameter | Type | Values |
|-----------|------|--------|
| `reg_address` | number | Hardware-specific register address |
| `size` | string | `"W"` (16-bit word) or `"DW"` (32-bit double-word) |

**Stub return:** `0`.

```lua
local val = ReadModbus(1000, "W")
local dval = ReadModbus(2000, "DW")
print(val, dval)
```

### WriteModbus — Write a Modbus Register

```lua
WriteModbus(reg_address, size, value)
```

| Parameter | Type | Values |
|-----------|------|--------|
| `reg_address` | number | Hardware-specific register address |
| `size` | string | `"W"` or `"DW"` |
| `value` | number | Value to write |

**Stub:** No hardware effect.

```lua
WriteModbus(1000, "W", 42)
WriteModbus(2000, "DW", 100000)
```

---

## Socket Communication

Socket functions allow Delta scripts to communicate with external devices over TCP/IP. In lu5, POSIX sockets (Linux) and Winsock (Windows) are used for real network I/O. The WebAssembly build stubs all socket calls.

### SocketClass — Create a TCP Client

Connects to a remote TCP server and returns a `DeltaSocket` userdata object.

```lua
local sock = SocketClass(host_ip, port [, spacing [, delimiter [, cmd [, sleeptime [, timeout]]]]])
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `host_ip` | string | — | Server IP address (dotted-decimal) |
| `port` | integer | — | Port number (1–65535) |
| `spacing` | string | `","` | Character used to split received data into a table |
| `delimiter` | string | `"\r\n"` | String appended to each `Send` call |
| `cmd` | string | `""` | Default command |
| `sleeptime` | number | `0.1` | Seconds between repeated sends |
| `timeout` | number | `10` | Receive timeout in seconds |

If the connection fails, the object is returned in the disconnected state. Use `CheckStatus` to detect this.

### SocketServer — Create a TCP Server

Binds a listening port, waits for one incoming client connection, and returns a `DeltaSocket` userdata object backed by the accepted client socket.

```lua
local server = SocketServer(port [, spacing [, delimiter [, cmd [, timeout]]]])
```

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `port` | integer | 3000–10000 | — |
| `spacing` | string | — | `";"` |
| `delimiter` | string | — | `"\r\n"` |
| `cmd` | string | — | `""` |
| `timeout` | number | — | `10` |

### DeltaSocket Methods

Both `SocketClass` and `SocketServer` return a `DeltaSocket` userdata with the following methods.

#### Send

```lua
sock:Send(cmd)
```

Sends `cmd` (string or number) to the remote endpoint, followed by the delimiter. Logs a warning if the socket is not connected.

#### Receive

```lua
local parts = sock:Receive()
```

Reads data from the socket until the delimiter is encountered, strips the delimiter, splits the result by the spacing character, and returns a Lua table of strings. Returns an empty table on WASM or if disconnected.

#### Close

```lua
sock:Close()
```

Closes the TCP connection. Calling `Close` on an already-closed socket is safe.

#### CheckStatus

```lua
local port, status, errcode = sock:CheckStatus()
```

Returns the port number, a status string (`"Connected"` or `"DisConnected"`), and an integer error code (`0` means no error).

### CheckAllStatus — Global Socket Status

Returns the status of all socket objects created in the current session as three parallel Lua arrays.

```lua
local ports, statuses, errcodes = CheckAllStatus()
```

### SocketVersion — Print Implementation Version

```lua
SocketVersion()   -- prints "DeltaSocket v1.0 (lu5 implementation)"
```

### Socket Error Code Constants

The following integer constants are registered as Lua globals for use with `errcode` values:

| Constant | Value | Meaning |
|----------|-------|---------|
| `SOCKET_OK` | `0x0000` | No error |
| `SOCKET_ERR_SESSION` | `0x0001` | Session error |
| `SOCKET_ERR_BUSY` | `0x0002` | Resource busy |
| `SOCKET_ERR_SEND_FAIL` | `0x0003` | Send failure |
| `SOCKET_ERR_PKT_SHORT` | `0x0007` | Packet too short |
| `SOCKET_ERR_ROLE` | `0x0008` | Role mismatch |
| `SOCKET_ERR_CONN_FULL` | `0x0009` | Connection table full |
| `SOCKET_ERR_CHAN` | `0x000A` | Channel error |
| `SOCKET_ERR_NULL_ADDR` | `0x000B` | Null address |
| `SOCKET_ERR_PORT_RANGE` | `0x000C` | Port out of range |
| `SOCKET_ERR_CREATE` | `0x000D` | Socket creation failed |
| `SOCKET_ERR_BIND` | `0x000E` | Bind failed |
| `SOCKET_ERR_LISTEN` | `0x0011` | Listen failed |
| `SOCKET_ERR_CONN_REFUSED` | `0x0012` | Connection refused |
| `SOCKET_ERR_NO_END_CODE` | `0x0031` | No end-of-message code |

---

## Multi-task Cooperative Execution

These functions simulate the Delta controller's hardware multi-tasking model inside lu5's single-threaded Lua environment. Each registered sub-function is called once per `AuxTasks()` invocation via `lua_pcall`, in registration order. This mimics the 15 ms round-robin time-slicing the real controller provides, without requiring threads.

### AuxTasksAdd — Register Sub-Functions

```lua
AuxTasksAdd(func1 [, func2, ..., func10])
```

| Parameter | Type | Notes |
|-----------|------|-------|
| `func1` | function | First task; may include motion commands |
| `func2`…`func10` | function (optional) | Additional tasks; should not include motion commands |

Calling `AuxTasksAdd` again replaces all previously registered functions. Up to 10 functions may be registered at once. All arguments must be functions; a non-function argument raises an error.

### AuxTasks — Execute One Round-Robin Cycle

```lua
AuxTasks()
```

Calls each function registered with `AuxTasksAdd` once, in order. If a sub-function raises an error, the error is logged and execution continues with the next function. Logs a warning if no tasks have been registered.

---

## Platform Caveats

The following table summarises how each DeltaAPI group behaves across the three build targets.

| Feature | Linux | Windows | WASM |
|---------|-------|---------|------|
| Digital I/O (DI/DO/ExtDI/ExtDO) | Stub | Stub | Stub |
| Motion (MovP/MovL/MovJ) | Stub | Stub | Stub |
| Speed/Accel setters | Stores value | Stores value | Stores value |
| DELAY (actual sleep) | `nanosleep` | `Sleep` (1 ms res.) | Stub (immediate) |
| WAIT | Stub (immediate) | Stub (immediate) | Stub (immediate) |
| Point storage (SetGlobalPoint/ReadPoint) | In-memory | In-memory | In-memory |
| Modbus (ReadModbus/WriteModbus) | Stub | Stub | Stub |
| SocketClass / SocketServer | POSIX sockets | Winsock2 (`ws2_32.lib`) | Stub |
| AuxTasks (cooperative multitasking) | Sequential pcall | Sequential pcall | Sequential pcall |

### Windows Socket Notes

On Windows, lu5 uses Winsock2 (`winsock2.h`, linked against `ws2_32.lib`). The Makefile already includes `-lws2_32` for the `win` platform target. No additional setup is required for scripts that use `SocketClass` or `SocketServer` on Windows.

### WASM Notes

The `LU5_WASM` preprocessor macro disables all network system calls. `SocketClass`, `SocketServer`, `Send`, and `Receive` log a warning and return empty/nil values so that the rest of the script can still execute.

---

## Quick-start Example

The following self-contained example exercises all DeltaAPI groups in a single lu5 script. Run it with `lu5 delta_api_demo.lua` (no window is opened).

```lua
-- delta_api_demo.lua
-- Exercises all DeltaAPI function groups in lu5 (no hardware required).

print("=== DeltaAPI Demo ===")

-- ── 1. Digital I/O ──────────────────────────────────────────────────────────
print("\n-- Digital I/O --")
print("DI(1)   =", DI(1))          -- single-pin read
print("DI(1,4) =", DI(1, 4))       -- bitmask read (4 pins)
DO(1, "ON")                         -- set output pin 1 ON
DO(2, "OFF", 0.0)                   -- set output pin 2 OFF (no delay in sim)
print("ExtDI(1,1) =", ExtDI(1, 1)) -- external board input
ExtDO(1, 2, "ON")                   -- external board output

-- ── 2. Motion commands ──────────────────────────────────────────────────────
print("\n-- Motion commands --")
MovP("GL_Home")   -- PTP move to GL_Home
MovL(2)           -- linear move to point #2
MovJ(1, 30.0)     -- rotate joint 1 to 30 degrees

-- ── 3. Speed and acceleration ────────────────────────────────────────────────
print("\n-- Speed / acceleration --")
SpdJ(60)      -- 60 % joint speed
AccJ(40)      -- 40 % joint acceleration
DecJ(40)      -- 40 % joint deceleration
SpdL(800)     -- 800 mm/s linear speed
AccL(5000)    -- 5000 mm/s^2 linear accel
DecL(5000)
Accur("HIGH") -- highest accuracy mode

-- ── 4. Timing and flow control ───────────────────────────────────────────────
print("\n-- Timing --")
DELAY(0.1)                  -- 100 ms pause (real sleep on Linux/Windows)
WAIT("DI", 1, "ON")         -- assumed met immediately in lu5
WAIT("DI", 3, "OFF", 5.0)   -- with timeout

-- ── 5. Point management ──────────────────────────────────────────────────────
print("\n-- Point management --")
SetGlobalPoint(1, "GL_Home",  0.0,    0.0,  150.0)
SetGlobalPoint(2, "GL_Pick",  300.0, -120.0, 50.0, 0.0, 0.0, 0.0)
SetGlobalPoint(3, "GL_Place", 200.0,  150.0, 80.0, 0.0, 0.0, 0.0,
               {0, 0, 0, 0, 0, 0})   -- with JRC table

local x = ReadPoint(1, "X")
local y = ReadPoint("GL_Pick", "Y")
local z = ReadPoint(2, "Z")
print(string.format("GL_Home.X = %g, GL_Pick.Y = %g, GL_Pick.Z = %g", x, y, z))

-- ── 6. Modbus register access ────────────────────────────────────────────────
print("\n-- Modbus --")
WriteModbus(1000, "W", 42)
WriteModbus(2000, "DW", 100000)
local w  = ReadModbus(1000, "W")    -- returns 0 in lu5
local dw = ReadModbus(2000, "DW")
print("ReadModbus W =", w, "  DW =", dw)

-- ── 7. Socket communication ──────────────────────────────────────────────────
print("\n-- Sockets --")
SocketVersion()  -- print version string

-- NOTE: The connect below will fail (no server running) and log a warning;
--       the script continues because SocketClass is fault-tolerant.
local sock = SocketClass("127.0.0.1", 9000, ",", "\r\n", nil, 0.1, 2)
local port, status, err = sock:CheckStatus()
print(string.format("Client socket: port=%d status=%s err=%d", port, status, err))
sock:Close()

-- ── 8. Multi-task cooperative execution ──────────────────────────────────────
print("\n-- AuxTasks --")
local tick = 0

local function task_motion()
    tick = tick + 1
    print("  motion task tick:", tick)
end

local function task_io()
    print("  IO task running")
end

AuxTasksAdd(task_motion, task_io)

-- Run 3 cooperative cycles
for _ = 1, 3 do
    AuxTasks()
end

print("\n=== Demo complete ===")
```
