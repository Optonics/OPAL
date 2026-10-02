#!/bin/sh
# Builds the firmware simulator and runs its tests or the timing benchmark.
# Needs g++ only (gcovr for --coverage). No board and no PlatformIO.
#
#   sh test/sim/run.sh                 the behaviour tests, with sanitizers
#   sh test/sim/run.sh --coverage      the same, then a line and branch report
#   sh test/sim/run.sh --bench [--quick]   the laser timing benchmark
set -eu
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/opal-sim"
mkdir -p "$OUT"
FIRMWARE="src/main.cpp src/MotionMGR.cpp src/SerialCMDReader.cpp src/Helpers.cpp lib/LaserController/DiodeLaserCtrl.cpp"
SIM="test/sim/sim.cpp test/sim/sim_world.cpp"
INCLUDES="-I test/sim/stubs -I lib/CircularBuffer -I lib/LaserController -I src"
# -fno-rtti as on the Teensy: LaserController declares isHalted() without a
# body, which links only without type information.
FLAGS="-std=gnu++17 -fno-rtti -DTEENSYDUINO=159 -Wall -Wno-unused-variable -Wno-unused-function -Wno-comment"

case "${1:-}" in
  --bench)
    shift
    # shellcheck disable=SC2086
    g++ $FLAGS -O2 $INCLUDES $FIRMWARE $SIM test/sim/bench_timing.cpp -o "$OUT/bench"
    "$OUT/bench" "$@"
    ;;
  --coverage)
    rm -rf "$OUT/cov" && mkdir -p "$OUT/cov"
    # shellcheck disable=SC2086
    g++ $FLAGS -O0 -g --coverage $INCLUDES $FIRMWARE $SIM test/sim/test_sim.cpp -o "$OUT/cov/tests"
    for t in $("$OUT/cov/tests" --list); do "$OUT/cov/tests" "$t"; done
    gcovr --root . --filter src/ --filter lib/LaserController/DiodeLaserCtrl.cpp \
      --object-directory "$OUT/cov" --txt --print-summary \
      --fail-under-line "${MIN_LINE_COVERAGE:-0}"
    ;;
  *)
    # shellcheck disable=SC2086
    g++ $FLAGS -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize=vptr \
      -fno-sanitize-recover=all $INCLUDES $FIRMWARE $SIM test/sim/test_sim.cpp -o "$OUT/tests"
    # Each test in its own process: the firmware keeps static state.
    fails=0
    for t in $("$OUT/tests" --list); do
      timeout 60 "$OUT/tests" "$t" || fails=$((fails + 1))
    done
    echo "simulator tests: $fails failed"
    test "$fails" = 0
    ;;
esac
