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
# Boot the release ELF under Unicorn's Cortex-M7 (Reset_Handler, constructors,
# Init with SEED held so blank storage skips the factory test), then drive the
# main loop, the codec's DMA interrupt and SysTick as the host firmware test's
# rig does: per millisecond one main loop pass, four audio blocks, one tick.
# Every call's instruction count (not cycles) and every output block's hash
# are recorded, so that two ELFs can be compared call by call.
#
#   transition_emu.py [--summary] ELF OUT.json [SCENARIO ...]
#   transition_emu.py --compare BASE.json CANDIDATE.json
#   transition_emu.py --check ELF

import hashlib
import json
import multiprocessing
import struct
import sys
import time

try:
  import unicorn
  from unicorn import arm_const as A
  from elftools.elf.elffile import ELFFile
except ImportError:
  unicorn = None  # --compare needs neither

REGIONS = (
    (0x00000000, 0x10000),  # ITCM
    (0x08000000, 0x200000),  # Flash, erased
    (0x20000000, 0x20000),  # DTCM
    (0x24000000, 0x80000),  # AXI SRAM
    (0x30000000, 0x48000),  # SRAM1-3
    (0x38000000, 0x10000),  # SRAM4
    (0x38800000, 0x1000),  # Backup SRAM
    (0x40000000, 0x20000000),  # Peripherals, as plain memory
    (0xE0000000, 0x100000))  # System control space, as plain memory
STOP = 0x0FF00000
FLASH = 0x08000000
GPIOA_IDR = 0x58020010
GPIOE_IDR = 0x58021010
DMA1_LISR = 0x40020000
LISR_HT, LISR_TC = 1 << 10, 1 << 11
# Switches: FREEZE PA11, SEED PA12, MEDIA PE8, ASSIGN PE7; pressed reads 0.
SWITCHES = {'freeze': (GPIOA_IDR, 11), 'seed': (GPIOA_IDR, 12),
            'media': (GPIOE_IDR, 8), 'assign': (GPIOE_IDR, 7)}
RCC_CR, RCC_CFGR = 0x58024400, 0x58024410
PWR_CSR1, PWR_D3CR = 0x58024804, 0x58024818
ADCS = (0x40022000, 0x40022100, 0x58026000)
FEATURE_REVERB, FEATURE_PROCESSOR, FEATURE_SYNTH = 2, 4, 7
FEATURE_PLAITS_BANK = 31
# State bytes (settings.h asserts the layout).
STATE_GAIN, STATE_GATE_OUTPUT, STATE_NO_AUTOSAVE, STATE_MONOPHONIC = 5, 6, 8, 9
BUDGET = 200000000
USAGE = '''usage: transition_emu.py [--summary] [--region BIN] ELF OUT.json [SCENARIO ...]
       transition_emu.py --compare BASE.json CANDIDATE.json
       transition_emu.py --check ELF
'''
# Pots: 7 muxed (FEEDBACK, DRY_WET, REVERB, then the TIME, SIZE, SHAPE and
# PITCH attenuverters), then TIME, DENSITY, PITCH, SIZE, SHAPE.
POTS = (0x6000, 0x9000, 0x6000, 0x8000, 0x8000, 0x8000, 0x8000,
        0x7000, 0xC000, 0x8000, 0x9000, 0x6000)
SQUARE_PERIOD = 96
SQUARE_LEVEL = 0x20000000
PROBE_LOW_CV, PROBE_HIGH_CV = 32768, 16000
# Members read or written by the rig, found in beads.cc's DWARF.
MEMBERS = {
    'Codec': ('block_size_', 'callback_'),
    'NormalizationDetector': ('rng_state_',),
    'Settings': ('state_', 'values_'),
    'Ui': ('freeze_button_state_',)}
# Functions whose inclusive counts are recorded, by symbol prefix.
INNER = (('_Z7Process', 'Process'),
         ('_Z18MonitorStateChangev', 'MonitorStateChange'),
         ('_ZN5beads2Ui4PollEv', 'Ui::Poll'))
# Globals used only to label phases; a candidate may lack them.
LABELS = ('codec_transition_frames', 'reverb_style_gain',
          'reverb_style_previous_gain', 'synth_mode')


