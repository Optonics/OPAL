# Firmware simulator

The real `setup()` and `loop()` of `src/main.cpp`, with `MotionMGR`, `SerialCMDReader`, `Helpers` and `DiodeLaserCtrl`, compiled for a PC. Time is simulated. The USB port, the laser pin and the galvo output are recorded with their times. Needs `g++` only; `gcovr` for the coverage report.

```
sh test/sim/run.sh                  behaviour tests, address and undefined-behaviour sanitizers
sh test/sim/run.sh --coverage       the same, then line and branch coverage (MIN_LINE_COVERAGE gates it)
sh test/sim/run.sh --bench          the laser timing benchmark (--quick for a short grid)
```

## What the model assumes

Each assumption is a bench measurement nobody has taken yet. Change the number in `sim.h` when it is measured.

| Assumption | Value | How to measure on the board |
|---|---|---|
| One pass of `loop()` | 2 us | Toggle a free pin at the top of `loop()`, read the period on a scope. The 4 MHz XY2-100 interrupt takes most of the CPU. |
| A PWM duty change reaches the pin | at the next PWM period (1 ms at 1 kHz) | Scope the laser pin (6) and a pin toggled at the `analogWrite`. The Teensy 4 FlexPWM reloads its compare registers once per period. |
| Mirror lag | 0 or 150 us | Drive a 10 mm `G0`, read the galvo position feedback. |
| Host round trip, `ok` to the next line | 0.25 to 2 ms, +/-30 % | Log the time of each `send_line` in `osls1_klipper/galvo/opal_link.py` on the CM4. |

## Lines the tests do not reach

Coverage stops short of 100 % on lines that nothing can execute. They are listed here, not hidden with exclusion markers in the firmware.

| File | Lines | Why |
|---|---|---|
| `src/SerialCMDReader.h` | the `return -1` of `has_command_at`, the `return 0` of `search_string` | Called only after `has_command` found the letter. |
| `src/SerialCMDReader.cpp` | the `M9` fallback without a position | Same reason: the `M` was found. |
| `src/SerialCMDReader.cpp` | `stop()` | Nothing calls it. |
| `src/main.cpp` | `Serial5: BUFFER OVERRUN` | `ReadSerial5` ends a line at `COMMAND_SIZE - 3`, before the overrun check can be true. |
| `src/MotionMGR.cpp` | `getStatus()`, the `default` of the status switch | Nothing calls `getStatus`; the status is only ever `IDLE` or `INTERPOLATING`. |
| `lib/LaserController/DiodeLaserCtrl.cpp` | `update()` without an argument | Nothing calls it (its own comment says so). |
