#!/usr/bin/env bash
# Run from repository root inside the pinned amd64 container.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: beads/build/build.sh EMPTY_OUTPUT_DIR [--wav]}
mode=${2:-}
[[ $# -le 2 && ( -z "$mode" || "$mode" == --wav ) ]] || exit 2
[[ "$out" = /* && "$out" != *[[:space:]]* ]] || { echo "Output must be an absolute path without whitespace" >&2; exit 2; }
[[ ! -e "$out" || -z "$(ls -A "$out")" ]] || { echo "Output must be empty" >&2; exit 2; }
[[ -f stmlib/makefile.inc && -f stm_audio_bootloader/qpsk/encoder.py ]] || { echo "Initialize submodules first" >&2; exit 1; }
mkdir -p "$out/objects/beads" "$out/objects/beads_bootloader"
for spec in 'beads/makefile beads' 'beads/bootloader/makefile beads_bootloader'; do
  read -r mf target <<< "$spec"
  nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
    "KIT_MAKEFILE=$mf" "BUILD_ROOT=$out/objects/" \
    "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" -j2 bin hex kit-info \
    > "$out/$target.log" 2>&1 || { tail -40 "$out/$target.log" >&2; exit 1; }
done
cp "$out/objects/beads/beads.bin" "$out/app.bin"
cp "$out/objects/beads/beads.elf" "$out/app.elf"
cp "$out/objects/beads_bootloader/beads_bootloader.bin" "$out/bootloader.bin"
# Independently reconstruct combo using the upstream HEX merge recipe.
nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile "BUILD_ROOT=$out/objects/" \
  "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" "$out/objects/beads/beads_bootloader_combo.bin" \
  > "$out/combo.log" 2>&1 || { cat "$out/combo.log" >&2; exit 1; }
python3 - "$out" <<'PYCODE'
import pathlib, subprocess, sys
p = pathlib.Path(sys.argv[1])
for target, origin in [('beads_bootloader', '08000000'), ('beads', '08020000')]:
    sections = subprocess.check_output(['arm-none-eabi-objdump', '-h', str(p/'objects'/target/(target+'.elf'))], text=True)
    vector = next(line.split() for line in sections.splitlines() if '.isr_vector' in line)
    if vector[3] != origin or vector[4] != origin:
        raise SystemExit('Unexpected vector VMA/LMA: '+repr(vector))
b = (p/'bootloader.bin').read_bytes()
a = (p/'app.bin').read_bytes()
if not 0 < len(b) <= 0x20000 or not a:
    raise SystemExit('Invalid payload sizes')
padded = b + bytes(0x20000-len(b))
(p/'bootloader-padded.bin').write_bytes(padded)
(p/'combo.bin').write_bytes(padded+a)
if (p/'objects/beads/beads_bootloader_combo.bin').read_bytes() != padded+a:
    raise SystemExit('Combo disagrees with upstream HEX merge')
PYCODE
(cd "$out" && sha256sum -c "$OLDPWD/beads/build/expected-hashes.sha256") || { echo "FATAL: released firmware hash mismatch" >&2; exit 1; }
if [[ "$mode" == --wav ]]; then
  # Pending: pinned gitlink does not implement stm32h7. Never substitute another encoder.
  command -v python2 >/dev/null || { echo "WAV pending: Python 2 encoder environment and stm32h7 support" >&2; exit 1; }
  PYTHONPATH="$PWD" python2 stm_audio_bootloader/qpsk/encoder.py \
    -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 "$out/app.bin"
else
  echo "WAV pending: pinned encoder lacks stm32h7 support; see beads/build/README.md" >&2
fi
(cd "$out" && sha256sum bootloader.bin bootloader-padded.bin app.bin app.elf combo.bin > SHA256SUMS
 if [[ -f app.wav ]]; then sha256sum app.wav >> SHA256SUMS; fi
 sha256sum -c SHA256SUMS)
