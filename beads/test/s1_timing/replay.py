#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
#
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

"""Replay an S1 timing ELF into the bench ABI decoder using Unicorn."""

import argparse
import json
from pathlib import Path
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'transition_emu'))
import region_boots as rb
import transition_emu as te

CYCCNT = 0xe0001004
TIM2_CNT = 0x40000024
DMA1_LIFCR = te.DMA1_LISR + 8
RNG_SR = 0x48021804
RNG_DR = 0x48021808
TRACE_HEADER = struct.Struct('<IIIQQIIII')
FRAME_BYTES = 56
SEAL_OFFSET = 15 * 1024 - 4
CONFIGURE = '_Z22ConfigureAndStartCodecbb'
OBSERVER = 's1_observer_cycles'
CALLBACK = 's1_callback_cycles'
IRQ = 's1_irq_cycles'
PROFILE = 0x08020400
CEILING = 's1_ceiling'
# Emulated core cycles per 250 us callback block: the block's own DMA flag
# returns one budget after its interrupt, the next half's flag at once.
BLOCK_CYCLES = 480000000 // 4000
SETTLE_MS = 3000


ReleaseMachine = te.Machine


class Machine(te.Machine):
  profile = None
  epoch = 0

  def __init__(self, image):
    super().__init__(image)
    Machine.epoch += 1
    self.tick_us = 0
    self.block_start = 0
    self.block_end = 0
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_READ, self.peripheral,
                    begin=CYCCNT, end=CYCCNT)
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_READ, self.peripheral,
                    begin=TIM2_CNT, end=TIM2_CNT)
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_WRITE, self.clear_dma,
                    begin=DMA1_LIFCR, end=DMA1_LIFCR)
    self.irq_start = None
    self.irq_half = 0
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_READ, self.late_dma,
                    begin=te.DMA1_LISR, end=te.DMA1_LISR)
    self.write(RNG_SR, '<I', 1)
    self.write(RNG_DR, '<I', Machine.epoch)
    if self.profile is not None:
      self.uc.mem_write(PROFILE, self.profile)

  def boot(self, recovery=True):
    # S1 polls the mailbox before DoEvents. Stop at the first call in that
    # loop so a mailbox main step runs one complete iteration.
    main = self.image.address('main')
    init = self.image.address('_Z4Initv')
    poll_head = self.image.address('_ZN5beads9s1_timing14codec_overrideEv')
    code = bytes(self.uc.mem_read(main, 0x40))
    initialized = False
    offset = 0
    while offset < len(code):
      length = te.thumb_length(struct.unpack_from('<H', code, offset)[0])
      if length == 4:
        target = self.bl_target(main + offset, code[offset:offset + 4])
        if initialized and target == poll_head:
          self.loop_head = main + offset
          break
        if target == init:
          initialized = True
      offset += length
    assert self.loop_head is not None, 'cannot locate the S1 main loop'
    self.uc.hook_add(te.unicorn.UC_HOOK_CODE, self.on_head,
                     begin=self.loop_head, end=self.loop_head)
    sp, reset = self.read(self.image.address('g_pfnVectors'), '<II')
    self.uc.reg_write(te.A.UC_ARM_REG_SP, sp)
    self.press('seed', recovery)
    count = self.run(reset & ~1, self.loop_head, 'boot')
    self.head = 0
    self.press('seed', False)
    self.main_context = self.uc.context_save()
    self.irq_stack = (self.uc.reg_read(te.A.UC_ARM_REG_SP) - 0x40) & ~7
    return count

  def on_block(self, uc, address, size, user):
    self.block_start = address
    self.block_end = address + size
    super().on_block(uc, address, size, user)

  def clear_dma(self, uc, access, address, size, value, user):
    status, = self.read(te.DMA1_LISR, '<I')
    self.write(te.DMA1_LISR, '<I', status & ~value)

  def late_dma(self, uc, access, address, size, value, user):
    # A callback past its block sees the next half's flag (and, two blocks
    # late, its own again). Instruction counts stand in for cycles.
    if self.irq_start is None:
      return
    elapsed = self.count - self.irq_start
    current = te.LISR_TC if self.irq_half else te.LISR_HT
    following = te.LISR_HT if self.irq_half else te.LISR_TC
    status, = self.read(te.DMA1_LISR, '<I')
    if elapsed >= BLOCK_CYCLES:
      status |= following
    if elapsed >= 2 * BLOCK_CYCLES:
      status |= current
    self.write(te.DMA1_LISR, '<I', status)

  def peripheral(self, uc, access, address, size, value, user):
    if address == TIM2_CNT:
      self.write(address, '<I', self.tick_us & 0xffffffff)
      return
    # The base emulator counts whole basic blocks at their entry.
    pc = uc.reg_read(te.A.UC_ARM_REG_PC)
    tail = bytes(uc.mem_read(pc, self.block_end - pc))
    remaining, i = 0, 0
    while i < len(tail):
      i += te.thumb_length(tail[i] | tail[i + 1] << 8)
      remaining += 1
    self.write(address, '<I', (self.count - remaining) & 0xffffffff)


