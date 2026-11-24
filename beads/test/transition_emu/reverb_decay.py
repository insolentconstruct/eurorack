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

"""Check fixed reverb coefficients through an installed profile."""

import argparse
import binascii
import struct

import transition_emu as te

PROFILE_ADDRESS = te.FLASH + 0x20400
REVERB_INPUT, SYNTH_LEVEL, MANUAL_DELAY, REVERB_DECAY = 33, 50, 53, 54
POTS_ADDRESS = '_ZN5beads7PotsAdc7values_E'
ENGINES = (
    ('synth', 'Synth'),
    ('plaits_synth', 'PlaitsSynth'),
    ('rings_synth', 'RingsSynth'))


def install_profile(image, value, state=1, extras=(), manual_delay=False):
  machine = te.Machine(image)
  profile = bytearray(machine.uc.mem_read(PROFILE_ADDRESS, 16))
  count, length = struct.unpack_from('<BH', profile, 5)
  entries = bytearray(machine.uc.mem_read(PROFILE_ADDRESS + 16, length))
  offset = 6 * count
  if manual_delay:
    for i in range(5, offset, 6):
      if entries[i] == 1:
        entries[i] = 0
    extras += ((MANUAL_DELAY, 1, 1, 0, 0, 1),)
  entry = bytes((REVERB_DECAY, state, value, 0, 0, 0))
  entry += b"".join(bytes(row) for row in extras)
  profile[5] = count + 1 + len(extras)
  struct.pack_into('<H', profile, 6, length + len(entry))
  profile += entries[:offset] + entry + entries[offset:]
  return bytes(profile) + struct.pack('<I', binascii.crc32(profile))


def check_mix(rig, name, cls, decay, maximum, slope=0.0):
  base = rig.image.member(name, cls, 'reverb_')
  offsets = rig.image.offsets['Reverb']
  levels = []
  for pot in (0x0800, 0xe000):
    rig.m.write(rig.image.address(POTS_ADDRESS) + 4, '<H', pot)
    rig.tick(256)
    mix, = rig.m.read(base + offsets['mix_'], '<f')
    feedback, = rig.m.read(base + offsets['reverb_time_'], '<f')
    expected = decay + slope * (mix / maximum)**1.5
    assert abs(feedback - expected) < 1e-6, (name, pot, feedback, expected)
    levels.append(mix)
  assert 0 <= levels[0] < levels[1] <= maximum, (name, levels)
  return levels


def check_assigned_cv(rig, decay):
  base = rig.image.member('processor', 'Processor', 'reverb_')
  offsets = rig.image.offsets['Reverb']
  rig.m.write(rig.state + 1, '<B', 2)  # Assign CV to REVERB.
  rig.m.write(rig.image.address(POTS_ADDRESS) + 4, '<H', 0x8000)
  levels = []
  for raw in (0x4000, 0xc000):
    rig.assign_cv = raw
    rig.tick(256)
    levels.append(rig.m.read(base + offsets['mix_'], '<f')[0])
    feedback, = rig.m.read(base + offsets['reverb_time_'], '<f')
    assert abs(feedback - decay) < 1e-6, (raw, feedback, decay)
  assert levels[0] != levels[1], levels
  rig.assign_cv = None
  rig.m.write(rig.state + 1, '<B', 0)


def check_manual_delay(rig, decay):
  position = rig.image.member(
      'processor', 'Processor', 'delay_mode_transition_')
  assert rig.m.read(position, '<f')[0] < 0.0
  rig.combo('media', 'seed')
  rig.tick(1800)
  assert rig.m.read(position, '<f')[0] > 0.0
  check_mix(rig, 'processor', 'Processor', decay, 0.9)
  rig.combo('media', 'seed')
  rig.tick(1800)
  assert rig.m.read(position, '<f')[0] < 0.0
  check_mix(rig, 'processor', 'Processor', decay, 0.9)
  print('Fixed decay %.2f passed with manual delay' % decay, flush=True)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('elf')
  parser.add_argument('--region', type=argparse.FileType('rb'))
  args = parser.parse_args()
  region = args.region.read() if args.region else None
  te.MEMBERS.update((cls, ('reverb_',)) for _, cls in ENGINES)
  te.MEMBERS['Processor'] = ('reverb_', 'delay_mode_transition_')
  te.MEMBERS['Reverb'] = ('mix_', 'reverb_time_')
  image = te.Image(args.elf)
  off = te.Rig(image, region, install_profile(image, 3, state=0))
  off.settle()
  media = off.state_byte(0)
  assert media == 3
  coupled = check_mix(off, 'processor', 'Processor', 0.3, 0.9, 0.6)
  off.set_media(2)
  off.tick(320)
  check_mix(off, 'processor', 'Processor', 0.45, 0.9, 0.5)
  for choice, decay in enumerate((0.30, 0.60, 0.85), 1):
    # Long also exercises the grains reverb route and synth output trim.
    extras = ((REVERB_INPUT, 1, 1, 0, 0, 0),
              (SYNTH_LEVEL, 1, 1, 0, 0, 0)) if choice == 3 else ()
    manual_delay = choice in (1, 3)
    profile = install_profile(
        image, choice, extras=extras, manual_delay=manual_delay)
    rig = te.Rig(image, region, profile)
    rig.settle()
    fixed = check_mix(rig, 'processor', 'Processor', decay, 0.9)
    assert fixed == coupled, (choice, fixed, coupled)
    print('Fixed decay %d passed in the processor' % choice, flush=True)
    if manual_delay:
      check_manual_delay(rig, decay)
  rig.set_media(2)
  rig.tick(320)
  check_assigned_cv(rig, decay)
  rig.set_media(media)
  rig.tick(320)
  rig.set_feature(te.FEATURE_PROCESSOR, 1)
  rig.tick(320)
  check_mix(rig, 'processor', 'Processor', decay, 0.9)
  rig.set_feature(te.FEATURE_PROCESSOR, 0)
  rig.tick(320)
  rig.set_feature(te.FEATURE_REVERB, 1)
  rig.tick(40)
  clouds = image.member('transition', 'EngineTransition', 'clouds_style_')
  assert rig.m.read(clouds, '<B')[0]
  check_mix(rig, 'processor', 'Processor', decay, 0.9)
  rig.left = rig.right = False
  rig.tick(300)
  rig.tick(9800, audio=False)
  rig.tick(600)
  assert rig.m.read(image.address('synth_mode'), '<B')[0]
  for selected, (name, cls) in enumerate(ENGINES if region else ENGINES[:1]):
    rig.set_feature(te.FEATURE_SYNTH, selected)
    rig.tick(700)
    assert rig.m.read(image.address('playing_synth'), '<B')[0] == selected
    check_mix(rig, name, cls, decay, 0.8)
    print('Fixed decay passed in %s' % cls, flush=True)
  rig.left = rig.right = True
  rig.tick(700)
  assert not rig.m.read(image.address('synth_mode'), '<B')[0]
  check_mix(rig, 'processor', 'Processor', decay, 0.9)


if __name__ == '__main__':
  main()
