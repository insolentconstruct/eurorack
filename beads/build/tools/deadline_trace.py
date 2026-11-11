#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.

"""Offline deadline trace decoder (beads/deadline_trace.h). Never contacts hardware."""

import argparse
import json
from pathlib import Path
import struct

BASE = 0x30041460
SIZE = 24832
HEADER_SIZE = 256
RECORD_SIZE = 32
RECORDS = 768
MAGIC = 0x444C5452
VERSION = 1
REQUEST = 4
WINDOW = 240000
POST_TRIGGER = 384
BLOCK_CYCLES = 120000
LAP_CYCLES = 210000
SLACK_BINS = 20
SAI_BITS = 7
NONE = 0xFFFFFFFF
# The build script pins its artifact copy; this source decoder is ABI-only.
BUILD_SHA = None
DECODER_VERSION = 1

STATES = ("idle", "pending", "recording", "ring_frozen", "complete")
REASONS = ("none", "window_end", "abort", "geometry")
CLASSES = ("on_time", "stale", "ambiguous", "invalid")
FLAGS = (
  ("silence", 1),
  ("no_store", 2),
  ("snapshot_long", 4),
  ("coalesced", 8),
  ("cadence", 16),
  ("lap", 32),
  ("spurious", 64),
  ("trigger", 128),
)
INVALID_FLAGS = 2 | 4 | 8 | 16 | 32 | 64
LISR_BITS = ("dmeif0", "teif0", "htif0", "tcif0", "dmeif1", "teif1", "htif1", "tcif1")

HEADER = struct.Struct("<IIIBBBbIIIIIIII2I14I")
TOTALS = struct.Struct("<i13IQ20I")
RECORD = struct.Struct("<IIII3BBBBBBHHBBH")
TOTAL_FIELDS = (
  "min_slack", "on_time", "stale", "ambiguous", "invalid", "coalesced",
  "cadence", "lap", "spurious", "no_store", "snapshot_long", "silence",
  "p0_count", "padding", "p0_cycles",
)
assert HEADER.size == 112 and TOTALS.size == 144 and RECORD.size == 32
assert HEADER_SIZE + RECORDS * RECORD_SIZE == SIZE


def classify(record, n):
  """The firmware's Classify: (class, slack, begin, end) in DMA words."""
  if record["flags"] & INVALID_FLAGS:
    return "invalid", None, None, None
  h = n // 2
  base = h * (record["half"] & 1)
  tx = [(n - x) % n for x in record["tx_ndtr"]]
  entry = h + (tx[0] + 2 * n - base - h) % n
  begin = entry + (tx[1] + n - tx[0]) % n
  end = entry + (tx[2] + n - tx[0]) % n
  slack = n - 1 - end
  if slack >= 0:
    return "on_time", slack, begin, end
  return ("stale" if begin >= n + 1 else "ambiguous"), slack, begin, end


def histogram_bin(slack):
  return 0 if slack < 0 else min(slack + 1, SLACK_BINS - 1)


def decode_header(data):
  if len(data) < HEADER_SIZE:
    raise ValueError("short header")
  f = HEADER.unpack_from(data, 0)
  magic, version, sequence = f[0:3]
  if magic != MAGIC or version != VERSION:
    raise ValueError("wrong trace firmware")
  if sequence & 1:
    raise ValueError("overlapping update; retry")
  state, reason, window_invalid, threshold = f[3:7]
  (generation, sample_rate, block_size, buffer_words, write_index, total,
   trigger_index, delay) = f[7:15]
  values = TOTALS.unpack_from(data, HEADER.size)
  totals = dict(zip(TOTAL_FIELDS, values))
  totals.pop("padding")
  histogram = list(values[len(TOTAL_FIELDS):])
  if state >= len(STATES) or reason >= len(REASONS):
    raise ValueError("unknown state or reason")
  return dict(
    sequence=sequence,
    state=STATES[state],
    reason=REASONS[reason],
    window_invalid=bool(window_invalid),
    threshold=threshold,
    generation=generation,
    sample_rate=sample_rate,
    block_size=block_size,
    buffer_words=buffer_words,
    write_index=write_index,
    total=total,
    trigger_index=None if trigger_index == NONE else trigger_index,
    delay=delay,
    sai_baseline=list(f[15:17]),
    sai_first=[
      [None if x == NONE else x for x in f[17 + SAI_BITS * b:17 + SAI_BITS * (b + 1)]]
      for b in range(2)
    ],
    totals=totals,
    histogram=histogram,
  )


def decode_record(data, offset):
  f = RECORD.unpack_from(data, offset)
  return dict(
    t_entry=f[0],
    t_store_begin=f[1],
    t_store_end=f[2],
    t_exit=f[3],
    tx_ndtr=list(f[4:7]),
    rx_ndtr_entry=f[7],
    lisr_entry=f[8],
    lisr_exit=f[9],
    half=f[10] & 1,
    firmware_class=CLASSES[(f[10] >> 1) & 3],
    flags=f[11],
    p0_cycles_pre=f[12],
    p0_cycles_store=f[13],
    p0_count_pre=f[14],
    p0_count_store=f[15],
    sequence=f[16],
  )


