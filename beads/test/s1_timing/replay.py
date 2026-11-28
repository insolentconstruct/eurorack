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
DMA1_RX_NDTR = te.DMA1_LISR + 0x2c
RNG_SR = 0x48021804
RNG_DR = 0x48021808
TRACE_HEADER = struct.Struct('<IIIQQIIII')
FRAME_BYTES = 56
ROW_EVIDENCE = struct.Struct('<BB')
ROW_EVIDENCE_OFFSET = 30
CALLBACK_IDS = {
    'Process': 1,
    'ProcessSynthLevel': 2,
    'ProcessAbyss': 3,
    'ProcessOliverb': 4,
    'ProcessExtendedReverb': 5,
}
SEAL_OFFSET = 15 * 1024 - 4
CONFIGURE = '_Z22ConfigureAndStartCodecbb'
OBSERVER = 's1_observer_cycles'
CALLBACK = 's1_callback_cycles'
IRQ = 's1_irq_cycles'
PROFILE = 0x08020400
CEILING = 's1_ceiling'
CEILING_ENTER = '_ZN5beads11cpu_ceiling5EnterEj'
# Synthetic service costs in instruction-count units. Unicorn does not model
# the NVIC here. Inject a pending interrupt at the start of Enter's observation
# and charge it at the first unmasked instruction.
PREEMPTION = {'none': 0, 'SysTick': 800, 'TIM8': 250}
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
    self.dma_period = BLOCK_CYCLES
    self.dma_entry_age = 0
    self.dma_events = 0
    self.dma_half = 0
    self.dma_frames = 0
    self.preemption = 0
    self.preempt_pending = False
    self.preemption_wait = None
    self.enter_reads = 0
    self.enter_start = image.address(CEILING_ENTER)
    self.enter_end = min(addr for addr in image.sym.values() if addr > self.enter_start)
    self.uc.hook_add(te.unicorn.UC_HOOK_CODE, self.service_pending,
                     begin=self.enter_start, end=self.enter_end - 2)
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_READ, self.late_dma,
                    begin=te.DMA1_LISR, end=te.DMA1_LISR)
    self.uc.hook_add(te.unicorn.UC_HOOK_MEM_READ, self.late_dma,
                    begin=DMA1_RX_NDTR, end=DMA1_RX_NDTR)
    self.write(RNG_SR, '<I', 1)
    self.write(RNG_DR, '<I', Machine.epoch)
    if self.profile is not None:
      self.uc.mem_write(PROFILE, self.profile)

  def on_block(self, uc, address, size, user):
    self.block_start = address
    self.block_end = address + size
    super().on_block(uc, address, size, user)

  def clear_dma(self, uc, access, address, size, value, user):
    self.advance_dma()
    status, = self.read(te.DMA1_LISR, '<I')
    self.write(te.DMA1_LISR, '<I', status & ~value)

  def late_dma(self, uc, access, address, size, value, user):
    self.advance_dma()

  def advance_dma(self):
    if self.irq_start is None:
      return
    # Completions occur once at each boundary. A same-half completion ORs
    # into the still-set bit and the subsequent acknowledgement clears both.
    elapsed = self.dma_entry_age + self.count - self.irq_start
    events, within_half = divmod(elapsed, self.dma_period)
    status, = self.read(te.DMA1_LISR, '<I')
    while self.dma_events < events:
      self.dma_events += 1
      self.dma_half ^= 1
      status |= te.LISR_TC if self.dma_half else te.LISR_HT
    self.write(te.DMA1_LISR, '<I', status)
    words = 2 * self.dma_frames
    remaining = words * (1 + self.dma_half) - within_half * words // self.dma_period
    self.write(DMA1_RX_NDTR, '<I', remaining)

  def peripheral(self, uc, access, address, size, value, user):
    if address == TIM2_CNT:
      self.write(address, '<I', self.tick_us & 0xffffffff)
      return
    # The base emulator counts whole basic blocks at their entry.
    pc = uc.reg_read(te.A.UC_ARM_REG_PC)
    tail = bytes(uc.mem_read(pc, self.block_end - pc))
    remaining = sum(1 for _ in te_instructions(tail))
    if self.preemption and self.enter_start <= pc < self.enter_end:
      self.enter_reads += 1
      if self.enter_reads == 2:
        assert uc.reg_read(te.A.UC_ARM_REG_PRIMASK) == 1
        self.preempt_pending = True
        self.preemption_start = self.count - remaining
    self.write(address, '<I', (self.count - remaining) & 0xffffffff)

  def service_pending(self, uc, pc, size, user):
    if self.preempt_pending and not uc.reg_read(te.A.UC_ARM_REG_PRIMASK):
      self.preempt_pending = False
      tail = bytes(uc.mem_read(pc, self.block_end - pc))
      remaining = sum(1 for _ in te_instructions(tail))
      self.preemption_wait = self.count - remaining - self.preemption_start
      self.count += self.preemption
      self.preemption = 0