def thumb_length(halfword):
  return 4 if halfword >> 11 in (0x1d, 0x1e, 0x1f) else 2


class Image(object):

  def __init__(self, path):
    self.path = path
    data = open(path, 'rb').read()
    self.sha256 = hashlib.sha256(data).hexdigest()
    elf = ELFFile(open(path, 'rb'))
    self.loads = []
    for s in elf.iter_sections():
      alloc = s['sh_flags'] & 2 and s['sh_type'] == 'SHT_PROGBITS'
      if alloc and s['sh_size']:
        self.loads.append((s['sh_addr'], s.data()))
    for p in elf.iter_segments():
      if p['p_type'] == 'PT_LOAD' and p['p_filesz']:
        self.loads.append((p['p_paddr'], p.data()))
    self.sym = {}
    for s in elf.get_section_by_name('.symtab').iter_symbols():
      kind = s['st_info']['type']
      if s.name and kind in ('STT_FUNC', 'STT_OBJECT', 'STT_NOTYPE'):
        thumb = 1 if kind == 'STT_FUNC' else 0
        self.sym.setdefault(s.name, s['st_value'] & ~thumb)
    self.offsets = self.member_offsets(elf)
    if 'transition' in self.sym:
      for label in LABELS[:-1]:
        self.sym[label] = self.sym['transition'] + \
            self.offsets['EngineTransition'][label + '_']

  def member_offsets(self, elf):
    found = {}
    members = dict(MEMBERS)
    if any('GestureController' in name for name in self.sym):
      members['GestureController'] = ('trim_flag_',)
    if 'transition' in self.sym:
      members['EngineTransition'] = tuple(n + '_' for n in LABELS[:-1])
    for cu in elf.get_dwarf_info().iter_CUs():
      top = cu.get_top_DIE()
      name = top.attributes.get('DW_AT_name')
      if not name or not name.value.decode().endswith('beads/beads.cc'):
        continue
      stack = [top]
      while stack:
        die = stack.pop()
        if die.tag in ('DW_TAG_class_type', 'DW_TAG_structure_type'):
          n = die.attributes.get('DW_AT_name')
          n = n.value.decode() if n else None
          if n in members and n not in found and die.has_children:
            fields = {}
            for c in die.iter_children():
              cn = c.attributes.get('DW_AT_name')
              loc = c.attributes.get('DW_AT_data_member_location')
              if c.tag == 'DW_TAG_member' and cn and loc:
                fields[cn.value.decode()] = loc.value
            if all(m in fields for m in members[n]):
              found[n] = fields
        if die.tag in ('DW_TAG_compile_unit', 'DW_TAG_namespace'):
          stack.extend(die.iter_children())
      break
    for n in members:
      if n not in found:
        raise RuntimeError('no DWARF layout for %s' % n)
    if 'GestureController' in found:
      found['Ui']['trim_flag_'] = found['Ui']['gestures_'] + \
          found['GestureController']['trim_flag_']
    return found

  def address(self, name):
    if name not in self.sym:
      raise RuntimeError('symbol %s not in %s' % (name, self.path))
    return self.sym[name]

  def member(self, obj, cls, field):
    return self.address(obj) + self.offsets[cls][field]


