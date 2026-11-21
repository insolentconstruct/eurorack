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
#   blob_pack.py SERVICES_V1_LD SERVICES_V2_LD OBJDUMP OUT.bin
#       [--library LIBRARY.bin]... BASE:ELF...
#
# Each ELF is the same objects linked with beads/blob/blob.ld and
# --emit-relocs at BASE. Every ELF must pass the gate (allowlists, not
# blocklists) and pack to the same bytes; otherwise nothing is written.
# Version 1 and 2 headers; a blob imports only its services version's
# exports. A library's export table becomes offsets after the GOT; each
# --library becomes a dependency record (ID and SHA-256) before the CRC.

import hashlib
import json
import re
import struct
import subprocess
import sys
import zlib

HEADER_SIZES = {1: 64, 2: 96}
MAGIC = 0x424d5a44
MAX_STATE = {1: 256, 2: 4352}
MAX_ARENA = 16384
KIND_MODEL = 1
KIND_LIBRARY = 2
MAX_LIBRARY_RAM = 65536
MAX_EXPORTS = 16
MAX_DEPENDENCIES = 4
R_ARM_ABS32 = 2
R_ARM_REL32 = 3
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
    r'\.rel\.blob_exports|(\.rel)?\.debug\w*)$')

# A call to a library: a function named plaits_blob_import_* that loads an
# export from the library record in r0 (PlaitsBlobLibrary) and tail-calls
# it. The gate's one exception to "no branch through a register".
IMPORT = 'plaits_blob_import_'
IMPORT_LOAD = re.compile(r'^ldr(\.w)?$')
IMPORT_OPERANDS = re.compile(r'^(r[0-9]|r1[0-2]|ip), \[r0, #(\d+)\]')
FUNCTION = re.compile(r'^[0-9a-f]+ <([^>]+)>:$')

# The libc functions a blob may copy in: no state, no errno.
LIBC = ('memcpy', 'memmove', 'memset')
ARCHIVE_MEMBER = re.compile(r'^(\S+\.a)\((\S+)\)$')

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
    return not (mnemonic.startswith('bx') and operands == 'lr')
  if 'pc}' in operands:
    return not (mnemonic.startswith('pop') or operands.startswith('sp!'))
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

  def SymbolValue(self, index):
    symtab = [s for s in self.sections if s[1] == SHT_SYMTAB][0]
    return struct.unpack_from('<I', self.data, symtab[4] + index * 16 + 4)[0]


def parse_services(path):
  exports = {}
  for line in open(path):
    m = re.match(r'^(PLAITS_BLOB_\w+)\s*=\s*(0x[0-9a-fA-F]+);', line)
    if m and 'SERVICES' not in m.group(1):
      exports[m.group(1)] = int(m.group(2), 16)
  return exports


def header_version(elf):
  data = elf.Bytes(elf.Section('.blob'))
  magic, size, abi, services = struct.unpack_from('<4I', data, 0)
  if magic != MAGIC or HEADER_SIZES.get(abi) != size:
    raise ValueError('the header is not first in .blob')
  kind = struct.unpack_from('<I', data, 16)[0] if abi == 2 else KIND_MODEL
  return abi, services, kind


def check_archives(map_path, failures):
  """Archive members only for the allowed libc functions, as the link map
  records why each was included."""
  lines = open(map_path).read().split('\n')
  try:
    start = lines.index('Archive member included to satisfy reference by '
                        'file (symbol)')
  except ValueError:
    return
  for i in range(start + 1, len(lines)):
    if lines[i].startswith(('Discarded', 'Memory', 'Allocating')):
      break
    m = ARCHIVE_MEMBER.match(lines[i].strip())
    if not m:
      continue
    reason = lines[i + 1].strip() if i + 1 < len(lines) else ''
    symbol = re.search(r'\((\w+)\)$', reason)
    if not m.group(1).endswith('/libc.a') or not symbol or \
        symbol.group(1) not in LIBC:
      failures.append('archive member %s for %s' % (
          m.group(2), symbol.group(1) if symbol else reason))


def library_record(path):
  """A dependency on the library blob at path: its ID and SHA-256."""
  data = open(path, 'rb').read()
  magic, size, abi, _, kind, library = struct.unpack_from('<6I', data, 0)
  if magic != MAGIC or abi != 2 or size != 96 or kind != KIND_LIBRARY or \
      not 0 < library < 256:
    raise ValueError('%s is not a library blob' % path)
  return struct.pack('<I', library) + hashlib.sha256(data).digest()


