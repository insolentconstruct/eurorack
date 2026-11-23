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
# Render the Rings synth blob and a separate vanilla build of Rings under
# Unicorn's Cortex-M7, scenario by scenario (rings_emu/harness.cc):
#   G2: block 24, every OUT and AUX word equal, the blob at two placements
#       that differ modulo 4 KiB, two FPSCR modes;
#   G2b: the same at block 12 (vanilla with only that constant changed);
#   G7: the blob writes only its RAM, the stack, its outputs and controls and
#       the generator's state; canaries around its RAM; no reconfiguration or
#       FX clear inside render;
#   G3: block 12 against vanilla block 24, a sound report and WAV pairs.
#
#   rings_compare.py OUT [JOBS]  (beads/test/rings_emu/build.sh's output)

import json
import math
import multiprocessing
import os
import struct
import sys

import unicorn
from unicorn import arm_const as A

FLASH, FLASH_SIZE = 0x08000000, 0x200000
DTCM, DTCM_SIZE = 0x20000000, 0x1000
RAM, RAM_SIZE = 0x24000000, 0x80000
STACK = RAM + RAM_SIZE - 0x100
STACK_LIMIT = STACK - 0x8000
BANK, BANK_SIZE = 0xC0000000, 0x48000
SYNTH_RAM = BANK + 0x1000
OUTPUT, OUTPUT_SIZE = 0xD0000000, 0x100000
STOP = 0x0FF00000
PLACEMENTS = (0x08080000, 0x08082FE0)
FPSCR = (0, 0x03000000)  # reset, and flush-to-zero with default NaN
RANDOM_STATE = 0x20000000
CANARY = b'\xa5' * 64
VANILLA, BLOB = 0, 1
NUM_SCENARIOS = 40
MODELS = ['Modal', 'Sympathetic strings', 'String', 'FM voice',
          'Sympathetic strings (quantized)', 'String and reverb']
FX = ['Formant', 'Chorus', 'Reverb', 'Formant 2', 'Ensemble', 'Reverb 2']
POLYPHONY = [1, 2, 4]
# Functions that only a reconfiguration or an FX change runs.
CONFIGURE_ONLY = ['_ZN5rings6String4InitEb', '_ZN5rings9Resonator4InitEv',
                  '_ZN5rings7FMVoice4InitEv']
MODEL_GAINS = '_ZN5rings4Part12model_gains_E'
# G3's second seed: the same notes with other noise bursts.
SEED_B = 0x5eed
LINK_BASE = 0x080c0000


