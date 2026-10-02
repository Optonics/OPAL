# Laser timing: baseline before the timing fixes

Firmware: `feature/diode-laser-control` with the parser fixes merged (`07ac23e`). Host: `osls1_klipper/galvo/opal_link.py` sends one line and waits for its `ok` ("line by line"). Measured in the simulator (`sh test/sim/run.sh --bench`). The model numbers are assumptions until the bench confirms them: see `test/sim/README.md`.

How to read the columns:

- **late off**: the mirror has stopped at the end of a scan line, and the beam is still enabled. The line end gets extra heat for this long.
- **early on**: the beam is enabled, and the mirror has not started the scan line yet.
- **still and lit per line**: all the time per line with the beam enabled and the mirror still.
- **lit in jumps**: the beam is high while the mirror jumps back to the next line start. Each jump crosses the part, so this is a streak across it.
- **lit / dark while scanning**: the share of the scanned length the beam is on or off. 1 kHz PWM at S204 (80 %) is on 0.8 ms and off 0.2 ms of each millisecond, so a scan line is dashed.

What the baseline shows:

1. Line by line, every `ok` round trip is time the beam waits. At 1 ms and 2500 mm/s, a 1 mm line is lit 0.4 ms while moving and about 1.7 ms standing still. The layer takes 166 ms instead of 17 ms.
2. A host that keeps the queue full (window 40) removes the waits, but the beam is then lit in almost every jump. The duty change to 0 reaches the pin only at the next 1 ms PWM period, and the jump comes microseconds after the `M5`. Thus the host change must not go live before the firmware turns the beam off at once.
3. 20 % of every scan line is dark at any speed: the 1 kHz PWM gaps.


Hatch layer of 40 lines, M5/G0/M3 S204/G1 per line, loop pass 2 us, PWM as the firmware sets it.

