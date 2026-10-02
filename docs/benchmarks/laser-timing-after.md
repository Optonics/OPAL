# Laser timing: after the timing fixes

Firmware: `feature/diode-laser-timing`. Same simulator, grid and assumptions as [the baseline](laser-timing-baseline.md).

What changed in the firmware:

- The beam fires only while the mirror scans a G1. `M3` arms it; it lights when the G1 moves and goes dark when the G1 ends, unless the next queued command is another G1 (the corners of a contour). A host slow to send the next line now leaves the beam dark, not lit on one spot.
- `G0` and `G28` disarm the beam, `M5` or not. After a jump the next command waits `JUMP_DELAY_MIN_US` plus `JUMP_DELAY_PER_MM_US` per mm, so the mirror has arrived.
- The laser pin goes dark at once (GPIO low) and lights at once (immediate duty load and a new PWM period).
- A whole line is read per loop pass, and no temperature or debug line is sent between commands.

Results, against the baseline:

1. Late off: 1 us everywhere (baseline up to 2963 us). Lit and standing still: 4 us per line (baseline up to 3563 us).
2. No streak in any jump, with either host (baseline up to 19.5 mm per line).
3. A host with 40 lines in flight is now safe and 7.5 times faster on short lines: 21 ms against 161 ms for a layer of 1 mm lines.
4. Still open: lines that take longer than one 0.8 ms on-phase are 20 % dark, the gaps of the 1 kHz PWM at S204. A higher PWM frequency closes them, if the diode driver accepts it: check its datasheet.
5. With mirror lag (the 150 us rows) the defaults are too short. The jump wait of 100 us plus 4 us per mm ends before a lagging mirror arrives, so the beam lights during the last part of the jump: 0.3 to 1.8 mm per line. A first-order mirror is within 5 % of its target after three time constants. Thus set `JUMP_DELAY_MIN_US` to about three times the measured lag, and `LASER_ON_DELAY_US` and `LASER_OFF_DELAY_US` to about the lag. All three wait for the bench measurement in `test/sim/README.md`.


Hatch layer of 40 lines, M5/G0/M3 S204/G1 per line, loop pass 2 us, PWM as the firmware sets it.

