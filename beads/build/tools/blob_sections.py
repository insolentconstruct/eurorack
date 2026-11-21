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
# Lay out a region (beads/blob/plaits_region.h): WAVE, TERR and PLTS sections
# in that order, each on a 32-byte flash word, with a 32-byte header and a
# trailing CRC-32. PLTS holds one entry per blob, then the blobs verbatim at
# 32-byte aligned offsets. Hygrostat's writer must produce the same bytes.
#
#   blob_sections.py OUT.bin RELEASE [--wave beads/resources.cc]
#       [--terrain GRIDS.bin] [BLOB.bin...]
#
# OUT.bin.json lists each section's offset, size, CRC and SHA-256: the
# profile directory's entries, or a release's default layout.

import argparse
import hashlib
import json
import re
import struct
import sys
import zlib

SECTION_MAGIC = 0x43535a44
SECTION_VERSION = 1
KINDS = {'WAVE': 0x45564157, 'TERR': 0x52524554, 'PLTS': 0x53544c50}
ALIGNMENT = 32
WAVE_ROWS = 193
WAVE_ROW = 260
TERRAIN = 64 * 64
MAX_TERRAINS = 16
MAX_BLOBS = 128
HEADER_SIZES = {1: 64, 2: 96}
KIND_MODEL = 1


def align(n):
  return (n + ALIGNMENT - 1) // ALIGNMENT * ALIGNMENT


def factory_waves(path):
  """Beads' wav_integrated_waves, as compiled into the app."""
  text = open(path).read()
  start = text.index('const int16_t wav_integrated_waves[] = {')
  end = text.index('};', start)
  values = [int(v) for v in re.findall(r'-?\d+', text[start:end])[1:]]
  if len(values) != WAVE_ROWS * WAVE_ROW:
    raise ValueError('%d wavetable samples' % len(values))
  return struct.pack('<%dh' % len(values), *values)


def check_waves(data):
  if len(data) != WAVE_ROWS * WAVE_ROW * 2:
    raise ValueError('WAVE must be %d rows of %d samples' % (
        WAVE_ROWS, WAVE_ROW))
  row = WAVE_ROW * 2
  for i in range(WAVE_ROWS):
    r = data[i * row:(i + 1) * row]
    if r[:8] != r[-8:]:
      raise ValueError('WAVE row %d: samples 0-3 differ from 256-259' % i)
  if data[-row:] != data[-2 * row:-row]:
    raise ValueError('WAVE guard row differs from the last wave')


def section(kind, release, count, payload):
  size = 32 + len(payload) + 4
  data = struct.pack('<8I', SECTION_MAGIC, KINDS[kind], SECTION_VERSION,
                     release, size, count, 0, 0) + payload
  return data + struct.pack('<I', zlib.crc32(data) & 0xffffffff)


def blob_id(blob):
  abi = struct.unpack_from('<I', blob, 8)[0]
  if struct.unpack_from('<I', blob, 4)[0] != HEADER_SIZES.get(abi):
    raise ValueError('not a blob')
  model = struct.unpack_from('<I', blob, 16 if abi == 1 else 20)[0]
  size = struct.unpack_from('<I', blob, 20 if abi == 1 else 28)[0]
  if size != len(blob):
    raise ValueError('a blob\'s size word does not match its file')
  return model


def plaits(release, blobs):
  if len(blobs) > MAX_BLOBS:
    raise ValueError('more than %d blobs' % MAX_BLOBS)
  blobs = sorted(blobs, key=blob_id)
  offset = 32 + 16 * len(blobs)
  entries = []
  for blob in blobs:
    offset = align(offset)
    entries.append((KIND_MODEL, blob_id(blob), offset, len(blob),
                    struct.unpack_from('<I', blob, len(blob) - 4)[0]))
    offset += len(blob)
  payload = bytearray(b'\xff' * (offset - 32))
  for i, (kind, model, start, size, crc) in enumerate(entries):
    struct.pack_into('<BBHIII', payload, 16 * i, kind, model, 0, start,
                     size, crc)
    payload[start - 32:start - 32 + size] = blobs[i]
  return section('PLTS', release, len(blobs), bytes(payload)), [
      {'model': m, 'offset': o, 'size': s,
       'sha256': hashlib.sha256(blobs[i]).hexdigest()}
      for i, (_, m, o, s, _) in enumerate(entries)]


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('out')
  parser.add_argument('release', type=lambda x: int(x, 0))
  parser.add_argument('--wave')
  parser.add_argument('--terrain')
  parser.add_argument('blobs', nargs='*')
  args = parser.parse_intermixed_args(argv[1:])

  sections = []
  if args.wave:
    waves = factory_waves(args.wave) if args.wave.endswith('.cc') else \
        open(args.wave, 'rb').read()
    check_waves(waves)
    sections.append(('WAVE', section('WAVE', args.release, WAVE_ROWS, waves),
                     None))
  if args.terrain:
    grids = open(args.terrain, 'rb').read()
    count = len(grids) // TERRAIN
    if not 1 <= count <= MAX_TERRAINS or len(grids) != count * TERRAIN:
      raise ValueError('TERR must be 1 to 16 grids of 64 x 64')
    sections.append(('TERR', section('TERR', args.release, count, grids),
                     None))
  if args.blobs:
    data, listed = plaits(args.release,
                          [open(p, 'rb').read() for p in args.blobs])
    sections.append(('PLTS', data, listed))

  region = bytearray()
  layout = {'release': args.release, 'sections': []}
  for kind, data, listed in sections:
    region += b'\xff' * (align(len(region)) - len(region))
    entry = {'kind': kind, 'offset': len(region), 'size': len(data),
             'crc': '0x%08x' % struct.unpack_from('<I', data, len(data) - 4),
             'sha256': hashlib.sha256(data).hexdigest()}
    if listed is not None:
      entry['blobs'] = listed
    layout['sections'].append(entry)
    region += data
  open(args.out, 'wb').write(region)
  json.dump(layout, open(args.out + '.json', 'w'), indent=1, sort_keys=True)
  for s in layout['sections']:
    print('%s at %d, %d B, crc %s' % (s['kind'], s['offset'], s['size'],
                                      s['crc']))
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
