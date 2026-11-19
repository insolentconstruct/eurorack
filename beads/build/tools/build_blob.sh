#!/usr/bin/env bash
# shellcheck disable=SC2016  # make expands $(...), not the shell.
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.
#
# Build one Plaits blob in the build image, from the Drizzlet root:
#   beads/build/tools/build_blob.sh OUT NAME SOURCE...
# Beads' release CCFLAGS and CPPFLAGS plus the blob flags, for these objects
# only; linked at three bases, gated and packed by blob_pack.py, then checked
# against beads/blob/expected-blobs.sha256 when NAME is listed there.
set -euo pipefail
out=${1:?usage: build_blob.sh OUT NAME SOURCE...}
name=${2:?usage: build_blob.sh OUT NAME SOURCE...}
shift 2
mkdir -p "$out/$name"
unset MAKEFLAGS MFLAGS MAKELEVEL
# Flag queries only: an empty dependency file keeps make out of the tree.
mkdir -p "$out/make/beads"
: > "$out/make/beads/depends.mk"
M="make -f beads/build/link-order.mk KIT_MAKEFILE=beads/makefile KIT_GCC9=1 \
  BUILD_ROOT=$out/make/ -o $out/make/beads/depends.mk -s --no-print-directory"
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
# The linker makes the GOT and .blob; an object's own, writable, is refused.
for object in "${objects[@]}"; do
  sections=$("${TOOLCHAIN}arm-none-eabi-objdump" -h "$object")
  if grep -qE '^ *[0-9]+ (\.got[^ ]*|\.blob) ' <<< "$sections"; then
    echo "GATE: $object supplies GOT or .blob data" >&2
    exit 1
  fi
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
pins=$PWD/beads/blob/expected-blobs.sha256
if grep -q "  $name.bin\$" "$pins"; then
  (cd "$out" && grep "  $name.bin\$" "$pins" | sha256sum -c -)
fi