class Machine(object):

  def __init__(self, image):
    self.image = image
    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
    uc.ctl_set_cpu_model(A.UC_CPU_ARM_CORTEX_M7)
    for base, size in REGIONS:
      uc.mem_map(base, size)
    uc.mem_write(FLASH, b'\xff' * 0x200000)
    uc.mem_map(STOP, 0x1000)
    for address, data in image.loads:
      uc.mem_write(address, data)
    self.uc = uc
    self.lengths = {}
    self.count = 0
    self.limit = BUDGET
    self.loop_head = None
    self.head = 0
    # Inclusive counts of the functions a refactor moves code out of.
    self.entries = {}
    for prefix, name in INNER:
      found = [a for s, a in image.sym.items() if s.startswith(prefix)]
      if len(found) == 1:
        self.entries[found[0]] = name
    self.frames = []
    self.inner = []
    self.block_hook = uc.hook_add(unicorn.UC_HOOK_BLOCK, self.on_block)
    tick = image.address('HAL_GetTick')
    uc.hook_add(unicorn.UC_HOOK_CODE, self.on_get_tick, begin=tick, end=tick)
    uc.hook_add(unicorn.UC_HOOK_MEM_UNMAPPED, self.on_unmapped)
    uc.hook_add(unicorn.UC_HOOK_MEM_READ, self.on_status, begin=RCC_CR,
                end=PWR_D3CR + 3)
    for base in ADCS:
      uc.hook_add(unicorn.UC_HOOK_MEM_READ, self.on_adc, begin=base,
                  end=base + 11)
    self.uw_tick = image.address('uwTick')
    self.gpio = {GPIOA_IDR: 0xffff, GPIOE_IDR: 0xffff}
    self.write_gpio()

  def on_block(self, uc, address, size, user):
    if address >> 12 == STOP >> 12:
      return
    n = self.lengths.get((address, size))
    if n is None:
      code = bytes(uc.mem_read(address, size))
      n, i = 0, 0
      while i < size:
        i += thumb_length(code[i] | code[i + 1] << 8)
        n += 1
      self.lengths[(address, size)] = n
    frames = self.frames
    if frames and address == frames[-1][2] and \
        uc.reg_read(A.UC_ARM_REG_SP) == frames[-1][3]:
      name, start, _, _ = frames.pop()
      self.inner.append([name, self.count - start])
    if address in self.entries:
      frames.append((self.entries[address], self.count,
                     uc.reg_read(A.UC_ARM_REG_LR) & ~1,
                     uc.reg_read(A.UC_ARM_REG_SP)))
    if address == self.loop_head:
      self.head += 1
      if self.head > 1:
        return  # on_head stops before it runs
    self.count += n
    if self.count > self.limit:
      uc.emu_stop()

  def on_head(self, uc, address, size, user):
    if self.head > 1:
      uc.emu_stop()

  def on_get_tick(self, uc, address, size, user):
    # Every HAL timeout loop sees time pass.
    tick, = struct.unpack('<I', uc.mem_read(self.uw_tick, 4))
    uc.mem_write(self.uw_tick, struct.pack('<I', (tick + 1) & 0xffffffff))
    return

  def on_status(self, uc, access, address, size, value, user):
    # Clock and regulator flags follow their enables at once.
    word, = struct.unpack('<I', uc.mem_read(address & ~3, 4))
    if address & ~3 == RCC_CR:
      on = word & 0x15011081  # PLL3, PLL2, PLL1, HSE, HSI48, CSI, HSI
      word = word & ~0x2a02a104 | on << 1 & 0x2a022100 | on << 2 & 4 | 0xc000
    elif address & ~3 == RCC_CFGR:
      word = word & ~0x38 | (word & 7) << 3
    elif address & ~3 in (PWR_CSR1, PWR_D3CR):
      word |= 1 << 13
    else:
      return
    uc.mem_write(address & ~3, struct.pack('<I', word))

  def on_adc(self, uc, access, address, size, value, user):
    # Calibration ends at once; the ADC is ready once enabled.
    base = address & ~0xff
    isr, _, cr = struct.unpack('<III', uc.mem_read(base, 12))
    if cr & 2:
      cr &= ~1
    cr &= ~0x80000032  # ADCAL, ADSTP, JADSTP and ADDIS done
    isr = isr | 0x1000 | (cr & 1)  # LDORDY, ADRDY
    uc.mem_write(base, struct.pack('<III', isr, _, cr))

  def on_unmapped(self, uc, access, address, size, value, user):
    sys.stderr.write('unmapped access %x at pc %x\n' % (
        address, uc.reg_read(A.UC_ARM_REG_PC)))
    return False

  def write_gpio(self):
    for address, value in self.gpio.items():
      self.uc.mem_write(address, struct.pack('<I', value))

  def press(self, switch, down):
    address, bit = SWITCHES[switch]
    if down:
      self.gpio[address] &= ~(1 << bit)
    else:
      self.gpio[address] |= 1 << bit
    self.write_gpio()

  def run(self, begin, until, what, end=None):
    start = self.count
    self.limit = start + BUDGET
    self.inner = []
    self.uc.emu_start(begin | 1, until)
    pc = self.uc.reg_read(A.UC_ARM_REG_PC)
    if self.frames:
      raise RuntimeError('%s stopped inside %s' % (what, self.frames[-1][0]))
    if pc != (until if end is None else end):
      raise RuntimeError('%s stopped at pc %x after %d instructions' % (
          what, pc, self.count - start))
    return self.count - start

  def call(self, name, *args):
    uc = self.uc
    for i, a in enumerate(args):
      uc.reg_write(A.UC_ARM_REG_R0 + i, a & 0xffffffff)
    uc.reg_write(A.UC_ARM_REG_SP, self.irq_stack)
    uc.reg_write(A.UC_ARM_REG_LR, STOP | 1)
    return self.run(self.image.address(name), STOP, name)

  def boot(self):
    """Reset_Handler to the main loop's head, with SEED held."""
    uc = self.uc
    image = self.image
    main = image.address('main')
    init = image.address('_Z4Initv')
    code = bytes(uc.mem_read(main, 0x40))
    i = 0
    while i < len(code):
      hw = code[i] | code[i + 1] << 8
      n = thumb_length(hw)
      if n == 4 and self.bl_target(main + i, code[i:i + 4]) == init:
        self.loop_head = main + i + 4
        break
      i += n
    if self.loop_head is None:
      raise RuntimeError('no call to Init at the start of main')
    uc.hook_add(unicorn.UC_HOOK_CODE, self.on_head, begin=self.loop_head,
                end=self.loop_head)
    sp, reset = struct.unpack('<II', uc.mem_read(
        image.address('g_pfnVectors'), 8))
    uc.reg_write(A.UC_ARM_REG_SP, sp)
    self.press('seed', True)
    n = self.run(reset & ~1, self.loop_head, 'boot')
    self.head = 0
    self.press('seed', False)
    self.main_context = uc.context_save()
    self.irq_stack = (uc.reg_read(A.UC_ARM_REG_SP) - 0x40) & ~7
    return n

  @staticmethod
  def bl_target(address, b):
    h1 = b[0] | b[1] << 8
    h2 = b[2] | b[3] << 8
    if h1 >> 11 != 0x1e or h2 >> 14 != 3 or not h2 & 0x1000:
      return None
    s = h1 >> 10 & 1
    j1, j2 = h2 >> 13 & 1, h2 >> 11 & 1
    i1, i2 = 1 - (j1 ^ s), 1 - (j2 ^ s)
    offset = (s << 24 | i1 << 23 | i2 << 22 | (h1 & 0x3ff) << 12 |
              (h2 & 0x7ff) << 1)
    if s:
      offset -= 1 << 25
    return address + 4 + offset

  def main_step(self):
    uc = self.uc
    uc.context_restore(self.main_context)
    self.head = 0
    n = self.run(self.loop_head, STOP, 'main loop', self.loop_head)
    self.head = 0
    self.main_context = uc.context_save()
    return n

  def read(self, address, fmt):
    return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

  def write(self, address, fmt, *values):
    self.uc.mem_write(address, struct.pack(fmt, *values))


