# DeltaAPI — Delta Robot Scripting in lu5

## Overview

lu5 includes a complete implementation of the Delta Robot API (DeltaAPI), which makes all Delta controller scripting functions available as first-class Lua globals inside every lu5 sketch. This allows Delta robot programs to be developed, simulated, and debugged on any desktop machine without requiring physical hardware.

The DeltaAPI is modelled on the Delta controller scripting language specification and covers nine functional groups: digital I/O, motion commands, speed and acceleration control, timing and flow control, point management, Modbus register access, socket communication, and cooperative multi-task execution.

## Stub Behaviour

Because lu5 runs on a desktop without a physical Delta controller, every function that would interact with hardware is implemented as a _stub_. Each stub validates its arguments exactly as the real controller would, emits a `[WARN]` log message via `LU5_WARN`, and returns a safe default value (typically `0`, `"OFF"`, or nothing). This design lets Delta programs run end-to-end in simulation while making every hardware-dependent call clearly visible in the console output.

The only exception is `DELAY`, which performs a real operating-system sleep so that time-sensitive Delta scripts retain correct pacing on Linux and Windows.

## Function Groups

### Group 1: Digital I/O (`delta_io.c`)

| Function | Signature | Stub Return |
|----------|-----------|-------------|
| `DI` | `DI(pin)` → string; `DI(pin, length)` → integer | `"OFF"` / `0` |
| `DO` | `DO(pin, status [, delay])` or `DO(pin, length, mask [, delay])` | nothing |
| `ExtDI` | `ExtDI(address, pin)` → string | `"OFF"` |
| `ExtDO` | `ExtDO(address, pin, status [, delay])` | nothing |

`DI` accepts either an integer pin number (1–24) or a string pin name. `DO` accepts pin numbers 1–12 and status strings `"ON"` or `"OFF"`. The multi-pin bitmask forms of both `DI` and `DO` accept a `length` parameter and encode all pin states in a single integer. `ExtDI` and `ExtDO` extend I/O to expansion boards addressed by a positive integer index.

### Group 2: Motion Commands (`delta_motion.c`)

| Function | Signature |
|----------|-----------|
| `MovP` | `MovP(point [, modifiers...])` |
| `MovL` | `MovL(point [, modifiers...])` |
| `MovJ` | `MovJ(joint, degree [, modifiers...])` |

`MovP` and `MovL` accept a point name (string) or point number (integer ≥ 1). They differ only in trajectory type on real hardware (PTP vs. straight-line Cartesian). `MovJ` rotates a single joint (1–6) to a target angle in degrees (−360 to +360). All three accept optional modifier arguments (SPD, ACC, DEC, PASS, Offset) which are silently accepted in stubs.

### Group 3: Speed and Acceleration (`delta_speed.c`)

| Function | Parameter | Range | Default |
|----------|-----------|-------|---------|
| `SpdJ` | speed % | 0.001–100 | 10 |
| `AccJ` | accel % | 0.001–100 | 10 |
| `DecJ` | decel % | 0.001–100 | 10 |
| `SpdL` | speed mm/s | 1–2000 | 100 |
| `AccL` | accel mm/s² | 1–25000 | 10 |
| `DecL` | decel mm/s² | 1–25000 | 10 |
| `Accur` | mode string | HIGH / STANDARD / MEDIUM / ROUGH / MAXROUGH | — |

All setters validate their range and store the value in module-level statics. `Accur` optionally takes a second argument `"CART"` as a Cartesian qualifier. Unlike motion commands, speed and accuracy setters are not pure stubs — they store values that motion commands would read on real hardware.

### Group 4: Timing and Flow Control (`delta_timing.c`)

`DELAY(seconds)` pauses execution for the given duration (minimum 0.001 s). On Linux it uses `nanosleep`; on Windows it uses `Sleep` (1 ms minimum resolution); in WebAssembly builds it logs a warning and returns immediately.

`WAIT(type, ...)` blocks until a hardware condition is met. In lu5 all conditions are assumed to be met immediately. The function accepts both the `DI`/`DO` form (`WAIT("DI", pin, "ON"|"OFF" [, timeout])`) and a Modbus variable form.

### Group 5: Point Management (`delta_point.c`)

`SetGlobalPoint(number, name, X, Y, Z [, RZ [, RY [, RX [, ...]]]] [, JRC])` stores a robot pose in the Lua global table `_delta_points`. Point numbers must be in the range 1–1000. Names that do not start with `"GL_"` trigger a warning because the real controller requires the prefix. Optional rotation axes and a JRC (Joint Reference Configuration) table may be appended.

`ReadPoint(point, item)` retrieves one named field from a stored point. The point may be specified by integer index or by its string name. Valid items are `"X"`, `"Y"`, `"Z"`, `"RX"`, `"RY"`, `"RZ"`, `"UF"`, `"TF"`, `"H"`, `"E"`, `"S"`, `"F"`, `"JRC"`, and `"name"`. Returns `nil` when the point or field is not found.

