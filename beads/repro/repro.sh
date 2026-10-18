#!/bin/sh
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
# Run the repro kit in Docker (default) or on this machine (--native).
# Usage: beads/repro/repro.sh [--native] run SET --tier desktop [--jobs N]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
if [ "${1:-}" = "--native" ]; then
  shift
  PYTHONPATH="$here" exec python3 -m repro "$@"
fi
docker build -q -t drizzlet-repro "$here" >/dev/null
exec docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME=/tmp -e BEADS_JOBS="${BEADS_JOBS:-}" \
  -v "$repo:/src" -w /src drizzlet-repro \
  sh -c 'PYTHONPATH=beads/repro exec python3 -m repro "$@"' repro "$@"