def te_instructions(code):
  offset = 0
  while offset < len(code):
    yield offset
    offset += te.thumb_length(struct.unpack_from('<H', code, offset)[0])


class Rig(te.Rig):
  def __init__(self, image, region):
    self.samples = []
    self.irq = []
    super().__init__(image, region)

  def record(self, fn, n, phase, silent=False):
    if fn == 'audio':
      self.irq.append(n)

  def block(self, dma_entry_age=0):
    size, = self.m.read(self.block_size, '<I')
    half = self.half
    assert 0 <= dma_entry_age < 2 * self.m.dma_period
    self.m.dma_entry_age = dma_entry_age
    self.m.irq_start = self.m.count
    self.m.dma_events, self.m.dma_half, self.m.dma_frames = 0, half, size
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

  def capture(self, directory, count=160, trigger_at=None, resume=False, paired=False,
              callback_id=None):
    directory.mkdir()
    (directory / 'preflight.log').write_text(self.burst())
    self.m.tick_us += 250
    assert self.mailbox(1, count)[-1] == 1
    if resume:
      assert self.mailbox(5)[-1] == 5
      self.posture(True, self.seed_patched)
    del self.samples[:]
    del self.irq[:]
    first_half = self.half
    observer, callback, dwt_irq = [], [], []
    callback_gate, irq_gate, injected, waits = [], [], [], []
    button = self.image.member('ui', 'Ui', 'seed_button_state_')
    locked = self.image.member('ui', 'Ui', 'seed_button_locked_')
    for i in range(count):
      if i == trigger_at:
        self.m.write(locked, '<B', 0)
        self.m.write(button, '<B', 1)
      if trigger_at is not None and i == trigger_at + 4:
        self.m.write(button, '<B', 0)
      scenario = tuple(PREEMPTION)[i % len(PREEMPTION)] if paired else 'none'
      self.m.preemption = PREEMPTION[scenario]
      self.m.enter_reads = 0
      self.m.preemption_wait = None
      self.block()
      assert not self.m.preemption and not self.m.preempt_pending, (scenario, self.m.enter_reads)
      if scenario != 'none':
        assert self.m.preemption_wait > 0
      injected.append(scenario)
      waits.append(self.m.preemption_wait)
      observer.append(self.m.read(self.image.address(OBSERVER), '<I')[0])
      callback.append(self.m.read(self.image.address(CALLBACK), '<I')[0])
      dwt_irq.append(self.word(IRQ))
      callback_gate.append(self.word('s1_callback_gate_cycles'))
      irq_gate.append(self.word('s1_irq_gate_cycles'))
    self.m.main_step()
    addr = self.image.address('s1_trace')
    header = TRACE_HEADER.unpack(self.m.uc.mem_read(addr, TRACE_HEADER.size))
    seal, = self.m.read(addr + SEAL_OFFSET, '<I')
    assert header[5] == count and header[8] == seal and seal % 2 == 0, header
    data = self.m.uc.mem_read(addr, TRACE_HEADER.size + count * FRAME_BYTES)
    if callback_id is not None:
      for i in range(count):
        offset = TRACE_HEADER.size + i * FRAME_BYTES + ROW_EVIDENCE_OFFSET
        evidence = ROW_EVIDENCE.unpack_from(data, offset)
        assert evidence == (callback_id, 1 + (first_half ^ (i & 1))), (i, evidence)
    (directory / 'callbacks.bin').write_bytes(data)
    (directory / 'output.s32').write_bytes(struct.pack(f'<{len(self.samples)}i', *self.samples))
    (directory / 'postflight.log').write_text(self.burst())
    assert 0 < max(callback) == self.word('s1_callback_peak')
    assert 0 < max(dwt_irq) == self.word('s1_irq_peak')
    assert all(c < d <= n for c, d, n in zip(callback, dwt_irq, self.irq))
    assert 0 < max(callback_gate) == self.word('s1_callback_gate_peak')
    assert max(callback_gate) < max(callback)
    assert max(callback_gate) < max(irq_gate) == self.word('s1_irq_gate_peak')
    (directory / 'instructions.json').write_text(json.dumps(dict(
        irq=self.irq, callback=callback, dwt_irq=dwt_irq, observer=observer,
        callback_gate=callback_gate, irq_gate=irq_gate, injected=injected, waits=waits,
        callback_gate_peak=max(callback_gate), irq_gate_peak=max(irq_gate))))
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
  for label, release in (('callback', r.callbacks), ('irq', r.irqs)):
    timing = counts[label + '_gate']
    assert len(timing) == len(release) == len(counts['injected']) == 64
    residual = [corrected - reference for corrected, reference in zip(timing, release)]
    results['release_' + label] = max(release)
    results['timing_' + label] = max(timing)
    results[label + '_residual'] = [min(residual), max(residual)]
    for scenario, charge in PREEMPTION.items():
      selected = [delta for delta, kind in zip(residual, counts['injected'])
                  if kind == scenario]
      assert min(selected) >= charge, (name, label, scenario, selected)
      results[label + '_' + scenario] = [min(selected), max(selected)]
  results['max_mask_wait_instructions'] = max(wait for wait in counts['waits']
                                              if wait is not None)
  (fixture / 'release-instructions.json').write_text(json.dumps(
      dict(callback=r.callbacks, irq=r.irqs)))
  print(json.dumps(results), flush=True)
  assert results['audio_match'], results