def check_imports(listing, failures):
  """Import functions keep their exact shape and are entered only at their
  start; returns how many there are."""
  functions = {}
  name = None
  for line in listing.splitlines():
    m = FUNCTION.match(line.strip())
    if m:
      name = m.group(1)
      functions[name] = []
      continue
    fields = line.split('\t')
    if name is None or len(fields) < 3 or \
        not fields[0].strip().endswith(':'):
      continue
    functions[name].append((fields[2].strip(),
                            fields[3].strip() if len(fields) > 3 else ''))
    target = re.search(r'<(%s\w*)\+0x[0-9a-f]+>' % IMPORT, line)
    if target:
      failures.append('a branch into the middle of %s' % target.group(1))
  count = 0
  for name, code in functions.items():
    if not name.startswith(IMPORT):
      continue
    count += 1
    # Zero padding after the tail call is never reached.
    while code and code[-1] == ('movs', 'r0, r0'):
      code.pop()
    if not import_shape(code):
      failures.append('import %s is not one load from the library record '
                      'in r0 and a tail call: %s' % (name, code))
  return count


def import_shape(code):
  """ldr rX, [r0, #k]; bx rX. With arguments on the stack GCC 9 borrows a
  register: push {rX}; ldr rX, [r0, #k]; mov ip, rX; ldr.w rX, [sp], #4;
  bx ip."""
  if len(code) == 5:
    saved = re.match(r'^\{(r[4-9]|r1[01])\}$', code[0][1])
    if code[0][0] != 'push' or not saved:
      return False
    register = saved.group(1)
    if code[2] != ('mov', 'ip, ' + register) or \
        code[3] != ('ldr.w', register + ', [sp], #4') or \
        code[4] != ('bx', 'ip'):
      return False
    load, call = code[1], ('bx', register)
  elif len(code) == 2:
    load, call = code
  else:
    return False
  operands = IMPORT_OPERANDS.match(load[1])
  if not operands or not IMPORT_LOAD.match(load[0]) or \
      (len(code) == 2 and call != ('bx', operands.group(1))) or \
      (len(code) == 5 and operands.group(1) != call[1]):
    return False
  offset = int(operands.group(2))
  return 4 <= offset < 4 + 4 * MAX_EXPORTS and offset % 4 == 0


