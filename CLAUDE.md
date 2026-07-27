# CLAUDE.md — OPAL firmware (Optonics fork)

Teensy 4.0 firmware driving the SLS printer's galvo mirrors (XY2-100) and
diode-laser PWM. Fork of opengalvo/OPAL (`upstream` remote); our branch
`feature/diode-laser-control` carries one substantive commit: diode laser
driver, float galvo mapping, serial dead-man, M400→`done` sync.
**This code gates a real Class-4 laser. Pre-alpha quality upstream.**
Before any change, invoke the `laser-thermal-safety` and `opal-gcode-contract`
skills (workspace root `.claude/skills/`).

**Why this repo exists:** commercial galvo controllers are closed boxes; we
forked a pre-alpha open one and are hardening it into something we can stake
the company's parts — and an operator's safety — on. Every guarantee we add
here (true-off laser states, exact motion sync, honest mapping) is a
guarantee the whole platform above it quietly relies on.

## Build / flash / test

```bash
pio run                # build (PlatformIO, [env:teensy40], 600 MHz)
pio run -t upload      # flash over USB
# Native unit tests (host g++, no hardware; commands in each file's header):
g++ -std=c++11 -Wall test/native/test_galvo_map.cpp -o /tmp/t && /tmp/t
g++ -std=c++11 -Wall test/native/test_laser_map.cpp -o /tmp/t && /tmp/t
```
`OPAL.ino` is a banner stub — PlatformIO is the build system. There is no
simulator; anything not covered by the native tests must be reasoned from
source and bench-verified.

## Configuration — the #1 trap

**Live config is `src/Helpers.h`, NOT `src/configuration.h`.**

- `Helpers.h`: `X_MAX_POS_MM`/`Y_MAX_POS_MM` (250 — must equal the slicer's
  `field_size_x/y`), `LASER_MAX` (255, S full-scale), `LASER_RESOLUTION` (12),
  `LASER_IS_DIODE`, `DEFAULT_FEEDRATE`, `BUFFERSIZE` (50), `MAX_VAL` (the
  "field unset" sentinel).
- `configuration.h`'s `X_MAX`/`Y_MAX` are **dead code** (referenced nowhere).
- **Axis-inversion typo is live:** `configuration.h` defines `INVERSE_X` but
  the code tests `AXIS_INVERSE_X` (`main.h:49`), so inversion is silently OFF.
  To invert, define `AXIS_INVERSE_X`, or fix the name mismatch properly.
- Pins: `Pins.h` — laser PSU SSR=2, galvo PSU SSR=3, laser PWM=6. XY2-100 pins
  are hardcoded in `lib/XY2_100/XY2_100.h:52-55` (clock 22, sync 17, X 19, Y 14).

## Execution model (changes must preserve these properties)

- `SerialCMDReader` parses USB `Serial` line-by-line; replies `ok` per line
  (G-lines also echo chip temp); enqueues into a 50-slot circular buffer and
  stops reading when full (that IS the flow control).
- `MotionMGR::tic()` pops **one command only while IDLE**; every move returns
  to IDLE before the next pop. This strict sequentiality is why `M400` →
  `done` is an exact motion-complete token for the Klipper host. Any
  concurrency here breaks per-layer sync platform-wide.
- Supported: G0 (teleport, laser untouched), G1 (interpolated at F mm/s),
  G28, G90/G91, M3/M4 (laser on, S duty), M5 (off), M9 (forward to Serial5),
  M17/M18 (galvo SSR), M80/M81 (laser PSU SSR), M400. **G2/G3 parse but do
  nothing. Z parses but drives nothing.**
- Coordinate map: `GalvoMap.h::mmToGalvoCount` — float scaling
  (~3.8 µm/count), saturates at field edges (no wrap). The old ~1 mm integer
  `map()` limit is upstream-only; don't reintroduce it.
- Two serial ports: USB `Serial` = host G-code; `Serial5` = secondary link
  (emits `G28` at boot, receives `M9` payloads). Don't conflate them.

## Laser-off guarantees (the safety core — never weaken)

1. Boot: pin 6 driven LOW before `laser->begin()`; SSRs LOW; laser disabled.
2. Every `tic()` with `CURRENT_LASERENABLED == false` forces PWM 0.
3. Serial dead-man: beam cut if laser on + IDLE + buffer empty + no serial
   line for `LASER_SERIAL_TIMEOUT_MS` (1000 ms). The IDLE+empty gating
   prevents false trips mid-layer — keep it, and keep `lastSerialMillis`
   updated on **every** received newline if you touch `handleSerial`.
4. `S0` maps to true 0 % duty (no CO2 tickle floor — diode laser).

Known gaps (documented, not yet closed): G0 does **not** force laser off — the
slicer's M5-before-jump discipline is the only guard (`LASER_G0_OFF_G1_ON`
exists but is commented out); dead-man can't fire on a Teensy hard-fault;
malformed input leaves laser state unchanged. Don't add features that rely on
these gaps being closed, and don't close them casually — the slicer and
klipper host assume current semantics (see `docs/OPAL_INTEGRATION.md` in the
slicer repo).

## Editing rules

- Keep `GalvoMap.h`/`LaserMap.h` pure and covered by the native tests; extend
  those tests for any mapping change (they pin the S204≈80 % and saturation
  behaviour).
- `processGcodes()` does per-command `new`/`delete` (heap churn) — known debt;
  if you touch it, prefer a fixed pool, and preserve strict sequentiality.
- Anything that changes the dialect (new M-code, timing, handshake) must be
  updated in the same change in: slicer `docs/OPAL_INTEGRATION.md` +
  generator, klipper `galvo/opal_link.py`, and the `opal-gcode-contract`
  skill + checker.
- `docs/future_work.md` is the deferred-work ledger (galvo settling delays,
  runtime calibration, etc.) — append there rather than half-implementing.
- Uncommitted state in the repo (README edits, untracked `docs/`) may belong
  to someone else — don't bundle it into your commits.