class Rig(te.Rig):
  def __init__(self, image, region):
    self.samples = []
    self.irq = []
    super().__init__(image, region)

  def record(self, fn, n, phase, silent=False):
    if fn == 'audio':
      self.irq.append(n)

  def block(self):
    size, = self.m.read(self.block_size, '<I')
    half = self.half
    self.m.irq_start, self.m.irq_half = self.m.count, half
    super().block()
    self.m.irq_start = None
    if size:
      tx = self.image.address('_ZN5beads5Codec14tx_dma_buffer_E')
      self.samples.extend(self.m.read(tx + 8 * size * half, f'<{2*size}i'))
    self.m.tick_us += 250

  def mailbox(self, request, count=0, codec=0, synth=0):
    address = self.image.address('s1_mailbox')
    self.m.write(address, '<4I', request, count, codec, synth)
    self.m.main_step()
    return self.m.read(address, '<5I')

  def request(self, request, count=0, codec=0, synth=0, wanted=None):
    status = self.mailbox(request, count, codec, synth)[-1]
    assert status == wanted, (request, status)

  def settle(self):
    # Request 8 reports 9 once the codec runs in the asked mode.
    for _ in range(SETTLE_MS):
      self.tick(1)
      if self.m.read(self.image.address('s1_mailbox') + 16, '<I')[0] == 9:
        return self.m.read(self.image.address('synth_mode'), '<B')[0]
    raise AssertionError('request 8 never settled')

  def hold(self, sequence, target):
    address = self.image.address(CEILING)
    command = sequence << 24 | target
    self.m.write(address + 8, '<I', command)
    self.block()
    words = self.m.read(address, '<16I')
    assert words[:2] == (0x4c494543, 2) and words[3] == command, words
    assert words[15] == (1 if target else 0), words
    return words

  def latch(self, directory, count, before, trigger_at, limit=400):
    # Pre-armed: the firmware seals on the callback with its own SEED edge.
    directory.mkdir()
    (directory / 'preflight.log').write_text(self.burst())
    self.m.tick_us += 250
    self.request(7, count, before, wanted=7)
    button = self.image.member('ui', 'Ui', 'seed_button_state_')
    locked = self.image.member('ui', 'Ui', 'seed_button_locked_')
    status = self.image.address('s1_mailbox') + 16
    for i in range(limit):
      if i == trigger_at:
        self.m.write(locked, '<B', 0)
        self.m.write(button, '<B', 1)
      if i == trigger_at + 4:
        self.m.write(button, '<B', 0)
      self.block()
      if not trigger_at <= i <= trigger_at + 4:
        self.m.main_step()  # the UI would read the switch over the pressed state
      if self.m.read(status, '<I')[0] == 2:
        break
    else:
      raise AssertionError('SEED-latched capture never published')
    addr = self.image.address('s1_trace')
    header = TRACE_HEADER.unpack(self.m.uc.mem_read(addr, TRACE_HEADER.size))
    seal, = self.m.read(addr + SEAL_OFFSET, '<I')
    assert header[5] == count and header[8] == seal and seal % 2 == 0, header
    data = bytes(self.m.uc.mem_read(addr, TRACE_HEADER.size + count * FRAME_BYTES))
    frames = [data[TRACE_HEADER.size + i * FRAME_BYTES:][:FRAME_BYTES] for i in range(count)]
    seqs = [struct.unpack_from('<I', f)[0] for f in frames]
    edges = [i for i, f in enumerate(frames) if f[15] & 4]
    assert seqs == list(range(seqs[0], seqs[0] + count)) and edges == [before], (seqs[:3], edges)
    assert header[4] - header[3] == 250 * count and header[6] == 0, header
    (directory / 'callbacks.bin').write_bytes(data)
    (directory / 'postflight.log').write_text(self.burst())
    return header

  def snapshot(self):
    words = self.m.read(self.image.address('s1_snapshot'), '<26I')
    assert words[0] and not words[0] & 1 and words[1] == 1, words
    return words

  def burst(self):
    lines = ['PROBE_FIELDS 27']
    for _ in range(8):
      self.m.tick_us += 10000
      self.m.main_step()
      row = self.snapshot()
      lines.append('PROBE ' + ' '.join(f'{x:08x}' for x in (*row, row[0])))
    return '\n'.join((*lines, 'KIT_COMPLETE', ''))

  def set_feature(self, feature, value):
    super().set_feature(feature, value)
    self.m.call('SysTick_Handler')

  def posture(self, synth, seed=True):
    normalized = self.image.member(
        'normalization_detector', 'NormalizationDetector', 'normalized_')
    self.m.write(normalized, '<8B', not seed, 1, 1, 1, 1, 1, synth, synth)
    self.left = self.right = not synth
    self.seed_patched = seed

  def configure(self, synth, seed=True):
    self.m.call(CONFIGURE, int(synth), 1)
    # Select this callback bracket explicitly. Panel transitions are separate.
    self.m.write(self.image.address('codec_transition_frames'), '<i', 0)
    self.posture(synth, seed)

  def capture(self, directory, count=160, trigger_at=None, resume=False):
    directory.mkdir()
    (directory / 'preflight.log').write_text(self.burst())
    self.m.tick_us += 250
    assert self.mailbox(1, count)[-1] == 1
    if resume:
      assert self.mailbox(5)[-1] == 5
      self.posture(True, self.seed_patched)
    del self.samples[:]
    del self.irq[:]
    observer, callback, dwt_irq = [], [], []
    button = self.image.member('ui', 'Ui', 'seed_button_state_')
    locked = self.image.member('ui', 'Ui', 'seed_button_locked_')
    for i in range(count):
      if i == trigger_at:
        self.m.write(locked, '<B', 0)
        self.m.write(button, '<B', 1)
      if trigger_at is not None and i == trigger_at + 4:
        self.m.write(button, '<B', 0)
      self.block()
      observer.append(self.m.read(self.image.address(OBSERVER), '<I')[0])
      callback.append(self.m.read(self.image.address(CALLBACK), '<I')[0])
      dwt_irq.append(self.m.read(self.image.address(IRQ), '<I')[0])
    self.m.main_step()
    addr = self.image.address('s1_trace')
    header = TRACE_HEADER.unpack(self.m.uc.mem_read(addr, TRACE_HEADER.size))
    seal, = self.m.read(addr + SEAL_OFFSET, '<I')
    assert header[5] == count and header[8] == seal and seal % 2 == 0, header
    data = self.m.uc.mem_read(addr, TRACE_HEADER.size + count * FRAME_BYTES)
    (directory / 'callbacks.bin').write_bytes(data)
    (directory / 'output.s32').write_bytes(struct.pack(f'<{len(self.samples)}i', *self.samples))
    (directory / 'postflight.log').write_text(self.burst())
    assert 0 < max(callback) == self.word('s1_callback_peak')
    assert 0 < max(dwt_irq) == self.word('s1_irq_peak')
    assert all(c < d <= n for c, d, n in zip(callback, dwt_irq, self.irq))
    callback_gate = self.word('s1_callback_gate_peak')
    irq_gate = self.word('s1_irq_gate_peak')
    assert 0 < callback_gate < max(callback)
    assert callback_gate < irq_gate < max(dwt_irq)
    (directory / 'instructions.json').write_text(json.dumps(dict(
        irq=self.irq, callback=callback, dwt_irq=dwt_irq, observer=observer,
        callback_gate_peak=callback_gate, irq_gate_peak=irq_gate)))
    return header

  def word(self, symbol):
    return self.m.read(self.image.address(symbol), '<I')[0]

  def check_callback(self, name):
    callback, = self.m.read(self.callback, '<I')
    symbol, = (symbol for symbol in self.image.sym
               if symbol.startswith(f'_Z{len(name)}{name}PK'))
    assert callback & ~1 == self.image.address(symbol), (name, hex(callback))