def replay_rc1(image, region, output, release):
  cases = (
      ('processor-48', 0, 3, 0, False, 0, 'Process', 48000),
      ('processor-32', 0, 2, 0, False, 0, 'Process', 32000),
      ('processor-24', 0, 0, 0, False, 0, 'Process', 24000),
      ('spectral-48', 1, 3, 0, False, 0, 'Process', 48000),
      ('synth-48', 0, 3, 0, True, 0, 'Process', 48000),
      ('synth-trim-48', 0, 3, 0, True, 1, 'ProcessSynthLevel', 48000),
      ('abyss-48', 3, 3, 0, False, 0, 'ProcessAbyss', 48000),
      ('abyss-32', 3, 2, 0, False, 0, 'ProcessAbyss', 32000),
      ('abyss-24', 3, 0, 0, False, 0, 'ProcessAbyss', 24000),
      ('oliverb-48', 2, 3, 0, False, 0, 'ProcessOliverb', 48000),
      ('oliverb-32', 2, 2, 0, False, 0, 'ProcessOliverb', 32000),
      ('oliverb-24', 2, 0, 0, False, 0, 'ProcessOliverb', 24000),
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
    r.capture(directory, count=64, paired=release is not None,
              callback_id=CALLBACK_IDS[callback])
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
      assert r.mailbox(8, 0)[-1] in (8, 9)
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
  te.MEMBERS['Ui'] += ('seed_button_state_', 'seed_button_locked_', 'processor_mode_',
                       'freeze_led_state_', 'mode_')
  te.MEMBERS['State'] = ('gate_output_mode',)
  image = te.Image(args.elf)
  region = args.region.read_bytes()

  release = te.Image(args.release) if args.release else None
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

  # Write request 6 after Poll read 0, while Flush publishes the seal.
  assert r.mailbox(1, 2)[-1] == 1
  r.block()
  r.block()
  request = image.address('s1_mailbox')
  seal = image.address('s1_trace') + SEAL_OFFSET
  def cancel_during_flush(uc, access, address, size, value, user):
    assert value and r.word('s1_mailbox') == 0
    r.m.write(request, '<I', 6)
  hook = r.m.uc.hook_add(te.unicorn.UC_HOOK_MEM_WRITE, cancel_during_flush,
                        begin=seal, end=seal)
  r.m.main_step()
  r.m.uc.hook_del(hook)
  assert r.word('s1_mailbox') == 6
  published_seal, = r.m.read(seal, '<I')
  r.m.main_step()
  assert r.m.read(request, '<5I')[::4] == (0, 2)
  assert r.m.read(seal, '<I') == (published_seal,)
  assert published_seal > 0

  for mode in (2, 3):
    r = Rig(image, region)
    r.configure(False)
    assert r.word('active_processor_mode') & 0xff == 0
    r.m.write(image.member('ui', 'Ui', 'processor_mode_'), '<B', mode)
    assert r.mailbox(8, 1)[-1] == 0xffffffff
    # DoEvents can commit settings before the next SysTick updates Ui.
    r.m.write(image.member('ui', 'Ui', 'processor_mode_'), '<B', 0)
    te.Rig.set_feature(r, te.FEATURE_PROCESSOR, mode)
    assert r.mailbox(8, 1)[-1] == 0xffffffff
  r = Rig(image, region)
  r.set_feature(te.FEATURE_PROCESSOR, 1)
  r.configure(False)
  r.tick(600)
  assert r.word('active_processor_mode') & 0xff == 1
  r.posture(True)  # The inputs are unpatched, but Spectral cannot self-play.
  assert r.mailbox(8, 0)[::4] == (0, 9)
  assert r.mailbox(8, 1)[::4] == (0, 0xffffffff)

  for member, value in (('freeze_led_state_', 1), ('mode_', 3)):
    r = Rig(image, region)
    r.configure(False)
    r.m.write(image.member('ui', 'Ui', member), '<B', value)
    assert r.mailbox(8, 1)[::4] == (0, 0xffffffff)
    assert r.mailbox(8, 0)[::4] == (0, 9)
  r = Rig(image, region)
  r.configure(False)
  r.m.write(image.address('codec_transition_frames'), '<i', 12000)
  assert r.mailbox(8, 1)[::4] == (0, 0xffffffff)
  print('Mailbox races and request 8 mode guards verified', flush=True)

  # The clear must neither consume the capture status nor schedule a 9.
  r = Rig(image, region)
  r.configure(False)
  assert r.mailbox(8, 1)[-1] in (8, 9)
  assert r.mailbox(1, 2)[-1] == 1
  assert r.mailbox(8, 0)[::4] == (0, 1)
  r.block()
  r.block()
  assert r.mailbox(8, 0)[::4] == (0, 2)
  assert r.mailbox(2, codec=48000, synth=48000)[-1] == 3
  r.m.main_step()
  assert r.m.read(request, '<5I')[::4] == (0, 3)

  # One late completion leaves only the opposite bit after acknowledgement.
  # A second, coalesced completion also leaves only that bit, but counts lost.
  for period, expected in ((BLOCK_CYCLES, 0), (BLOCK_CYCLES // 2, 2)):
    r = Rig(image, region)
    r.configure(False)
    r.m.dma_period = period
    assert r.mailbox(1, 2)[-1] == 1
    r.hold(1, 121200)
    status, = r.m.read(te.DMA1_LISR, '<I')
    assert status == (te.LISR_TC if r.half else te.LISR_HT)
    r.block()
    assert r.m.dma_events == (2 if expected else 1)
    r.m.main_step()
    address = image.address('s1_trace')
    header = TRACE_HEADER.unpack(r.m.uc.mem_read(address, TRACE_HEADER.size))
    assert header[5:7] == (2, expected), header
    for i in range(2):
      flags, = r.m.read(address + TRACE_HEADER.size + i * FRAME_BYTES + 15, '<B')
      assert flags & 1
  # Enter 375 us after TC with HT already pending. A short IRQ stays in
  # HT without losing TC. By the end of a 400 us IRQ, both flags have
  # recurred and NDTR is back in HT.
  lost = image.address('_ZN5beads9s1_timingL4lostE')
  for delay, events, expected in ((0, 1, 0), (178500, 3, 1)):
    r = Rig(image, region)
    r.configure(False)
    if not r.half:
      r.block()
    assert r.half == 1
    assert r.mailbox(1, 2)[-1] == 1
    r.m.preemption = delay
    r.block(dma_entry_age=3 * BLOCK_CYCLES // 2)
    assert r.irq[-1] < 2 * BLOCK_CYCLES
    assert r.m.dma_events == events and r.m.dma_half == 0
    assert r.m.read(te.DMA1_LISR, '<I') == (te.LISR_HT,)
    assert r.m.read(lost, '<B') == (expected,)
    r.block()
    r.m.main_step()
    header = TRACE_HEADER.unpack(r.m.uc.mem_read(image.address('s1_trace'), TRACE_HEADER.size))
    assert header[5:7] == (2, expected), header
  print('DMA pre-ack coalescence and late-entry lost count verified', flush=True)

  # The timing image excludes DWT_CALIBRATION. Use a Thumb tail call
  # in the emulator scratch page to exercise the fallback with live audio.
  r = Rig(image, region)
  r.configure(False)
  stub = te.STOP + 4
  process, = (addr for symbol, addr in image.sym.items() if symbol.startswith('_Z7ProcessPK'))
  r.m.write(stub, '<HHI', 0xf8df, 0xf000, process | 1)  # ldr.w pc, [pc, #0]
  r.m.write(r.callback, '<I', stub | 1)
  r.capture(args.output / 'other-callback', count=2, callback_id=6)
  print('Other callback identity verified on both halves', flush=True)

  replay_rc1(image, region, args.output, release)

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
    assert r.mailbox(8, 0)[::4] == (0, 9)
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
  r.request(8, 0, wanted=2)
  for _ in range(SETTLE_MS):
    r.tick(1)
    if not r.m.read(image.address('synth_mode'), '<B')[0]:
      break
  else:
    raise AssertionError('clearing the force did not leave synth mode')
  assert r.m.read(request, '<5I')[::4] == (0, 2)

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

  # The fault-cue counter is sampled in main, outside the release SysTick.
  r = Rig(image, region)
  r.configure(True)
  r.m.write(image.address('playable_synths'), '<B', 0)
  clock = image.address('_ZN6stmlib12system_clockE')
  for milliseconds, expected in ((125, 0), (250, 1), (251, 1), (375, 1), (500, 2)):
    r.m.write(clock, '<I', milliseconds)
    r.m.tick_us += 25000
    r.m.main_step()
    assert r.snapshot()[-2:] == (1, expected)
  print('Fault-cue counter verified outside SysTick', flush=True)

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
