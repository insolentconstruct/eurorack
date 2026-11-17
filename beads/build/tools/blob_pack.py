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
# Gate and pack one Plaits blob (beads/blob/plaits_blob.h).
#
#   blob_pack.py SERVICES_LD OBJDUMP OUT.bin BASE:ELF [BASE:ELF...]
#
# Each ELF is the same objects linked with beads/blob/blob.ld and
# --emit-relocs at BASE. Every ELF must pass the gate (allowlists, not
# blocklists) and pack to the same bytes; otherwise nothing is written.

import hashlib
import json
import re
import struct
import subprocess
import sys
import zlib

HEADER_SIZE = 64
MAGIC = 0x424d5a44
MAX_ALIGNMENT = 32
SHF_ALLOC = 2
SHT_SYMTAB = 2
SHT_REL = 9
SHN_UNDEF = 0
SHN_ABS = 0xfff1

# Position-independent relocations applied to the blob: PC-relative branches,
# calls and addresses, and GOT entries addressed from a PC-relative GOT base.
ALLOWED_RELOCATIONS = {
  3: 'R_ARM_REL32',
  10: 'R_ARM_THM_CALL',
  11: 'R_ARM_THM_PC8',
  25: 'R_ARM_BASE_PREL',
  26: 'R_ARM_GOT_BREL',
  30: 'R_ARM_THM_JUMP24',
  51: 'R_ARM_THM_JUMP19',
  96: 'R_ARM_GOT_PREL',
  102: 'R_ARM_THM_JUMP11',
  103: 'R_ARM_THM_JUMP8',
  105: 'R_ARM_THM_PC12',
}

# Non-allocated sections that never reach the blob's bytes.
ALLOWED_INFO = re.compile(
    r'^(|\.symtab|\.strtab|\.shstrtab|\.ARM\.attributes|\.rel\.blob|'
    r'(\.rel)?\.debug\w*)$')

# Branches through a register other than a return (virtual calls, function
# pointers), with any condition. TBB and TBH stay: their tables are relative.
BRANCH = re.compile(
    r'^(bx|blx)(eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le|al)?'
    r'(\.n|\.w)?$')
RETURN = re.compile(r'^pc, \[sp\], #4')


def indirect(line):
  fields = line.split('\t')
  if len(fields) < 4 or not fields[0].strip().endswith(':'):
    return False
  mnemonic = fields[2].strip()
  operands = fields[3].strip()
  if BRANCH.match(mnemonic):
    return operands != 'lr'
  if 'pc}' in operands:
    return not operands.startswith('sp!')
  return operands.split(',')[0] == 'pc' and not RETURN.match(operands)