| host | round trip ms | line mm | mm/s | mirror lag us | done | layer ms | late off mean us | late off max us | early on mean us | still and lit us per line | lit in jumps mm per line | lit while scanning | dark while scanning |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| line by line | 0.25 | 1 | 500 | 0 | yes | 85.5 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 1 | 500 | 0 | yes | 160.8 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 1 | 500 | 0 | yes | 321.3 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 1 | 500 | 0 | yes | 85.3 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 2 | 500 | 0 | yes | 165.6 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 2 | 500 | 0 | yes | 167.5 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 2 | 500 | 0 | yes | 321.5 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 2 | 500 | 0 | yes | 165.4 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 5 | 500 | 0 | yes | 406.1 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 5 | 500 | 0 | yes | 408.0 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 5 | 500 | 0 | yes | 410.8 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 5 | 500 | 0 | yes | 405.9 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 20 | 500 | 0 | yes | 1608.4 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 20 | 500 | 0 | yes | 1610.3 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 20 | 500 | 0 | yes | 1613.0 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 20 | 500 | 0 | yes | 1608.2 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 1 | 2500 | 0 | yes | 40.3 | 1 | 1 | 4 | 4 | 0.000 | 99% | 0% |
| line by line | 1.00 | 1 | 2500 | 0 | yes | 160.7 | 1 | 1 | 4 | 4 | 0.000 | 99% | 0% |
| line by line | 2.00 | 1 | 2500 | 0 | yes | 321.3 | 1 | 1 | 4 | 4 | 0.000 | 99% | 0% |
| window 40 | 1.00 | 1 | 2500 | 0 | yes | 21.3 | 1 | 1 | 4 | 4 | 0.000 | 99% | 0% |
| line by line | 0.25 | 2 | 2500 | 0 | yes | 40.6 | 1 | 1 | 4 | 4 | 0.000 | 100% | 0% |
| line by line | 1.00 | 2 | 2500 | 0 | yes | 160.7 | 1 | 1 | 4 | 4 | 0.000 | 100% | 0% |
| line by line | 2.00 | 2 | 2500 | 0 | yes | 321.3 | 1 | 1 | 4 | 4 | 0.000 | 100% | 0% |
| window 40 | 1.00 | 2 | 2500 | 0 | yes | 37.4 | 1 | 1 | 4 | 4 | 0.000 | 100% | 0% |
| line by line | 0.25 | 5 | 2500 | 0 | yes | 86.1 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 5 | 2500 | 0 | yes | 160.8 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 5 | 2500 | 0 | yes | 321.3 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 5 | 2500 | 0 | yes | 85.9 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 20 | 2500 | 0 | yes | 328.4 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 1.00 | 20 | 2500 | 0 | yes | 330.3 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 2.00 | 20 | 2500 | 0 | yes | 333.0 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 20 | 2500 | 0 | yes | 328.2 | 1 | 1 | 4 | 4 | 0.000 | 80% | 20% |
| line by line | 0.25 | 1 | 500 | 150 | yes | 85.5 | 1 | 1 | 171 | 53 | 0.297 | 62% | 20% |
| line by line | 1.00 | 1 | 500 | 150 | yes | 160.8 | 1 | 1 | 44 | 44 | 0.000 | 72% | 20% |
| line by line | 2.00 | 1 | 500 | 150 | yes | 321.3 | 1 | 1 | 44 | 44 | 0.000 | 72% | 20% |
| window 40 | 1.00 | 1 | 500 | 150 | yes | 85.3 | 1 | 1 | 171 | 53 | 0.297 | 62% | 20% |
| line by line | 0.25 | 2 | 500 | 150 | yes | 165.6 | 1 | 1 | 263 | 57 | 0.689 | 69% | 20% |
| line by line | 1.00 | 2 | 500 | 150 | yes | 167.5 | 1 | 1 | 251 | 58 | 0.646 | 69% | 20% |
| line by line | 2.00 | 2 | 500 | 150 | yes | 321.5 | 1 | 1 | 44 | 44 | 0.000 | 76% | 20% |
| window 40 | 1.00 | 2 | 500 | 150 | yes | 165.4 | 1 | 1 | 263 | 57 | 0.689 | 69% | 20% |
| line by line | 0.25 | 5 | 500 | 150 | yes | 406.1 | 1 | 1 | 382 | 58 | 1.809 | 74% | 20% |
| line by line | 1.00 | 5 | 500 | 150 | yes | 408.0 | 1 | 1 | 370 | 60 | 1.766 | 74% | 20% |
| line by line | 2.00 | 5 | 500 | 150 | yes | 410.8 | 1 | 1 | 370 | 60 | 1.766 | 74% | 20% |
| window 40 | 1.00 | 5 | 500 | 150 | yes | 405.9 | 1 | 1 | 382 | 58 | 1.809 | 74% | 20% |
| line by line | 0.25 | 20 | 500 | 150 | yes | 1608.4 | 1 | 1 | 525 | 60 | 5.311 | 78% | 20% |
| line by line | 1.00 | 20 | 500 | 150 | yes | 1610.3 | 1 | 1 | 513 | 62 | 5.268 | 78% | 20% |
| line by line | 2.00 | 20 | 500 | 150 | yes | 1613.0 | 1 | 1 | 513 | 62 | 5.268 | 78% | 20% |
| window 40 | 1.00 | 20 | 500 | 150 | yes | 1608.2 | 1 | 1 | 525 | 60 | 5.311 | 78% | 20% |
| line by line | 0.25 | 1 | 2500 | 150 | yes | 40.3 | 1 | 1 | 59 | 55 | 0.040 | 60% | 0% |
| line by line | 1.00 | 1 | 2500 | 150 | yes | 160.7 | 1 | 1 | 44 | 44 | 0.000 | 63% | 0% |
| line by line | 2.00 | 1 | 2500 | 150 | yes | 321.3 | 1 | 1 | 44 | 44 | 0.000 | 63% | 0% |
| window 40 | 1.00 | 1 | 2500 | 150 | yes | 21.3 | 1 | 1 | 6 | 55 | 0.040 | 52% | 0% |
| line by line | 0.25 | 2 | 2500 | 150 | yes | 40.6 | 1 | 1 | 20 | 58 | 0.093 | 72% | 0% |
| line by line | 1.00 | 2 | 2500 | 150 | yes | 160.7 | 1 | 1 | 44 | 44 | 0.000 | 81% | 0% |
| line by line | 2.00 | 2 | 2500 | 150 | yes | 321.3 | 1 | 1 | 44 | 44 | 0.000 | 81% | 0% |
| window 40 | 1.00 | 2 | 2500 | 150 | yes | 37.4 | 1 | 1 | 44 | 58 | 0.198 | 69% | 0% |
| line by line | 0.25 | 5 | 2500 | 150 | yes | 86.1 | 1 | 1 | 151 | 58 | 1.118 | 62% | 20% |
| line by line | 1.00 | 5 | 2500 | 150 | yes | 160.8 | 1 | 1 | 44 | 44 | 0.000 | 72% | 20% |
| line by line | 2.00 | 5 | 2500 | 150 | yes | 321.3 | 1 | 1 | 44 | 44 | 0.000 | 72% | 20% |
| window 40 | 1.00 | 5 | 2500 | 150 | yes | 85.9 | 1 | 1 | 151 | 58 | 1.118 | 62% | 20% |
| line by line | 0.25 | 20 | 2500 | 150 | yes | 328.4 | 1 | 1 | 289 | 58 | 4.387 | 74% | 20% |
| line by line | 1.00 | 20 | 2500 | 150 | yes | 330.3 | 1 | 1 | 284 | 60 | 4.347 | 74% | 20% |
| line by line | 2.00 | 20 | 2500 | 150 | yes | 333.0 | 1 | 1 | 284 | 60 | 4.347 | 74% | 20% |
| window 40 | 1.00 | 20 | 2500 | 150 | yes | 328.2 | 1 | 1 | 289 | 58 | 4.387 | 74% | 20% |
