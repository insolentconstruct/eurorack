#!/usr/bin/env bash
# Run inside the build image, from the repository root.
set -euo pipefail
out=${1:?usage: beads/test/options.sh OUTPUT_DIR}
mkdir -p "$out"
for advanced in normal advanced; do
  flags=()
  if [[ "$advanced" == advanced ]]; then flags+=(-DENABLE_ADVANCED_FEATURES); fi
  g++ -std=c++14 -O2 -ffp-contract=off -DSTM32H7XX -DTEST -Wall -Werror \
    -Wno-int-to-pointer-cast -Wno-unused-local-typedefs \
    -Ibeads/test/options_stubs -I. "${flags[@]}" \
    beads/test/options_test.cc beads/ui.cc beads/settings.cc \
    beads/drivers/switches.cc -o "$out/options-$advanced"
  "$out/options-$advanced" "$out"
done
