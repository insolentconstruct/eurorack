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
# Lay out a blob region (beads/blob/plaits_blob.h, PlaitsBlobRegionHeader):
# the table, the blobs verbatim at 32-byte aligned offsets, then a CRC-32 of
# everything before it. This is Hygrostat's job; here it builds test regions.
#
#   blob_region.py OUT.bin RELEASE BLOB.bin [BLOB.bin...]
#
# Prints the region CRC, which the profile would select.

import hashlib
import json
import struct
import sys
import zlib

REGION_MAGIC = 0x47525a44
REGION_VERSION = 1
SERVICES_VERSION = 1
ALIGNMENT = 32
HEADER = '<6I'
ENTRY = '<4I'


def main(argv):
  out = argv[1]
  release = int(argv[2], 0)
  blobs = [open(path, 'rb').read() for path in argv[3:]]
  table_size = struct.calcsize(HEADER) + len(blobs) * struct.calcsize(ENTRY)
  entries = []
  offset = table_size
  for blob in blobs:
    offset = (offset + ALIGNMENT - 1) // ALIGNMENT * ALIGNMENT
    model, size = struct.unpack_from('<I', blob, 16)[0], len(blob)
    if struct.unpack_from('<I', blob, 20)[0] != size:
      raise ValueError('a blob\'s size word does not match its file')
    entries.append((model, offset, size, struct.unpack_from('<I', blob,
                                                            size - 4)[0]))
    offset += size
  size = offset + 4
  data = bytearray(b'\xff' * offset)
  struct.pack_into(HEADER, data, 0, REGION_MAGIC, REGION_VERSION, release,
                   SERVICES_VERSION, size, len(blobs))
  for i, entry in enumerate(entries):
    struct.pack_into(ENTRY, data, struct.calcsize(HEADER) +
                     i * struct.calcsize(ENTRY), *entry)
    data[entry[1]:entry[1] + entry[2]] = blobs[i]
  crc = zlib.crc32(bytes(data)) & 0xffffffff
  data += struct.pack('<I', crc)
  open(out, 'wb').write(data)
  json.dump({
      'release': release,
      'size': len(data),
      'crc': '0x%08x' % crc,
      'blobs': [{'model': m, 'offset': o, 'size': s,
                 'sha256': hashlib.sha256(blobs[i]).hexdigest()}
                for i, (m, o, s, _) in enumerate(entries)]},
            open(out + '.json', 'w'), indent=1, sort_keys=True)
  print('%s %d B crc 0x%08x' % (out, len(data), crc))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
