# OPAL firmware — future work (galvo + laser quality/safety)

Deferred firmware improvements identified while comparing OPAL against the
SLS4All.Compact galvo/laser pipeline. **None of these are implemented yet** —
the current firmware works for single-layer bring-up. They are listed roughly in
priority order. File/line references are to the firmware as of this writing.

## Context: we have an F-theta lens

OPAL maps `mm -> 16-bit DAC count` with a **linear** scale
(`src/GalvoMap.h`, `mmToGalvoCount()`). For a bare two-mirror galvo over a flat
field this would be wrong (you'd get pincushion/trapezoid distortion and a
curved focal plane), which is why SLS4All carries a full trigonometric bed
projection with per-axis correction polynomials.

**We have an F-theta lens**, which makes focal-plane displacement linear in scan
angle and keeps focus flat across the field. Galvo angle is linear in DAC count,
so bed position is linear in DAC count — i.e. **the linear map is the correct
model for our optics.** We therefore do NOT need a distortion-correction
polynomial. What we DO need is correct scale/centering (item 1) plus residual
edge correction only if measurement demands it.

---

## 1. Scale + center calibration  (HIGH — do first)

**Current:** `src/configuration.h` hard-codes `X_MAX 250` / `Y_MAX 250` (mm) as
the full-DAC-swing field size, with `INVERSE_X` set. `GalvoMap.h` scales
`(pos / maxMm) * 65535`. If the real field at full DAC swing isn't exactly
250 mm, or the field isn't centered, **every coordinate is mis-scaled** — a
commanded 100 mm square won't be 100 mm.

**Fix:** add a per-axis **gain + offset** calibration (2 points per axis is
enough with an F-theta lens — no higher-order terms needed):
- Mark a known reference (e.g. a 100 mm square) at low power on anodized stock or
  paper, measure actual size and center offset.
- Store `scaleX, scaleY, offsetX, offsetY` (and keep `INVERSE_X`) and apply in
  `mmToGalvoCount()`.
- Optionally expose as a runtime command so calibration doesn't need a reflash.

**Residual (LOW, optional):** a two-mirror galvo + non-telecentric F-theta lens
still has a small residual pincushion (sub-percent) at the corners. Only add a
small correction table if edge measurement exceeds tolerance. Start linear.

## 2. Galvo settling / laser timing delays  (HIGH — biggest mark-quality win)

**Current:** `MotionMGR::tic()` (`src/MotionMGR.cpp`) sets the interpolated galvo
position and the laser power in the same step, with **no lead/lag compensation**.
The physical mirror lags the commanded position, so:
- the laser is already firing at the start of a vector before the mirror has
  arrived -> **start-of-vector burn-in / blooming**;
- at corners the beam cuts the corner / rounds it;
- the laser turns off at the commanded vector end while the mirror is still
  settling -> **tail marking**.

This is independent of the lens and gets worse with scan speed.

**Fix:** implement the standard galvo marking delays (this is what SLS4All's
`CompensatePwmLatency` + `LaserOffMinDuration` do):
- **laser-on delay** — wait after starting a mark move before enabling the beam;
- **laser-off delay** — keep position settling after the beam is cut;
- **jump delay** — settle dwell after a rapid (see item 4);
- **mark / polygon delay** — short dwell at vertices between connected segments.
Make each a tunable constant (start) and expose for calibration.

## 3. Enforce laser-off on G0  (HIGH — safety, small change)

**Current:** `processGcode()` case 0 (`G0`) only sets XY; it does **not** touch
laser state. Only `M3`/`M5` gate the beam (`CURRENT_LASERENABLED`). Today the
slicer always emits `M5` before each `G0` travel and `M3` before each mark
(verified against real sliced output), so it's safe **in practice** — but the
firmware trusts the slicer on a Class-4 laser.

**Fix:** enable the existing `LASER_G0_OFF_G1_ON` path (`#define` is commented in
`configuration.h`), or have the `G0` handler force `CURRENT_LASERENABLED = false`.
Defense in depth: a single missing `M5` should never leave the beam on during a
rapid.

## 4. Jump (rapid) settle handling  (MEDIUM)

**Current:** in `interpolateMove()`, a `G0` (`CURRENT_CODE == 0`) teleports
straight to the target in a single tic and immediately returns to `IDLE`, so the
next `G1` can begin before the mirror has physically reached the jump endpoint.

**Fix:** add a jump-settle dwell proportional to jump distance before the next
move is allowed to start (couples with the jump delay in item 2). Optionally cap
the commanded feedrate so slicer travels like `F5000` mm/s don't command motion
faster than the galvo's physical bandwidth (the firmware would otherwise "finish"
the interpolation while the mirror lags far behind).

## 5. Eliminate per-command heap churn  (MEDIUM — long-print robustness)

**Current:** `processGcodes()` does `currentGcode = new GCode(...)` then
`delete currentGcode` for **every** command. A layer is thousands of vectors, so
this is thousands of malloc/free per layer on the Teensy -> heap fragmentation
risk over a long build.

**Fix:** process the popped `GCode` as a stack value, or use a fixed pool. No
per-command dynamic allocation.

## 6. Minor cleanups  (LOW)

- `processMcode()` `M9` has `// TODO: Should set a 'WaitForM400Sync' Flag` —
  finish or remove.
- `calculateMoveLengthNanos()` carries `//TODO: Verify unit conversions`; F is in
  mm/s (consistent with the slicer output) — confirm and drop the TODO.

---

## What is already good (don't regress)

- **Tier-1 serial dead-man** in `tic()` (cut beam if laser-on + IDLE +
  empty-buffer + serial silence > `LASER_SERIAL_TIMEOUT_MS`). Correctly gated so
  it can't false-trip mid-layer.
- **`M400` -> `"done"`** motion-complete token: accurate because commands run
  strictly sequentially and every move returns to IDLE before the next pops.
- **`LaserMap.h`** maps `S=0 -> 0%` duty (true-off on every laser-off), no CO2
  tickle floor — correct for a PWM diode.
