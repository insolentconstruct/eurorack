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
# Render each blob and the same engine compiled in, under Unicorn's Cortex-M7,
# and compare every OUT and AUX word and the shared generator's state. The
# region (WAVE and PLTS sections) is copied into flash at two placements.
# Also: model-switch sequences with poisoned storage, a write guard around
# the blob voice's slot and arena, and the reader's faults and synth locks.
#
#   render_compare.py OUT  (beads/test/blob_emu/build.sh's output)

import hashlib
import json
import random
import struct
import sys
import zlib

import unicorn
from unicorn import arm_const as A

FLASH, FLASH_SIZE = 0x08000000, 0x200000
DTCM, DTCM_SIZE = 0x20000000, 0x1000
RAM, RAM_SIZE = 0x24000000, 0x80000
STACK = RAM + RAM_SIZE - 0x100
STACK_LIMIT = STACK - 0x4000
STOP = 0x0FF00000
PLACEMENTS = (0x080C0000, 0x080C2FE0)
REGION_END = 0x08100000
RELEASE = 20261121
FPSCR = (0, 0x03000000)  # reset, and flush-to-zero with default NaN
RANDOM_STATE = 0x20000000
# Blob model: compiled-in engine (harness.cc's order).
PAIRS = {36: 0, 37: 1, 18: 2, 43: 3}
BLOCK = 12
STATUS = ['OK', 'PLACEMENT', 'HEADER', 'ABI', 'SERVICES', 'LAYOUT', 'RAM',
          'CRC', 'DIGEST', 'IMPORTS', 'NO_REGION', 'RELEASE', 'SELECTION',
          'TABLE', 'MODEL', 'KIND', 'LIBRARY', 'CONTENT', 'DIRECTORY',
          'ECC']