def gate(elf, base, exports, objdump, path, num_libraries):
  failures = []
  blob = elf.Section('.blob')
  abi, services, kind = header_version(elf)
  library = kind == KIND_LIBRARY
  if blob[3] != base:
    failures.append('.blob at 0x%08x, not 0x%08x' % (blob[3], base))
  if blob[8] > MAX_ALIGNMENT:
    failures.append('.blob needs %d-byte alignment' % blob[8])
  for name, s in zip(elf.names, elf.sections):
    if name == '.blob' or (library and name == '.blob_exports'):
      continue
    if s[2] & SHF_ALLOC or not ALLOWED_INFO.match(name):
      failures.append('section %s (flags 0x%x, %d B)' % (name, s[2], s[5]))
  if kind not in (KIND_MODEL, KIND_LIBRARY):
    failures.append('kind %d is neither a model nor a library' % kind)

  symbols = elf.Symbols()
  if abi == 1 and services != 1:
    failures.append('a version 1 header names services version %d' %
                    services)
  # Every export is a valid absolute symbol; only the blob's version imports.
  all_values = set(v for e in exports.values() for v in e.values())
  import_values = set(exports.get(services, {}).values())
  if not import_values:
    failures.append('services version %d is not known' % services)
  for name, entries in symbols.items():
    for value, shndx, info in entries:
      if shndx == SHN_UNDEF and name:
        failures.append('undefined symbol %s' % name)
      elif shndx == SHN_ABS and info & 0xf in (1, 2) and \
          value not in all_values:
        failures.append('absolute symbol %s = 0x%08x' % (name, value))

  kinds = {}
  data = elf.Bytes(blob)
  # --emit-relocs keeps every relocation; a blob may need none of its own.
  if not any(s[1] == SHT_REL for s in elf.sections):
    failures.append('no relocation sections: link with --emit-relocs')
  for name, s in zip(elf.names, elf.sections):
    if s[1] != SHT_REL or name != '.rel.blob':
      continue
    # Relocations the linker applied inside .blob.
    for i in range(s[5] // 8):
      offset, info = struct.unpack_from('<II', elf.data, s[4] + i * 8)
      kind = info & 0xff
      label = ALLOWED_RELOCATIONS.get(kind, 'type %d' % kind)
      kinds[label] = kinds.get(label, 0) + 1
      if kind not in ALLOWED_RELOCATIONS:
        failures.append('relocation %s at 0x%08x' % (label, offset))
      elif kind == R_ARM_REL32:
        # A text-relative literal: the symbol it names is inside the blob.
        target = elf.SymbolValue(info >> 8)
        if not base <= target < base + len(data):
          failures.append('R_ARM_REL32 at 0x%08x names 0x%08x, outside '
                          'the blob' % (offset, target))

  got = symbols['plaits_blob_got'][0][0]
  got_end = symbols['plaits_blob_got_end'][0][0]
  # The reserved words of a static link's .got.plt stay zero.
  reserved = symbols['plaits_blob_got_plt'][0][0]
  if any(data[reserved - base:got - base]):
    failures.append('nonzero reserved GOT words')
  imports = []
  for address in range(got, got_end, 4):
    value, = struct.unpack_from('<I', data, address - base)
    imports.append(value)
    if value not in import_values:
      failures.append('GOT entry 0x%08x is not a version %d export' % (
          value, services))

  # The declared RAM fits the slot and arena of the blob's ABI; a library's
  # RAM and scratch, what the synth can give.
  state, arena = struct.unpack_from('<2I', data, 24 if abi == 1 else 32)
  max_state = MAX_LIBRARY_RAM if library else MAX_STATE.get(abi, 0)
  if state > max_state:
    failures.append('state of %d B is over the %d B slot' % (
        state, max_state))
  if arena > MAX_ARENA:
    failures.append('arena of %d B is over %d B' % (arena, MAX_ARENA))

  # A library's export table: one absolute word per export, naming code or
  # constants inside the blob.
  table = []
  if library:
    table = library_exports(elf, base, got, failures)
  elif '.blob_exports' in elf.names:
    failures.append('a model has an export table')

  # The source names how many libraries it needs; the build gives them.
  dependencies = struct.unpack_from('<I', data, 64)[0] if abi == 2 else 0
  if dependencies != num_libraries or dependencies > MAX_DEPENDENCIES:
    failures.append('the header lists %d libraries, the build gives %d' % (
        dependencies, num_libraries))

  check_archives(path + '.map', failures)

  listing = subprocess.check_output(
      [objdump, '-d', '-j', '.blob', path]).decode()
  name = None
  for line in listing.splitlines():
    m = FUNCTION.match(line.strip())
    if m:
      name = m.group(1)
    elif indirect(line) and not (name or '').startswith(IMPORT):
      failures.append('indirect branch: ' + line.strip())
  if check_imports(listing, failures) and not dependencies:
    failures.append('library imports without a library to call')
  return failures, kinds, imports, table


def library_exports(elf, base, got, failures):
  table = []
  if '.blob_exports' not in elf.names:
    failures.append('a library without an export table')
    return table
  section = elf.Section('.blob_exports')
  words = elf.Bytes(section)
  relocated = {}
  for name, s in zip(elf.names, elf.sections):
    if s[1] == SHT_REL and name == '.rel.blob_exports':
      for i in range(s[5] // 8):
        offset, info = struct.unpack_from('<II', elf.data, s[4] + i * 8)
        if info & 0xff != R_ARM_ABS32:
          failures.append('export relocation type %d' % (info & 0xff))
        relocated[offset - section[3]] = relocated.get(
            offset - section[3], 0) + 1
  count = len(words) // 4
  if not 1 <= count <= MAX_EXPORTS or len(words) % 4:
    failures.append('%d B of exports: 1 to %d words' % (len(words),
                                                        MAX_EXPORTS))
  header = HEADER_SIZES[2]
  for i in range(count):
    value, = struct.unpack_from('<I', words, 4 * i)
    if relocated.get(4 * i) != 1:
      failures.append('export %d is not one absolute address' % i)
    if not base + header <= value < got:
      failures.append('export %d at 0x%08x is outside the code' % (i, value))
    table.append(value - base)
  return table


# Header words the packer fills: size, the three entries, GOT offset and
# size; by version. A library's reset and render words hold its export
# table's offset and count.
FILLED = {1: (5, 8, 9, 10, 11, 12), 2: (7, 10, 11, 12, 13, 14)}
DEPENDENCIES_OFFSET = 15


def pack(elf, base, table, records):
  symbols = elf.Symbols()
  data = bytearray(elf.Bytes(elf.Section('.blob')))
  abi, _, kind = header_version(elf)
  count = HEADER_SIZES[abi] // 4
  words = list(struct.unpack_from('<%dI' % count, data, 0))
  def entry(name):
    return symbols[name][0][0] - base
  got = symbols['plaits_blob_got'][0][0] - base
  got_end = symbols['plaits_blob_got_end'][0][0] - base
  if got_end != len(data):
    raise ValueError('the GOT does not end .blob')
  size, init, reset, render, got_offset, got_size = FILLED[abi]
  # Code, the GOT, a library's exports, the dependencies, the CRC.
  data += b''.join(struct.pack('<I', offset) for offset in table)
  if records:
    words[DEPENDENCIES_OFFSET] = len(data)
    data += b''.join(records)
  words[size] = len(data) + 4
  if kind == KIND_LIBRARY:
    words[init] = entry('plaits_blob_init') if \
        'plaits_blob_init' in symbols else 0
    words[reset] = got_end
    words[render] = len(table)
    entries = (words[init],) if words[init] else ()
  else:
    words[init] = entry('plaits_blob_init')
    words[reset] = entry('plaits_blob_reset')
    words[render] = entry('plaits_blob_render')
    entries = (words[init], words[reset], words[render])
  words[got_offset] = got
  words[got_size] = got_end - got
  for offset in entries:
    if not offset & 1:
      raise ValueError('an entry point is not Thumb code')
  struct.pack_into('<%dI' % count, data, 0, *words)
  data += struct.pack('<I', zlib.crc32(bytes(data)) & 0xffffffff)
  return bytes(data)


def main(argv):
  v1 = parse_services(argv[1])
  v2 = dict(v1)
  v2.update(parse_services(argv[2]))
  exports = {1: v1, 2: v2}
  objdump = argv[3]
  out = argv[4]
  rest = argv[5:]
  libraries = []
  while rest and rest[0] == '--library':
    libraries.append(rest[1])
    rest = rest[2:]
  records = [library_record(p) for p in libraries]
  failures = []
  packed = {}
  inside = {}
  evidence = {'links': [], 'dependencies': [
      {'id': struct.unpack_from('<I', r)[0], 'sha256': r[4:].hex()}
      for r in records]}
  for spec in rest:
    base_text, path = spec.split(':', 1)
    base = int(base_text, 16)
    elf = Elf(path)
    f, kinds, imports, table = gate(elf, base, exports, objdump, path,
                                    len(records))
    failures += ['%s: %s' % (path, x) for x in f]
    if f:
      continue
    data = pack(elf, base, table, records)
    packed[base] = data
    # A secondary check: words that address the blob from this base. Data
    # may look like an address at one base; a pointer moves with every base.
    inside[base] = set(
        o for o in range(0, len(data) - 3, 2)
        if base <= struct.unpack_from('<I', data, o)[0] < base + len(data))
    evidence['links'].append({
        'base': '0x%08x' % base,
        'sha256': hashlib.sha256(data).hexdigest(),
        'relocations': kinds,
        'imports': ['0x%08x' % v for v in imports]})
  moving = None
  for base, offsets in inside.items():
    data = packed[base]
    relative = set((o, struct.unpack_from('<I', data, o)[0] - base)
                   for o in offsets)
    moving = relative if moving is None else moving & relative
  if moving:
    failures.append('blob addresses at offsets %s' % sorted(moving)[:8])
  if not failures and len(set(packed.values())) != 1:
    failures.append('the packed bytes differ between bases')
  if not failures and len(packed) < 3:
    failures.append('link at three bases at least')
  for f in failures:
    print('GATE: ' + f, file=sys.stderr)
  if failures:
    return 1
  data = list(packed.values())[0]
  open(out, 'wb').write(data)
  abi = struct.unpack_from('<I', data, 8)[0]
  words = struct.unpack_from('<%dI' % (HEADER_SIZES[abi] // 4), data, 0)
  if abi == 1:
    model, state, arena, entries, got = (
        words[4], words[6], words[7], words[8:11], words[11:13])
  else:
    model, state, arena, entries, got = (
        words[5], words[8], words[9], words[10:13], words[13:15])
  evidence['kind'] = 'library' if abi == 2 and words[4] == KIND_LIBRARY \
      else 'model'
  evidence.update({
      'abi': abi,
      'size': len(data),
      'sha256': hashlib.sha256(data).hexdigest(),
      'model': model,
      'state_size': state,
      'arena_size': arena,
      'entries': ['0x%x' % w for w in entries],
      'got': ['0x%x' % got[0], got[1]]})
  json.dump(evidence, open(out + '.json', 'w'), indent=1, sort_keys=True)
  print('%s %d B sha256 %s' % (out, len(data), evidence['sha256']))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
