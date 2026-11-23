#!/usr/bin/env python3
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
#
# -----------------------------------------------------------------------------
#
# Boot the release ELF over faulted copies of its default region (no profile
# directory: the release's own layout) and check which synths may play, which
# one plays in synth mode, whether it is heard, and the [A] error cue: the
# WAVE/PLTS rows of ABI.md's fault table.
#
#   region_boots.py ELF REGION.bin OUT.json

import hashlib
import json
import struct
import sys

import transition_emu as te

SYNTHS = ('beads', 'plaits', 'rings', 'silent')
SECTIONS = ('WAVE', 'TERR', 'PLTS', 'RNGS')
STATUS = ('ok', 'placement', 'header', 'abi', 'services', 'layout', 'ram',
          'crc', 'digest', 'imports', 'no region', 'release', 'selection',
          'table', 'model', 'kind', 'library', 'content', 'directory', 'ecc',
          'dependency')


def sections(region):
  """Each section's offset and size, walking the headers."""
  found, at = {}, 0
  while at + 32 <= len(region):
    magic, kind, _, _, size = struct.unpack_from('<5I', region, at)
    if magic != 0x43535a44:
      break
    found[struct.pack('<I', kind).decode()] = (at, size)
    at = (at + size + 31) // 32 * 32
  return found


def variants(region):
  """(name, region bytes, expected playable, expected playing) per row."""
  s = sections(region)
  wave, plts = s['WAVE'], s['PLTS']
  def flip(data, offset):
    data = bytearray(data)
    data[offset] ^= 1
    return bytes(data)
  def erase(data, offset, size):
    data = bytearray(data)
    data[offset:offset + size] = b'\xff' * size
    return bytes(data)
  wave_bad = flip(region, wave[0] + 32 + 5000)
  plts_bad = flip(region, plts[0] + plts[1] // 2)
  both_bad = flip(wave_bad, plts[0] + plts[1] // 2)
  only_wave = region[:wave[1]]
  return [
      ('as released', region, ('beads', 'plaits', 'rings'), 'beads'),
      ('WAVE damaged', wave_bad, ('plaits', 'rings'), 'plaits'),
      ('WAVE erased', erase(region, wave[0], wave[1]), ('plaits', 'rings'),
       'plaits'),
      ('PLTS damaged', plts_bad, ('beads', 'rings'), 'beads'),
      ('WAVE only (install cut in PLTS)', only_wave, ('beads',), 'beads'),
      ('WAVE and PLTS damaged', both_bad, ('rings',), 'silent'),
      ('erased', b'', (), 'silent')]


def boot(image, region):
  r = te.Rig(image, region)
  r.settle()
  m = r.m
  playable, = m.read(image.address('playable_synths'), '<B')
  # As the synth scenario: inputs unplugged, then the 10 s synth timer.
  r.left = r.right = False
  r.tick(300)
  r.tick(9800, audio=False)
  r.recording = True
  r.tick(1500)
  playing, = m.read(image.address('playing_synth'), '<I')
  identity = m.read(image.address('plaits_region'), '<3I16I')
  silent = all(c[2] for c in r.calls if c[0] == 'audio')
  return {
      'playable': [n for i, n in enumerate(SYNTHS[:3]) if playable >> i & 1],
      'playing': SYNTHS[playing],
      'sections': {n: STATUS[identity[3 + 4 * i]]
                   for i, n in enumerate(SECTIONS)},
      'synth_fault': m.read(image.member('ui', 'Ui', 'synth_fault_'),
                            '<B')[0],
      'silent': silent,
      'out_sha256': r.out.hexdigest(),
      'region_sha256': hashlib.sha256(region).hexdigest()}


def main(argv):
  if len(argv) != 4:
    sys.stderr.write('usage: region_boots.py ELF REGION.bin OUT.json\n')
    return 2
  te.MEMBERS['Ui'] = te.MEMBERS['Ui'] + ('synth_fault_',)
  image = te.Image(argv[1])
  region = open(argv[2], 'rb').read()
  results, failures = [], 0
  for name, data, playable, playing in variants(region):
    got = boot(image, data)
    ok = got['playable'] == list(playable) and got['playing'] == playing and \
        got['silent'] == (playing == 'silent') and \
        bool(got['synth_fault']) == (playing == 'silent')
    failures += not ok
    got.update(name=name, ok=ok)
    results.append(got)
    print('%-32s %s playable %-20s plays %-6s %s cue %d  %s' % (
        name, 'OK  ' if ok else 'FAIL', ','.join(got['playable']) or '-',
        got['playing'], 'silent' if got['silent'] else 'heard ',
        got['synth_fault'],
        ' '.join('%s:%s' % kv for kv in got['sections'].items())))
  json.dump({'elf_sha256': image.sha256, 'boots': results}, open(argv[3], 'w'),
            indent=1, sort_keys=True)
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
