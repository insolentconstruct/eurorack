#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct

# Run from the repository root.
set -euo pipefail
export LC_ALL=C
unset MAKEFLAGS MFLAGS MAKELEVEL
out=${1:?usage: beads/build/build.sh EMPTY_OUTPUT_DIR --wav   (from the repository root, on the host)}
mode=${2:-}
[[ $# -le 2 && ( -z "$mode" || "$mode" == --wav ) ]] || exit 2
[[ "$out" = /* && "$out" != *[[:space:]]* ]] || { echo "Output must be an absolute path without whitespace" >&2; exit 2; }
[[ ! -e "$out" || -z "$(ls -A "$out")" ]] || { echo "Output must be empty" >&2; exit 2; }
if [[ "$mode" == --wav ]]; then
  command -v docker >/dev/null 2>&1 || { echo "Docker is required" >&2; exit 1; }
  root="$(git rev-parse --show-toplevel)"
  [[ "$root" != *,* && "$out" != *,* ]] || { echo "Docker mount paths must not contain commas" >&2; exit 2; }
  [[ -f "$root/stm_audio_bootloader/qpsk/encoder.py" && -f "$root/stmlib/makefile.inc" ]] || { echo "Run: git submodule update --init stmlib stm_audio_bootloader" >&2; exit 1; }
  [[ "$(git -C "$root/stm_audio_bootloader" rev-parse HEAD)" == 39d95a8be305c6bc4a99fd218518d832c7ff821c ]] || { echo "Wrong encoder commit" >&2; exit 1; }
  mkdir -p "$out"
  name=${RELEASE_NAME:-drizzlet-dev}
  [[ "$name" =~ ^drizzlet-[A-Za-z0-9._-]+$ ]] || { echo "RELEASE_NAME must match drizzlet-[A-Za-z0-9._-]+" >&2; exit 2; }
  build_image=${BEADS_BUILD_IMAGE:-beads-build-kit}
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" "$build_image" \
    nice -n 10 bash beads/build/build.sh /out
  encoder_start=$SECONDS
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" -w /src \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" -e PYTHONPATH=/src "$build_image" \
    nice -n 10 /usr/bin/qemu-x86_64-static \
    -cpu SandyBridge,-avx,-x2apic,-tsc-deadline /usr/bin/python3 \
    stm_audio_bootloader/qpsk/encoder.py \
    -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 /out/app.bin
  echo "Pinned-CPU encoding (including container startup): $((SECONDS - encoder_start)) seconds"
  mv "$out/app.wav" "$out/$name.wav"
  # Check the WAV against the tested file.
  echo "$(cut -c1-64 "$root/beads/build/expected-wav.sha256")  $name.wav" | (cd "$out" && sha256sum -c -)
  docker run --rm --platform linux/amd64 --network none --user "$(id -u):$(id -g)" \
    --mount "type=bind,src=$root,dst=/src,readonly" --mount "type=bind,src=$out,dst=/out" -e RELEASE_NAME="$name" "$build_image" \
    nice -n 10 python3 beads/build/tools/release.py /out
  exit 0
fi
# Without --wav, this runs inside the compiler container.
[[ -n "${TOOLCHAIN_PATH:-}" ]] || { echo "Use --wav on the host; the other mode runs inside the build container" >&2; exit 2; }
[[ -f stmlib/makefile.inc && -f stm_audio_bootloader/qpsk/encoder.py ]] || { echo "Initialize submodules first" >&2; exit 1; }
mkdir -p "$out/objects/beads" "$out/objects/beads_bootloader"
for spec in 'beads/makefile beads' 'beads/bootloader/makefile beads_bootloader'; do
  read -r mf target <<< "$spec"
  tc=$TOOLCHAIN_PATH
  gcc9=1
  if [[ "$target" == beads_bootloader ]]; then
    tc=${GOLDEN_TOOLCHAIN_PATH:?Missing golden compiler}
    gcc9=0
  fi
  nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
    "KIT_MAKEFILE=$mf" "BUILD_ROOT=$out/objects/" \
    "TOOLCHAIN_PATH=$tc" "KIT_GCC9=$gcc9" -j"$(nproc)" bin hex kit-info notice-deps \
    > "$out/$target.log" 2>&1 || { tail -40 "$out/$target.log" >&2; exit 1; }
done
cp "$out/objects/beads/beads.bin" "$out/app.bin"
cp "$out/objects/beads/beads.elf" "$out/app.elf"
cp "$out/objects/beads_bootloader/beads_bootloader.bin" "$out/bootloader.bin"
# Combo image, as in stmlib's makefile.
nice -n 10 make --no-print-directory -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile "BUILD_ROOT=$out/objects/" \
  "TOOLCHAIN_PATH=$TOOLCHAIN_PATH" KIT_GCC9=1 "$out/objects/beads/beads_bootloader_combo.bin" \
  > "$out/combo.log" 2>&1 || { cat "$out/combo.log" >&2; exit 1; }
python3 - "$out" <<'PYCODE'
import pathlib, subprocess, sys
p = pathlib.Path(sys.argv[1])
for target, origin in [('beads_bootloader', '08000000'), ('beads', '08020000')]:
    sections = subprocess.check_output(['arm-none-eabi-objdump', '-h', str(p/'objects'/target/(target+'.elf'))], text=True)
    vector = next(line.split() for line in sections.splitlines() if '.isr_vector' in line)
    if vector[3] != origin or vector[4] != origin:
        raise SystemExit('Unexpected vector VMA/LMA: '+repr(vector))
    if target == 'beads':
        profile = next((line.split() for line in sections.splitlines()
                        if '.profile' in line), None)
        if not profile or profile[3] != '08020400' or profile[2] != '00001000':
            raise SystemExit(
                'Missing or misplaced profile section: '+repr(profile))
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
(cd "$out" && grep -v "  calibration.bin$" "$OLDPWD/beads/build/expected-hashes.sha256" | sha256sum -c -) || { echo "FATAL: branch firmware hash mismatch" >&2; exit 1; }
(cd "$out" && sha256sum bootloader.bin bootloader-padded.bin app.bin app.elf combo.bin > SHA256SUMS
 sha256sum -c SHA256SUMS)
