#!/usr/bin/env bash
# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
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
# Build actual released settings implementations and cross-load recovery images.
set -euo pipefail

out=${1:?usage: beads/test/recovery_compat.sh OUTPUT_DIR}
mkdir -p "$out"
out=$(cd "$out" && pwd)
[[ -z $(ls -A "$out") ]] || {
  echo 'Output directory must be fresh and empty' >&2
  exit 1
}
names=(stock beta1 beta2 beta3 tuneup1 tuneup2 beta4)
# Mutable Instruments' master (2021/2024 storage), Beta 1, Beta 2, Beta 3,
# both beads-tuneup releases, and Beta 4 as integrated so far.
refs=(f4cfa7697095a28d292bff758beb51deb49b5a86
  drizzlet-2026.10.18
  drizzlet-2026.11.09
  drizzlet-2026.11.16
  beads-tuneup-2026.11.09
  beads-tuneup-2026.11.20
  76c10369e9b00bc9f5d280b095465f2059a233ce)
flags=(-DTEST -DSTM32H7XX -O1 -funsigned-char -ffp-contract=off
  -Wno-int-to-pointer-cast)
mkdir -p "$out/tmp"
export TMPDIR=$out/tmp

g++ "${flags[@]}" -DOLD_STUBS -Ibeads/test/recovery_stubs -I. \
  beads/test/recovery_image.cc beads/recovery/settings_recovery.cc \
  -o "$out/recovery_image"

for i in "${!names[@]}"; do
  name=${names[$i]}
  ref=${refs[$i]}
  src=$out/$name
  mkdir -p "$src"
  # RECOVERY_COMPAT_SOURCES: the refs' beads trees, for a copy without git.
  if [[ -n ${RECOVERY_COMPAT_SOURCES:-} ]]; then
    cp -R "$RECOVERY_COMPAT_SOURCES/$name/beads" "$src/"
  else
    git archive "$ref" beads | tar -x -C "$src"
  fi
  includes=(-I"$src/beads/test/common_stubs"
    -I"$src/beads/test/stubs" -I"$src" -I.)
  extra=()
  sources=("$src/beads/settings.cc")
  if [[ -f $src/beads/profile.cc ]]; then
    sources+=("$src/beads/profile.cc" "$src/beads/dsp/pitch_quantizer.cc"
      stmlib/utils/random.cc)
  fi
  if (( i < 2 )); then
    includes=(-Ibeads/test/recovery_stubs -I"$src" -I.)
    extra=(-DOLD_STUBS)
  fi
  # Generate only API adapters; never substitute a settings implementation.
  python3 - "$src" <<'PY'
import pathlib
import re
import sys

src = pathlib.Path(sys.argv[1])
header = (src / 'beads/settings.h').read_text()
getters = re.findall(
  r'inline\s+[\w:]+\s+(\w+)\(\)\s+const', header)
exclude = {'storage_damaged', 'write_locked', 'repair'}
lines = ['using namespace beads;', 'void ChangeOptions(Settings& s) {']
for option in ('clouds_style', 'spectral'):
  if 'set_' + option + '(' in header:
    lines.append('  s.set_' + option + '(true);')
lines += ['}', 'void SaveOptions(Settings& s, int i) {']
if 'void SaveOptions(' in header:
  lines += ['  s.set_clouds_style(i % 2 == 0);', '  s.SaveOptions();']
lines += ['}', 'void CheckStorage(const Settings& s) {']
for api in ('storage_damaged', 'write_locked'):
  if api in getters:
    lines.append('  Require(!s.' + api + '(), "' + api + '");')
if 'repair' in getters:
  lines.append('  Require(s.repair() == STORAGE_REPAIR_NONE, "repair");')
if 'void SaveOptions(' in header:
  lines.append('  Require(!s.chunk_storage_.record_found(), "DRZL found");')
lines += ['}', 'void CheckOptions(const Settings& s, const Settings& d) {']
for api in getters:
  if api not in exclude:
    lines.append('  Require(s.' + api + '() == d.' + api + '(),')
    lines.append('      "option ' + api + '");')
