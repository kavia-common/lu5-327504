-- delta_advanced_example.lua
--
-- Demonstrates advanced DeltaAPI features:
--   Point management  : SetGlobalPoint, ReadPoint
--   Modbus            : ReadModbus, WriteModbus
--   Socket client     : SocketClass, Send, Receive, CheckStatus, Close
--   Socket utilities  : CheckAllStatus, SocketVersion
--   Cooperative tasks : AuxTasksAdd, AuxTasks
--
-- Run with:   lu5 examples/delta_advanced_example.lua
--
-- No hardware or network server is required.  Socket connections will fail
-- (no server running) and SocketClass will return a DisConnected object —
-- this is normal and is handled gracefully in the example.

print("=== Delta Advanced Features Example ===")
print()

-- ─────────────────────────────────────────────────────────────────────────────
-- 1. Point Management
-- ─────────────────────────────────────────────────────────────────────────────
print("-- Point Management --")

-- Define a set of named global points.
-- The name must begin with "GL_" to match Delta controller convention.
SetGlobalPoint(1,  "GL_Home",   0.0,    0.0,  200.0)
SetGlobalPoint(2,  "GL_Pick1",  350.0, -100.0, 30.0, 0.0, 0.0, 0.0)
SetGlobalPoint(3,  "GL_Pick2",  350.0,  100.0, 30.0, 0.0, 0.0, 0.0)
SetGlobalPoint(10, "GL_Place",  -80.0,  200.0, 30.0, 0.0, 0.0, 0.0,
               {0, 0, 0, 0, 0, 0})   -- 6-axis point with JRC table

-- Read fields back
local home_x = ReadPoint(1, "X")
local home_y = ReadPoint(1, "Y")
local home_z = ReadPoint(1, "Z")
print(string.format("GL_Home  X=%.1f  Y=%.1f  Z=%.1f", home_x, home_y, home_z))

-- Look up a point by name instead of number
local pick1_y = ReadPoint("GL_Pick1", "Y")
print(string.format("GL_Pick1 Y=%.1f", pick1_y))

-- A missing point or field returns nil
local missing = ReadPoint(999, "X")
print("ReadPoint(999, 'X') =", missing)  -- nil

print()

-- ─────────────────────────────────────────────────────────────────────────────
-- 2. Modbus Register Access
-- ─────────────────────────────────────────────────────────────────────────────
print("-- Modbus Registers --")

-- Write a 16-bit word to register 1000
WriteModbus(1000, "W", 42)
print("WriteModbus(1000, 'W', 42) done (stub)")

-- Write a 32-bit double-word to register 2000
WriteModbus(2000, "DW", 99999)
print("WriteModbus(2000, 'DW', 99999) done (stub)")

-- Read back (always returns 0 in lu5 because there is no Modbus hardware)
local w  = ReadModbus(1000, "W")
local dw = ReadModbus(2000, "DW")
print(string.format("ReadModbus 1000/W=%d  2000/DW=%d  (both 0 in lu5)", w, dw))

print()

-- ─────────────────────────────────────────────────────────────────────────────
-- 3. Socket Communication
-- ─────────────────────────────────────────────────────────────────────────────
print("-- Socket Communication --")

-- Print the implementation version string
SocketVersion()

-- ── TCP Client ────────────────────────────────────────────────────────────
-- Attempt to connect to a local echo server (will fail gracefully in lu5).
-- Parameters: IP, port, spacing, delimiter, cmd, sleeptime, timeout
local client = SocketClass("127.0.0.1", 9000, ",", "\r\n", nil, 0.1, 2)

local c_port, c_status, c_err = client:CheckStatus()
print(string.format("Client: port=%d  status=%s  err=%d",
      c_port, c_status, c_err))

if c_status == "Connected" then
    -- Only reached when a real server is running at 127.0.0.1:9000
    client:Send("HELLO")
    local reply = client:Receive()
    print("Reply table length:", #reply)
end

client:Close()
print("Client socket closed.")
print()

-- ── CheckAllStatus ────────────────────────────────────────────────────────
-- Inspect every socket object created so far.
local ports, statuses, errs = CheckAllStatus()
print("CheckAllStatus:")
for i = 1, #ports do
    print(string.format("  [%d] port=%d  status=%s  err=%d",
          i, ports[i], statuses[i], errs[i]))
end

-- ── Socket Error Constants ─────────────────────────────────────────────────
-- These constants are registered automatically.  Use them to interpret err codes.
print()
print("Socket error code constants (sample):")
print("  SOCKET_OK          =", SOCKET_OK)
print("  SOCKET_ERR_SESSION =", SOCKET_ERR_SESSION)
print("  SOCKET_ERR_CREATE  =", SOCKET_ERR_CREATE)

print()

-- ─────────────────────────────────────────────────────────────────────────────
-- 4. Cooperative Multi-task Execution
-- ─────────────────────────────────────────────────────────────────────────────
print("-- Cooperative Multi-task Execution --")
print()

-- In the Delta controller, AuxTasksAdd registers up to 10 sub-functions that
-- are run in round-robin 15 ms slices.  In lu5 each call to AuxTasks() runs
-- all functions once, sequentially (single-threaded simulation).

-- Shared state between tasks
local cycle      = 0
local output_on  = false

local function task_motion()
    -- First task: allowed to issue motion commands
    cycle = cycle + 1
    print(string.format("  [task_motion] cycle=%d -- MovP(GL_Home)", cycle))
    MovP("GL_Home")
end

local function task_io()
    -- Second task: toggles a digital output
    output_on = not output_on
    local state = output_on and "ON" or "OFF"
    print(string.format("  [task_io]     cycle=%d -- DO(1, '%s')", cycle, state))
    DO(1, state)
end

local function task_monitor()
    -- Third task: reads sensor data
    local di_state = DI(5)
    print(string.format("  [task_monitor] cycle=%d -- DI(5)=%s", cycle, di_state))
end

-- Register all three tasks
AuxTasksAdd(task_motion, task_io, task_monitor)

-- Execute 4 cooperative cycles (mimics the main program loop on the controller)
print("Running 4 cooperative task cycles:")
for i = 1, 4 do
    print(string.format("\n  ── Cycle %d ──", i))
    AuxTasks()
end

print()
print("=== Advanced example complete ===")
