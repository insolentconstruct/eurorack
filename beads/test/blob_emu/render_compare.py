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
# -----------------------------------------------------------------------------
#
# Render each blob and the same engine compiled in, under Unicorn's Cortex-M7,
# and compare every OUT and AUX word. The region is copied into flash at two
# placements; the reader's refusals are checked on faulted copies.
#
#   render_compare.py OUT  (beads/test/blob_emu/build.sh's output)

import hashlib
import json
import random
import struct
import sys

import unicorn
from unicorn import arm_const as A

FLASH, FLASH_SIZE = 0x08000000, 0x200000
RAM, RAM_SIZE = 0x24000000, 0x80000
STACK = RAM + RAM_SIZE - 0x100
STOP = 0x0FF00000
PLACEMENTS = (0x080C0000, 0x080C2FE0)
REGION_END = 0x08100000
RELEASE = 20261117
FPSCR = (0, 0x03000000)  # reset, and flush-to-zero with default NaN
# (blob model, compiled-in engine index)
PAIRS = {36: 0, 37: 1}
NUM_COMPILED = 2
BLOCK = 12
STATUS = ['OK', 'PLACEMENT', 'HEADER', 'ABI', 'SERVICES', 'LAYOUT', 'RAM',
          'CRC', 'DIGEST', 'IMPORTS', 'NO_REGION', 'RELEASE', 'SELECTION',
          'TABLE', 'MODEL']
SINE = 0x08021440


def symbols(path):
  table = {}
  for line in open(path):
    fields = line.split()
    if len(fields) == 3:
      table[fields[2]] = int(fields[0], 16)
  return table


def segments(path):
  d = open(path, 'rb').read()
  phoff, = struct.unpack_from('<I', d, 0x1c)
  phentsize, phnum = struct.unpack_from('<HH', d, 0x2a)
  for i in range(phnum):
    kind, offset, vaddr, paddr, filesz, memsz, flags, align = \
        struct.unpack_from('<8I', d, phoff + i * phentsize)
    if kind == 1 and filesz:
      yield vaddr, d[offset:offset + filesz]