class Elf(object):

  def __init__(self, path):
    self.data = open(path, 'rb').read()
    d = self.data
    if d[:4] != b'\x7fELF' or d[4] != 1 or d[5] != 1:
      raise ValueError('%s: not a little-endian ELF32 file' % path)
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    self.sections = []
    for i in range(shnum):
      self.sections.append(struct.unpack_from(
          '<IIIIIIIIII', d, shoff + i * shentsize))
    names = self.sections[shstrndx]
    self.names = [self.String(names[4], s[0]) for s in self.sections]

  def String(self, offset, index):
    end = self.data.index(b'\0', offset + index)
    return self.data[offset + index:end].decode()

  def Section(self, name):
    return self.sections[self.names.index(name)]

  def Bytes(self, section):
    return self.data[section[4]:section[4] + section[5]]

  def Symbols(self):
    symtab = [s for s in self.sections if s[1] == SHT_SYMTAB][0]
    strtab = self.sections[symtab[6]]
    symbols = {}
    for i in range(symtab[5] // 16):
      name, value, size, info, other, shndx = struct.unpack_from(
          '<IIIBBH', self.data, symtab[4] + i * 16)
      symbols.setdefault(self.String(strtab[4], name), []).append(
          (value, shndx, info))
    return symbols


def parse_services(path):
  exports = {}
  for line in open(path):
    m = re.match(r'^(PLAITS_BLOB_\w+)\s*=\s*(0x[0-9a-fA-F]+);', line)
    if m:
      exports[m.group(1)] = int(m.group(2), 16)
  return exports


def gate(elf, base, exports, objdump, path):
  failures = []
  blob = elf.Section('.blob')
  if blob[3] != base:
    failures.append('.blob at 0x%08x, not 0x%08x' % (blob[3], base))
  if blob[8] > MAX_ALIGNMENT:
    failures.append('.blob needs %d-byte alignment' % blob[8])
  for name, s in zip(elf.names, elf.sections):
    if name == '.blob':
      continue
    if s[2] & SHF_ALLOC or not ALLOWED_INFO.match(name):
      failures.append('section %s (flags 0x%x, %d B)' % (name, s[2], s[5]))

  symbols = elf.Symbols()
  import_values = set(v for k, v in exports.items() if k not in (
      'PLAITS_BLOB_SERVICES', 'PLAITS_BLOB_SERVICES_END'))
  for name, entries in symbols.items():
    for value, shndx, info in entries:
      if shndx == SHN_UNDEF and name:
        failures.append('undefined symbol %s' % name)
      elif shndx == SHN_ABS and info & 0xf in (1, 2) and \
          value not in import_values:
        failures.append('absolute symbol %s = 0x%08x' % (name, value))

  kinds = {}
  # --emit-relocs keeps every relocation; a blob may need none of its own.
  if not any(s[1] == SHT_REL for s in elf.sections):
    failures.append('no relocation sections: link with --emit-relocs')
  for name, s in zip(elf.names, elf.sections):
    if s[1] != SHT_REL or name != '.rel.blob':
      continue
    for i in range(s[5] // 8):
      offset, info = struct.unpack_from('<II', elf.data, s[4] + i * 8)
      kind = info & 0xff
      label = ALLOWED_RELOCATIONS.get(kind, 'type %d' % kind)
      kinds[label] = kinds.get(label, 0) + 1
      if kind not in ALLOWED_RELOCATIONS:
        failures.append('relocation %s at 0x%08x' % (label, offset))

  got = symbols['plaits_blob_got'][0][0]
  got_end = symbols['plaits_blob_got_end'][0][0]
  data = elf.Bytes(blob)
  # The reserved words of a static link's .got.plt stay zero.
  reserved = symbols['plaits_blob_got_plt'][0][0]
  if any(data[reserved - base:got - base]):
    failures.append('nonzero reserved GOT words')
  imports = []
  for address in range(got, got_end, 4):
    value, = struct.unpack_from('<I', data, address - base)
    imports.append(value)
    if value not in import_values:
      failures.append('GOT entry 0x%08x is not an export' % value)

  listing = subprocess.check_output(
      [objdump, '-d', '-j', '.blob', path]).decode()
  for line in listing.splitlines():
    if indirect(line):
      failures.append('indirect branch: ' + line.strip())
  return failures, kinds, imports


def pack(elf, base):
  symbols = elf.Symbols()
  data = bytearray(elf.Bytes(elf.Section('.blob')))
  words = list(struct.unpack_from('<16I', data, 0))
  if words[0] != MAGIC or words[1] != HEADER_SIZE:
    raise ValueError('the header is not first in .blob')
  def entry(name):
    return symbols[name][0][0] - base
  got = symbols['plaits_blob_got'][0][0] - base
  got_end = symbols['plaits_blob_got_end'][0][0] - base
  words[5] = len(data) + 4
  words[8] = entry('plaits_blob_init')
  words[9] = entry('plaits_blob_reset')
  words[10] = entry('plaits_blob_render')
  words[11] = got
  words[12] = got_end - got
  for offset in words[8:11]:
    if not offset & 1:
      raise ValueError('an entry point is not Thumb code')
  struct.pack_into('<13I', data, 0, *words[:13])
  data += struct.pack('<I', zlib.crc32(bytes(data)) & 0xffffffff)
  return bytes(data)


def main(argv):
  exports = parse_services(argv[1])
  objdump = argv[2]
  out = argv[3]
  failures = []
  packed = {}
  evidence = {'links': []}
  for spec in argv[4:]:
    base_text, path = spec.split(':', 1)
    base = int(base_text, 16)
    elf = Elf(path)
    f, kinds, imports = gate(elf, base, exports, objdump, path)
    failures += ['%s: %s' % (path, x) for x in f]
    data = pack(elf, base)
    packed[base] = data
    # A secondary check: a word equal to an address inside the blob.
    inside = [o for o in range(0, len(data) - 3, 2)
        if base <= struct.unpack_from('<I', data, o)[0] < base + len(data)]
    if inside:
      failures.append('%s: blob addresses at offsets %s' % (path, inside[:8]))
    evidence['links'].append({
        'base': '0x%08x' % base,
        'sha256': hashlib.sha256(data).hexdigest(),
        'relocations': kinds,
        'imports': ['0x%08x' % v for v in imports]})
  if len(set(packed.values())) != 1:
    failures.append('the packed bytes differ between bases')
  if len(packed) < 3:
    failures.append('link at three bases at least')
  for f in failures:
    print('GATE: ' + f, file=sys.stderr)
  if failures:
    return 1
  data = list(packed.values())[0]
  open(out, 'wb').write(data)
  words = struct.unpack_from('<13I', data, 0)
  evidence.update({
      'size': len(data),
      'sha256': hashlib.sha256(data).hexdigest(),
      'model': words[4],
      'state_size': words[6],
      'arena_size': words[7],
      'entries': ['0x%x' % w for w in words[8:11]],
      'got': ['0x%x' % words[11], words[12]]})
  json.dump(evidence, open(out + '.json', 'w'), indent=1, sort_keys=True)
  print('%s %d B sha256 %s' % (out, len(data), evidence['sha256']))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
