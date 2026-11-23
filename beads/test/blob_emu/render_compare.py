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
# region (WAVE, PLTS and RNGS sections) is copied into flash at two
# placements, and again with the models moved away from the libraries they
# call: four layouts of library and dependent.
# Also: model-switch sequences with poisoned storage, a write guard around
# the blob voice's slot and arena, and the reader's faults (each section's,
# RNGS's included) and synth locks.
#
#   render_compare.py OUT  (beads/test/blob_emu/build.sh's output)

import hashlib
import json
import multiprocessing
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
PLACEMENTS = (0x08080000, 0x08082FE0)
REGION_END = 0x08100000
RELEASE = 20261121
FPSCR = (0, 0x03000000)  # reset, and flush-to-zero with default NaN
RANDOM_STATE = 0x20000000
# Blob-to-engine order follows harness.cc: Palette, sine, then PlaitsVoice.
# The 6-op engine serves models 2-4.
PAIRS = dict([
    (36, 0), (254, 1), (43, 2), (32, 3), (29, 4),
    (40, 5), (37, 6), (39, 7), (41, 8), (31, 9),
    (33, 10), (34, 11), (35, 12), (38, 13), (42, 14),
    (44, 15)] + [
    (m, 16 + (m if m < 2 else 2 if m < 5 else m - 2)) for m in range(24)])
# Library users: 6-op FM, Wave terrain, Wavetable, Chords, String machine,
# Chiptune and Speech; dependencies come from profile-features.json.
DEPENDENTS = (2, 3, 4, 5, 6, 7, 13, 14, 15)
# Clamped blobs may differ only where terrain_probe.h counts out-of-range
# lookups; Wave terrain 5-7 clamp wave and sample positions to the bank.
KNOWN = {5: 'terrains 5-7 clamped where Plaits read past the bank'}
# A -> B -> A switch sequences: every stock model returns once.
SWITCHES = [(m, (m + 5) % 24) for m in range(24)] + [
    (2, 3), (4, 2), (13, 43), (18, 36), (43, 18),
    (32, 36), (29, 32), (40, 29), (37, 40), (39, 37),
    (41, 39), (31, 41), (33, 31), (34, 33), (35, 34),
    (38, 35), (42, 38), (44, 42)]
# Tapfield needs a held note: low notes retrigger before its register clocks.
NOTE_CONTROL_STEPS = {35: [
    (1 if block == 0 else 0, 84.0, 0.5, 0.5, 0.5, 0.8)
    for block in range(200)]}
BLOCK = 12
STATUS = ['OK', 'PLACEMENT', 'HEADER', 'ABI', 'SERVICES', 'LAYOUT', 'RAM',
          'CRC', 'DIGEST', 'IMPORTS', 'NO_REGION', 'RELEASE', 'SELECTION',
          'TABLE', 'MODEL', 'KIND', 'LIBRARY', 'CONTENT', 'DIRECTORY',
          'ECC', 'DEPENDENCY']
LOCKS = ['AS_PROFILED', 'FORCE_BEADS', 'FORCE_PLAITS', 'SILENT']
SECTIONS = ('WAVE', 'TERR', 'PLTS', 'RNGS')
KINDS = {'WAVE': 0x45564157, 'TERR': 0x52524554, 'PLTS': 0x53544c50,
         'RNGS': 0x53474e52}
SINE = 0x08021440
POISON = struct.pack('<I', 0x7fa5a5a5)  # a NaN no engine writes
SLOT_SIZE = 4352
ARENA_SIZE = 16384
# A rebuilt blob fades in over its first 48 samples on a squared ramp
# (plaits_blob_engine.h).
FADE_SIZE = 48


def f32(x):
  return struct.unpack('<f', struct.pack('<f', x))[0]


FADE_STEP = f32(1.0 / FADE_SIZE)


