# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.

"""Run only in an offline oracle container; DWT is an instruction-count stub."""

import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent / "oracle"))
import machine as M
import manual_gap as G
import icount as I
import dwt
import fill as F
import sides as SD

from itcm_emulator import qualify_itcm

def main():
  M.qualify()
  scratch = tempfile.TemporaryDirectory(prefix="beads-dwt-")
  lib = I.native_library(Path(scratch.name))
  rows = []
  for variant in ("release", "calibration"):
    elf = Path(sys.argv[1]) / (variant + "/beads/beads.elf")
    G.DRIZZLET_ELF = hashlib.sha256(elf.read_bytes()).hexdigest()
    G.DRIZZLET_PAYLOAD = hashlib.sha256(
      elf.with_suffix(".bin").read_bytes()
    ).hexdigest()
    table = SD.symbol_table(SD.elf32.Elf(elf.read_bytes()))
    F.CV_VALUES = SD.ram_object(table, "_ZN5beads5CvAdc7values_E", 12)
    F.POT_VALUES = SD.ram_object(table, "_ZN5beads7PotsAdc7values_E", 24)
    side = G.prepare(str(elf), "drizzlet")
    itcm = qualify_itcm(side)
    p = G.Panel(side, 3, ())
    m = p.x.m
    if itcm:
      m.write(0, itcm)
    p.x.main_pass = lambda: None
    p.step(20)
    counter = lib.ic_new()
    assert lib.ic_attach(counter, m.uc._uch) == 0
    original = m.call
    counts = []

    def counted(entry, *args, **kw):
      audio = entry == p.x.fn[p.x.callback]
      if audio:
        lib.ic_start(counter, 0)
      result = original(entry, *args, **kw)
      if audio:
        lib.ic_stop(counter)
        counts.append(int(lib.ic_total(counter)))
      return result

    m.call = counted
    # MMIO CYCCNT stub: deterministic monotonic ticks per counter read.
    if variant == "calibration":
      old = m._mmio_read
      # mmio_map captured bound method at construction; replace mapping callbacks.
      tick = [0]

      def read(uc, offset, size, base):
        if base + offset == 0xE0001004:
          tick[0] += 1000
          return tick[0]
        return old(uc, offset, size, base)

      m.uc.mem_unmap(0xE0000000, 0x100000)
      m.uc.mmio_map(
        0xE0000000, 0x100000, read, 0xE0000000, m._mmio_write, 0xE0000000
      )
      m.write(dwt.BASE + 12, b"\1")
    audio = p.step(64)
    row = dict(
      variant=variant,
      counts=counts,
      output=hashlib.sha256(audio).hexdigest(),
    )
    if variant == "calibration":
      stats = dwt.decode(m.read(dwt.BASE, dwt.SIZE))
      assert stats["brackets"][0]["count"] == 64, stats
      assert stats["brackets"][1]["count"] == 64, stats
      assert stats["brackets"][1]["maximum"] == 1000, stats
      generation = stats["generation"]
      m.write(dwt.BASE + 12, b"\1")
      p.step(1)
      after = dwt.decode(m.read(dwt.BASE, dwt.SIZE))
      assert after["generation"] == generation + 1
      assert after["brackets"][0]["count"] == 1
      assert m.read(dwt.BASE + 12, 1) == b"\0"
      row["stats"] = stats
      row["reset"] = after
      # Exercise the actual fixture initializer, then skip warmup for a short
      # structural check. This does not establish DSP settling or CPU cycles.
      m.write(dwt.BASE + 12, b"\3")
      p.step(1)
      fixture = dwt.decode(m.read(dwt.BASE, dwt.SIZE))
      assert fixture["fixture"] and fixture["brackets"][0]["count"] == 0
      # ABI block counter at offset 20; synthetic flag remains at 13.
      assert m.read(dwt.BASE + 13, 1) == b"\1"
      m.w32(dwt.BASE + 20, 16000)
      p.step(2)
      fixture = dwt.decode(m.read(dwt.BASE, dwt.SIZE))
      assert all(r["count"] == 2 for r in fixture["brackets"]), fixture
      row["fixture"] = fixture
      # Force the last block of a panel capture and prove it stops updating.
      m.write(dwt.BASE + 12, b"\1")
      p.step(1)
      m.w32(dwt.BASE + 28, 239999)
      p.step(1)
      frozen = m.read(dwt.BASE, dwt.SIZE)
      p.step(2)
      assert m.read(dwt.BASE, dwt.SIZE) == frozen
      row["freeze_verified"] = True
    rows.append(row)
    lib.ic_free(counter)
  print(json.dumps(rows, indent=2))
  assert rows[0]["output"] == rows[1]["output"]
  scratch.cleanup()


if __name__ == "__main__":
  main()
