# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.

"""Run only in an offline oracle container. The codec interrupt itself drives
each block; CYCCNT and DMA NDTR are scripted, so classes are structural, never
timing measurements."""

import hashlib
import json
from pathlib import Path
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent / "oracle"))
import machine as M
import manual_gap as G
import fill as F
import sides as SD

import dwt
import deadline_trace as T
from itcm_emulator import qualify_itcm

LISR, TX_NDTR, RX_NDTR, CYCCNT = 0x40020000, 0x40020014, 0x4002002C, 0xE0001004
HT, TC = 1 << 10, 1 << 11
SEED_PIN, FREEZE_PIN = 1 << 10, 1 << 11
REQUEST = dwt.BASE + 12
N = 48
# Live (deadline_trace.h): total at +32.
LIVE_TOTAL = 32


class Script(dict):
  """The MMIO stub's store, with CYCCNT and NDTR reads scripted per block."""

  offset = 0

  def begin(self, block, tx=(), rx=None, lisr=None, jump=0, jump_after=0, persist=False):
    if persist:
      self.offset += jump
      jump = 0
    self.time = block * T.BLOCK_CYCLES + self.offset
    self.reads = 0
    self.jump, self.jump_after = jump, jump_after
    self.tx = list(tx)
    self.rx = rx
    self.lisr = lisr
    self.lisr_reads = 0

  def get(self, key, default=0):
    if key == CYCCNT and hasattr(self, "time"):
      self.reads += 1
      extra = self.jump if self.reads > self.jump_after else 0
      return (self.time + 8 * self.reads + extra) & 0xFFFFFFFF
    if key == TX_NDTR and getattr(self, "tx", None):
      return self.tx.pop(0) if len(self.tx) > 1 else self.tx[0]
    if key == RX_NDTR and getattr(self, "rx", None) is not None:
      return self.rx
    if key == LISR and getattr(self, "lisr", None) is not None:
      # The trace's entry snapshot reads LISR first; the handler's dispatch reads it next.
      if isinstance(self.lisr, tuple):
        value = self.lisr[0] if self.traced and not self.lisr_reads else self.lisr[1]
        self.lisr_reads += 1
        return value
      return self.lisr
    return dict.get(self, key, default)


def ndtr(position):
  return N - position if position else N


# Half 0 TX positions at entry, StoreBegin, StoreEnd and exit; LISR; CYCCNT
# jump (cycles, unaffected reads, persistent); expected class.
ON_TIME = ((26, 40, 41, 42), None, (0, 0, False), "on_time")
CASES = [
  ("on_time", ON_TIME),
  ("stale", ((44, 3, 5, 6), None, (0, 0, False), "stale")),
  ("ambiguous", ((40, 46, 1, 2), None, (0, 0, False), "ambiguous")),
  ("coalesced", ((26, 40, 41, 42), HT | TC, (0, 0, False), "invalid")),
  ("cadence", ((26, 40, 41, 42), None, (70000, 0, True), "invalid")),
  ("lap", ((26, 40, 41, 42), None, (250000, 2, False), "invalid")),
  ("spurious", ((26, 40, 41, 42), 0, (0, 0, False), "invalid")),
  # TC raised after the entry snapshot: the handler writes half 1.
  ("late_tc", ((26, 40, 41, 42), (HT, HT | TC), (0, 0, False), "invalid")),
]