class ReleaseRig(te.Rig):
  configure = Rig.configure
  posture = Rig.posture
  samples = None

  def set_feature(self, feature, value):
    super().set_feature(feature, value)
    self.m.call('SysTick_Handler')

  def record(self, fn, n, phase, silent=False):
    if fn != 'audio' or self.samples is None:
      return
    self.irqs.append(n)
    callback, = [count for name, count in self.m.inner if name == 'codec callback']
    self.callbacks.append(callback)
    tx = self.image.address('_ZN5beads5Codec14tx_dma_buffer_E')
    count, = self.m.read(self.block_size, '<I')
    self.samples.extend(self.m.read(
        tx + 8 * count * (self.half ^ 1), f'<{2 * count}i'))


def pair_release(image, region, fixture, name, processor, media, reverb, synth, trim):
  te.Machine = ReleaseMachine
  try:
    r = ReleaseRig(image, region)
  finally:
    te.Machine = Machine
  r.configure(False)
  r.set_media(media)
  for feature, value in ((te.FEATURE_SYNTH, 0), (te.FEATURE_PROCESSOR, processor),
                         (te.FEATURE_REVERB, reverb), (50, trim)):
    r.set_feature(feature, value)
  r.tick(320)
  if synth:
    r.configure(True)
    for _ in range(16):
      r.block()
  for _ in range(9):
    r.m.main_step()
  callback, = r.m.read(r.callback, '<I')
  r.m.entries[callback & ~1] = 'codec callback'
  r.samples, r.irqs, r.callbacks = [], [], []
  for _ in range(64):
    r.block()
  counts = json.loads((fixture / 'instructions.json').read_text())
  results = dict(case=name, audio_match=(fixture / 'output.s32').read_bytes() ==
                 struct.pack(f'<{len(r.samples)}i', *r.samples))
  for label, release in (('callback', max(r.callbacks)), ('irq', max(r.irqs))):
    timing = counts[label + '_gate_peak']
    results['release_' + label] = release
    results['timing_' + label] = timing
    results[label + '_residual'] = timing - release
  print(json.dumps(results), flush=True)
  assert results['audio_match'], results


