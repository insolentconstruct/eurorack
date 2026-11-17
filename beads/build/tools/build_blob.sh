#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)

# Build one Plaits blob in the build image, from the Drizzlet root:
#   beads/build/tools/build_blob.sh OUT NAME SOURCE...
# Beads' own CCFLAGS and CPPFLAGS, plus the blob flags for these objects only;
# linked at three bases, then gated and packed by blob_pack.py.
set -euo pipefail
out=${1:?usage: build_blob.sh OUT NAME SOURCE...}
name=${2:?usage: build_blob.sh OUT NAME SOURCE...}
shift 2
mkdir -p "$out/$name"
unset MAKEFLAGS MFLAGS MAKELEVEL
M="make -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile KIT_GCC9=1 \
  -s --no-print-directory"
CCFLAGS=$($M --eval 'blob_q: ; @echo $(CCFLAGS)' blob_q)
CPPFLAGS=$($M --eval 'blob_q: ; @echo $(CPPFLAGS)' blob_q)
TOOLCHAIN=$($M --eval 'blob_q: ; @echo $(TOOLCHAIN_BIN)' blob_q)
BLOB_FLAGS="-fPIC -fvisibility=hidden -mpic-data-is-text-relative \
-mno-single-pic-base -fno-threadsafe-statics -fno-stack-protector"
printf 'CCFLAGS %s\nCPPFLAGS %s\nBLOB_FLAGS %s\n' \
  "$CCFLAGS" "$CPPFLAGS" "$BLOB_FLAGS" > "$out/$name/flags.txt"
"${TOOLCHAIN}arm-none-eabi-gcc" --version | sed -n 1p >> "$out/$name/flags.txt"
objects=()
for source in "$@"; do
  object="$out/$name/$(basename "${source%.*}").o"
  # shellcheck disable=SC2086
  "${TOOLCHAIN}arm-none-eabi-g++" -c $CCFLAGS $CPPFLAGS $BLOB_FLAGS \
    "$source" -o "$object"
  objects+=("$object")
done
links=()
for base in 0x080c0000 0x080d0fe0 0x080e0020; do
  elf="$out/$name/$name-$base.elf"
  "${TOOLCHAIN}arm-none-eabi-ld" -T beads/blob/blob.ld \
    --defsym=blob_base="$base" --gc-sections --emit-relocs \
    -Map="$elf.map" -o "$elf" "${objects[@]}"
  links+=("$base:$elf")
done
python3 beads/build/tools/blob_pack.py beads/blob/services_v1.ld \
  "${TOOLCHAIN}arm-none-eabi-objdump" "$out/$name.bin" "${links[@]}"
