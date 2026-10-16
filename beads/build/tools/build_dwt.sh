#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)

# Run on the coordinator from a clean committed Drizzlet root. No SWD.
set -euo pipefail
lane=${1:?absolute output directory required}
context=${BUILD_CONTEXT:-}
docker_cmd=(docker)
if [[ -n "$context" ]]; then docker_cmd+=(--context "$context"); fi
if [[ -n "$context" && -n ${BUILD_DRAIN_DIR:-} && -e "$BUILD_DRAIN_DIR/$context" ]]; then
  echo "build context is draining" >&2
  exit 1
fi
mkdir -p "$lane"
git archive HEAD > "$lane/source.tar"
git -C stmlib archive HEAD > "$lane/stmlib.tar"
container="dwt-build-$$"
"${docker_cmd[@]}" run -d --name "$container" beads-build-kit:gcc9 sleep infinity
trap '"${docker_cmd[@]}" rm -f "$container" >/dev/null' EXIT
"${docker_cmd[@]}" cp "$lane/source.tar" "$container:/tmp/source.tar"
"${docker_cmd[@]}" cp "$lane/stmlib.tar" "$container:/tmp/stmlib.tar"
"${docker_cmd[@]}" exec "$container" bash -c '
set -eu
mkdir -p /src
cd /src
tar xf /tmp/source.tar
tar xf /tmp/stmlib.tar -C stmlib
for variant in release calibration; do
  calibration=0
  [ "$variant" != calibration ] || calibration=1
  nice -n 10 make -o beads/resources.cc -o beads/resources.h \
    -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile \
    BUILD_ROOT=/$variant/ TOOLCHAIN_PATH="$TOOLCHAIN_PATH" KIT_GCC9=1 \
    DWT_CALIBRATION=$calibration -j2 bin > /tmp/$variant.log 2>&1
done
sha256sum /release/beads/beads.bin /calibration/beads/beads.bin
'
for variant in release calibration; do
  "${docker_cmd[@]}" cp "$container:/$variant/beads" "$lane/$variant"
  "${docker_cmd[@]}" cp "$container:/tmp/$variant.log" "$lane/$variant.log"
done
