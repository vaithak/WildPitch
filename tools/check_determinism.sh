#!/usr/bin/env bash
# Build the headless tests with gcc and clang at -O0 and -O2 and verify they all produce the
# same golden hash. Our cross-compiler proxy for cross-platform determinism until ARM CI exists.
set -euo pipefail
cd "$(dirname "$0")/.."
status=0
for cc in gcc clang; do
  for opt in -O0 -O2; do
    dir="build-det/${cc}${opt}"
    cmake -S . -B "$dir" -G Ninja -DWP_BUILD_GAME=OFF -DCMAKE_C_COMPILER=$cc \
          -DCMAKE_BUILD_TYPE=None -DCMAKE_C_FLAGS="$opt" >/dev/null
    cmake --build "$dir" >/dev/null
    if out=$("$dir/test_determinism" 2>&1); then
      echo "OK   $cc $opt: $(echo "$out" | grep 'final hash')"
    else
      echo "FAIL $cc $opt:"; echo "$out"; status=1
    fi
  done
done
exit $status
