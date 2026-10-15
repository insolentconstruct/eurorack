#!/usr/bin/env bash
# Run inside the build image, from the repository root.
set -euo pipefail
out=${1:?usage: beads/test/delay.sh OUTPUT_DIR}
mkdir -p "$out"
g++ -std=c++14 -O1 -DTEST -Wall -Werror -Wno-unused-local-typedefs \
  -fsanitize=address,undefined,integer-divide-by-zero -fno-sanitize-recover=all \
  -I. \
  beads/test/delay_test.cc beads/dsp/delay_controller.cc beads/dsp/delay_clock.cc \
  beads/resources.cc stmlib/dsp/units.cc stmlib/utils/random.cc -o "$out/delay"
ASAN_OPTIONS=detect_leaks=0 "$out/delay" "$out"