class Rig(object):

  def __init__(self, image, region=None):
    self.m = Machine(image)
    # The region as an install leaves it, from the app's region start.
    if region:
      self.m.uc.mem_write(image.address('plaits_region_begin'), region)
    self.image = image
    self.left = self.right = True
    self.seed_patched = False
    self.freeze_gate = False
    self.sample = 0
    self.half = 0
    self.calls = []
    self.recording = False
    self.out = hashlib.sha256()
    self.digest = hashlib.sha256()
    self.watches = {}
    self.block_size = image.member('codec', 'Codec', 'block_size_')
    self.callback = image.member('codec', 'Codec', 'callback_')
    self.rng = image.member('normalization_detector', 'NormalizationDetector',
                            'rng_state_')
    self.state = image.member('settings', 'Settings', 'state_')
    self.values = image.member('settings', 'Settings', 'values_')
    self.labels = all(n in image.sym for n in LABELS)
    self.boot_count = self.m.boot()
    self.m.write(image.address('_ZN5beads7PotsAdc7values_E'), '<12H', *POTS)

  def probe(self):
    rng, = self.m.read(self.rng, '<I')
    return rng >> 31

  def record(self, fn, n, phase, silent=False):
    # Runs of identical calls are stored once, with their length.
    inner = self.m.inner
    for watch in self.watches.values():
      watch[2].add(self.m.read(watch[0], watch[1])[0])
    if not self.recording:
      return
    self.digest.update(b'%s %d %d %r\n' % (fn.encode(), n, silent, inner))
    c = self.calls[-1] if self.calls else None
    if c and c[:3] == [fn, n, int(silent)] and c[4:] == [phase, inner]:
      c[3] += 1
    else:
      self.calls.append([fn, n, int(silent), 1, phase, inner])

  def phase(self):
    if not self.labels:
      return None
    sym = self.image.sym
    frames, = self.m.read(sym['codec_transition_frames'], '<i')
    gain, = self.m.read(sym['reverb_style_gain'], '<f')
    previous, = self.m.read(sym['reverb_style_previous_gain'], '<f')
    synth, = self.m.read(sym['synth_mode'], '<B')
    callback, = self.m.read(self.callback, '<I')
    if not callback:
      return 'paused'
    if frames:
      return 'blanking'
    if gain == 0.0 and previous == 0.0:
      return 'silent'
    if gain != 1.0 or previous != 1.0:
      return 'fading'
    return 'synth' if synth else 'steady'

  def block(self):
    m = self.m
    size, = m.read(self.block_size, '<I')
    if not size:
      return
    probe = self.probe()
    cv = PROBE_HIGH_CV if probe else PROBE_LOW_CV
    m.write(self.image.address('_ZN5beads5CvAdc7values_E'), '<6H', *[cv] * 6)
    frames, events = [], []
    for i in range(size):
      level = SQUARE_LEVEL if (self.sample // (SQUARE_PERIOD // 2)) % 2 \
          else -SQUARE_LEVEL
      self.sample += 1
      unpatched = 1 if probe else -1
      frames.append(level if self.left else unpatched)
      frames.append(-level if self.right else unpatched)
      seed = probe if not self.seed_patched else False
      # The pins read low for an event.
      pins = 0xffff
      if seed:
        pins &= ~(1 << 10)
      if self.freeze_gate:
        pins &= ~(1 << 11)
      events.extend((pins, 0))
    offset = self.half * size
    address = self.image.address
    m.write(address('_ZN5beads5Codec14rx_dma_buffer_E') + 8 * offset,
            '<%di' % (2 * size), *frames)
    m.write(address('_ZN5beads5Codec11idr_buffer_E') + 8 * offset,
            '<%dI' % (2 * size), *events)
    m.write(DMA1_LISR, '<I', LISR_TC if self.half else LISR_HT)
    tx = self.image.address('_ZN5beads5Codec14tx_dma_buffer_E') + 8 * offset
    phase = self.phase()
    n = m.call('DMA1_Stream1_IRQHandler')
    out = bytes(m.uc.mem_read(tx, 8 * size))
    self.half ^= 1
    if self.recording:
      self.out.update(out)
    self.record('audio', n, phase, not any(out))

  def tick(self, ms, audio=True):
    for _ in range(ms):
      phase = self.phase()
      self.record('main', self.m.main_step(), phase)
      if audio:
        for _ in range(4):
          self.block()
      phase = self.phase()
      self.record('tick', self.m.call('SysTick_Handler'), phase)

  def settle(self):
    # Eight 32 ms probe windows plus 250 ms of codec blanking.
    self.tick(600)

  def set_feature(self, feature, value):
    self.m.write(self.values + feature, '<B', value)

  def set_media(self, media):
    self.m.write(self.state, '<B', media)

  def tap(self, switch, ms=60):
    self.m.press(switch, True)
    self.tick(ms)
    self.m.press(switch, False)

  def combo(self, held, switch):
    self.m.press(held, True)
    self.tick(30)
    self.tap(switch)
    self.tick(30)
    self.m.press(held, False)
    self.tick(100)

  def state_byte(self, offset):
    return self.m.read(self.state + offset, '<B')[0]

  def ui_flag(self, field):
    return self.m.read(self.image.member('ui', 'Ui', field), '<B')[0]

  def expect(self, condition, what):
    if not condition:
      raise RuntimeError('gesture had no effect: %s' % what)


def scenario_steady(r):
  r.recording = True
  r.tick(50)


def scenario_media(r):
  media, = r.m.read(r.state, '<B')
  r.recording = True
  r.tick(5)
  r.set_media((media + 2) % 4)
  r.tick(320)
  r.set_media(media)
  r.tick(320)


def scenario_clouds(r):
  r.recording = True
  r.tick(5)
  r.set_feature(FEATURE_REVERB, 1)
  r.tick(30)
  r.set_feature(FEATURE_REVERB, 0)
  r.tick(30)


def scenario_processor(r):
  # Granular to spectral and back: fade, engine swap, codec restart.
  r.recording = True
  r.tick(5)
  r.set_feature(FEATURE_PROCESSOR, 1)
  r.tick(320)
  r.set_feature(FEATURE_PROCESSOR, 0)
  r.tick(320)


def scenario_freeze_fade(r):
  # FREEZE pressed halfway through a quality change's fade-out.
  media, = r.m.read(r.state, '<B')
  r.recording = True
  r.tick(5)
  r.set_media((media + 2) % 4)
  r.tick(5)
  r.tap('freeze')
  r.tick(320)
  r.tap('freeze')
  r.tick(100)


def scenario_gestures(r):
  r.recording = True
  r.tick(20)
  # Each combo twice, so the state returns: ASSIGN+SEED gate output,
  # ASSIGN+FREEZE autosave, MEDIA+SEED monophonic.
  for held, switch, offset in (('assign', 'seed', STATE_GATE_OUTPUT),
                               ('assign', 'freeze', STATE_NO_AUTOSAVE),
                               ('media', 'seed', STATE_MONOPHONIC)):
    for _ in range(2):
      before = r.state_byte(offset)
      r.combo(held, switch)
      r.expect(r.state_byte(offset) == (not before),
               '%s+%s toggles state byte %d' % (held, switch, offset))
  # MEDIA+FREEZE trims the frozen recording and unfreezes.
  r.tap('freeze')
  r.tick(100)
  r.expect(r.ui_flag('freeze_button_state_'), 'FREEZE freezes')
  trim = r.image.member('ui', 'Ui', 'trim_flag_')
  r.watches['trim'] = (trim, '<B', set())
  media = r.state_byte(0)
  r.combo('media', 'freeze')
  r.expect(1 in r.watches.pop('trim')[2] and
           not r.ui_flag('freeze_button_state_') and
           r.state_byte(0) == media, 'MEDIA+FREEZE trims')
  # MEDIA held while FEEDBACK turns sets a manual gain.
  gain = r.state_byte(STATE_GAIN)
  pots = r.image.address('_ZN5beads7PotsAdc7values_E')
  r.m.press('media', True)
  r.tick(30)
  for value in range(POTS[0], 0xF000, 0x400):
    r.m.write(pots, '<H', value)
    r.tick(2)
  r.tick(30)
  r.m.press('media', False)
  r.tick(100)
  manual = r.state_byte(STATE_GAIN)
  r.expect(manual not in (0, gain), 'MEDIA+FEEDBACK sets a manual gain')
  r.m.write(pots, '<H', POTS[0])
  r.tick(100)
  # A MEDIA hold alone returns to auto gain.
  r.tap('media', 1700)
  r.tick(100)
  r.expect(r.state_byte(STATE_GAIN) == 0 and r.state_byte(0) == media,
           'MEDIA hold restarts auto gain')


def scenario_synth(r):
  r.left = r.right = False
  r.recording = True
  # Detection with audio, then the 10 s timer on the clock alone.
  r.tick(300)
  r.tick(9800, audio=False)
  r.tick(600)
  # Plaits selected, but the erased flash has no models: Beads' synth plays,
  # so MEDIA+FREEZE runs the profile's Buffer trim, not a bank step (#353).
  r.set_feature(FEATURE_SYNTH, 1)
  r.tick(100)
  bank = r.m.read(r.values + FEATURE_PLAITS_BANK, '<B')[0]
  r.combo('media', 'freeze')
  r.expect(r.m.read(r.values + FEATURE_PLAITS_BANK, '<B')[0] == bank,
           'MEDIA+FREEZE leaves the bank of a Plaits that cannot play')
  r.set_feature(FEATURE_SYNTH, 0)
  r.tick(100)
  r.left = r.right = True
  r.tick(700)


def scenario_ui(r):
  # FREEZE on and off, a SEED tap, the options page held open and left.
  r.recording = True
  r.tick(20)
  for switch in ('freeze', 'freeze', 'seed'):
    r.tap(switch)
    r.tick(100)
  r.tap('assign', 3200)
  r.tick(200)
  r.tap('assign')
  r.tick(1200)


SCENARIOS = (
    ('steady', scenario_steady),
    ('media', scenario_media),
    ('clouds_style', scenario_clouds),
    ('processor', scenario_processor),
    ('freeze_fade', scenario_freeze_fade),
    ('gestures', scenario_gestures),
    ('synth', scenario_synth),
    ('ui', scenario_ui))


def run(arguments):
  path, name, region = arguments
  start = time.time()
  r = Rig(Image(path), region)
  r.settle()
  dict(SCENARIOS)[name](r)
  phases = {}
  for fn, n, silent, repeat, phase, inner in r.calls:
    for function, count in [[fn, n]] + inner:
      p = phases.setdefault('%s %s' % (function, phase),
                            [0, count, count, 0])
      p[0] += repeat
      p[1] = min(p[1], count)
      p[2] = max(p[2], count)
      p[3] += count * repeat
  sys.stderr.write('%s: %.1f s\n' % (name, time.time() - start))
  return {
      'name': name,
      'total': sum(c[1] * c[3] for c in r.calls),
      'calls_sha256': r.digest.hexdigest(),
      'out_sha256': r.out.hexdigest(),
      'boot': r.boot_count,
      'phases': phases,
      'calls': r.calls}


def check(path):
  """Block counts against Unicorn's per-instruction hook.

  That hook reports IT instructions and the instructions they make
  conditional inconsistently: those are left out, every other instruction
  must be reported once per execution of its block.
  """
  r = Rig(Image(path))
  r.tick(300)
  m = r.m
  blocks, hits = {}, {}

  def on_code(uc, address, size, user):
    hits[address] = hits.get(address, 0) + 1

  def on_block(uc, address, size, user):
    # Machine.on_block runs first: a second loop head is the stop.
    if address >> 12 != STOP >> 12 and not (
        address == m.loop_head and m.head > 1):
      blocks[(address, size)] = blocks.get((address, size), 0) + 1

  hooks = [m.uc.hook_add(unicorn.UC_HOOK_CODE, on_code),
           m.uc.hook_add(unicorn.UC_HOOK_BLOCK, on_block)]
  m.uc.ctl_flush_tb()
  failures = 0
  for name, step in (('tick', lambda: m.call('SysTick_Handler')),
                     ('main', m.main_step), ('audio', r.block)):
    blocks.clear()
    hits.clear()
    start = m.count
    step()
    counted = m.count - start
    shadowed = set()
    for address, size in blocks:
      i = 0
      while i < size:
        hw, = m.read(address + i, '<H')
        if hw & 0xff00 == 0xbf00 and hw & 0xf:
          shadowed.add(address + i)
          pc = address + i + 2
          for _ in range(5 - (hw & -hw & 0xf).bit_length()):
            shadowed.add(pc)
            pc += thumb_length(m.read(pc, '<H')[0])
        i += thumb_length(hw)
    expected, conditional = {}, 0
    for (address, size), n in blocks.items():
      i = 0
      while i < size:
        if address + i in shadowed:
          conditional += n
        else:
          expected[address + i] = expected.get(address + i, 0) + n
        i += thumb_length(m.read(address + i, '<H')[0])
    agree = sum(n for pc, n in expected.items() if hits.get(pc) == n)
    stray = [pc for pc in hits if pc >> 12 != STOP >> 12 and
             pc not in expected and not any(
                 a <= pc < a + s for a, s in blocks)]
    ok = agree == sum(expected.values()) and \
        agree + conditional == counted and not stray
    print('%s: %d instructions, %d agree with the per-instruction hook, '
          '%d IT or conditional: %s' % (
              name, counted, agree, conditional, 'OK' if ok else 'FAIL'))
    failures += not ok
    for pc, n in sorted(expected.items()):
      if hits.get(pc) != n:
        print('  %x: %d executions, hook saw %d' % (pc, n, hits.get(pc, 0)))
  for h in hooks:
    m.uc.hook_del(h)
  return 1 if failures else 0


def scenarios(path):
  results = json.load(open(path))['scenarios']
  names = [s['name'] for s in results]
  if not names or len(set(names)) != len(names):
    raise RuntimeError('%s: empty or repeated scenario list' % path)
  return results


def compare(base_path, candidate_path):
  try:
    base, candidate = scenarios(base_path), scenarios(candidate_path)
  except RuntimeError as e:
    print(e)
    return 1
  names = [s['name'] for s in base]
  if names != [s['name'] for s in candidate]:
    print('scenarios differ: %s -> %s' % (
        ' '.join(names), ' '.join(s['name'] for s in candidate)))
    return 1
  failures = 0
  boots = (base[0]['boot'], candidate[0]['boot'])
  if any(s['boot'] != boots[0] for s in base) or \
      any(s['boot'] != boots[1] for s in candidate):
    print('boot: counts differ between scenarios of one file')
    failures += 1
  print('boot: %s, %d instructions -> %d' % (
      'identical' if boots[0] == boots[1] else 'differs', boots[0], boots[1]))
  failures += boots[0] != boots[1]
  keys = ('calls_sha256', 'out_sha256', 'total', 'phases')
  for b, c in zip(base, candidate):
    name = b['name']
    same = [b[k] == c[k] for k in keys]
    if all(same):
      print('%s: identical, %d instructions' % (name, b['total']))
      continue
    failures += 1
    print('%s: %s differ, %d instructions -> %d' % (
        name, ' and '.join(k for k, x in zip(keys, same) if not x),
        b['total'], c['total']))
    if 'calls' in b and 'calls' in c:
      expand = lambda x: [(fn, n, silent, inner, phase)
                          for fn, n, silent, repeat, phase, inner in x['calls']
                          for _ in range(repeat)]
      eb, ec = expand(b), expand(c)
      shown = 0
      for i in range(max(len(eb), len(ec))):
        x = eb[i] if i < len(eb) else None
        y = ec[i] if i < len(ec) else None
        if x is None or y is None or x[:4] != y[:4]:
          print('  call %d: %s -> %s' % (i, x, y))
          shown += 1
          if shown == 20:
            print('  ...')
            break
    for key in sorted(set(b['phases']) | set(c['phases'])):
      x, y = b['phases'].get(key), c['phases'].get(key)
      if x != y:
        print('  %-24s %s -> %s' % (key, x, y))
  return 1 if failures else 0


def main(argv):
  if len(argv) == 3 and argv[1] == '--check':
    return check(argv[2])
  if len(argv) == 4 and argv[1] == '--compare':
    return compare(argv[2], argv[3])
  summary = '--summary' in argv
  argv = [a for a in argv if a != '--summary']
  region = None
  if '--region' in argv:
    i = argv.index('--region')
    region = open(argv[i + 1], 'rb').read()
    del argv[i:i + 2]
  if len(argv) < 3:
    sys.stderr.write(USAGE)
    return 2
  path, out = argv[1], argv[2]
  names = argv[3:] or [n for n, _ in SCENARIOS]
  image = Image(path)
  pool = multiprocessing.Pool(min(len(names), multiprocessing.cpu_count()))
  results = pool.map(run, [(path, n, region) for n in names])
  for s in results:
    print('%-14s %12d instructions' % (s['name'], s['total']))
    for key, (n, low, high, total) in sorted(s['phases'].items()):
      print('  %-28s %6d calls %8d..%-8d' % (key, n, low, high))
    if summary:
      del s['calls']
  # A summary is for reading; full call lists are kept compact.
  json.dump({'elf_sha256': image.sha256, 'unicorn': unicorn.__version__,
             'region_sha256': region and hashlib.sha256(region).hexdigest(),
             'scenarios': results}, open(out, 'w'), sort_keys=True,
            indent=1 if summary else None)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
