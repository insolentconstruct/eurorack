#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

# Builds storage-recovery.wav inside beads-build-kit, from the repository
# root: the golden compiler, then the encoder with the pinned CPU.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: beads/recovery/build.sh EMPTY_OUTPUT_DIR (inside the kit)}
[[ "$out" = /* ]] || { echo "Output must be an absolute path" >&2; exit 2; }
[[ ! -e "$out" || -z "$(ls -A "$out")" ]] || { echo "Output must be empty" >&2; exit 2; }
[[ -n "${GOLDEN_TOOLCHAIN_PATH:-}" ]] || { echo "Run inside beads-build-kit" >&2; exit 2; }
mkdir -p "$out"
make --no-print-directory -f beads/recovery/makefile \
  "TOOLCHAIN_PATH=$GOLDEN_TOOLCHAIN_PATH" "BUILD_ROOT=$out/objects/" -j2 bin \
  > "$out/build.log" 2>&1 || { tail -40 "$out/build.log" >&2; exit 1; }
cp "$out/objects/storage-recovery/storage-recovery.bin" "$out/"
cp "$out/objects/storage-recovery/storage-recovery.elf" "$out/"
PYTHONPATH="$PWD" /usr/bin/qemu-x86_64-static -cpu SandyBridge,-avx,-x2apic,-tsc-deadline \
  /usr/bin/python3 stm_audio_bootloader/qpsk/encoder.py \
  -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 "$out/storage-recovery.bin"
"${GOLDEN_TOOLCHAIN_PATH}bin/arm-none-eabi-gcc" --version | head -1 > "$out/toolchain.txt"
(cd "$out" && sha256sum storage-recovery.bin storage-recovery.elf storage-recovery.wav > SHA256SUMS)
