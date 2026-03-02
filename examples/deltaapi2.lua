-- DeltaAPI-2 endpoints demo (lu5 binding)
--
-- This example is designed to run even without real robot hardware.
-- The current DeltaAPI-2 backend is stubbed/deterministic:
-- - DI/ExtDI always return "OFF"
-- - ReadModbus always returns 0
-- - WAIT for non-zero Modbus targets errors to avoid infinite blocking

function setup()
  createWindow(600, 200)
  frameRate(10)

  print("DI(1) ->", DI(1))
  DO(1, "ON", 0.05)
  print("ReadModbus(0x1010, 'DW') ->", ReadModbus(0x1010, "DW"))

  function Motion()
    -- motion commands are stubbed; will error if called
    -- MovP(1)
  end

  function Output1()
    DO(1, "ON")
    DO(1, "OFF")
  end

  function Output2()
    DO(2, "ON")
    DO(2, "OFF")
  end

  AuxTasksAdd(Motion, Output1, Output2)
end

t = 0

function draw()
  background("purple")
  textSize(14)
  text("DeltaAPI-2 demo (stub backend). DI(1)=" .. DI(1), 10, 30)

  -- cooperative multi-task tick
  AuxTasks()

  t = t + deltaTime
  if t > 1.0 then
    -- WAIT for IO OFF will complete immediately in stub backend
    WAIT("DI", 1, "OFF", 100)
    t = 0
  end

  -- show that DELAY blocks (kept short)
  -- DELAY(0.01)
end