def faded(word, position):
  """The compiled-in sample as the fade leaves it, rounded as the M7 does."""
  y, = struct.unpack('<f', word)
  ramp = f32(position * FADE_STEP)
  return struct.pack('<f', y * f32(ramp * ramp))


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
    self.fade = FADE_SIZE

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
            release=RELEASE, library_ram=None):
    if library_ram is not None:
      self.uc.mem_write(self.sym['harness_library_ram_size'],
                        struct.pack('<I', library_ram))
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
    # PlaitsRegionIdentity: three words, four per section, the count.
    count = 3 + 4 * len(SECTIONS)
    raw = bytes(self.uc.mem_read(self.sym['harness_identity'],
                                 4 * count + 4))
    words = struct.unpack_from('<%dI' % (count + 1), raw, 0)
    sections = {}
    for i, name in enumerate(SECTIONS):
      status, address, size, crc = words[3 + 4 * i:7 + 4 * i]
      sections[name] = {'status': STATUS[status],
                        'address': '0x%08x' % address, 'size': size,
                        'crc': '0x%08x' % crc}
    blobs = bytes(self.uc.mem_read(
        self.sym['harness_identity'] + 4 * count + 4, words[count]))
    return {'release': words[0], 'services_status': STATUS[words[1]],
            'directory_status': STATUS[words[2]], 'sections': sections,
            'num_blobs': words[count],
            'blob_status': [STATUS[b] for b in blobs]}

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
    # AUX may stay unwritten: PLAITS_NO_AUX skips it, blob and compiled
    # alike, and the comparison sees the same poison in both.
    if any(out[i:i + 4] == POISON for i in range(0, len(out), 4)):
      raise RuntimeError('%s left a sample unwritten' % label)
    return out, aux

  def render_blob(self, p, seed):
    self.parameters(p)
    state = self.call('harness_render_blob', BLOCK, seed)
    return self.outputs('blob') + (state,)

  def render_compiled(self, engine, p, seed):
    self.parameters(p)
    counter = self.sym['harness_terrain_overshoot']
    self.uc.mem_write(counter, b'\0\0\0\0')
    state = self.call('harness_render_compiled', engine, BLOCK, seed)
    self.overshoot = self.word(counter)
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
    self.fade = 0

  def select_compiled(self, engine, model):
    self.call('harness_select_compiled', engine, model)

  def rebuild_compiled(self, engine, model):
    self.call('harness_rebuild_compiled', engine, model)


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
  # Every 6-op patch (HARMONICS picks one of 32), a note each.
  for patch in range(32):
    for block in range(8):
      steps.append((1 if block == 0 else 0, 60.0,
                    (patch + 0.5) / (32 * 1.02), 0.5, 0.5, 0.8))
  r = random.Random(seed)
  for _ in range(2000):
    steps.append((r.choice((0, 0, 0, 1)), r.uniform(0.0, 127.0),
                  r.random(), r.random(), r.random(), r.random()))
  # Gates as the synth sends them: held notes (rising edge with HIGH, HIGH,
  # then released), and unpatched (drone) blocks.
  for i in range(24):
    p = (r.uniform(24.0, 96.0), r.random(), r.random(), r.random(), 0.8)
    for block in range(16):
      trigger = 5 if block == 0 else 4 if block < 8 else 0
      steps.append((trigger,) + p)
  for i in range(32):
    for block in range(8):
      steps.append((2, 48.0 + i, i / 31.0, 0.5, block / 7.0, 0.8))
  return steps


