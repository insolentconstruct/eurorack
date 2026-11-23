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
# Boot the release ELF over faulted copies of its region with a terrain grid
# and profile directory, and check which synths may play, which
# one plays in synth mode, whether it is heard, and the [A] error cue: the
# WAVE/PLTS rows of ABI.md's fault table.
#
#   region_boots.py ELF REGION.bin OUT.json

import argparse
import binascii
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
  """Independent failed writes: header, payload, tail, and whole section."""
  layout = sections(region)
  def damage(data, kind, fault):
    if fault == 'ok':
      return data
    offset, size = layout[kind]
    data = bytearray(data)
    if fault == 'crc':
      data[offset + 32] ^= 1
    elif fault == 'magic':
      data[offset] ^= 1
    elif fault == 'truncated':
      data[offset + size // 2:offset + size] = b'\xff' * (size - size // 2)
    else:
      data[offset:offset + size] = b'\xff' * size
    return bytes(data)
  states = ('ok', 'crc', 'magic', 'truncated', 'absent')
  cases = []
  for w in states:
    for p in states:
      playable = tuple(n for n, ok in (
          ('beads', w == 'ok'), ('plaits', p == 'ok'), ('rings', True)) if ok)
      cases.append(('WAVE %s PLTS %s' % (w, p),
                    damage(damage(region, 'WAVE', w), 'PLTS', p), playable))
  for fault in ('absent', 'crc', 'magic', 'truncated'):
    cases.append(('TERR ' + fault, damage(region, 'TERR', fault),
                  ('beads', 'plaits', 'rings')))
  # A genuinely shortened installation: everything after the cut is erased.
  offset, size = layout['PLTS']
  cases.append(('install cut inside PLTS', region[:offset + size // 2],
                ('beads',)))
  return cases


def install_profile(image, region):
  """Keep factory entries and add a checked directory containing TERR."""
  m = te.Machine(image)
  profile = bytearray(m.uc.mem_read(0x08020400, 4096))
  used, = struct.unpack_from('<H', profile, 6)
  records = bytearray()
  for kind, (offset, size) in sections(region).items():
    crc, = struct.unpack_from('<I', region, offset + size - 4)
    records += struct.pack('<4I', int.from_bytes(kind.encode(), 'little'),
                           offset, size, crc)
  profile[4] = 3
  profile[14] = 1
  profile[15] = len(records) // 16
  records = bytes([5] * 8) + records
  profile[16 + used:16 + used + len(records)] = records
  used += len(records)
  struct.pack_into('<H', profile, 6, used)
  struct.pack_into('<I', profile, 16 + used,
                   binascii.crc32(profile[:16 + used]))
  return bytes(profile)


def with_terrain(region):
  """Insert one grid before PLTS, retaining every released section byte."""
  layout = sections(region)
  if 'TERR' in layout:
    return region
  wave_end = layout['PLTS'][0]
  release, = struct.unpack_from('<I', region, 12)
  size = 32 + 4096 + 4
  header = struct.pack('<8I', 0x43535a44, int.from_bytes(b'TERR', 'little'),
                       1, release, size, 1, 0, 0)
  terrain = header + bytes((i * 3) % 256 for i in range(4096))
  terrain += struct.pack('<I', binascii.crc32(terrain))
  terrain += b'\xff' * (-len(terrain) % 32)
  return region[:wave_end] + terrain + region[wave_end:]


class InstalledMachine(te.Machine):
  profile = None
  storage = None

  def __init__(self, image):
    super().__init__(image)
    self.uc.hook_del(self.block_hook)
    self.uc.mem_write(0x08020400, self.profile)
    if self.storage is not None:
      self.uc.mem_write(0x081c0000, self.storage)
    self.writes = []
    def write(uc, access, address, size, value, user):
      self.writes.append((address, size))
    for begin, end in ((0x08020400, 0x080213ff),
                       (0x081c0000, 0x081fffff)):
      self.uc.hook_add(te.unicorn.UC_HOOK_MEM_WRITE, write,
                       begin=begin, end=end)

  def on_head(self, uc, address, size, user):
    self.head += 1
    if self.head > 1:
      uc.emu_stop()

  def run(self, begin, until, what, end=None):
    # Fault boots need behaviour, not Python callbacks counting every block.
    self.uc.emu_start(begin | 1, until, timeout=10000000)
    pc = self.uc.reg_read(te.A.UC_ARM_REG_PC)
    if pc != (until if end is None else end):
      raise RuntimeError('%s stopped at pc %x' % (what, pc))
    return 0

  def press(self, switch, down):
    # Valid saved records permit an ordinary boot without the recovery chord.
    super().press(switch, False if switch == 'seed' and
                  self.storage is not None else down)


def boot(image, region, selected, saved_state):
  r = te.Rig(image, region)
  m = r.m
  initial_state = bytes(m.uc.mem_read(r.state, 24))
  r.settle()
  r.recording = True
  r.tick(100)
  granular_sha256 = r.out.hexdigest()
  r.recording = False
  r.calls = []
  r.out = hashlib.sha256()
  playable, = m.read(image.address('playable_synths'), '<B')
  # As the synth scenario: inputs unplugged, then the 10 s synth timer.
  r.left = r.right = False
  timer = image.address('synth_mode_start_time')
  for _ in range(20):
    r.tick(100)
    if m.read(timer, '<I')[0]:
      break
  else:
    raise RuntimeError('unplugged inputs did not start the synth timer')
  # No audio or user events during the synth timer: advance both clocks.
  for clock in (image.address('_ZN6stmlib12system_clockE'),
                image.address('uwTick')):
    m.write(clock, '<I', (m.read(clock, '<I')[0] + 10000) & 0xffffffff)
  r.tick(600)
  if not m.read(image.address('synth_mode'), '<B')[0]:
    raise RuntimeError('synth mode did not start')
  r.calls = []
  r.out = hashlib.sha256()
  r.recording = True
  r.tick(100)
  pwm = []
  for _ in range(250):
    r.tick(1, audio=False)
    pwm.append(m.read(0x40000838, '<3I'))
  # SynthEngine is one byte (-fshort-enums).
  playing, = m.read(image.address('playing_synth'), '<B')
  identity = m.read(image.address('plaits_region'), '<3I16I')
  silent = all(c[2] for c in r.calls if c[0] == 'audio')
  return {
      'granular_sha256': granular_sha256,
      'selection_unchanged': m.read(r.values + te.FEATURE_SYNTH, '<B')[0] == selected,
      'profile_unchanged': bytes(m.uc.mem_read(0x08020400, 4096)) ==
          InstalledMachine.profile,
      'storage_unchanged': bytes(m.uc.mem_read(0x081c0000, 0x40000)) ==
          InstalledMachine.storage,
      'state_unchanged': initial_state == saved_state ==
          bytes(m.uc.mem_read(r.state, 24)),
      'storage_writes': len(m.writes),
      'cue_pwm': sorted(set(pwm)),
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
  parser = argparse.ArgumentParser()
  parser.add_argument('elf')
  parser.add_argument('region')
  parser.add_argument('out')
  parser.add_argument('--shard', default='1/1')
  args = parser.parse_args(argv[1:])
  shard, shards = map(int, args.shard.split('/'))
  assert 1 <= shard <= shards
  te.MEMBERS['Ui'] = te.MEMBERS['Ui'] + ('synth_fault_',)
  image = te.Image(args.elf)
  region = with_terrain(open(args.region, 'rb').read())
  InstalledMachine.profile = install_profile(image, region)
  te.Machine = InstalledMachine
  # Initialize valid calibration, options and State once, then ordinary boots.
  warm = te.Rig(image, region)
  InstalledMachine.storage = bytes(warm.m.uc.mem_read(0x081c0000, 0x40000))
  # A nondefault saved State and each saved Synth choice survive fallback.
  warm.m.write(warm.state + te.STATE_NO_AUTOSAVE, '<B', 1)
  saved = {}
  for selected in (0, 1):
    warm.set_feature(te.FEATURE_SYNTH, selected)
    warm.m.call('_ZN5beads8Settings9SaveStateEv', image.address('settings'))
    saved[selected] = bytes(warm.m.uc.mem_read(0x081c0000, 0x40000))
  saved_state = bytes(warm.m.uc.mem_read(warm.state, 24))
  results, failures = [], 0
  granular_reference = None
  terrain_reference = None
  cases = [(name, data, playable, selected)
           for name, data, playable in variants(region) for selected in (0, 1)]
  for index, (name, data, playable, selected) in enumerate(cases):
    if index % shards != shard - 1:
      continue
    playing = SYNTHS[selected] if SYNTHS[selected] in playable else \
        'beads' if 'beads' in playable else \
        'plaits' if 'plaits' in playable else 'silent'
    InstalledMachine.storage = saved[selected]
    got = boot(image, data, selected, saved_state)
    # JSON converts tuples to lists; compare tuples before serialization.
    cue_ok = playing != 'silent' or got['cue_pwm'] == [(0, 0, 0), (191, 191, 191)]
    statuses_ok = all((got['sections'][kind] == 'ok') == (engine in playable)
                      for kind, engine in (('WAVE', 'beads'), ('PLTS', 'plaits')))
    statuses_ok = statuses_ok and ((got['sections']['TERR'] == 'ok') ==
                                  (not name.startswith('TERR ')))
    ok = got['playable'] == list(playable) and got['playing'] == playing and \
        got['silent'] == (playing == 'silent') and \
        bool(got['synth_fault']) == (playing == 'silent') and cue_ok and \
        statuses_ok and all(got[k] for k in (
            'profile_unchanged', 'storage_unchanged', 'state_unchanged',
            'selection_unchanged')) and \
        not got['storage_writes']
    if granular_reference is None:
      granular_reference = got['granular_sha256']
    got['granular_unchanged'] = got['granular_sha256'] == granular_reference
    ok = ok and got['granular_unchanged']
    if name.startswith('TERR ') and selected == 1:
      if terrain_reference is None:
        terrain_reference = got['out_sha256']
      got['stock_terrain_equal'] = got['out_sha256'] == terrain_reference
      ok = ok and got['stock_terrain_equal']
    failures += not ok
    got.update(name=name, selected=SYNTHS[selected], ok=ok)
    results.append(got)
    print('%s selected %s: %s %s' % (
        name, SYNTHS[selected], 'OK' if ok else 'FAIL', json.dumps(got)),
        flush=True)
    json.dump({'elf_sha256': image.sha256, 'boots': results},
              open(args.out, 'w'), indent=1, sort_keys=True)
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