def ring_order(header):
  """(first slot, count, index of the first record) of the ring's records."""
  total = header["total"]
  trigger = header["trigger_index"]
  written = total if trigger is None else min(total, trigger + POST_TRIGGER + 1)
  if header["write_index"] != written % RECORDS:
    raise ValueError("ring write index does not match the record count")
  count = min(written, RECORDS)
  return (written - count) % RECORDS, count, written - count


def flag_names(flags):
  return [name for name, bit in FLAGS if flags & bit]


def lisr_names(byte):
  return [name for i, name in enumerate(LISR_BITS) if byte >> i & 1]


def decode(data, generation=None, sample_rate=48000):
  """Header, totals, ring records and the window's admission for the plan."""
  if len(data) != SIZE:
    raise ValueError("wrong dump length")
  header = decode_header(data)
  n = header["buffer_words"]
  totals = header["totals"]
  problems = []
  valid = totals["on_time"] + totals["stale"] + totals["ambiguous"]
  # Totals are published when the window completes or aborts.
  if header["state"] == "complete":
    if valid + totals["invalid"] != header["total"]:
      problems.append("class counts do not add up to the total")
    if sum(header["histogram"]) != valid:
      problems.append("slack histogram does not match the valid count")
  records = []
  first, count, index = ring_order(header)
  mismatches = 0
  for k in range(count):
    r = decode_record(data, HEADER_SIZE + RECORD_SIZE * ((first + k) % RECORDS))
    r["index"] = index + k
    if r["sequence"] != (index + k) & 0xFFFF:
      problems.append("ring sequence break at record %d" % (index + k))
      break
    cls, slack, begin, end = classify(r, n) if n else ("invalid", None, None, None)
    r["class"] = cls
    r["slack"] = slack
    # A late start can see the whole half consumed: at most n // 2 words.
    r["stale_words"] = min(n // 2, begin - n) if cls == "stale" else 0
    r["flag_names"] = flag_names(r["flags"])
    r["lisr_entry_names"] = lisr_names(r["lisr_entry"])
    r["lisr_exit_names"] = lisr_names(r["lisr_exit"])
    if n:
      h = n // 2
      rx = (n - r["rx_ndtr_entry"]) % n
      r["rx_lateness_words"] = (rx - h * ((r["half"] + 1) % 2)) % n
    if cls != r["firmware_class"]:
      mismatches += 1
    spans = [(r[t] - r["t_entry"]) & 0xFFFFFFFF
             for t in ("t_store_begin", "t_store_end", "t_exit")]
    if not r["flags"] & INVALID_FLAGS and not spans[0] <= spans[1] <= spans[2] < 1 << 31:
      problems.append("timestamps out of order at record %d" % r["index"])
    records.append(r)
  if mismatches:
    problems.append("%d ring records classified differently by firmware" % mismatches)
  admission = list(problems)
  if header["state"] != "complete":
    admission.append("trace not complete")
  if header["reason"] != "window_end" or header["window_invalid"]:
    admission.append("window ended by %s" % header["reason"])
  if header["total"] != WINDOW:
    admission.append("window covers %d blocks, not %d" % (header["total"], WINDOW))
  if generation is not None and header["generation"] != generation:
    admission.append("generation %d, expected %d" % (header["generation"], generation))
  if sample_rate is not None and (
      header["sample_rate"], header["block_size"], header["buffer_words"]) != (
      sample_rate, sample_rate // 4000, sample_rate // 1000):
    admission.append("geometry %d Hz, %d frames, %d words" % (
      header["sample_rate"], header["block_size"], header["buffer_words"]))
  return dict(
    header=header,
    ring=dict(
      count=count,
      first_index=index,
      classes={c: sum(r["class"] == c for r in records) for c in CLASSES},
      firmware_mismatches=mismatches,
    ),
    records=records,
    integrity=problems,
    admitted=not admission,
    admission=admission,
    definitely_stale=totals["stale"],
    ambiguous=totals["ambiguous"],
    invalid=totals["invalid"],
    min_slack_words=None if valid == 0 else totals["min_slack"],
  )


def main():
  p = argparse.ArgumentParser(description=__doc__)
  p.add_argument("dump", type=Path)
  p.add_argument("--generation", type=int)
  p.add_argument("--records", action="store_true", help="include every ring record")
  a = p.parse_args()
  result = decode(a.dump.read_bytes(), a.generation)
  if not a.records:
    result.pop("records")
  print(json.dumps(result, indent=2))


if __name__ == "__main__":
  main()
