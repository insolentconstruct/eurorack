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
# Compare libm copies with the pinned archive across float bit patterns.

import json
import os
import random
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'blob_emu'))
from render_compare import Machine, symbols, STACK, STOP, A


def inputs():
  words = [0, 0x80000000, 0x7f800000, 0xff800000,
           0x7fc00000, 0xffc00000, 0x7f800001, 0xff800001]
  for exponent in range(256):
    for mantissa in (0, 1, 0x3fffff, 0x7ffffe, 0x7fffff):
      for sign in (0, 0x80000000):
        words.append(sign | (exponent << 23) | mantissa)
  for i in range(-8192, 8193):
    words.append(struct.unpack('<I', struct.pack('<f', i / 256.0))[0])
  rng = random.Random(20261123)
  words.extend(rng.getrandbits(32) for _ in range(65536))
  return words


def run(folder):
  sym = symbols(os.path.join(folder, 'harness.nm'))
  elf = os.path.join(folder, 'harness.elf')
  blob = open(os.path.join(folder, 'libm_probe.bin'), 'rb').read()
  render, = struct.unpack_from('<I', blob, 48)
  words = inputs()
  results = []
  for base in (0x080c0000, 0x080d0fe0):
    for fpscr in (0, 0x03000000):
      ref = Machine(elf, sym, b'', base, fpscr)
      test = Machine(elf, sym, blob, base, fpscr)
      mismatches = []
      flags = []
      for machine in (ref, test):
        machine.uc.mem_write(sym['diagnostics'], struct.pack('<I', fpscr))
      for start in range(0, len(words), 128):
        chunk = words[start:start + 128]
        data = struct.pack('<%dI' % len(chunk), *chunk)
        ref.uc.mem_write(sym['input'], data)
        test.uc.mem_write(sym['out'], data)
        ref.call('reference', len(chunk))
        uc = test.uc
        for reg in (A.UC_ARM_REG_R0, A.UC_ARM_REG_R1,
                    A.UC_ARM_REG_R2):
          uc.reg_write(reg, 0)
        uc.reg_write(A.UC_ARM_REG_R0, sym['diagnostics'])
        uc.reg_write(A.UC_ARM_REG_R3, sym['out'])
        uc.reg_write(A.UC_ARM_REG_SP, STACK)
        uc.reg_write(A.UC_ARM_REG_LR, STOP | 1)
        uc.mem_write(STACK, struct.pack('<3I', sym['aux'], len(chunk), 0))
        uc.emu_start((base + render) | 1, STOP, count=50000000)
        if uc.reg_read(A.UC_ARM_REG_PC) != STOP:
          raise RuntimeError('probe did not return')
        for name, address in [('out', sym['out']), ('aux', sym['aux']),
                              ('expm1', sym['diagnostics'] + 4)]:
          expected = bytes(ref.uc.mem_read(address, len(data)))
          actual = bytes(uc.mem_read(address, len(data)))
          if expected != actual:
            for i, word in enumerate(chunk):
              a = expected[4*i:4*i+4]
              b = actual[4*i:4*i+4]
              if a != b and len(mismatches) < 20:
                mismatches.append([name, hex(word), a.hex(), b.hex()])
        for function in range(3):
          address = sym['diagnostics'] + 4 + 128 * 4 * (function + 1)
          expected = bytes(ref.uc.mem_read(address, len(data)))
          actual = bytes(uc.mem_read(address, len(data)))
          if expected != actual:
            for i, word in enumerate(chunk):
              a = expected[4*i:4*i+4]
              b = actual[4*i:4*i+4]
              if a != b and len(flags) < 20:
                flags.append([function, hex(word), a.hex(), b.hex()])
      result = dict(base=hex(base), fpscr=hex(fpscr), inputs=len(words),
                    mismatches=mismatches, flags=flags)
      results.append(result)
      print(json.dumps(result), flush=True)
  with open(os.path.join(folder, 'libm-results.json'), 'w') as output:
    json.dump(results, output, indent=2)
  return 1 if any(r['mismatches'] or r['flags'] for r in results) else 0


if __name__ == '__main__':
  sys.exit(run(sys.argv[1]))