if 'FeatureValue(ProfileFeature' in header:
  lines += ['  for (int i = 0; i < FEATURE_LAST; ++i) {',
       '    const ProfileFeature f = ProfileFeature(i);',
       '    Require(s.FeatureValue(f) == d.FeatureValue(f),',
       '        "feature default differs");', '  }']
lines += ['}']
license = pathlib.Path('beads/test/recovery_compat/runtime.h').read_text()
license = license.split('// ---')[0]
(src / 'compat_options.h').write_text(license + '\n'.join(lines) + '\n')
PY
  g++ "${flags[@]}" "${extra[@]}" "${includes[@]}" \
    -I"$src" beads/test/recovery_compat_test.cc \
    "${sources[@]}" -o "$out/$name-driver"
  echo "BUILT $name ($ref)"
done

count=0
for i in "${!names[@]}"; do
  name=${names[$i]}
  "$out/$name-driver" save "$out/source.bin" 1.1 0.9
  "$out/stock-driver" save "$out/factory.bin" 1.0 1.0
  cases=(healthy reset)
  if (( i >= 2 )); then
    cases+=(checksum factory-primary)
  else
    cases+=(garbage-mirror)
  fi
  for case in "${cases[@]}"; do
    python3 - "$out" "$case" "$name" <<'PY'
import pathlib
import struct
import sys

out = pathlib.Path(sys.argv[1])
case, name = sys.argv[2:]
data = bytearray((out / 'source.bin').read_bytes())

def chunks(base):
  end = base + 0x20000
  found = []
  while base + 8 <= end:
    tag, size, checksum = struct.unpack_from('<IHH', data, base)
    if tag == 0xffffffff:
      break
    assert size and base + 8 + size <= end, 'invalid saved chunk'
    payload = data[base + 8:base + 8 + size]
    assert checksum == (sum(payload) & 0xffff) ^ 0xffff
    found.append(tag)
    base += (8 + size + 31) & ~31
  return found

assert chunks(0xe0000).count(0x54415453) >= 5, 'missing STAT history'
if name not in ('stock', 'beta1'):
  assert chunks(0xc0000)[0] == 0x494c4143, 'missing CALI mirror'
else:
  assert data[0xc0000:0xe0000] == b'\xff' * 0x20000
if name == 'beta4':
  assert chunks(0xc0000).count(0x4c5a5244) >= 3, 'missing DRZL history'
if case in ('checksum', 'reset'):
  data[0xe0006] ^= 1
if case == 'reset':
  data[0xc0006] ^= 1
if case == 'factory-primary':
  factory = (out / 'factory.bin').read_bytes()
  size = (8 + struct.unpack_from('<H', factory, 0xe0004)[0] + 31) & ~31
  data[0xe0000:0xe0000 + size] = factory[0xe0000:0xe0000 + size]
if case == 'garbage-mirror':
  data[0xc0000:0xe0000] = b'\x5a' * 0x20000
(out / 'input.bin').write_bytes(data)
PY
    result=KEPT
    scale=1.1
    offset=0.9
    if [[ $case == reset ]]; then
      result=RESET
      scale=1.0
      offset=1.0
    fi
    "$out/recovery_image" "$out/input.bin" "$out/recovered.bin" "$result"
    python3 - "$out/recovered.bin" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
assert data[:0xc0000] == b'\xff' * 0xc0000, 'buffer not erased'
size = (8 + struct.unpack_from('<H', data, 0xc0004)[0] + 31) & ~31
assert data[0xc0000:0xc0000 + size] == data[0xe0000:0xe0000 + size]
assert data[0xc0000 + size:0xe0000] == b'\xff' * (0x20000 - size)
PY
    for loader in "${names[@]}"; do
      "$out/$loader-driver" load "$out/recovered.bin" "$scale" "$offset"
      echo "PASS $case $name $loader"
      count=$((count + 1))
    done
  done
done
echo "PASS $count compatibility checks"