class Run:
  def __init__(self, root, variant, armed):
    elf = Path(root) / (variant + "/beads/beads.elf")
    G.DRIZZLET_ELF = hashlib.sha256(elf.read_bytes()).hexdigest()
    G.DRIZZLET_PAYLOAD = hashlib.sha256(elf.with_suffix(".bin").read_bytes()).hexdigest()
    self.table = table = SD.symbol_table(SD.elf32.Elf(elf.read_bytes()))
    F.CV_VALUES = SD.ram_object(table, "_ZN5beads5CvAdc7values_E", 12)
    F.POT_VALUES = SD.ram_object(table, "_ZN5beads7PotsAdc7values_E", 24)
    side = G.prepare(str(elf), "drizzlet")
    itcm = qualify_itcm(side)
    self.p = p = G.Panel(side, 3, ())
    self.m = m = p.x.m
    if itcm:
      m.write(0, itcm)
    script = Script(m.mmio)
    m.mmio = script
    self.script = script
    script.traced = False
    obj = lambda name: SD.defined_symbol(table, name, "OBJECT").value
    self.isr = SD.defined_symbol(table, "DMA1_Stream1_IRQHandler", "FUNC").value & ~1
    # The 2024 driver has no Pause; its Stop completes the trace the same way.
    stop = "_ZN5beads5Codec5PauseEv" if "_ZN5beads5Codec5PauseEv" in table else "_ZN5beads5Codec4StopEv"
    self.pause = SD.defined_symbol(table, stop, "FUNC").value & ~1
    self.instance = obj("_ZN5beads5Codec9instance_E")
    self.tx = obj("_ZN5beads5Codec14tx_dma_buffer_E")
    self.rx = obj("_ZN5beads5Codec14rx_dma_buffer_E")
    self.idr = obj("_ZN5beads5Codec11idr_buffer_E")
    self.transition = obj("codec_transition_frames")
    self.live = obj("_ZN5beads15dwt_calibration5trace4liveE") \
        if "_ZN5beads15dwt_calibration5trace4liveE" in table else None
    self.variant, self.armed = variant, armed
    self.audio = bytearray()
    self.block = 0
    # Every executed store into the codec's TX buffer: block, instruction, offset, bytes.
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB | capstone.CS_MODE_MCLASS)
    self.stores = []

    def store(uc, access, address, size, value, user):
      pc = uc.reg_read(m.UA.UC_ARM_REG_PC)
      ins = next(md.disasm(bytes(uc.mem_read(pc, 4)), pc, 1))
      self.stores.append((self.block, ins.mnemonic, address - self.tx, size))
    m.uc.hook_add(m.unicorn.UC_HOOK_MEM_WRITE, store, begin=self.tx, end=self.tx + 8 * 48 - 1)

  def step(self, count=1, case=ON_TIME):
    p, m, x = self.p, self.m, self.p.x
    positions, lisr, jump, _ = case
    for _ in range(count):
      p.inputs.due(p.b)
      x.set_inputs(p.inputs.idr_words())
      n = x.block_size
      half = self.block & 1
      frames = [(int(.1 * 2147483648 * np.sin(2 * np.pi * (p.frame + i) / 64)),
                 int(.07 * 2147483648 * np.sin(2 * np.pi * (p.frame + i) / 37))) for i in range(n)]
      gates = [p.inputs.freeze_bit(p.frame + i) for i in range(n)]
      frames, gates, cv = p.inputs.normalled(x.pin, frames, gates, [32768] * 6)
      m.write(F.CV_VALUES, struct.pack("<6H", *cv))
      m.write(F.POT_VALUES, struct.pack("<12H", *p.inputs.pots(p.pots)))
      m.write(self.rx + 8 * n * half, b"".join(struct.pack("<ii", *v) for v in frames))
      m.write(self.idr + 8 * n * half, b"".join(
        struct.pack("<II", (0 if g & 1 else SEED_PIN) | (0 if g & 2 else FREEZE_PIN), 0) for g in gates))
      shift = 0 if half == 0 else (N // 2)
      tx = [ndtr((q + shift) % N) for q in positions]
      self.script.begin(self.block, tx, ndtr(N // 2 if half == 0 else 0),
                        (TC if half else HT) if lisr is None else lisr, *jump)
      x.block_index = p.b
      # Live (deadline_trace.h) begins with pending and active: an entry snapshot reads LISR.
      self.script.traced = self.live is not None and any(m.read(self.live, 2))
      m.call(self.isr, [], fpscr=M.FZ)
      self.audio += m.read(self.tx + 8 * n * half, 8 * n)
      if p.b % 4 == 3:
        m.call(x.fn["systick"], [], fpscr=M.FZ)
        x.main_pass()
      p.frame += n
      p.b += 1
      self.block += 1

  def request(self, value):
    self.m.write(REQUEST, bytes([value if self.armed or value != T.REQUEST else 1]))

  def region(self):
    return self.m.read(T.BASE, T.SIZE) if self.live is not None else None


def scenario(run):
  checks = {}
  run.step(20)
  # The request block is record 0; the stats window starts with it too.
  run.request(T.REQUEST)
  run.step(1)
  if run.live is not None:
    h = T.decode_header(run.region())
    checks["after_request"] = dict(state=h["state"], generation=h["generation"],
                                   total=h["total"], request=run.m.read(REQUEST, 1)[0])
  run.step(9)  # codec transition: the silence path
  run.m.write(run.transition, struct.pack("<i", 0))
  expected = []
  for name, case in CASES:
    run.step(2, ON_TIME)
    expected += ["on_time", "on_time"]
    run.step(1, case)
    expected.append(case[3])
  run.step(16)
  expected += ["on_time"] * 16
  if run.live is not None:
    decoded = T.decode(run.region(), generation=1)
    records = decoded["records"]
    checks["classes"] = [r["class"] for r in records]
    checks["firmware_classes"] = [r["firmware_class"] for r in records]
    checks["flags"] = [r["flag_names"] for r in records]
    checks["integrity"] = decoded["integrity"]
    checks["trigger_index"] = decoded["header"]["trigger_index"]
    checks["expected"] = ["on_time"] * 10 + expected if run.armed else []
    checks["stats_count"] = dwt.decode(run.m.read(dwt.BASE, dwt.SIZE))["brackets"][0]["count"]
    checks["total"] = decoded["header"]["total"]
    checks["spurious"] = sum("spurious" in f for f in checks["flags"])
  # Request 1 disarms a recording trace.
  run.request(1)
  run.step(1)
  if run.live is not None:
    checks["disarm"] = T.decode_header(run.region())
  # Generation 2: wrap the ring, trigger at record 801, freeze 384 records later.
  run.request(T.REQUEST)
  run.step(801)
  run.step(1, CASES[1][1])
  run.step(384)
  frozen = run.region()
  run.step(10)
  if run.live is not None and run.armed:
    decoded = T.decode(frozen, generation=2)
    later = T.decode(run.region(), generation=2)
    ring = decoded["records"]
    checks["freeze"] = dict(
      state=decoded["header"]["state"], trigger=decoded["header"]["trigger_index"],
      first=ring[0]["index"], last=ring[-1]["index"], count=len(ring),
      stale=[r["index"] for r in ring if r["class"] == "stale"],
      integrity=decoded["integrity"],
      ring_unchanged=frozen[T.HEADER_SIZE:] == run.region()[T.HEADER_SIZE:],
      total_after=later["header"]["total"], write_index_after=later["header"]["write_index"],
      write_index=decoded["header"]["write_index"])
  # Window end: two blocks short of the window, then two more.
  if run.live is not None and run.armed:
    run.m.w32(run.live + LIVE_TOTAL, T.WINDOW - 2)
  run.step(2)
  end = run.region()
  run.step(2)
  if run.live is not None:
    checks["window_end"] = dict(T.decode_header(end), frozen_after=run.region() == end)
  # Abort: re-arm, record three blocks, then Pause (Stop on the 2024 driver).
  run.request(T.REQUEST)
  run.step(3)
  run.m.call(run.pause, [run.m.u32(run.instance)], fpscr=M.FZ)
  run.step(4)
  if run.live is not None:
    checks["abort"] = T.decode_header(run.region())
  # A request served by a callback after the abort, before the codec stops
  # calling back, must not re-arm: the callback is run directly here.
  run.request(T.REQUEST)
  x = run.p.x
  x.block(bytes(8 * x.block_size), bytes(x.block_size), [32768] * 6, [0] * 12, M.FZ, False)
  if run.live is not None:
    checks["after_abort_request"] = T.decode_header(run.region())
  return checks


def store_order(run):
  """Store sequence of one silence block and one processed block, checked."""
  result = {}
  for name, block in (("silence", 26), ("processed", 40)):
    rows = [(mnemonic, offset, size) for b, mnemonic, offset, size in run.stores if b == block]
    half = 8 * 12 * (block & 1)
    offsets = [offset - half for _, offset, _ in rows]
    covered = []
    for _, offset, size in rows:
      covered += range(offset - half, offset - half + size)
    assert sorted(covered) == list(range(96)), (name, rows)
    # The processed path must store in address order; fill's order is reported.
    assert name == "silence" or offsets == sorted(offsets), (name, rows)
    result[name] = dict(ascending=offsets == sorted(offsets), offsets=offsets,
                        mnemonics=sorted(set(m for m, _, _ in rows)),
                        sizes=sorted(set(s for _, _, s in rows)), stores=len(rows),
                        first=rows[0], last=rows[-1])
  return result


def main():
  M.qualify()
  root = sys.argv[1]
  rows = []
  for variant, armed in (("release", False), ("calibration", False), ("trace", False), ("trace", True)):
    run = Run(root, variant, armed)
    checks = scenario(run)
    rows.append(dict(variant=variant, armed=armed, blocks=run.block, stores=store_order(run),
                     audio_bytes=len(run.audio), codec=list(run.p.x.codec),
                     output=hashlib.sha256(bytes(run.audio)).hexdigest(),
                     nonzero=any(run.audio), checks=checks))
  print(json.dumps(rows, indent=2, default=str))
  outputs = {r["output"] for r in rows}
  assert len(outputs) == 1, "audio differs between variants"
  assert all(r["nonzero"] for r in rows)
  disarmed, armed = rows[2]["checks"], rows[3]["checks"]
  assert disarmed["after_request"]["state"] == "idle"
  assert "freeze" not in disarmed
  assert disarmed["window_end"]["state"] == "idle" and disarmed["abort"]["state"] == "idle"
  assert armed["after_request"] == dict(state="recording", generation=1, total=1, request=0)
  assert armed["integrity"] == [], armed["integrity"]
  assert armed["classes"] == armed["expected"], (armed["classes"], armed["expected"])
  assert armed["firmware_classes"] == armed["classes"]
  assert all("silence" in f for f in armed["flags"][:10])
  late = 10 + 3 * [name for name, _ in CASES].index("late_tc") + 2
  assert armed["flags"][late] == ["coalesced"], armed["flags"][late]
  # Same window: every record but the spurious one ran a callback.
  assert armed["stats_count"] == armed["total"] - armed["spurious"]
  assert disarmed["disarm"]["state"] == "idle"
  disarm = armed["disarm"]
  assert (disarm["state"], disarm["reason"], disarm["window_invalid"], disarm["generation"]) == \
      ("complete", "abort", True, 1)
  assert armed["trigger_index"] == armed["expected"].index("stale")
  f = armed["freeze"]
  assert (f["state"], f["trigger"], f["first"], f["last"], f["count"], f["stale"], f["integrity"]) == \
      ("ring_frozen", 801, 801 + 385 - 768, 801 + 384, 768, [801], []), f
  assert f["ring_unchanged"] and f["total_after"] == 801 + 385 + 10 and f["write_index_after"] == f["write_index"]
  end = armed["window_end"]
  assert (end["state"], end["reason"], end["window_invalid"], end["total"], end["generation"]) == \
      ("complete", "window_end", False, T.WINDOW, 2) and end["frozen_after"]
  late_request = armed["after_abort_request"]
  assert (late_request["state"], late_request["reason"], late_request["generation"]) == \
      ("complete", "abort", 3), late_request
  abort = armed["abort"]
  assert (abort["state"], abort["reason"], abort["window_invalid"], abort["generation"]) == \
      ("complete", "abort", True, 3)
  print("STRUCTURAL_TRACE_PASS")


if __name__ == "__main__":
  main()