LOCKS = ['AS_PROFILED', 'FORCE_BEADS', 'FORCE_PLAITS', 'SILENT']
SECTIONS = ('WAVE', 'TERR', 'PLTS')
KINDS = {'WAVE': 0x45564157, 'TERR': 0x52524554, 'PLTS': 0x53544c50}
SINE = 0x08021440
POISON = struct.pack('<I', 0x7fa5a5a5)  # a NaN no engine writes
SLOT_SIZE = 4352
ARENA_SIZE = 16384


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
    uc.mem_map(DTCM, DTCM_SIZE)
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
    self.base = base
    self.size = len(region)
    self.arena = None

  def call(self, name, *args):
    uc = self.uc
    for i, a in enumerate(args):
      uc.reg_write(A.UC_ARM_REG_R0 + i, a & 0xffffffff)
    uc.reg_write(A.UC_ARM_REG_SP, STACK)
    uc.reg_write(A.UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(self.sym[name] | 1, STOP, count=400000000)
    if uc.reg_read(A.UC_ARM_REG_PC) != STOP:
      raise RuntimeError('%s did not return' % name)
    return uc.reg_read(A.UC_ARM_REG_R0)

  def word(self, address):
    return struct.unpack('<I', bytes(self.uc.mem_read(address, 4)))[0]

  def setup(self, layout, digests, directory=None, defaults=None,
            release=RELEASE):
    for i, d in enumerate(digests):
      self.uc.mem_write(self.sym['harness_digests'] + 32 * i, d)
    self.uc.mem_write(self.sym['harness_num_digests'],
                      struct.pack('<I', len(digests)))
    entries = layout['sections'] if directory is None else directory
    data = b''.join(struct.pack('<4I', KINDS[s['kind']], s['offset'],
                                s['size'], int(s['crc'], 16))
                    for s in entries)
    self.uc.mem_write(self.sym['harness_directory'], data)
    self.uc.mem_write(self.sym['harness_num_entries'],
                      struct.pack('<I', len(entries)))
    if defaults is not None:
      data = b''.join(struct.pack('<3I', KINDS[s['kind']], s['offset'],
                                  s['size']) + bytes.fromhex(s['sha256'])
                      for s in defaults)
      self.uc.mem_write(self.sym['harness_defaults'], data)
      self.uc.mem_write(self.sym['harness_num_defaults'],
                        struct.pack('<I', len(defaults)))
    status = self.call('harness_setup', self.base, REGION_END, release,
                       0 if defaults is not None else 1)
    return STATUS[status]

  def identity(self):
    # PlaitsRegionIdentity: three words, three sections of four, the count.
    raw = bytes(self.uc.mem_read(self.sym['harness_identity'], 64))
    words = struct.unpack_from('<16I', raw, 0)
    sections = {}
    for i, name in enumerate(SECTIONS):
      status, address, size, crc = words[3 + 4 * i:7 + 4 * i]
      sections[name] = {'status': STATUS[status],
                        'address': '0x%08x' % address, 'size': size,
                        'crc': '0x%08x' % crc}
    return {'release': words[0], 'services_status': STATUS[words[1]],
            'directory_status': STATUS[words[2]], 'sections': sections,
            'num_blobs': words[15]}

  def statuses(self):
    identity = self.identity()
    return tuple(identity['sections'][s]['status'] for s in SECTIONS)

  def lock(self):
    return LOCKS[self.word(self.sym['harness_lock'])]

  def parameters(self, p):
    trigger, note, harmonics, timbre, morph, accent = p
    # plaits::EngineParameters' order.
    self.uc.mem_write(self.sym['harness_parameters'], struct.pack(
        '<i5f', trigger, note, timbre, morph, harmonics, accent))
    for name in ('harness_out', 'harness_aux'):
      self.uc.mem_write(self.sym[name], POISON * BLOCK)

  def outputs(self, label):
    out = bytes(self.uc.mem_read(self.sym['harness_out'], 4 * BLOCK))
    aux = bytes(self.uc.mem_read(self.sym['harness_aux'], 4 * BLOCK))
    for x in (out, aux):
      if any(x[i:i + 4] == POISON for i in range(0, len(x), 4)):
        raise RuntimeError('%s left a sample unwritten' % label)
    return out, aux

  def render_blob(self, p, seed):
    self.parameters(p)
    state = self.call('harness_render_blob', BLOCK, seed)
    return self.outputs('blob') + (state,)

  def render_compiled(self, engine, p, seed):
    self.parameters(p)
    state = self.call('harness_render_compiled', engine, BLOCK, seed)
    return self.outputs('engine %d' % engine) + (state,)

  def select_blob(self, model):
    # Poison the slot and arena: the voice clears the slot; a blob must not
    # depend on what the arena held.
    slot = self.word(self.sym['harness_slot'])
    arena = self.sym['harness_blob_voice'] + self.arena
    self.uc.mem_write(slot, POISON * (SLOT_SIZE // 4))
    self.uc.mem_write(arena, POISON * (ARENA_SIZE // 4))
    if self.call('harness_select_blob', model):
      raise RuntimeError('no blob for model %d' % model)

  def select_compiled(self, engine):
    self.call('harness_select_compiled', engine)


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


def compare_blocks(m, engine, steps, seed, nudge=0.0, digest=None,
                   stats=None):
  """Blob voice against compiled engine, block by block, from one seed."""
  differ = samples = 0
  for block, (trigger, note, harmonics, timbre, morph, accent) in enumerate(
      steps):
    p = (trigger, note, harmonics, timbre, morph, accent)
    a_out, a_aux, a_seed = m.render_blob(p, seed)
    b_out, b_aux, b_seed = m.render_compiled(
        engine, (trigger, note + nudge) + p[2:], seed)
    if digest is not None:
      digest.update(a_out + a_aux)
    for x, y in ((a_out, b_out), (a_aux, b_aux)):
      for i in range(0, len(x), 4):
        samples += 1
        v, = struct.unpack_from('<f', x, i)
        if v != v or v in (float('inf'), float('-inf')):
          raise RuntimeError('non-finite output')
        if x[i:i + 4] != y[i:i + 4]:
          differ += 1
          if stats is not None:
            w, = struct.unpack_from('<f', y, i)
            stats['max'] = max(stats.get('max', 0.0), abs(v - w))
            stats['peak'] = max(stats.get('peak', 0.0), abs(w))
            stats['last_block'] = block
    if a_seed != b_seed:
      raise RuntimeError('the generator diverged')
    seed = a_seed
  return differ, samples, seed


class WriteGuard(object):
  """Records writes made by code inside the region outside the places a
  blob may write: its slot and arena, the stack, the outputs and the shared
  generator's state."""

  def __init__(self, m, state_size, arena_size):
    self.violations = []
    slot = m.word(m.sym['harness_slot'])
    arena = m.sym['harness_blob_voice'] + m.arena
    self.allowed = [(slot, slot + state_size), (arena, arena + arena_size),
                    (STACK_LIMIT, STACK), (RANDOM_STATE, RANDOM_STATE + 4),
                    (m.sym['harness_out'], m.sym['harness_out'] + 4 * BLOCK),
                    (m.sym['harness_aux'], m.sym['harness_aux'] + 4 * BLOCK)]
    self.begin, self.end = m.base, m.base + m.size
    self.handle = m.uc.hook_add(unicorn.UC_HOOK_MEM_WRITE, self.hook)

  def hook(self, uc, access, address, size, value, user):
    pc = uc.reg_read(A.UC_ARM_REG_PC)
    if not self.begin <= pc < self.end:
      return
    if not any(lo <= address and address + size <= hi
               for lo, hi in self.allowed):
      self.violations.append('pc 0x%08x wrote %d B at 0x%08x' % (
          pc, size, address))


def main(argv):
  out = argv[1]
  elf = out + '/harness.elf'
  sym = symbols(out + '/harness.nm')
  region = open(out + '/region.bin', 'rb').read()
  layout = json.load(open(out + '/region.bin.json'))
  plts = [s for s in layout['sections'] if s['kind'] == 'PLTS'][0]
  wave = [s for s in layout['sections'] if s['kind'] == 'WAVE'][0]
  blobs = {b['model']: b for b in plts['blobs']}
  digests = [bytes.fromhex(b['sha256']) for b in plts['blobs']]
  steps = drive(1)
  results = {'unicorn': unicorn.__version__, 'blocks': len(steps),
             'block_size': BLOCK, 'runs': [], 'switches': [], 'controls': [],
             'faults': [], 'locks': []}
  failures = []
  findings = []

  def machine(base=PLACEMENTS[0], fpscr=0, data=region, patches=()):
    return Machine(elf, sym, data, base, fpscr, patches)

  # BlobVoice: 64 B of canary, the engine (whose last member is the slot),
  # 64 B of canary, the arena, 64 B of canary.
  def arena_of(m):
    slot = m.word(sym['harness_slot'])
    return slot + SLOT_SIZE + 64 - sym['harness_blob_voice']

  for base in PLACEMENTS:
    for fpscr in FPSCR:
      m = machine(base, fpscr)
      status = m.setup(layout, digests)
      if status != 'OK':
        failures.append('0x%08x: PLTS %s' % (base, status))
        continue
      m.arena = arena_of(m)
      for model, engine in sorted(PAIRS.items()):
        m.select_blob(model)
        m.select_compiled(engine)
        h = hashlib.sha256()
        differ, samples, _ = compare_blocks(m, engine, steps, 0x21, digest=h)
        run = {'base': '0x%08x' % base, 'fpscr': '0x%08x' % fpscr,
               'model': model, 'differ': differ, 'samples': samples,
               'sha256': h.hexdigest()}
        results['runs'].append(run)
        print('0x%08x fpscr 0x%08x model %d: %d of %d differ' % (
            base, fpscr, model, differ, samples))
        if differ:
          failures.append('model %d differs at 0x%08x' % (model, base))
      results['identity'] = m.identity()

  def state_of(model):
    info = blobs[model]
    blob = region[plts['offset'] + info['offset']:]
    abi = struct.unpack_from('<I', blob, 8)[0]
    return struct.unpack_from('<I', blob, 24 if abi == 1 else 32)[0]

  # Switch sequences, A -> B -> A: the blob voice constructs on every switch
  # in a poisoned slot and arena; compiled-in engines only Reset. Retained
  # state shows as differences after the return; they are reported.
  sequence = steps[:300]
  for first, second in ((18, 43), (43, 18), (18, 36), (43, 36), (36, 18)):
    m = machine(PLACEMENTS[1])
    m.setup(layout, digests)
    m.arena = arena_of(m)
    seed = 0x21
    phases = []
    for model, part in ((first, sequence[:100]), (second, sequence[100:200]),
                        (first, sequence[200:])):
      m.select_blob(model)
      m.select_compiled(PAIRS[model])
      slot = m.word(sym['harness_slot'])
      cleared = POISON not in bytes(m.uc.mem_read(slot, state_of(model)))
      stats = {}
      differ, samples, seed = compare_blocks(m, PAIRS[model], part, seed,
                                             stats=stats)
      phases.append({'model': model, 'differ': differ, 'samples': samples,
                     'slot_cleared': cleared, 'max_difference':
                     stats.get('max', 0.0), 'compiled_peak':
                     stats.get('peak', 0.0), 'last_differing_block':
                     stats.get('last_block')})
      if not cleared:
        failures.append('switch %d->%d: poison left in the slot' % (
            first, second))
    results['switches'].append({'sequence': [first, second, first],
                                'phases': phases})
    print('switch %d -> %d -> %d: %s' % (first, second, first, ', '.join(
        '%d of %d differ' % (p['differ'], p['samples']) for p in phases)))
    if phases[0]['differ'] or phases[1]['differ']:
      failures.append('switch %d->%d differs before the return' % (
          first, second))
    if phases[2]['differ']:
      findings.append('model %d after %d: %d of %d samples differ from the '
                      'compiled-in engine, which keeps its state across a '
                      'switch (Reset only); largest difference %.3g, '
                      'compiled peak %.3g, last differing block %d of %d' % (
                          first, second, phases[2]['differ'],
                          phases[2]['samples'], phases[2]['max_difference'],
                          phases[2]['compiled_peak'],
                          phases[2]['last_differing_block'],
                          len(sequence[200:])))

  # Each comparison must see a difference: the note nudged by 0.01.
  m = machine()
  m.setup(layout, digests)
  m.arena = arena_of(m)
  for model, engine in sorted(PAIRS.items()):
    m.select_blob(model)
    m.select_compiled(engine)
    differ, samples, _ = compare_blocks(m, engine, steps[:200], 0x21,
                                        nudge=0.01)
    results['controls'].append({'case': 'note nudged, model %d' % model,
                                'differ': differ, 'samples': samples})
    print('control: note nudged, model %d: %d of %d differ' % (
        model, differ, samples))
    if not differ:
      failures.append('model %d: a nudged note compared equal' % model)

  # Particle Noise draws: a different seed must change its output.
  m.select_blob(18)
  m.select_compiled(2)
  a = m.render_blob(steps[1], 0x21)
  b = m.render_compiled(2, steps[1], 0x22)
  results['controls'].append({'case': 'another seed, model 18',
                              'equal': a[:2] == b[:2]})
  if a[:2] == b[:2]:
    failures.append('model 18 did not depend on the shared generator')

  # Blob code runs inside the region and writes only its own places.
  base = PLACEMENTS[1]
  m = machine(base)
  m.setup(layout, digests)
  m.arena = arena_of(m)
  canary = b'\x5a\xc3\x3c\xa5' * 16
  voice = sym['harness_blob_voice']
  slot = m.word(sym['harness_slot'])
  bands = (voice, slot + SLOT_SIZE, voice + m.arena + ARENA_SIZE)
  for band in bands:
    m.uc.mem_write(band, canary)
  executed = [0]
  def count(uc, address, size, user):
    executed[0] += 1
  m.uc.hook_add(unicorn.UC_HOOK_CODE, count, begin=base,
                end=base + len(region) - 1)
  for model, engine in sorted(PAIRS.items()):
    info = blobs[model]
    blob = region[plts['offset'] + info['offset']:
                  plts['offset'] + info['offset'] + info['size']]
    abi = struct.unpack_from('<I', blob, 8)[0]
    state, arena = struct.unpack_from('<2I', blob, 24 if abi == 1 else 32)
    guard = WriteGuard(m, state, arena)
    executed[0] = 0
    m.select_blob(model)
    for p in steps[:50]:
      m.render_blob(p, 0x21)
    inside = executed[0]
    m.uc.hook_del(guard.handle)
    executed[0] = 0
    m.select_compiled(engine)
    m.render_compiled(engine, steps[0], 0x21)
    results['controls'].append({
        'case': 'model %d: region code and writes' % model,
        'blob_instructions': inside, 'compiled_instructions': executed[0],
        'stray_writes': guard.violations[:8]})
    print('control: model %d: %d instructions in the region, compiled %d, '
          '%d stray writes' % (model, inside, executed[0],
                               len(guard.violations)))
    if not inside or executed[0] or guard.violations:
      failures.append('model %d: region execution or writes' % model)
  canaries_ok = all(bytes(m.uc.mem_read(band, 64)) == canary
                    for band in bands)
  results['controls'].append({'case': 'canaries', 'intact': canaries_ok})
  if not canaries_ok:
    failures.append('a canary around the slot or arena changed')

  # Faults, each section on its own; a damaged section hides no other.
  pbase = PLACEMENTS[0]
  p_off, w_off = plts['offset'], wave['offset']

  def crc_fix(data, offset, size):
    body = data[offset:offset + size - 4]
    return data[:offset + size - 4] + struct.pack(
        '<I', zlib.crc32(body) & 0xffffffff) + data[offset + size:]

  def flip(data, at):
    return data[:at] + bytes([data[at] ^ 1]) + data[at + 1:]

  first_blob = p_off + plts['blobs'][0]['offset']
  guard_row = w_off + 32 + 192 * 520 + 10
  cases = [
      ('intact', region, None, None, ('OK', 'NO_REGION', 'OK')),
      ('PLTS byte flipped', flip(region, first_blob + 200), None, None,
       ('OK', 'NO_REGION', 'CRC')),
      ('PLTS interrupted', region[:p_off + 2000] +
       b'\xff' * (len(region) - p_off - 2000), None, None,
       ('OK', 'NO_REGION', 'CRC')),
      ('PLTS blob byte flipped, CRCs refreshed', crc_fix(
          flip(region, first_blob + 200), p_off, plts['size']), None, None,
       ('OK', 'NO_REGION', 'SELECTION')),
      ('WAVE guard row changed, CRC refreshed', crc_fix(
          flip(region, guard_row), w_off, wave['size']), None, None,
       ('SELECTION', 'NO_REGION', 'OK')),
      ('WAVE erased', b'\xff' * p_off + region[p_off:], None, None,
       ('NO_REGION', 'NO_REGION', 'OK')),
      ('stale tail after PLTS', region + b'DZSC' * 64, None, None,
       ('OK', 'NO_REGION', 'OK')),
  ]
  for name, data, directory, defaults, expected in cases:
    m = machine(pbase, data=data)
    m.setup(layout, digests)
    got = m.statuses()
    results['faults'].append({'case': name, 'statuses': got})
    print('fault: %s: %s' % (name, ' '.join(got)))
    if got != expected:
      failures.append('%s: %s, not %s' % (name, got, expected))

  # The guard row on its own: selected by a directory with its new CRC, the
  # content check refuses it.
  data = crc_fix(flip(region, guard_row), w_off, wave['size'])
  new_crc = '0x%08x' % struct.unpack_from('<I', data, w_off + wave['size'] - 4)
  directory = [dict(s) for s in layout['sections']]
  directory[0]['crc'] = new_crc
  m = machine(pbase, data=data)
  m.setup(layout, digests, directory=directory)
  got = m.statuses()
  results['faults'].append({'case': 'WAVE guard row, selected',
                            'statuses': got})
  print('fault: WAVE guard row, selected: %s' % ' '.join(got))
  if got[0] != 'CONTENT' or got[2] != 'OK':
    failures.append('WAVE guard row, selected: %s' % (got,))

  # Release, selection, digests, services, directory layout.
  more = [
      ('another release', {'release': RELEASE - 1}, ('RELEASE', 'NO_REGION',
                                                     'RELEASE')),
      ('digest not listed', {'digests': digests[1:]}, ('OK', 'NO_REGION',
                                                       'DIGEST')),
      ('directory out of order', {'directory': layout['sections'][::-1]},
       ('DIRECTORY',) * 3),
      ('directory past the region', {'directory': [dict(
          plts, offset=REGION_END - pbase - 64)]}, ('DIRECTORY',) * 3),
      ('directory misaligned', {'directory': [dict(
          plts, offset=plts['offset'] + 4)]}, ('DIRECTORY',) * 3),
      ('directory overlapping', {'directory': [
          dict(wave, size=wave['size'] + 64), plts]}, ('DIRECTORY',) * 3),
      ('PLTS only', {'directory': [plts]}, ('NO_REGION', 'NO_REGION', 'OK')),
      ('another selection', {'directory': [dict(
          plts, crc='0x%08x' % (int(plts['crc'], 16) ^ 1))]},
       ('NO_REGION', 'NO_REGION', 'SELECTION')),
  ]
  for name, options, expected in more:
    m = machine(pbase)
    status = m.setup(layout, options.get('digests', digests),
                     directory=options.get('directory'),
                     release=options.get('release', RELEASE))
    got = m.statuses()
    results['faults'].append({'case': name, 'statuses': got})
    print('fault: %s: %s' % (name, ' '.join(got)))
    if got != expected:
      failures.append('%s: %s, not %s' % (name, got, expected))
  m = machine(pbase, patches=[(SINE + 100, b'\x5a')])
  m.setup(layout, digests)
  got = m.statuses()
  results['faults'].append({'case': 'services export byte flipped',
                            'statuses': got})
  print('fault: services export byte flipped: %s' % ' '.join(got))
  if got != ('OK', 'NO_REGION', 'SERVICES'):
    failures.append('services export byte flipped: %s' % (got,))

  # Without a profile directory: the release's default layout, by digest.
  defaults = layout['sections']
  for name, data, expected in (
      ('defaults', region, ('OK', 'NO_REGION', 'OK')),
      ('defaults, PLTS changed', crc_fix(flip(region, first_blob + 200),
                                         p_off, plts['size']),
       ('OK', 'NO_REGION', 'DIGEST')),
      ('defaults, erased region', b'\xff' * len(region),
       ('NO_REGION', 'NO_REGION', 'NO_REGION'))):
    m = machine(pbase, data=data)
    m.setup(layout, digests, defaults=defaults)
    got = m.statuses()
    results['faults'].append({'case': name, 'statuses': got})
    print('fault: %s: %s' % (name, ' '.join(got)))
    if got != expected:
      failures.append('%s: %s, not %s' % (name, got, expected))

  # The synth locks for every WAVE and PLTS outcome (ABI.md, faults).
  wave_states = {'ok': region, 'fault': flip(region, w_off + 1000),
                 'absent': b'\xff' * p_off + region[p_off:]}
  expected_locks = {
      ('ok', 'ok'): 'AS_PROFILED', ('ok', 'fault'): 'FORCE_BEADS',
      ('ok', 'absent'): 'FORCE_BEADS', ('fault', 'ok'): 'FORCE_PLAITS',
      ('absent', 'ok'): 'FORCE_PLAITS', ('fault', 'fault'): 'SILENT',
      ('fault', 'absent'): 'SILENT', ('absent', 'fault'): 'SILENT',
      ('absent', 'absent'): 'SILENT'}
  for (w, p), expected in sorted(expected_locks.items()):
    data = wave_states[w]
    if p == 'fault':
      data = flip(data, first_blob + 200)
    elif p == 'absent':
      data = data[:p_off] + b'\xff' * (len(region) - p_off)
    m = machine(pbase, data=data)
    m.setup(layout, digests)
    got = m.lock()
    results['locks'].append({'wave': w, 'plts': p, 'lock': got,
                             'statuses': m.statuses()})
    print('lock: WAVE %s, PLTS %s: %s' % (w, p, got))
    if got != expected:
      failures.append('lock WAVE %s PLTS %s: %s, not %s' % (w, p, got,
                                                           expected))

  results['findings'] = findings
  json.dump(results, open(out + '/render_compare.json', 'w'), indent=1,
            sort_keys=True)
  for f in findings:
    print('FINDING: ' + f)
  for f in failures:
    print('ERROR: ' + f)
  if not failures:
    print('render_compare: PASS')
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
