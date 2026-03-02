-- delta_motion_example.lua
--
-- Demonstrates motion commands and speed/acceleration settings from the DeltaAPI:
--   SpdJ, AccJ, DecJ, SpdL, AccL, DecL, Accur
--   MovP, MovL, MovJ
--   DELAY, WAIT
--
-- Run with:   lu5 examples/delta_motion_example.lua
--
-- Hardware is not required.  All motion commands are stubs that validate
-- arguments and emit [WARN] messages.  DELAY performs an actual sleep
-- (on Linux/Windows) so the timing portion of the example behaves correctly.

print("=== Delta Motion Example ===")
print()

-- ─── Define target points before any motion ───────────────────────────────
SetGlobalPoint(1, "GL_Home",  0.0,    0.0,  200.0)
SetGlobalPoint(2, "GL_Pick",  350.0, -100.0, 30.0, 0.0, 0.0, 0.0)
SetGlobalPoint(3, "GL_Place", 200.0,  180.0, 30.0, 0.0, 0.0, 0.0)
print("Points defined: GL_Home, GL_Pick, GL_Place")
print()

-- ─── Speed and acceleration profile ──────────────────────────────────────
-- Joint motion settings (percentage of rated speed)
SpdJ(50)    -- 50 % of maximum joint speed
AccJ(30)    -- 30 % acceleration for joint moves
DecJ(30)    -- 30 % deceleration for joint moves

-- Linear (Cartesian) motion settings (absolute SI units)
SpdL(600)   -- 600 mm/s maximum linear speed
AccL(3000)  -- 3000 mm/s^2 acceleration
DecL(3000)  -- 3000 mm/s^2 deceleration

-- Positioning accuracy mode
Accur("HIGH")

print("Motion profile: SpdJ=50%, SpdL=600mm/s, Accur=HIGH")
print()

-- ─── Point-to-point (PTP) motion ─────────────────────────────────────────
print("-- MovP: point-to-point motion --")

-- Move to point by string name
print("MovP('GL_Home') ...")
MovP("GL_Home")

-- Move to point by integer index
print("MovP(2) -- GL_Pick ...")
MovP(2)

-- Brief delay to simulate dwell time at pick position
DELAY(0.2)

print("MovP(3) -- GL_Place ...")
MovP(3)
DELAY(0.1)

-- Return home
MovP(1)
print()

-- ─── Linear (Cartesian) motion ────────────────────────────────────────────
print("-- MovL: linear Cartesian motion --")

-- Use a lower speed for linear moves near a sensitive area
SpdL(200)

print("MovL('GL_Pick') along straight path ...")
MovL("GL_Pick")

print("MovL('GL_Place') along straight path ...")
MovL("GL_Place")

-- Restore speed
SpdL(600)
print()

-- ─── Single-axis joint motion ─────────────────────────────────────────────
print("-- MovJ: single-axis joint motion --")

-- Rotate joint 1 to 45 degrees
print("MovJ(1, 45) ...")
MovJ(1, 45.0)

-- Rotate joint 2 to -90 degrees
print("MovJ(2, -90) ...")
MovJ(2, -90.0)

-- Return joint 1 to 0
print("MovJ(1, 0) ...")
MovJ(1, 0.0)
print()

-- ─── Timing and flow control ──────────────────────────────────────────────
print("-- DELAY and WAIT --")

-- Pause for 0.5 seconds (real sleep on Linux / Windows; no-op on WASM)
print("DELAY(0.5) ...")
DELAY(0.5)
print("Delay complete.")

-- WAIT for a DI pin condition (assumed met immediately in lu5)
print("WAIT('DI', 1, 'ON') -- assumed met immediately")
WAIT("DI", 1, "ON")

-- WAIT with timeout
print("WAIT('DI', 3, 'OFF', 5.0) -- timeout 5 s")
WAIT("DI", 3, "OFF", 5.0)

print()
print("=== Motion example complete ===")
print("Note: motion commands are stubs in lu5. [WARN] lines are expected.")