def replay_rc1(image, region, output, release):
  cases = (
      ('processor-48', 0, 3, 0, False, 0, 'Process', 48000),
      ('processor-32', 0, 2, 0, False, 0, 'Process', 32000),
      ('spectral-48', 1, 3, 0, False, 0, 'Process', 48000),
      ('synth-48', 0, 3, 0, True, 0, 'Process', 48000),
      ('synth-trim-48', 0, 3, 0, True, 1, 'ProcessSynthLevel', 48000),
      ('abyss-48', 3, 3, 0, False, 0, 'ProcessAbyss', 48000),
      ('abyss-32', 3, 2, 0, False, 0, 'ProcessAbyss', 32000),
      ('oliverb-48', 2, 3, 0, False, 0, 'ProcessOliverb', 48000),
      ('oliverb-32', 2, 2, 0, False, 0, 'ProcessOliverb', 32000),
      ('bloom-48', 0, 3, 3, False, 0, 'ProcessExtendedReverb', 48000),
      ('bloom-synth-48', 0, 3, 3, True, 1, 'ProcessExtendedReverb', 48000),
  )
  for name, processor, media, reverb, synth, trim, callback, codec in cases:
    print(f'Replay {name}', flush=True)
    r = Rig(image, region)
    r.configure(False)
    r.set_media(media)
    r.set_feature(te.FEATURE_SYNTH, 0)
    r.set_feature(te.FEATURE_PROCESSOR, processor)
    r.set_feature(te.FEATURE_REVERB, reverb)
    r.set_feature(50, trim)
    r.tick(320)
    if synth:
      r.configure(True)
      for _ in range(16):
        r.block()
    r.check_callback(callback)
    directory = output / name
    r.capture(directory, count=64)
    callbacks = (directory / 'callbacks.bin').read_bytes()
    for offset in range(TRACE_HEADER.size, len(callbacks), FRAME_BYTES):
      assert struct.unpack_from('<H', callbacks, offset + 8)[0] == codec
      assert not callbacks[offset + 15] & 1
    assert max(map(abs, r.samples))
    # Both DMA halves must take the hold while the selected callback is live.
    before = r.hold(1, BLOCK_CYCLES * 9 // 10)
    r.block()
    after = r.m.read(image.address(CEILING), '<16I')
    assert after[5] == before[5] + 1
    assert r.word(CALLBACK) >= BLOCK_CYCLES * 9 // 10
    r.hold(2, 0)
    if processor in (2, 3):
      assert r.mailbox(8, 1)[-1] == 0xffffffff
      r.m.write(r.state + image.offsets['State']['gate_output_mode'], '<B', 1)
      for command, flags in ((3, 1 << 21), (4, 1 << 23)):
        r.hold(command, flags)
        # The first DMA frame carries the previous callback's last frame.
        r.block()
        del r.samples[:]
        r.block()
        assert not any(r.samples[1::2]), (name, command)
        if flags & (1 << 23):
          assert any(r.samples[::2]), name
    if release:
      pair_release(release, region, directory, name, processor, media, reverb, synth, trim)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('elf', type=Path)
  parser.add_argument('region', type=Path)
  parser.add_argument('output', type=Path)
  parser.add_argument('--release', type=Path, help='compare paired fixtures with this release ELF')
  args = parser.parse_args()
  args.output.mkdir(exist_ok=True, parents=True)
  te.Machine = Machine
  te.MEMBERS['NormalizationDetector'] += ('normalized_',)
  te.MEMBERS['Ui'] += ('seed_button_state_', 'seed_button_locked_')
  te.MEMBERS['State'] = ('gate_output_mode',)
  image = te.Image(args.elf)
  region = args.region.read_bytes()

  release = te.Image(args.release) if args.release else None
  replay_rc1(image, region, args.output, release)

  r = Rig(image, region)
  r.configure(False)
  assert r.mailbox(1, 2)[-1] == 1
  r.block()
  assert r.mailbox(6)[-1] == 6
  r.block()
  assert r.word('s1_mailbox') == 0
  assert r.mailbox(1, 2)[-1] == 1
  r.block()
  r.block()
  assert r.mailbox(6)[-1] == 2

  for codec, synth in ((48000, 24000), (32000, 48000),
                       (24000, 32000), (48000, 48000)):
    print(f'Replay Plaits {codec}/{synth}', flush=True)
    r = Rig(image, region)
    r.set_feature(te.FEATURE_SYNTH, 1)
    r.m.main_step()
    assert r.mailbox(2, codec=codec, synth=synth)[-1] == 3
    r.configure(True)
    for _ in range(16):
      r.block()
    assert r.mailbox(4)[-1] == 4
    directory = args.output / f'startup-{codec}-{synth}'
    r.capture(directory, resume=True)
    packed = (directory / 'callbacks.bin').read_bytes()
    paths, = struct.unpack_from('<H', packed, TRACE_HEADER.size + 22)
    assert paths & 0x8f == 0x8a, paths
    assert r.snapshot()[7:10] == (codec, synth, codec // 4000)
    r.capture(args.output / f'plaits-{codec}-{synth}', trigger_at=16)

  for placement in range(1, 4):
    r = Rig(image, region)
    r.set_feature(41, placement)
    r.configure(False)
    for _ in range(16):
      r.block()
    directory = args.output / f'rings-{placement}'
    r.capture(directory)
    packed = (directory / 'callbacks.bin').read_bytes()
    for offset in range(TRACE_HEADER.size, len(packed), FRAME_BYTES):
      assert packed[offset + 15] & 8 and packed[offset + 19] == placement

  # Request 8 with both inputs patched: the stock no-input path into Plaits.
  r = Rig(image, region)
  r.set_feature(te.FEATURE_SYNTH, 1)
  r.posture(False)
  r.tick(600)
  assert r.m.read(image.address('synth_mode'), '<B')[0] == 0
  r.request(8, 1, wanted=8)
  assert r.settle() == 1
  directory = args.output / 'synth-forced'
  r.capture(directory, count=64, trigger_at=8)
  packed = (directory / 'callbacks.bin').read_bytes()
  for offset in range(TRACE_HEADER.size, len(packed), FRAME_BYTES):
    paths, = struct.unpack_from('<H', packed, offset + 22)
    assert paths & 0xf == 0xa, paths
  assert max(map(abs, r.samples)) > 2**31 // 1000, 'forced Plaits is silent'
  r.request(8, 0, wanted=8)
  assert r.settle() == 0

  # Pre-armed SEED latch on the bench posture (inputs patched, request 8, the
  # main loop running): 136 callbacks before the edge, one sealed chunk.
  r = Rig(image, region)
  r.set_feature(te.FEATURE_SYNTH, 1)
  r.posture(False)
  r.tick(600)
  r.request(8, 1, wanted=8)
  assert r.settle() == 1
  r.latch(args.output / 'seed-latched', 273, 136, trigger_at=200)

  # The ceiling hold. Request 9, pre-armed at the natural load, seals on the
  # first late callback: 1010 permille while the overload starves the main loop
  # (no main_step until the hold is released); 900 never seals and is cancelled.
  status = image.address('s1_mailbox') + 16
  for name, permille, sequence in (('ceiling-overrun', 1010, 1), ('ceiling-control', 900, 3)):
    r = Rig(image, region)
    r.configure(False)
    for _ in range(16):
      r.block()
    directory = args.output / name
    preflight = r.burst()
    r.m.tick_us += 250
    r.request(9, 273, 136, wanted=9)
    for _ in range(140):  # more than the 136 kept before the trigger
      r.block()
    target = BLOCK_CYCLES * permille // 1000
    r.hold(sequence, target)
    for _ in range(150):
      r.block()
    words = json.dumps(dict(zip(
        ('magic version command ack rejects armed_blocks frames blocks on_time ambiguous late first_late '
         'g2 pending max_store state').split(), r.m.read(image.address(CEILING), '<16I'))))
    r.hold(sequence + 1, 0)
    r.m.main_step()
    if permille < 1000:
      assert r.m.read(status, '<I')[0] == 9, 'a clean hold sealed request 9'
      r.request(6, wanted=6)
      r.hold(sequence + 2, target)
      r.capture(directory, count=48)
      (directory / 'ceiling.json').write_text(words)
      r.hold(sequence + 3, 0)
      packed = (directory / 'callbacks.bin').read_bytes()
      assert not any(packed[o + 15] & 1 for o in range(TRACE_HEADER.size, len(packed), FRAME_BYTES))
      continue
    assert r.m.read(status, '<I')[0] == 2, 'the overrun never sealed request 9'
    addr = image.address('s1_trace')
    header = TRACE_HEADER.unpack(r.m.uc.mem_read(addr, TRACE_HEADER.size))
    seal, = r.m.read(addr + SEAL_OFFSET, '<I')
    assert header[5] == 273 and header[8] == seal and seal % 2 == 0, header
    data = bytes(r.m.uc.mem_read(addr, TRACE_HEADER.size + 273 * FRAME_BYTES))
    frames = [data[TRACE_HEADER.size + i * FRAME_BYTES:][:FRAME_BYTES] for i in range(273)]
    misses = [f[15] & 1 for f in frames]
    cycles = [struct.unpack_from('<I', f, 4)[0] for f in frames]
    assert misses == [0] * 136 + [1] * 137 and min(cycles[136:]) >= target > max(cycles[:136]), misses
    directory.mkdir()
    (directory / 'preflight.log').write_text(preflight)
    (directory / 'ceiling.json').write_text(words)
    (directory / 'callbacks.bin').write_bytes(data)
    (directory / 'postflight.log').write_text(r.burst())

  terrain = rb.with_terrain(region)
  Machine.profile = rb.install_profile(image, terrain)
  cases = {
      'WAVE ok PLTS ok': (1, (1, 1, 1)),
      'WAVE crc PLTS ok': (0, (2, 1, 1)),
      'WAVE ok PLTS truncated': (1, (1, 2, 1)),
      'TERR crc': (1, (1, 1, 2)),
  }
  for name, data, _ in rb.variants(terrain):
    if name not in cases:
      continue
    selected, status = cases[name]
    r = Rig(image, data)
    r.set_feature(te.FEATURE_SYNTH, selected)
    r.m.main_step()
    r.configure(True)
    for _ in range(32):
      r.block()
    assert r.snapshot()[21:24] == status, name
    r.capture(args.output / name.replace(' ', '-'), trigger_at=16)


if __name__ == '__main__':
  main()