class Machine(object):

  def __init__(self, elf, sym, region, base, fpscr, patches=()):
    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
    uc.ctl_set_cpu_model(A.UC_CPU_ARM_CORTEX_M7)
    if uc.ctl_get_cpu_model() != A.UC_CPU_ARM_CORTEX_M7:
      raise RuntimeError('unicorn did not select the Cortex-M7 model')
    uc.mem_map(FLASH, FLASH_SIZE)
    uc.mem_write(FLASH, b'\xff' * FLASH_SIZE)
    uc.mem_map(RAM, RAM_SIZE)
    uc.mem_map(STOP, 0x1000)
    for address, data in segments(elf):
      uc.mem_write(address, data)
    uc.mem_write(base, region)
    for address, data in patches:
      uc.mem_write(address, data)
    # vmsr fpscr, r0; vmrs r1, fpscr
    uc.mem_write(STOP + 0x100, bytes.fromhex('e1ee100af1ee101a'))
    uc.reg_write(A.UC_ARM_REG_R0, fpscr)
    uc.emu_start((STOP + 0x100) | 1, STOP + 0x108)
    if uc.reg_read(A.UC_ARM_REG_R1) & 0x07c00000 != fpscr & 0x07c00000:
      raise RuntimeError('FPSCR did not take')
    self.uc = uc
    self.sym = sym

  def call(self, name, *args):
    uc = self.uc
    for i, a in enumerate(args):
      uc.reg_write(A.UC_ARM_REG_R0 + i, a & 0xffffffff)
    uc.reg_write(A.UC_ARM_REG_SP, STACK)
    uc.reg_write(A.UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(self.sym[name] | 1, STOP, count=200000000)
    if uc.reg_read(A.UC_ARM_REG_PC) != STOP:
      raise RuntimeError('%s did not return' % name)
    return uc.reg_read(A.UC_ARM_REG_R0)

  def setup(self, base, expected_crc, digests):
    for i, d in enumerate(digests):
      self.uc.mem_write(self.sym['harness_digests'] + 32 * i, d)
    return STATUS[self.call('harness_setup', base, REGION_END, RELEASE,
                            expected_crc)]

  def identity(self):
    raw = bytes(self.uc.mem_read(self.sym['harness_identity'], 24 + 8 * 48))
    words = struct.unpack_from('<6I', raw, 0)
    blobs = []
    for i in range(words[5]):
      model, address, size, status = struct.unpack_from('<4I', raw, 24 + 48 * i)
      blobs.append({'model': model, 'address': '0x%08x' % address,
                    'size': size, 'status': STATUS[status],
                    'sha256': raw[40 + 48 * i:72 + 48 * i].hex()})
    return {'release': words[0], 'services_version': words[1],
            'services_status': STATUS[words[2]],
            'region_status': STATUS[words[3]],
            'region_crc': '0x%08x' % words[4], 'blobs': blobs}

  def render(self, engine, parameters, size=BLOCK):
    trigger, note, harmonics, timbre, morph, accent = parameters
    # plaits::EngineParameters' order.
    self.uc.mem_write(self.sym['harness_parameters'], struct.pack(
        '<i5f', trigger, note, timbre, morph, harmonics, accent))
    self.call('harness_render', engine, size)
    return (bytes(self.uc.mem_read(self.sym['harness_out'], 4 * size)),
            bytes(self.uc.mem_read(self.sym['harness_aux'], 4 * size)))


def drive(seed):
  """The parameter sequence: a grid with note-ons, then random blocks."""
  steps = []
  values = (0.0, 0.25, 0.5, 0.75, 1.0)
  for note in (24.0, 48.0, 60.5, 84.0, 108.0):
    for h in values:
      for t in values:
        for m in values:
          for block in range(4):
            steps.append((1 if block == 0 else 0, note, h, t, m, 0.5))
  r = random.Random(seed)
  for _ in range(2000):
    steps.append((r.choice((0, 0, 0, 1)), r.uniform(0.0, 127.0),
                  r.random(), r.random(), r.random(), r.random()))
  return steps


def compare(m, blob_engine, compiled_engine, steps, nudge=0.0):
  m.call('harness_reset', blob_engine)
  m.call('harness_reset', compiled_engine)
  differ = 0
  samples = 0
  h = hashlib.sha256()
  for trigger, note, harmonics, timbre, morph, accent in steps:
    p = (trigger, note, harmonics, timbre, morph, accent)
    a = m.render(blob_engine, p)
    b = m.render(compiled_engine, (trigger, note + nudge) + p[2:])
    h.update(a[0] + a[1])
    for x, y in ((a[0], b[0]), (a[1], b[1])):
      for i in range(0, len(x), 4):
        samples += 1
        differ += x[i:i + 4] != y[i:i + 4]
        v, = struct.unpack_from('<f', x, i)
        if v != v or v in (float('inf'), float('-inf')):
          raise RuntimeError('non-finite output')
  return differ, samples, h.hexdigest()


def main(argv):
  out = argv[1]
  elf = out + '/harness.elf'
  sym = symbols(out + '/harness.nm')
  region = open(out + '/region.bin', 'rb').read()
  layout = json.load(open(out + '/region.bin.json'))
  crc = int(layout['crc'], 16)
  digests = [bytes.fromhex(b['sha256']) for b in layout['blobs']]
  steps = drive(1)
  results = {'unicorn': unicorn.__version__, 'blocks': len(steps),
             'block_size': BLOCK, 'runs': [], 'controls': []}
  failures = []

  for base in PLACEMENTS:
    for fpscr in FPSCR:
      m = Machine(elf, sym, region, base, fpscr)
      status = m.setup(base, crc, digests)
      identity = m.identity()
      if status != 'OK':
        failures.append('0x%08x: region %s' % (base, status))
        continue
      for i, blob in enumerate(identity['blobs']):
        compiled = PAIRS[blob['model']]
        differ, samples, digest = compare(m, NUM_COMPILED + i, compiled, steps)
        run = {'base': '0x%08x' % base, 'fpscr': '0x%08x' % fpscr,
               'model': blob['model'], 'address': blob['address'],
               'differ': differ, 'samples': samples, 'sha256': digest}
        results['runs'].append(run)
        print('0x%08x fpscr 0x%08x model %d at %s: %d of %d differ' % (
            base, fpscr, blob['model'], blob['address'], differ, samples))
        if differ:
          failures.append('model %d differs at 0x%08x' % (blob['model'], base))
      results['identity'] = identity

  # The comparison must see a difference: the note nudged by 0.01.
  m = Machine(elf, sym, region, PLACEMENTS[0], 0)
  m.setup(PLACEMENTS[0], crc, digests)
  differ, samples, _ = compare(m, NUM_COMPILED, 0, steps[:200], nudge=0.01)
  results['controls'].append({'case': 'note nudged', 'differ': differ})
  if not differ:
    failures.append('a nudged note compared equal')

  # The blob engines execute the region's bytes; the compiled ones do not.
  base = PLACEMENTS[1]
  m = Machine(elf, sym, region, base, 0)
  m.setup(base, crc, digests)
  executed = [0]
  def count(uc, address, size, user):
    executed[0] += 1
  m.uc.hook_add(unicorn.UC_HOOK_CODE, count, begin=base,
                end=base + len(region) - 1)
  for engine, inside in ((0, False), (1, False), (NUM_COMPILED, True),
                         (NUM_COMPILED + 1, True)):
    executed[0] = 0
    m.render(engine, steps[0])
    results['controls'].append({'case': 'engine %d in the region' % engine,
                                'instructions': executed[0]})
    if bool(executed[0]) != inside:
      failures.append('engine %d: %d instructions in the region' % (
          engine, executed[0]))

  # The reader's refusals, each on a faulted copy.
  base = PLACEMENTS[0]
  cases = [
      ('services export byte flipped', region, crc, digests,
       [(SINE + 100, b'\x5a')], 'SERVICES'),
      ('interrupted update', region[:len(region) // 2] +
       b'\xff' * (len(region) - len(region) // 2), crc, digests, [], 'CRC'),
      ('stale tail after the region', region, crc, digests,
       [(base + len(region), b'DZMB' * 64)], 'OK'),
      ('another selection', region, crc ^ 1, digests, [], 'SELECTION'),
      ('no selection', region, 0, digests, [], 'NO_REGION'),
      ('erased region', b'\xff' * 64, crc, digests, [], 'NO_REGION'),
      ('digest not listed', region, crc, digests[1:], [], 'DIGEST'),
  ]
  for name, data, expected_crc, listed, patches, expected in cases:
    m = Machine(elf, sym, data, base, 0, patches)
    status = m.setup(base, expected_crc, listed)
    results['controls'].append({'case': name, 'status': status})
    print('control: %s: %s' % (name, status))
    if status != expected:
      failures.append('%s: %s, not %s' % (name, status, expected))
  # Another release's region: its table says 20261116.
  m = Machine(elf, sym, region, base, 0)
  status = STATUS[m.call('harness_setup', base, REGION_END, RELEASE - 1, crc)]
  results['controls'].append({'case': 'another release', 'status': status})
  print('control: another release: %s' % status)
  if status != 'RELEASE':
    failures.append('another release: %s' % status)

  json.dump(results, open(out + '/render_compare.json', 'w'), indent=1,
            sort_keys=True)
  for f in failures:
    print('ERROR: ' + f)
  if not failures:
    print('render_compare: PASS')
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