def scenario_name(s):
  if s < 18:
    return '%s, polyphony %d' % (MODELS[s // 3], POLYPHONY[s % 3])
  if s < 36:
    return 'Disastrous peace %s, polyphony %d' % (FX[(s - 18) // 3],
                                                  POLYPHONY[s % 3])
  return ['Model and polyphony changes', 'Part and Disastrous peace '
          'alternating, FX changes', 'PITCH steps strum (SEED unpatched)',
          'Held: damping 1, long tail'][s - 36]


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

  def __init__(self, block, blob, base, fpscr):
    uc = unicorn.Uc(unicorn.UC_ARCH_ARM, unicorn.UC_MODE_THUMB)
    uc.ctl_set_cpu_model(A.UC_CPU_ARM_CORTEX_M7)
    if uc.ctl_get_cpu_model() != A.UC_CPU_ARM_CORTEX_M7:
      raise RuntimeError('unicorn did not select the Cortex-M7 model')
    for address, size in ((FLASH, FLASH_SIZE), (DTCM, DTCM_SIZE),
                          (RAM, RAM_SIZE), (BANK, BANK_SIZE),
                          (OUTPUT, OUTPUT_SIZE), (STOP, 0x1000)):
      uc.mem_map(address, size)
    uc.mem_write(FLASH, b'\xff' * FLASH_SIZE)
    for address, data in segments(CTX['elf'][block]):
      uc.mem_write(address, data)
    uc.mem_write(base, blob)
    self.ram_size, = struct.unpack_from('<I', blob, 32)
    uc.mem_write(SYNTH_RAM - len(CANARY), CANARY)
    uc.mem_write(SYNTH_RAM + self.ram_size, CANARY)
    # vmsr fpscr, r0; vmrs r1, fpscr
    uc.mem_write(STOP + 0x100, bytes.fromhex('e1ee100af1ee101a'))
    uc.reg_write(A.UC_ARM_REG_R0, fpscr)
    uc.emu_start((STOP + 0x100) | 1, STOP + 0x108)
    if uc.reg_read(A.UC_ARM_REG_R1) & 0x07c00000 != fpscr & 0x07c00000:
      raise RuntimeError('FPSCR did not take')
    self.uc = uc
    self.sym = CTX['sym'][block]
    self.base = base
    self.size = len(blob)

  def call(self, name, *args):
    uc = self.uc
    for i, a in enumerate(args[:4]):
      uc.reg_write(A.UC_ARM_REG_R0 + i, a & 0xffffffff)
    # Arguments after the fourth go on the stack (AAPCS), 8-byte aligned.
    sp = STACK - 8 * ((len(args[4:]) + 1) // 2)
    for i, a in enumerate(args[4:]):
      uc.mem_write(sp + 4 * i, struct.pack('<I', a & 0xffffffff))
    uc.reg_write(A.UC_ARM_REG_SP, sp)
    uc.reg_write(A.UC_ARM_REG_LR, STOP | 1)
    uc.emu_start(self.sym[name] | 1, STOP, count=4000000000)
    if uc.reg_read(A.UC_ARM_REG_PC) != STOP:
      raise RuntimeError('%s did not return' % name)
    return uc.reg_read(A.UC_ARM_REG_R0)

  def run(self, mode, scenario, seed=None):
    if seed is not None:
      self.uc.mem_write(self.sym['harness_seed'], struct.pack('<I', seed))
    n = self.call('harness_run', mode, scenario, self.base, SYNTH_RAM, OUTPUT)
    if not n:
      raise RuntimeError('scenario %d: a target was refused' % scenario)
    return bytes(self.uc.mem_read(OUTPUT, 8 * n))

  def canaries(self):
    return bytes(self.uc.mem_read(SYNTH_RAM - len(CANARY), len(CANARY))) + \
        bytes(self.uc.mem_read(SYNTH_RAM + self.ram_size, len(CANARY))) == \
        CANARY * 2


class Guard(object):
  """Writes by blob code outside its RAM, the stack, its outputs, the
  controls and the generator's state; and calls that only a configure may
  make, by entry."""

  def __init__(self, m, block):
    sym = m.sym
    self.allowed = [
        (SYNTH_RAM, SYNTH_RAM + m.ram_size), (STACK_LIMIT, STACK),
        (RANDOM_STATE, RANDOM_STATE + 4),
        (sym['harness_controls'], sym['harness_controls'] + 36),
        (sym['harness_out'], sym['harness_out'] + 4 * block),
        (sym['harness_aux'], sym['harness_aux'] + 4 * block)]
    self.begin, self.end = m.base, m.base + m.size
    self.violations = []
    self.entry = None
    self.calls = {}
    # Rings' reverb buffer is the core's last member: a clear writes it all.
    self.reverb = (SYNTH_RAM + m.ram_size - 65536, SYNTH_RAM + m.ram_size)
    self.written = 0
    self.most = {}
    m.uc.hook_add(unicorn.UC_HOOK_MEM_WRITE, self.write)
    header = struct.unpack_from('<24I', bytes(m.uc.mem_read(m.base, 96)))
    entries = {'init': header[10], 'configure': header[11],
               'render': header[12]}
    blob_sym = CTX['blob_sym'][block]
    targets = dict((name, blob_sym[name]) for name in CONFIGURE_ONLY)
    for name, offset in entries.items():
      address = m.base + (offset & ~1)
      m.uc.hook_add(unicorn.UC_HOOK_CODE, self.enter, name, address, address)
    for name, address in targets.items():
      address = m.base + (address & ~1) - LINK_BASE
      m.uc.hook_add(unicorn.UC_HOOK_CODE, self.reach, name, address, address)

  def write(self, uc, access, address, size, value, user):
    pc = uc.reg_read(A.UC_ARM_REG_PC)
    if not self.begin <= pc < self.end:
      return
    if self.reverb[0] <= address < self.reverb[1]:
      self.written += size
    if not any(lo <= address and address + size <= hi
               for lo, hi in self.allowed):
      self.violations.append('pc 0x%08x wrote %d B at 0x%08x' % (
          pc, size, address))

  def enter(self, uc, address, size, name):
    self.close()
    self.entry = name

  def close(self):
    """Ends a call: the most reverb bytes one call of its entry wrote."""
    if self.entry:
      self.most[self.entry] = max(self.most.get(self.entry, 0), self.written)
    self.written = 0

  def reach(self, uc, address, size, name):
    key = '%s in %s' % (name, self.entry)
    self.calls[key] = self.calls.get(key, 0) + 1


CTX = {}


def load(out):
  CTX['out'] = out
  CTX['elf'] = {b: '%s/harness%d.elf' % (out, b) for b in (24, 12)}
  CTX['sym'] = {b: symbols('%s/harness%d.nm' % (out, b)) for b in (24, 12)}
  CTX['blob'] = {24: open(out + '/rings_synth_24.bin', 'rb').read(),
                 12: open(out + '/rings_synth.bin', 'rb').read()}
  CTX['blob_sym'] = {24: symbols(out + '/rings_synth_24.nm'),
                     12: symbols(out + '/rings_synth.nm')}


def differ(a, b):
  wa = struct.unpack('<%dI' % (len(a) // 4), a)
  wb = struct.unpack('<%dI' % (len(b) // 4), b)
  return sum(1 for x, y in zip(wa, wb) if x != y) + abs(len(wa) - len(wb))


def peak(data):
  return max(abs(x) for x in struct.unpack('<%df' % (len(data) // 4), data))


def identity_job(job):
  """G2 or G2b: one scenario, one FPSCR mode, vanilla and both
  placements."""
  block, scenario, fpscr = job
  blob = CTX['blob'][block]
  reference = Machine(block, blob, PLACEMENTS[0], fpscr).run(VANILLA, scenario)
  result = {'block': block, 'scenario': scenario, 'fpscr': fpscr,
            'samples': len(reference) // 4, 'differ': [],
            'peak': peak(reference)}
  for base in PLACEMENTS:
    m = Machine(block, blob, base, fpscr)
    rendered = m.run(BLOB, scenario)
    result['differ'].append(differ(reference, rendered))
    if not m.canaries():
      result['canaries'] = 'overwritten at 0x%08x' % base
    if fpscr == 0 and base == PLACEMENTS[0]:
      result['blob'] = rendered
  if fpscr == 0:
    result['reference'] = reference
    if block == 24:
      result['reference_b'] = Machine(block, blob, PLACEMENTS[0], 0).run(
          VANILLA, scenario, SEED_B)
  return result


def guard_job(job):
  block, scenario = job
  m = Machine(block, CTX['blob'][block], PLACEMENTS[1], 0)
  g = Guard(m, block)
  m.run(BLOB, scenario)
  g.close()
  return {'block': block, 'scenario': scenario,
          'violations': g.violations[:8], 'calls': g.calls,
          'reverb_bytes': g.most, 'canaries': m.canaries()}


def control_job(job):
  """A perturbed constant (Part's first model gain) must change the
  output."""
  block = job
  blob = bytearray(CTX['blob'][block])
  offset = CTX['blob_sym'][block][MODEL_GAINS] - LINK_BASE
  gain, = struct.unpack_from('<f', blob, offset)
  word, = struct.unpack_from('<I', blob, offset)
  struct.pack_into('<I', blob, offset, word + 1)
  # The CRC is not checked here: the harness calls the entries directly.
  reference = Machine(block, bytes(blob), PLACEMENTS[0], 0).run(VANILLA, 0)
  rendered = Machine(block, bytes(blob), PLACEMENTS[0], 0).run(BLOB, 0)
  return {'block': block, 'gain': gain, 'differ': differ(reference, rendered)}


def work(job):
  """A worker's emulator fault comes back as text: UcError does not
  pickle."""
  kind, arguments = job
  try:
    return JOBS[kind](arguments)
  except Exception as e:  # pylint: disable=broad-except
    return {'error': '%s %r: %r' % (kind, arguments, e)}


JOBS = {'identity': identity_job, 'guard': guard_job, 'control': control_job}


def rms(values):
  return math.sqrt(sum(v * v for v in values) / max(1, len(values)))


def wav(path, data):
  """32-bit float stereo WAV at 48 kHz."""
  frames = len(data) // 8
  header = struct.pack('<4sI4s4sIHHIIHH4sI', b'RIFF', 36 + len(data),
                       b'WAVE', b'fmt ', 16, 3, 2, 48000, 48000 * 8, 8, 32,
                       b'data', len(data))
  open(path, 'wb').write(header + data)
  return frames


def sound_report(reference, rendered, second, scenario, out):
  r = struct.unpack('<%df' % (len(reference) // 4), reference)
  b = struct.unpack('<%df' % (len(rendered) // 4), rendered)
  d = [x - y for x, y in zip(r, b)]
  # The floor: vanilla block 24 against itself with other noise bursts.
  r2 = struct.unpack('<%df' % (len(second) // 4), second)
  seed_rms = rms([x - y for x, y in zip(r, r2)])
  first = next((i // 2 for i, v in enumerate(d) if v), None)
  ref_rms = rms(r)
  diff_rms = rms(d)
  stem = 'g3-%02d' % scenario
  # WAV pairs for polyphony 1 and the transitions.
  wavs = []
  if scenario >= 36 or scenario % 3 == 0:
    wavs = [stem + '-vanilla24.wav', stem + '-blob12.wav',
            stem + '-vanilla24-seed2.wav']
    wav('%s/g3/%s' % (out, wavs[0]), reference)
    wav('%s/g3/%s' % (out, wavs[1]), rendered)
    wav('%s/g3/%s' % (out, wavs[2]), second)
  return {
      'scenario': scenario, 'name': scenario_name(scenario),
      'reference_rms': ref_rms, 'difference_rms': diff_rms,
      'relative': diff_rms / ref_rms if ref_rms else None,
      'relative_db': 20 * math.log10(diff_rms / ref_rms)
      if ref_rms and diff_rms else None,
      'seed_relative': seed_rms / ref_rms if ref_rms else None,
      'reference_peak': max(abs(x) for x in r),
      'difference_peak': max(abs(x) for x in d),
      'first_difference': first, 'wavs': wavs}


def main(argv):
  out = argv[1]
  jobs = int(argv[2]) if len(argv) > 2 else multiprocessing.cpu_count()
  load(out)
  os.makedirs(out + '/g3', exist_ok=True)
  failures = []
  results = {'unicorn': unicorn.__version__, 'placements': [
      '0x%08x' % p for p in PLACEMENTS], 'fpscr': ['0x%08x' % f for f in FPSCR],
      'blobs': {}, 'identity': [], 'guards': [], 'controls': [], 'g3': []}
  for block in (24, 12):
    header = struct.unpack_from('<24I', CTX['blob'][block])
    results['blobs'][block] = {'size': len(CTX['blob'][block]),
                               'ram_size': header[8],
                               'controls_size': header[18],
                               'block_size': header[19]}
    if header[19] != block or header[4] != 3:
      failures.append('block %d: the blob header says kind %d, block %d' %
                      (block, header[4], header[19]))
  pool = multiprocessing.Pool(jobs, load, (out,))
  identity = [(block, s, f) for block in (24, 12)
              for s in range(NUM_SCENARIOS) for f in FPSCR]
  references = {}
  seconds = {}
  blobs12 = {}
  for r in pool.imap_unordered(work, [('identity', j) for j in identity]):
    if 'error' in r:
      failures.append(r['error'])
      continue
    if 'reference' in r and r['block'] == 24:
      references[r['scenario']] = r.pop('reference')
      seconds[r['scenario']] = r.pop('reference_b')
    if 'blob' in r and r['block'] == 12:
      blobs12[r['scenario']] = r.pop('blob')
    r.pop('reference', None)
    r.pop('blob', None)
    results['identity'].append(r)
    label = 'G2 ' if r['block'] == 24 else 'G2b'
    print('%s block %d fpscr 0x%08x %-50s differ %s of %d, peak %.3f' % (
        label, r['block'], r['fpscr'], scenario_name(r['scenario']),
        r['differ'], r['samples'], r['peak']))
    sys.stdout.flush()
    if any(r['differ']):
      failures.append('%s scenario %d fpscr 0x%x: %s words differ' % (
          label, r['scenario'], r['fpscr'], r['differ']))
    if not r['peak']:
      failures.append('scenario %d is silent' % r['scenario'])
    if 'canaries' in r:
      failures.append('scenario %d: canaries %s' % (r['scenario'],
                                                    r['canaries']))
  for r in pool.imap(work, [('control', b) for b in (24, 12)]):
    if 'error' in r:
      failures.append(r['error'])
      continue
    results['controls'].append(r)
    print('control: block %d, model gain %.7f nudged by one ulp: %d words '
          'differ' % (r['block'], r['gain'], r['differ']))
    if not r['differ']:
      failures.append('block %d: the perturbed gain changed nothing' %
                      r['block'])
  # Every scenario at Beads' block size; the changes also at Rings' own.
  guards = [(12, s) for s in range(NUM_SCENARIOS)] + \
      [(24, s) for s in (36, 37, 38, 39)]
  for r in pool.imap(work, [('guard', g) for g in guards]):
    if 'error' in r:
      failures.append(r['error'])
      continue
    results['guards'].append(r)
    print('G7 block %d %s: %d stray writes, canaries %s, calls %s, most '
          'reverb bytes per call %s' % (
              r['block'], scenario_name(r['scenario']), len(r['violations']),
              'intact' if r['canaries'] else 'OVERWRITTEN', r['calls'],
              r['reverb_bytes']))
    for v in r['violations']:
      failures.append('G7 block %d scenario %d: %s' % (r['block'],
                                                       r['scenario'], v))
    if not r['canaries']:
      failures.append('G7 scenario %d: canaries overwritten' % r['scenario'])
    for key in r['calls']:
      if key.endswith(' in render'):
        failures.append('G7 block %d scenario %d: %s' % (
            r['block'], r['scenario'], key))
    if r['reverb_bytes'].get('render', 0) >= 32768:
      failures.append('G7 block %d scenario %d: a render cleared the FX '
                      'buffer' % (r['block'], r['scenario']))
  # The guard can fail: configures did reconfigure and clear.
  calls = set(k for r in results['guards'] for k in r['calls'])
  for key in ('_ZN5rings6String4InitEb in configure',
              '_ZN5rings7FMVoice4InitEv in configure'):
    if key not in calls:
      failures.append('G7: no %s seen: the hooks see nothing' % key)
  if not any(r['reverb_bytes'].get('configure', 0) >= 65536
             for r in results['guards']):
    failures.append('G7: no FX clear seen in a configure')
  pool.close()

  for s in range(NUM_SCENARIOS):
    if s in references and s in blobs12:
      results['g3'].append(sound_report(references[s], blobs12[s],
                                        seconds[s], s, out))
  json.dump(results, open(out + '/rings-compare.json', 'w'), indent=1,
            sort_keys=True)
  print('\nG3: block 12 against vanilla block 24 (difference RMS / '
        'vanilla RMS; then vanilla 24 against itself with another seed)')
  for r in results['g3']:
    print('  %-50s %8.4f %7s dB  seed %6.4f  peak diff %.3f of %.3f, from '
          'sample %s' % (
              r['name'], r['relative'] or 0.0,
              '%.1f' % r['relative_db'] if r['relative_db'] is not None
              else '-', r['seed_relative'] or 0.0, r['difference_peak'],
              r['reference_peak'], r['first_difference']))
  for f in failures:
    print('ERROR: ' + f)
  if not failures:
    print('rings: G2, G2b and G7 pass')
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