### Group 6: Modbus Register Access (`delta_modbus.c`)

`ReadModbus(address, size)` and `WriteModbus(address, size, value)` access 16-bit (`"W"`) or 32-bit (`"DW"`) Modbus registers on the Delta controller's internal memory bus. Both are stubs in lu5. `ReadModbus` always returns `0`; `WriteModbus` logs a warning and discards the value.

### Group 7: Socket Communication (`delta_socket.c`)

Socket functions implement the Delta controller's free-communication protocol over TCP/IP. On Linux, POSIX sockets are used. On Windows, Winsock2 (`winsock2.h`, linked against `ws2_32.lib`) is used. On WebAssembly, all network calls are stubbed.

`SocketClass(ip, port [, spacing [, delimiter [, cmd [, sleeptime [, timeout]]]]])` creates a TCP client socket and attempts to connect to the specified server. The function returns a `DeltaSocket` userdata object even if the connection fails, so the caller can inspect the status via `CheckStatus()`.

`SocketServer(port [, spacing [, delimiter [, cmd [, timeout]]]])` binds a listening port (3000–10000), waits for one incoming connection, and returns a `DeltaSocket` backed by the accepted client socket.

Each `DeltaSocket` object exposes four methods:

- `sock:Send(cmd)` — sends a string or number followed by the configured delimiter.
- `sock:Receive()` — reads until the delimiter, splits by the spacing character, and returns a Lua table of strings.
- `sock:Close()` — closes the TCP connection; safe to call multiple times.
- `sock:CheckStatus()` — returns `port, statusString, errcode`.

`CheckAllStatus()` returns three parallel Lua arrays (ports, statuses, error codes) for all socket objects created in the current session. `SocketVersion()` prints the implementation version string.

Fourteen named error code constants (`SOCKET_OK`, `SOCKET_ERR_SESSION`, …, `SOCKET_ERR_NO_END_CODE`) are registered as Lua globals.

### Group 8: Cooperative Multi-task Execution (`delta_tasks.c`)

`AuxTasksAdd(func1 [, func2, ..., func10])` registers up to ten Lua functions for cooperative execution. All arguments must be functions; a non-function argument raises a Lua error. Calling `AuxTasksAdd` again replaces all previously registered functions.

`AuxTasks()` calls each registered function once, in registration order, using `lua_pcall` so that errors in one task do not abort the others. This simulates the Delta controller's 15 ms round-robin time-slicing inside lu5's single-threaded environment.

## Platform Compatibility

| Feature | Linux | Windows | WASM |
|---------|-------|---------|------|
| Digital I/O | Stub | Stub | Stub |
| Motion commands | Stub | Stub | Stub |
| Speed / accel setters | In-memory | In-memory | In-memory |
| `DELAY` (real sleep) | `nanosleep` | `Sleep` (1 ms res.) | Stub |
| `WAIT` | Stub | Stub | Stub |
| Point storage | In-memory Lua table | In-memory Lua table | In-memory Lua table |
| Modbus | Stub | Stub | Stub |
| TCP sockets | POSIX | Winsock2 | Stub |
| Cooperative tasks | Sequential `pcall` | Sequential `pcall` | Sequential `pcall` |

## Source Files

| File | Purpose |
|------|---------|
| `src/bindings/delta_io.c` | `DI`, `DO`, `ExtDI`, `ExtDO` |
| `src/bindings/delta_motion.c` | `MovP`, `MovL`, `MovJ` |
| `src/bindings/delta_speed.c` | `SpdJ`, `AccJ`, `DecJ`, `SpdL`, `AccL`, `DecL`, `Accur` |
| `src/bindings/delta_timing.c` | `DELAY`, `WAIT` |
| `src/bindings/delta_point.c` | `SetGlobalPoint`, `ReadPoint` |
| `src/bindings/delta_modbus.c` | `ReadModbus`, `WriteModbus` |
| `src/bindings/delta_socket.c` | `SocketClass`, `SocketServer`, `CheckAllStatus`, `SocketVersion`, `DeltaSocket` methods |
| `src/bindings/delta_tasks.c` | `AuxTasksAdd`, `AuxTasks` |
| `src/lu5_bindings.c` | Central registration of all DeltaAPI globals |

## Example Sketches

Three ready-to-run example Lua sketches are provided under `examples/`:

- `examples/delta_io_example.lua` — Exercises `DI`, `DO`, `ExtDI`, `ExtDO`.
- `examples/delta_motion_example.lua` — Exercises speed setters, `MovP`, `MovL`, `MovJ`, `DELAY`, and `WAIT`.
- `examples/delta_advanced_example.lua` — Exercises point management, Modbus, socket communication, and cooperative tasks.

A comprehensive single-file demo is also embedded in `docs/delta_api.md` as the Quick-start Example.

Run any sketch with:

```bash
lu5 examples/delta_io_example.lua
lu5 examples/delta_motion_example.lua
lu5 examples/delta_advanced_example.lua
```
