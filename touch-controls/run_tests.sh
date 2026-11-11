#!/bin/sh
# Builds and runs the host-side unit tests for the touch controls' logic. Needs only a C++ compiler: no Android SDK / NDK.
# These tests live outside app/ on purpose: the game's CMake globs every .cpp under soh/Enhancements, and the test
# program has its own main().
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
CORE="$HERE/../../app/jni/src/soh/soh/Enhancements/touch-controls"
OUT="${TMPDIR:-/tmp}/touch_core_test"
${CXX:-g++} -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$CORE" "$HERE/touch_core_test.cpp" "$CORE/TouchCore.cpp" -o "$OUT"
"$OUT"
