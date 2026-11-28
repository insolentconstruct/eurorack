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

"""Assemble an offline S1 flash span and receipt from the linked candidate."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys

APP_BASE = 0x08020000
APP_END = 0x08100000


def main():
  out = Path(sys.argv[1])
  nm = sys.argv[2]
  app = (out / 'app.bin').read_bytes()
  region = (out / 'region.bin').read_bytes()
  elf = out / 'app.elf'
  symbols = {}
  for line in subprocess.check_output([nm, str(elf)], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
      symbols[fields[2]] = int(fields[0], 16)
  begin = symbols['plaits_region_begin']
  trace = symbols['s1_trace']
  seal_offset = 15 * 1024 - 4
  snapshot = symbols['s1_snapshot']
  mailbox = symbols['s1_mailbox']
  ceiling = symbols['s1_ceiling']
  if (begin != APP_BASE + ((len(app) + 4095) & ~4095) or
      begin + len(region) > APP_END or
      trace & 31 or not 0x24000000 <= trace <= 0x24080000 - 15 * 1024 or
      not 0x20000000 <= snapshot <= 0x20020000 - 104 or
      not 0x20000000 <= mailbox <= 0x20020000 - 20 or
      ceiling & 3 or not 0x20000000 <= ceiling <= 0x20020000 - 64):
    raise ValueError('Timing ELF layout or flash fit differs')
  image = app.ljust(begin - APP_BASE, b'\xff') + region
  image = image.ljust(APP_END - APP_BASE, b'\xff')
  (out / 'image.bin').write_bytes(image)
  digest = hashlib.sha256(elf.read_bytes()).hexdigest()
  receipt = dict(abi=1, elf_sha256=digest, images=[hashlib.sha256(image).hexdigest()],
                 snapshot_address=snapshot, mailbox_address=mailbox,
                 trace_address=trace, seal_offset=seal_offset, ceiling_address=ceiling,
                 region_address=begin)
  (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
  main()
