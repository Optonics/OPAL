#!/bin/sh
# Builds and runs the host tests of the G-code parser with the address and
# undefined-behaviour sanitizers. Needs g++ only, no board and no PlatformIO.
# A parser that loops forever fails through the timeout. A test name as the
# argument runs that test alone.
set -eu
TIMEOUT_S=20
cd "$(dirname "$0")/../.."
OUT="${TMPDIR:-/tmp}/opal-native-tests"
g++ -std=gnu++17 -g -O1 -fno-omit-frame-pointer \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -DTEENSYDUINO=159 \
  -Wall -Wno-unused-variable -Wno-unused-function \
  -I test/native/stubs -I lib/CircularBuffer -I src \
  test/native/test_serial_reader.cpp src/SerialCMDReader.cpp \
  -o "$OUT"
timeout "$TIMEOUT_S" "$OUT" "$@"