| host | round trip ms | line mm | mm/s | mirror lag us | done | layer ms | late off mean us | late off max us | early on mean us | still and lit us per line | lit in jumps mm per line | lit while scanning | dark while scanning |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| line by line | 0.25 | 1 | 500 | 0 | yes | 81.3 | 1 | 1 | 0 | 0 | 0.582 | 79% | 19% |
| line by line | 1.00 | 1 | 500 | 0 | yes | 165.6 | 508 | 985 | 548 | 1096 | 0.678 | 80% | 20% |
| line by line | 2.00 | 1 | 500 | 0 | yes | 326.1 | 526 | 1409 | 1538 | 2063 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 1 | 500 | 0 | yes | 80.6 | 0 | 0 | 0 | 0 | 0.987 | 78% | 20% |
| line by line | 0.25 | 2 | 500 | 0 | yes | 161.3 | 1 | 1 | 0 | 0 | 1.155 | 79% | 20% |
| line by line | 1.00 | 2 | 500 | 0 | yes | 168.2 | 0 | 0 | 0 | 138 | 1.404 | 80% | 20% |
| line by line | 2.00 | 2 | 500 | 0 | yes | 326.3 | 423 | 943 | 1544 | 1935 | 1.151 | 80% | 20% |
| window 40 | 1.00 | 2 | 500 | 0 | yes | 160.6 | 0 | 0 | 0 | 0 | 1.959 | 79% | 20% |
| line by line | 0.25 | 5 | 500 | 0 | yes | 401.3 | 1 | 1 | 0 | 0 | 2.881 | 80% | 20% |
| line by line | 1.00 | 5 | 500 | 0 | yes | 403.4 | 1 | 1 | 894 | 22 | 3.632 | 80% | 20% |
| line by line | 2.00 | 5 | 500 | 0 | yes | 406.1 | 0 | 0 | 0 | 42 | 2.755 | 80% | 20% |
| window 40 | 1.00 | 5 | 500 | 0 | yes | 400.6 | 0 | 0 | 0 | 0 | 4.885 | 80% | 20% |
| line by line | 0.25 | 20 | 500 | 0 | yes | 1601.3 | 1 | 1 | 0 | 0 | 11.505 | 80% | 20% |
| line by line | 1.00 | 20 | 500 | 0 | yes | 1603.4 | 1 | 1 | 894 | 22 | 14.506 | 80% | 20% |
| line by line | 2.00 | 20 | 500 | 0 | yes | 1606.1 | 0 | 0 | 0 | 42 | 11.005 | 80% | 20% |
| window 40 | 1.00 | 20 | 500 | 0 | yes | 1600.6 | 0 | 0 | 0 | 0 | 19.508 | 80% | 20% |
| line by line | 0.25 | 1 | 2500 | 0 | yes | 45.2 | 371 | 467 | 0 | 410 | 0.552 | 48% | 7% |
| line by line | 1.00 | 1 | 2500 | 0 | yes | 165.6 | 1089 | 1849 | 579 | 1665 | 0.000 | 80% | 19% |
| line by line | 2.00 | 1 | 2500 | 0 | yes | 326.2 | 2035 | 2963 | 1529 | 3563 | 0.000 | 74% | 25% |
| window 40 | 1.00 | 1 | 2500 | 0 | yes | 16.6 | 0 | 0 | 0 | 0 | 0.802 | 74% | 18% |
| line by line | 0.25 | 2 | 2500 | 0 | yes | 45.5 | 41 | 77 | 0 | 276 | 1.601 | 77% | 18% |
| line by line | 1.00 | 2 | 2500 | 0 | yes | 165.6 | 714 | 1449 | 579 | 1290 | 0.000 | 79% | 20% |
| line by line | 2.00 | 2 | 2500 | 0 | yes | 326.2 | 1635 | 2563 | 1529 | 3163 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 2 | 2500 | 0 | yes | 32.6 | 0 | 0 | 0 | 0 | 1.474 | 77% | 19% |
| line by line | 0.25 | 5 | 2500 | 0 | yes | 81.3 | 0 | 0 | 0 | 0 | 3.020 | 80% | 20% |
| line by line | 1.00 | 5 | 2500 | 0 | yes | 165.7 | 474 | 971 | 544 | 1045 | 3.126 | 80% | 20% |
| line by line | 2.00 | 5 | 2500 | 0 | yes | 326.2 | 558 | 1361 | 1529 | 2086 | 0.000 | 80% | 20% |
| window 40 | 1.00 | 5 | 2500 | 0 | yes | 80.6 | 0 | 0 | 0 | 0 | 4.907 | 79% | 20% |
| line by line | 0.25 | 20 | 2500 | 0 | yes | 321.3 | 0 | 0 | 0 | 0 | 12.020 | 80% | 20% |
| line by line | 1.00 | 20 | 2500 | 0 | yes | 323.4 | 0 | 0 | 0 | 22 | 15.025 | 80% | 20% |
| line by line | 2.00 | 20 | 2500 | 0 | yes | 331.8 | 1 | 1 | 1662 | 181 | 9.008 | 80% | 20% |
| window 40 | 1.00 | 20 | 2500 | 0 | yes | 320.6 | 0 | 0 | 0 | 0 | 19.531 | 80% | 20% |
| line by line | 0.25 | 1 | 500 | 150 | yes | 81.3 | 1 | 1 | 480 | 50 | 0.546 | 60% | 18% |
| line by line | 1.00 | 1 | 500 | 150 | yes | 165.6 | 102 | 356 | 559 | 684 | 0.486 | 80% | 21% |
| line by line | 2.00 | 1 | 500 | 150 | yes | 326.1 | 323 | 1174 | 1578 | 1900 | 0.000 | 78% | 20% |
| window 40 | 1.00 | 1 | 500 | 150 | yes | 80.6 | 0 | 0 | 0 | 51 | 0.675 | 62% | 16% |
| line by line | 0.25 | 2 | 500 | 150 | yes | 161.3 | 1 | 1 | 480 | 57 | 1.287 | 68% | 19% |
| line by line | 1.00 | 2 | 500 | 150 | yes | 168.2 | 0 | 0 | 0 | 74 | 1.304 | 72% | 18% |
| line by line | 2.00 | 2 | 500 | 150 | yes | 326.3 | 151 | 420 | 1584 | 1626 | 0.718 | 79% | 20% |
| window 40 | 1.00 | 2 | 500 | 150 | yes | 160.6 | 0 | 0 | 0 | 58 | 1.571 | 69% | 17% |
| line by line | 0.25 | 5 | 500 | 150 | yes | 401.3 | 1 | 1 | 480 | 57 | 3.600 | 74% | 19% |
| line by line | 1.00 | 5 | 500 | 150 | yes | 403.4 | 1 | 1 | 0 | 73 | 4.020 | 74% | 19% |
| line by line | 2.00 | 5 | 500 | 150 | yes | 406.1 | 0 | 0 | 0 | 101 | 2.872 | 74% | 19% |
| window 40 | 1.00 | 5 | 500 | 150 | yes | 400.6 | 0 | 0 | 0 | 58 | 4.293 | 74% | 18% |
| line by line | 0.25 | 20 | 500 | 150 | yes | 1601.3 | 1 | 1 | 480 | 58 | 15.300 | 78% | 20% |
| line by line | 1.00 | 20 | 500 | 150 | yes | 1603.4 | 1 | 1 | 0 | 74 | 17.038 | 78% | 20% |
| line by line | 2.00 | 20 | 500 | 150 | yes | 1606.1 | 0 | 0 | 0 | 102 | 12.467 | 78% | 20% |
| window 40 | 1.00 | 20 | 500 | 150 | yes | 1600.6 | 0 | 0 | 0 | 59 | 17.975 | 78% | 20% |
| line by line | 0.25 | 1 | 2500 | 150 | yes | 45.2 | 1 | 1 | 45 | 136 | 0.163 | 69% | 11% |
| line by line | 1.00 | 1 | 2500 | 150 | yes | 165.6 | 729 | 1422 | 594 | 1466 | 0.000 | 74% | 20% |
| line by line | 2.00 | 1 | 2500 | 150 | yes | 326.2 | 1810 | 2738 | 1569 | 3378 | 0.000 | 72% | 20% |
| window 40 | 1.00 | 1 | 2500 | 150 | yes | 16.6 | 0 | 0 | 0 | 52 | 0.049 | 43% | 10% |
| line by line | 0.25 | 2 | 2500 | 150 | yes | 45.5 | 36 | 68 | 112 | 58 | 0.789 | 72% | 18% |
| line by line | 1.00 | 2 | 2500 | 150 | yes | 165.6 | 400 | 1012 | 594 | 1086 | 0.000 | 76% | 19% |
| line by line | 2.00 | 2 | 2500 | 150 | yes | 326.2 | 1400 | 2328 | 1569 | 2968 | 0.000 | 77% | 18% |
| window 40 | 1.00 | 2 | 2500 | 150 | yes | 32.6 | 0 | 0 | 0 | 58 | 0.569 | 50% | 12% |
| line by line | 0.25 | 5 | 2500 | 150 | yes | 81.3 | 0 | 0 | 0 | 58 | 2.683 | 61% | 18% |
| line by line | 1.00 | 5 | 2500 | 150 | yes | 165.7 | 82 | 343 | 557 | 665 | 2.317 | 79% | 21% |
| line by line | 2.00 | 5 | 2500 | 150 | yes | 326.2 | 352 | 1127 | 1569 | 1920 | 0.000 | 78% | 20% |
| window 40 | 1.00 | 5 | 2500 | 150 | yes | 80.6 | 0 | 0 | 0 | 58 | 3.309 | 63% | 16% |
| line by line | 0.25 | 20 | 2500 | 150 | yes | 321.3 | 0 | 0 | 0 | 59 | 14.506 | 73% | 19% |
| line by line | 1.00 | 20 | 2500 | 150 | yes | 323.4 | 0 | 0 | 0 | 82 | 16.262 | 73% | 19% |
| line by line | 2.00 | 20 | 2500 | 150 | yes | 331.8 | 1 | 1 | 1702 | 108 | 12.143 | 74% | 19% |
| window 40 | 1.00 | 20 | 2500 | 150 | yes | 320.6 | 0 | 0 | 0 | 59 | 16.874 | 73% | 18% |
