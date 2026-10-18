#!/usr/bin/env bash
# Copyright 2026 Insolent Construct. MIT; see beads/build/LICENSE.
set -euo pipefail
out=${1:?usage: beads/test/storage_compat.sh OUTPUT_DIR [STOCK_SOURCE_DIR]}
mkdir -p "$out"
out=$(cd "$out" && pwd)
stock=${2:-${STOCK_SOURCE_DIR:-$out/stock-source}}
if [[ $# == 1 && -z ${STOCK_SOURCE_DIR:-} ]]; then
  mkdir -p "$stock/beads"
  for file in background_buffer_backup.cc background_buffer_backup.h settings.cc settings.h; do
    git -c safe.directory="$PWD" show "f4cfa76:beads/$file" > "$stock/beads/$file"
  done
fi
for variant in base stock; do
  sources=beads
  includes=()
  if [[ "$variant" == stock ]]; then
    sources=$stock/beads
    includes=(-I"$stock")
  fi
  g++ -DTEST -DSTM32H7XX -Os -Wno-int-to-pointer-cast \
    "${includes[@]}" -Ibeads/test/backup_stubs -Ibeads/test/stubs -I. \
    beads/test/storage_compat_test.cc "$sources/background_buffer_backup.cc" \
    "$sources/settings.cc" -o "$out/$variant"
done
for media in 0 1 2 3; do
  for mono in 0 1; do
    "$out/base" save "$out/base-save.bin" "$media" "$mono"
    "$out/stock" load "$out/base-save.bin" "$media" "$mono"
    "$out/stock" save "$out/stock-save.bin" "$media" "$mono"
    "$out/base" load "$out/stock-save.bin" "$media" "$mono"
  done
done
echo 'PASS actual stock/base settings and backup readers, both directions, all 8 formats'
