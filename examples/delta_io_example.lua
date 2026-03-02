-- delta_io_example.lua
--
-- Demonstrates Digital I/O functions from the DeltaAPI:
--   DI, DO, ExtDI, ExtDO
--
-- Run with:   lu5 examples/delta_io_example.lua
--
-- Because lu5 has no physical Delta controller, all I/O calls are stubs.
-- DI / ExtDI always return "OFF" or 0, and DO / ExtDO log a [WARN] message.

print("=== Delta Digital I/O Example ===")
print()

-- ── DI: Single-pin read ────────────────────────────────────────────────────
-- Read digital input pin 1.  Returns "ON" or "OFF".
local pin1 = DI(1)
print("DI(1) =", pin1)          -- "OFF" in lu5

-- Read by pin name (string form)
local valve = DI("VALVE_IN")
print("DI('VALVE_IN') =", valve)

-- ── DI: Multi-pin bitmask read ─────────────────────────────────────────────
-- Read 8 consecutive input pins starting at pin 1.
-- Returns an integer bitmask (stub: always 0).
local bits = DI(1, 8)
print("DI(1, 8) bitmask =", bits)

print()

-- ── DO: Single-pin output ──────────────────────────────────────────────────
-- Turn output pin 1 ON.
print("Setting DO(1, 'ON') ...")
DO(1, "ON")

-- Turn output pin 2 OFF immediately.
print("Setting DO(2, 'OFF') ...")
DO(2, "OFF")

-- Turn output pin 3 ON after a 0.5-second delay.
-- (DELAY stub in WASM; real sleep on Linux/Windows)
print("Setting DO(3, 'ON', 0.5) with 0.5 s delay ...")
DO(3, "ON", 0.5)

-- ── DO: Multi-pin bitmask output ───────────────────────────────────────────
-- Set output pins 1-4 using a bitmask (0b0101 = pins 1 and 3 ON).
print("Setting DO(1, 4, 5) multi-pin bitmask ...")
DO(1, 4, 5)   -- 5 == 0b0101: pin1=ON, pin2=OFF, pin3=ON, pin4=OFF

print()

-- ── ExtDI / ExtDO: External expansion board ────────────────────────────────
-- External boards extend the I/O capacity beyond the onboard pins.

-- Read pin 3 on external board at address 1.
local ext_in = ExtDI(1, 3)
print("ExtDI(1, 3) =", ext_in)      -- "OFF" in lu5

-- Set pin 5 on external board 1 to ON.
print("Setting ExtDO(1, 5, 'ON') ...")
ExtDO(1, 5, "ON")

-- Set pin 6 on external board 2 to OFF with a 0.2-second delay.
print("Setting ExtDO(2, 6, 'OFF', 0.2) ...")
ExtDO(2, 6, "OFF", 0.2)

print()
print("=== I/O example complete ===")
print("Note: all outputs above are stubs. [WARN] lines from lu5 are expected.")