def compare_blocks(m, engine, steps, seed, nudge=0.0, digest=None,
                   stats=None, retained=False):
  """Blob voice against compiled engine, block by block, from one seed.
  Inside the blob's fade-in the expected OUT sample is the compiled one
  times the fade's gain; every other sample is the compiled sample itself."""
  differ = samples = 0
  for block, (trigger, note, harmonics, timbre, morph, accent) in enumerate(
      steps):
    p = (trigger, note, harmonics, timbre, morph, accent)
    a_out, a_aux, a_seed = m.render_blob(p, seed)
    b_out, b_aux, b_seed = m.render_compiled(
        engine, (trigger, note + nudge) + p[2:], seed)
    if stats is not None and m.overshoot:
      stats.setdefault('overshoot_blocks', []).append(block)
    if digest is not None:
      digest.update(a_out + a_aux)
    for x, y, fades in ((a_out, b_out, True), (a_aux, b_aux, False)):
      for i in range(0, len(x), 4):
        samples += 1
        if not fades and x[i:i + 4] == POISON:
          if y[i:i + 4] != POISON:
            differ += 1
            if stats is not None:
              stats.setdefault('positions', []).append((block, 1, i // 4))
          continue
        v, = struct.unpack_from('<f', x, i)
        if v != v or v in (float('inf'), float('-inf')):
          raise RuntimeError('non-finite output')
        position = m.fade + i // 4 if fades else FADE_SIZE
        expected = faded(y[i:i + 4], position) if position < FADE_SIZE \
            else y[i:i + 4]
        if stats is not None and position < FADE_SIZE:
          stats['fade_peak'] = max(stats.get('fade_peak', 0.0), abs(v))
        if stats is not None and fades:
          stats['out_peak'] = max(stats.get('out_peak', 0.0), abs(v))
        if x[i:i + 4] != expected:
          differ += 1
          if stats is not None:
            stats.setdefault('positions', []).append(
                (block, 0 if fades else 1, i // 4))
            w, = struct.unpack_from('<f', y, i)
            stats['max'] = max(stats.get('max', 0.0), abs(v - w))
            stats['peak'] = max(stats.get('peak', 0.0), abs(w))
            stats['last_block'] = block
    # A nudged note, or a compiled-in engine's retained state, may draw
    # differently: compared from the blob's generator on, and reported.
    if a_seed != b_seed and not nudge:
      if not retained:
        raise RuntimeError('the generator diverged')
      if stats is not None:
        stats.setdefault('diverged_block', block)
    seed = a_seed
    m.fade += BLOCK
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


# Per process: the harness, regions and layouts (load()).
CTX = {}


def load(out):
  CTX['elf'] = out + '/harness.elf'
  CTX['sym'] = symbols(out + '/harness.nm')
  CTX['region'] = open(out + '/region.bin', 'rb').read()
  CTX['layout'] = json.load(open(out + '/region.bin.json'))
  CTX['gap_region'] = open(out + '/region-gap.bin', 'rb').read()
  CTX['gap_layout'] = json.load(open(out + '/region-gap.bin.json'))
  plts = [s for s in CTX['layout']['sections'] if s['kind'] == 'PLTS'][0]
  rngs = [s for s in CTX['layout']['sections'] if s['kind'] == 'RNGS'][0]
  # The synth's digest last: the cases below drop PLTS entries by index.
  CTX['digests'] = [bytes.fromhex(b['sha256'])
                    for b in plts['blobs'] + rngs['blobs']]
  CTX['terr_region'] = open(out + '/region-terr.bin', 'rb').read()
  CTX['terr_layout'] = json.load(open(out + '/region-terr.bin.json'))
  CTX['steps'] = drive(1)


def machine(base=PLACEMENTS[0], fpscr=0, data=None, patches=()):
  return Machine(CTX['elf'], CTX['sym'],
                 CTX['region'] if data is None else data, base, fpscr,
                 patches)


def arena_of(m):
  """BlobVoice: 64 B of canary, the engine (whose last member is the slot),
  64 B of canary, the arena, 64 B of canary."""
  slot = m.word(CTX['sym']['harness_slot'])
  return slot + SLOT_SIZE + 64 - CTX['sym']['harness_blob_voice']


def plts_of(layout):
  return [s for s in layout['sections'] if s['kind'] == 'PLTS'][0]


def entry_of(layout, key, value):
  """A library's entry, or the model entry whose blob serves the ID (6-op
  FM serves 2-4: its header's first ID and count)."""
  p = plts_of(layout)
  data = CTX['gap_region'] if layout is CTX['gap_layout'] else CTX['region']
  for b in p['blobs']:
    if key == 'model' and 'model' in b:
      at = p['offset'] + b['offset']
      first, count = b['model'], 1
      if struct.unpack_from('<I', data, at + 8)[0] == 2:
        first, count = struct.unpack_from('<2I', data, at + 20)
      if first <= value < first + count:
        return b
    elif b.get(key) == value:
      return b
  raise KeyError('%s %d' % (key, value))


def address(layout, base, key, value):
  return base + plts_of(layout)['offset'] + entry_of(
      layout, key, value)['offset']


def state_of(model):
  plts = plts_of(CTX['layout'])
  info = entry_of(CTX['layout'], 'model', model)
  blob = CTX['region'][plts['offset'] + info['offset']:]
  abi = struct.unpack_from('<I', blob, 8)[0]
  return struct.unpack_from('<I', blob, 24 if abi == 1 else 32)[0]


def dependencies(layout, model):
  """The library IDs a model's blob lists, from its header and list."""
  plts = plts_of(layout)
  info = entry_of(CTX['layout'], 'model', model)
  blob = CTX['region'][plts['offset'] + info['offset']:]
  if struct.unpack_from('<I', blob, 8)[0] != 2:
    return []
  offset, count = struct.unpack_from('<2I', blob, 60)
  return [struct.unpack_from('<I', blob, offset + 36 * i)[0]
          for i in range(count)]


def identity_run(job):
  """One model at one layout and FPSCR mode, against its engine."""
  try:
    return identity_compare(*job)
  except RuntimeError as e:
    return {'model': job[0], 'error': '%s at 0x%08x%s' % (
        e, job[1], ' gap' if job[3] else '')}


def identity_compare(model, base, fpscr, gap):
  data = CTX['gap_region'] if gap else CTX['region']
  layout = CTX['gap_layout'] if gap else CTX['layout']
  m = machine(base, fpscr, data)
  status = m.setup(layout, CTX['digests'])
  if status != 'OK':
    return {'model': model, 'error': 'PLTS %s' % status}
  m.arena = arena_of(m)
  engine = PAIRS[model]
  m.select_blob(model)
  m.select_compiled(engine, model)
  h = hashlib.sha256()
  stats = {}
  differ, samples, _ = compare_blocks(m, engine, CTX['steps'], 0x21,
                                      digest=h, stats=stats)
  run = {'base': '0x%08x' % base, 'gap': gap, 'fpscr': '0x%08x' % fpscr,
         'model': model,
         'blob': '0x%08x' % address(layout, base, 'model', model),
         'differ': differ, 'samples': samples, 'sha256': h.hexdigest(),
         'out_peak': stats.get('out_peak', 0.0),
         'positions': sorted(stats.get('positions', [])),
         'overshoot_blocks': stats.get('overshoot_blocks', [])}
  libraries = dependencies(layout, model)
  if libraries:
    run['libraries'] = dict(
        (str(i), '0x%08x' % address(layout, base, 'library', i))
        for i in libraries)
  if not gap and base == PLACEMENTS[0] and fpscr == 0:
    run['identity'] = m.identity()
  return run


def terrain_run(job):
  """One TERR grid, and the compiled engine without its user data."""
  base, fpscr = job
  run = {'model': 5, 'base': '0x%08x' % base,
         'fpscr': '0x%08x' % fpscr}
  try:
    layout = CTX['terr_layout']
    terr = [s for s in layout['sections'] if s['kind'] == 'TERR'][0]
    steps = CTX['steps']
    run['grid_blocks'] = sum(min(1.05 * p[2], 1.0) * 7.9999 >= 7
                             for p in steps)
    if not run['grid_blocks']:
      raise RuntimeError('drive never reaches the user grid')
    for user_data in (True, False):
      m = machine(base, fpscr, CTX['terr_region'])
      status = m.setup(layout, CTX['digests'])
      identity = m.identity()
      count = m.word(m.sym['harness_num_terrains'])
      if status != 'OK' or identity['sections']['TERR']['status'] != 'OK' \
          or count != 1:
        raise RuntimeError('TERR setup: %s, %s, count %d' % (
            status, identity['sections']['TERR']['status'], count))
      m.arena = arena_of(m)
      if user_data:
        m.uc.mem_write(m.sym['harness_user_data'], struct.pack(
            '<I', base + terr['offset'] + 32))
      m.select_blob(5)
      m.select_compiled(PAIRS[5], 5)
      stats = {}
      differ, samples, _ = compare_blocks(m, PAIRS[5], steps, 0x21,
                                          stats=stats)
      if user_data:
        run.update({'identity': identity, 'num_terrains': count,
                    'differ': differ, 'samples': samples,
                    'out_peak': stats.get('out_peak', 0.0),
                    'positions': stats.get('positions', []),
                    'overshoot_blocks': stats.get('overshoot_blocks', [])})
      else:
        run['null_differ'] = differ
        run['null_samples'] = samples
    return run
  except RuntimeError as e:
    run['error'] = str(e)
    return run


def switch_run(pair):
  """A -> B -> A: the blob voice constructs on every switch in a poisoned
  slot and arena; compiled-in engines only Reset. Retained state shows as
  differences after the return; they are reported."""
  try:
    run = switch_compare(pair[0], pair[1], False)
    fresh = switch_compare(pair[0], pair[1], True)
    run['fresh_differ'] = [p['differ'] for p in fresh['phases']]
    return run
  except RuntimeError as e:
    return {'sequence': [pair[0], pair[1], pair[0]], 'error': str(e)}


def switch_compare(first, second, fresh):
  """fresh: each selection compares with the compiled-in engine built afresh
  (first selection after power-on), which a rebuilt blob must equal."""
  sequence = CTX['steps'][:300]
  m = machine(PLACEMENTS[1])
  m.setup(CTX['layout'], CTX['digests'])
  m.arena = arena_of(m)
  seed = 0x21
  phases = []
  for model, part in ((first, sequence[:100]), (second, sequence[100:200]),
                      (first, sequence[200:])):
    before = m.word(RANDOM_STATE)
    m.select_blob(model)
    if fresh:
      m.rebuild_compiled(PAIRS[model], model)
    else:
      m.select_compiled(PAIRS[model], model)
    drew = m.word(RANDOM_STATE) != before
    slot = m.word(CTX['sym']['harness_slot'])
    cleared = POISON not in bytes(m.uc.mem_read(slot, state_of(model)))
    stats = {}
    # The return, or a second model of one compiled-in engine, meets the
    # state the compiled-in engine kept.
    retained = not fresh and bool(phases) and PAIRS[model] == PAIRS[first]
    differ, samples, seed = compare_blocks(m, PAIRS[model], part, seed,
                                           stats=stats, retained=retained)
    phases.append({'model': model, 'differ': differ, 'samples': samples,
                   'slot_cleared': cleared, 'switch_drew': drew,
                   'max_difference': stats.get('max', 0.0),
                   'compiled_peak': stats.get('peak', 0.0),
                   'last_differing_block': stats.get('last_block'),
                   'fade_window_peak': stats.get('fade_peak', 0.0),
                   'generator_diverged_block': stats.get('diverged_block')})
  return {'sequence': [first, second, first], 'phases': phases}


def main(argv):
  out = argv[1]
  jobs = int(argv[2]) if len(argv) > 2 else multiprocessing.cpu_count()
  load(out)
  sym = CTX['sym']
  region = CTX['region']
  layout = CTX['layout']
  plts = plts_of(layout)
  wave = [s for s in layout['sections'] if s['kind'] == 'WAVE'][0]
  rngs = [s for s in layout['sections'] if s['kind'] == 'RNGS'][0]
  blobs = {b['model']: b for b in plts['blobs'] if 'model' in b}
  libraries = [b for b in plts['blobs'] if 'library' in b]
  digests = CTX['digests']
  steps = CTX['steps']
  results = {'unicorn': unicorn.__version__, 'blocks': len(steps),
             'block_size': BLOCK, 'runs': [], 'switches': [], 'controls': [],
             'faults': [], 'locks': [], 'terrain': []}
  failures = []
  findings = []
  pool = multiprocessing.Pool(jobs, load, (out,))

  # Dependents use two placements, each with and without a library/model gap.
  # Independent models use two placements.
  runs = [(model, base, fpscr, gap) for gap in (False, True)
          for base in PLACEMENTS for fpscr in FPSCR
          for model in sorted(PAIRS) if not gap or model in DEPENDENTS]
  for run in pool.imap(identity_run, runs):
    if 'error' in run:
      failures.append('model %d: %s' % (run['model'], run['error']))
      continue
    if 'identity' in run:
      results['identity'] = run.pop('identity')
    results['runs'].append(run)
    print('%s%s fpscr %s model %d: %d of %d differ' % (
        run['base'], ' gap' if run['gap'] else '', run['fpscr'],
        run['model'], run['differ'], run['samples']))
    sys.stdout.flush()
    if not run['out_peak']:
      failures.append('model %d: silent at %s' % (run['model'], run['base']))
    known = KNOWN.get(run['model'])
    overshoot = set(run['overshoot_blocks'])
    if run['differ'] and known and overshoot and all(
        p[0] in overshoot for p in run['positions']):
      findings.append('model %d: %d of %d samples differ at %s%s fpscr %s: '
                      '%s' % (run['model'], run['differ'], run['samples'],
                              run['base'], ' gap' if run['gap'] else '',
                              run['fpscr'], known) + ' (%d of %d '
                      'blocks with lookups past the bank differ)' % (
                          len(set(p[0] for p in run['positions'])),
                          len(overshoot)))
    elif run['differ']:
      failures.append('model %d differs at %s%s' % (
          run['model'], run['base'], ' gap' if run['gap'] else ''))

  terrain = [(base, fpscr) for base in PLACEMENTS for fpscr in FPSCR]
  for run in pool.imap(terrain_run, terrain):
    results['terrain'].append(run)
    label = 'terrain %s fpscr %s model 5' % (run['base'], run['fpscr'])
    if 'error' in run:
      print('%s: %s' % (label, run['error']))
      failures.append('%s: %s' % (label, run['error']))
      continue
    print('%s: %d of %d differ, NULL %d of %d differ' % (
        label, run['differ'], run['samples'], run['null_differ'],
        run['null_samples']))
    sys.stdout.flush()
    if not run['out_peak']:
      failures.append('%s: silent' % label)
    if not run['null_differ']:
      failures.append('%s: NULL user data compared equal' % label)
    overshoot = set(run['overshoot_blocks'])
    if run['differ'] and overshoot and all(
        p[0] in overshoot for p in run['positions']):
      findings.append('%s: %d of %d samples differ: %s; user grid '
                      'coordinates clamped (%d of %d blocks with lookups '
                      'past the bank differ)' % (
                          label, run['differ'], run['samples'], KNOWN[5],
                          len(set(p[0] for p in run['positions'])),
                          len(overshoot)))
    elif run['differ']:
      failures.append('%s differs outside overshoot blocks' % label)

  for switch in pool.imap(switch_run, SWITCHES):
    first, second = switch['sequence'][:2]
    if 'error' in switch:
      failures.append('switch %d->%d: %s' % (first, second, switch['error']))
      continue
    phases = switch['phases']
    results['switches'].append(switch)
    if any(switch['fresh_differ']):
      failures.append('switch %d->%d: a rebuilt blob differs from the '
                      'engine built afresh: %s' % (first, second,
                                                   switch['fresh_differ']))
    print('switch %d -> %d -> %d: %s' % (first, second, first, ', '.join(
        '%d of %d differ' % (p['differ'], p['samples']) for p in phases)))
    if any(not p['slot_cleared'] for p in phases):
      failures.append('switch %d->%d: poison left in the slot' % (
          first, second))
    if any(p['switch_drew'] for p in phases):
      failures.append('switch %d->%d: a switch drew from the generator' % (
          first, second))
    # Models served by one compiled-in engine (6-op banks) share its state.
    shared = PAIRS[first] == PAIRS[second]
    if phases[0]['differ'] or (phases[1]['differ'] and not shared):
      failures.append('switch %d->%d differs before the return' % (
          first, second))
    for i in (1, 2) if shared else (2,):
      p = phases[i]
      if p['differ']:
        findings.append('model %d after %d: %d of %d samples differ from '
                        'the compiled-in engine, which keeps its state '
                        'across a switch (Reset only); largest difference '
                        '%.3g, compiled peak %.3g, last differing block %d '
                        'of %d, peak inside the fade-in %.3g' % (
                            p['model'], phases[i - 1]['model'], p['differ'],
                            p['samples'], p['max_difference'],
                            p['compiled_peak'], p['last_differing_block'],
                            p['samples'] // (2 * BLOCK),
                            p['fade_window_peak']))
  pool.close()
  pool.join()

  # Each comparison must see a difference: the note nudged by 0.01.
  m = machine()
  m.setup(layout, digests)
  m.arena = arena_of(m)
  for model, engine in sorted(PAIRS.items()):
    m.select_blob(model)
    m.select_compiled(engine, model)
    differ, samples, _ = compare_blocks(
        m, engine, NOTE_CONTROL_STEPS.get(model, steps[:200]), 0x21,
        nudge=0.01)
    results['controls'].append({'case': 'note nudged, model %d' % model,
                                'differ': differ, 'samples': samples})
    print('control: note nudged, model %d: %d of %d differ' % (
        model, differ, samples))
    if not differ:
      failures.append('model %d: a nudged note compared equal' % model)

  # Particle Noise draws: a different seed must change its output.
  m.select_blob(18)
  m.select_compiled(PAIRS[18], 18)
  a = m.render_blob(steps[1], 0x21)
  b = m.render_compiled(PAIRS[18], steps[1], 0x22)
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
    info = entry_of(layout, 'model', model)
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
    m.select_compiled(engine, model)
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

  r_off, r_size = rngs['offset'], rngs['size']
  first_blob = p_off + plts['blobs'][0]['offset']
  synth = r_off + rngs['blobs'][0]['offset']
  guard_row = w_off + 32 + 192 * 520 + 10
  cases = [
      ('intact', region, None, None, ('OK', 'NO_REGION', 'OK', 'OK')),
      ('PLTS byte flipped', flip(region, first_blob + 200), None, None,
       ('OK', 'NO_REGION', 'CRC', 'OK')),
      ('PLTS interrupted', region[:p_off + 2000] +
       b'\xff' * (len(region) - p_off - 2000), None, None,
       ('OK', 'NO_REGION', 'CRC', 'NO_REGION')),
      ('PLTS blob byte flipped, CRCs refreshed', crc_fix(
          flip(region, first_blob + 200), p_off, plts['size']), None, None,
       ('OK', 'NO_REGION', 'SELECTION', 'OK')),
      ('WAVE guard row changed, CRC refreshed', crc_fix(
          flip(region, guard_row), w_off, wave['size']), None, None,
       ('SELECTION', 'NO_REGION', 'OK', 'OK')),
      ('WAVE erased', b'\xff' * p_off + region[p_off:], None, None,
       ('NO_REGION', 'NO_REGION', 'OK', 'OK')),
      ('RNGS byte flipped', flip(region, synth + 200), None, None,
       ('OK', 'NO_REGION', 'OK', 'CRC')),
      ('RNGS interrupted', region[:r_off + 2000] +
       b'\xff' * (len(region) - r_off - 2000), None, None,
       ('OK', 'NO_REGION', 'OK', 'CRC')),
      ('RNGS blob byte flipped, CRCs refreshed', crc_fix(
          flip(region, synth + 200), r_off, r_size), None, None,
       ('OK', 'NO_REGION', 'OK', 'SELECTION')),
      ('RNGS erased', region[:r_off] + b'\xff' * r_size, None, None,
       ('OK', 'NO_REGION', 'OK', 'NO_REGION')),
      ('stale tail after RNGS', region + b'DZSC' * 64, None, None,
       ('OK', 'NO_REGION', 'OK', 'OK')),
  ]
  for name, data, directory, defaults, expected in cases:
    m = machine(pbase, data=data)
    m.setup(layout, digests)
    got = m.statuses()
    results['faults'].append({'case': name, 'statuses': got})
    print('fault: %s: %s' % (name, ' '.join(got)))
    if got != expected:
      failures.append('%s: %s, not %s' % (name, got, expected))
    # The synth blob is served exactly when RNGS is valid.
    served = m.word(sym['harness_synth_blob'])
    if served != (pbase + synth if got[3] == 'OK' else 0):
      failures.append('%s: synth blob at 0x%08x' % (name, served))

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
  if got[0] != 'CONTENT' or got[2:] != ('OK', 'OK'):
    failures.append('WAVE guard row, selected: %s' % (got,))

  # Release, selection, digests, services, directory layout.
  every = len(SECTIONS)
  more = [
      ('another release', {'release': RELEASE - 1},
       ('RELEASE', 'NO_REGION', 'RELEASE', 'RELEASE')),
      ('digest not listed', {'digests': digests[1:]},
       ('OK', 'NO_REGION', 'DIGEST', 'OK')),
      ('synth digest not listed', {'digests': digests[:-1]},
       ('OK', 'NO_REGION', 'OK', 'DIGEST')),
      ('directory out of order', {'directory': layout['sections'][::-1]},
       ('DIRECTORY',) * every),
      ('directory past the region', {'directory': [dict(
          plts, offset=REGION_END - pbase - 64)]}, ('DIRECTORY',) * every),
      ('directory misaligned', {'directory': [dict(
          plts, offset=plts['offset'] + 4)]}, ('DIRECTORY',) * every),
      ('RNGS misaligned', {'directory': [plts, dict(
          rngs, offset=rngs['offset'] + 4)]}, ('DIRECTORY',) * every),
      ('directory overlapping', {'directory': [
          dict(wave, size=wave['size'] + 64), plts]}, ('DIRECTORY',) * every),
      ('PLTS only', {'directory': [plts]},
       ('NO_REGION', 'NO_REGION', 'OK', 'NO_REGION')),
      ('RNGS only', {'directory': [rngs]},
       ('NO_REGION', 'NO_REGION', 'NO_REGION', 'OK')),
      ('another selection', {'directory': [dict(
          plts, crc='0x%08x' % (int(plts['crc'], 16) ^ 1))]},
       ('NO_REGION', 'NO_REGION', 'SELECTION', 'NO_REGION')),
      ('another RNGS selection', {'directory': [plts, dict(
          rngs, crc='0x%08x' % (int(rngs['crc'], 16) ^ 1))]},
       ('NO_REGION', 'NO_REGION', 'OK', 'SELECTION')),
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
  # Libraries: refused for their own reason, then every dependent with them.
  names = [b.get('model', b.get('library')) for b in plts['blobs']]
  kinds = ['library' if 'library' in b else 'model' for b in plts['blobs']]
  library_ids = [b['library'] for b in libraries]
  users = dict((m, dependencies(layout, m)) for m in blobs)

  def ram_of(library):
    info = [b for b in libraries if b['library'] == library][0]
    blob = region[plts['offset'] + info['offset']:]
    return struct.unpack_from('<I', blob, 32)[0]

  def align(size):
    return (size + 31) // 32 * 32

  def expect(refused, status):
    """Every library and model: the refused libraries' status, DEPENDENCY
    for their users, OK for the rest."""
    table = dict((('library', i), status if i in refused else 'OK')
                 for i in library_ids)
    for model, listed in users.items():
      table[('model', model)] = 'DEPENDENCY' if set(listed) & set(refused) \
          else 'OK'
    return table

  # The records, then each library's RAM in ID order: the last with RAM
  # comes up one byte short.
  needed = align(len(libraries) * 68) + sum(align(ram_of(i))
                                            for i in library_ids)
  last_with_ram = [i for i in library_ids if ram_of(i)][-1]
  speech_library = names.index(3)
  library_cases = [
      ('library RAM not given', {'library_ram': 0}, 'RAM',
       expect(library_ids, 'RAM')),
      ('library RAM one byte short', {'library_ram': needed - 1}, 'RAM',
       expect([last_with_ram], 'RAM')),
      ('speech library not listed', {'digests': digests[:speech_library] +
                                     digests[speech_library + 1:]}, 'DIGEST',
       expect([3], 'DIGEST')),
  ]
  for name, options, expected, entries in library_cases:
    m = machine(pbase)
    m.setup(layout, options.get('digests', digests),
            library_ram=options.get('library_ram'))
    identity = m.identity()
    got = identity['sections']['PLTS']['status']
    statuses = dict(((k, n), st) for k, n, st in zip(
        kinds, names, identity['blob_status']))
    results['faults'].append({'case': name, 'plts': got, 'entries': dict(
        ('%s %d' % k, v) for k, v in sorted(statuses.items()))})
    print('fault: %s: PLTS %s, %s' % (name, got, ' '.join(
        '%s %d %s' % (k[0], k[1], v) for k, v in sorted(statuses.items())
        if v != 'OK')))
    if got != expected or statuses != entries or m.lock() != 'FORCE_BEADS':
      failures.append('%s: PLTS %s, %s' % (name, got, statuses))

  m = machine(pbase, patches=[(SINE + 100, b'\x5a')])
  m.setup(layout, digests)
  got = m.statuses()
  results['faults'].append({'case': 'services export byte flipped',
                            'statuses': got})
  print('fault: services export byte flipped: %s' % ' '.join(got))
  if got != ('OK', 'NO_REGION', 'SERVICES', 'SERVICES'):
    failures.append('services export byte flipped: %s' % (got,))

  # Without a profile directory: the release's default layout, by digest.
  defaults = layout['sections']
  for name, data, expected in (
      ('defaults', region, ('OK', 'NO_REGION', 'OK', 'OK')),
      ('defaults, PLTS changed', crc_fix(flip(region, first_blob + 200),
                                         p_off, plts['size']),
       ('OK', 'NO_REGION', 'DIGEST', 'OK')),
      ('defaults, RNGS changed', crc_fix(flip(region, synth + 200),
                                         r_off, r_size),
       ('OK', 'NO_REGION', 'OK', 'DIGEST')),
      ('defaults, erased region', b'\xff' * len(region),
       ('NO_REGION',) * len(SECTIONS))):
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
