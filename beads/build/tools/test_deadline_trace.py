# Copyright 2026 Insolent Construct.
# Author: Insolent Construct (insolentconstruct@gmail.com)
# MIT; see beads/build/LICENSE.

import struct
import unittest
import deadline_trace as T

N = 48
CLASS_BITS = {name: i for i, name in enumerate(T.CLASSES)}


def record(index, half, tx, flags=0, cls=None, rx=24, t=(0, 1000, 2000, 3000)):
  r = dict(half=half, tx_ndtr=list(tx), flags=flags)
  if cls is None:
    cls = T.classify(r, N)[0]
  return struct.pack(
    "<IIII3BBBBBBHHBBH", t[0], t[1], t[2], t[3], tx[0], tx[1], tx[2], rx,
    0, 0, half | CLASS_BITS[cls] << 1, flags, 0, 0, 0, 0, index & 0xFFFF)


def dump(records, total, trigger=None, state=4, reason=1, sequence=2,
         generation=1, totals=None, histogram=None, geometry=(48000, 12, 48)):
  b = bytearray(T.SIZE)
  written = total if trigger is None else min(total, trigger + T.POST_TRIGGER + 1)
  first = written - len(records)
  for k, data in enumerate(records):
    struct.pack_into("<32s", b, T.HEADER_SIZE + T.RECORD_SIZE * ((first + k) % T.RECORDS), data)
  struct.pack_into(
    "<IIIBBBbIIIIIIII", b, 0, T.MAGIC, T.VERSION, sequence, state, reason,
    int(reason != 1), 1, generation, geometry[0], geometry[1], geometry[2],
    written % T.RECORDS, total, T.NONE if trigger is None else trigger, 0)
  for i in range(14):
    struct.pack_into("<I", b, 56 + 4 * i, T.NONE)
  if totals is None:
    totals = dict(on_time=total)
  values = [totals.get(name, 0) for name in T.TOTAL_FIELDS]
  values[0] = totals.get("min_slack", 3)
  if histogram is None:
    histogram = [0] * T.SLACK_BINS
    histogram[4] = totals.get("on_time", 0) + totals.get("stale", 0) + totals.get("ambiguous", 0)
  struct.pack_into("<i13IQ20I", b, 112, *(values + histogram))
  return bytes(b)


def ndtr(position):
  return N - position if position else N


class ClassifyTest(unittest.TestCase):
  def check(self, half, positions, cls, slack):
    r = dict(half=half, tx_ndtr=[ndtr(p) for p in positions], flags=0)
    got = T.classify(r, N)
    self.assertEqual((got[0], got[1]), (cls, slack))
    return got

  def test_on_time(self):
    self.check(0, (26, 40, 41), "on_time", 6)

  def test_wrap_other_half(self):
    # Half 1: the DMA reads half 0 at entry, words 2-20.
    self.check(1, (2, 18, 20), "on_time", 3)

  def test_ndtr_wraps_inside_record(self):
    self.check(0, (40, 46, 1), "ambiguous", -2)

  def test_boundaries(self):
    self.check(0, (30, 40, 47), "on_time", 0)
    self.check(0, (30, 40, 0), "ambiguous", -1)
    # Word 0 may only be preloaded at StoreBegin: not yet definite.
    self.check(0, (30, 0, 2), "ambiguous", -3)
    got = self.check(0, (30, 1, 2), "stale", -3)
    self.assertEqual(got[2] - N, 1)

  def test_definitely_stale(self):
    got = self.check(0, (44, 3, 5), "stale", -6)
    self.assertEqual(got[2] - N, 3)

  def test_whole_half_consumed(self):
    # NDTR 4, 18, 17 on half 0: the DMA is past the half at StoreBegin.
    records = [record(0, 0, (4, 18, 17))]
    result = T.decode(dump(records, 1, state=2, reason=0))
    self.assertEqual(result["records"][0]["class"], "stale")
    self.assertEqual(result["records"][0]["stale_words"], N // 2)

  def test_invalid_flags(self):
    for name in ("no_store", "snapshot_long", "coalesced", "cadence", "lap", "spurious"):
      flag = dict(T.FLAGS)[name]
      r = dict(half=0, tx_ndtr=[ndtr(44), ndtr(3), ndtr(5)], flags=flag)
      self.assertEqual(T.classify(r, N)[0], "invalid")
    for name in ("silence", "trigger"):
      r = dict(half=0, tx_ndtr=[ndtr(44), ndtr(3), ndtr(5)], flags=dict(T.FLAGS)[name])
      self.assertEqual(T.classify(r, N)[0], "stale")


class DecodeTest(unittest.TestCase):
  def full(self, **kw):
    records = [record(T.WINDOW - 768 + k, k & 1, (ndtr(26), ndtr(40), ndtr(41)) if not k & 1
                      else (ndtr(2), ndtr(18), ndtr(20))) for k in range(768)]
    return dump(records, T.WINDOW, **kw)

  def test_admitted(self):
    result = T.decode(self.full(), generation=1)
    self.assertTrue(result["admitted"], result["admission"])
    self.assertEqual(result["ring"]["count"], 768)
    self.assertEqual(result["ring"]["first_index"], T.WINDOW - 768)
    self.assertEqual(result["ring"]["classes"]["on_time"], 768)
    self.assertEqual(result["records"][0]["rx_lateness_words"], 0)

  def test_wrong_generation(self):
    self.assertFalse(T.decode(self.full(), generation=2)["admitted"])

  def test_seqlock_tear(self):
    with self.assertRaises(ValueError):
      T.decode(self.full(sequence=3))

  def test_abort_and_geometry(self):
    for reason, name in ((2, "abort"), (3, "geometry")):
      result = T.decode(self.full(reason=reason))
      self.assertFalse(result["admitted"])
      self.assertTrue(result["header"]["window_invalid"])
      self.assertIn("window ended by " + name, result["admission"])

  def test_geometry(self):
    result = T.decode(self.full(geometry=(32000, 8, 32)))
    self.assertFalse(result["admitted"])

  def test_short_window(self):
    records = [record(k, k & 1, (ndtr(26), ndtr(40), ndtr(41)) if not k & 1
                      else (ndtr(2), ndtr(18), ndtr(20))) for k in range(10)]
    result = T.decode(dump(records, 10, state=2, reason=0))
    self.assertFalse(result["admitted"])
    self.assertIn("trace not complete", result["admission"])
    # The activation block only primes the cadence: record 0 is the first full one.
    self.assertEqual(result["records"][0]["index"], 0)
    self.assertEqual(result["integrity"], [])

  def test_frozen_ring(self):
    trigger = 1000
    first = trigger + T.POST_TRIGGER + 1 - 768
    records = []
    for k in range(768):
      index = first + k
      tx = (ndtr(44), ndtr(3), ndtr(5)) if index == trigger else (ndtr(26), ndtr(40), ndtr(41))
      records.append(record(index, 0, tx, flags=128 if index == trigger else 0))
    totals = dict(on_time=T.WINDOW - 1, stale=1, min_slack=-6)
    result = T.decode(dump(records, T.WINDOW, trigger=trigger, totals=totals))
    self.assertTrue(result["admitted"], result["admission"])
    self.assertEqual(result["ring"]["first_index"], first)
    stale = [r for r in result["records"] if r["class"] == "stale"]
    self.assertEqual([r["index"] for r in stale], [trigger])
    self.assertEqual(stale[0]["stale_words"], 3)
    self.assertEqual(result["definitely_stale"], 1)

  def test_coalesced_and_cadence(self):
    records = [record(k, 1, (ndtr(2), ndtr(18), ndtr(20)), flags=8 if k == 3 else 16 if k == 5 else 0)
               for k in range(10)]
    totals = dict(on_time=8, invalid=2, coalesced=1, cadence=1)
    result = T.decode(dump(records, 10, state=2, reason=0, totals=totals,
                           histogram=[0, 0, 0, 0, 8] + [0] * 15))
    self.assertEqual(result["integrity"], [])
    self.assertEqual([r["class"] for r in result["records"]].count("invalid"), 2)
    self.assertEqual(result["records"][3]["flag_names"], ["coalesced"])

  def test_integrity(self):
    records = [record(k, 0, (ndtr(26), ndtr(40), ndtr(41))) for k in range(4)]
    records[2] = record(2, 0, (ndtr(26), ndtr(40), ndtr(41)), cls="stale")
    result = T.decode(dump(records, 4, state=2, reason=0))
    self.assertIn("1 ring records classified differently by firmware", result["integrity"])
    records = [record(k, 0, (ndtr(26), ndtr(40), ndtr(41))) for k in range(4)]
    records[2] = record(7, 0, (ndtr(26), ndtr(40), ndtr(41)))
    self.assertIn("ring sequence break at record 2",
                  T.decode(dump(records, 4, state=2, reason=0))["integrity"])
    result = T.decode(dump(records[:2], 2, totals=dict(on_time=3)))
    self.assertIn("class counts do not add up to the total", result["integrity"])
    # Totals are only published at completion.
    result = T.decode(dump(records[:2], 2, state=2, reason=0, totals=dict(on_time=3)))
    self.assertEqual(result["integrity"], [])
    records = [record(k, 0, (ndtr(26), ndtr(40), ndtr(41)), t=(0, 2000, 1000, 3000)) for k in range(2)]
    self.assertIn("timestamps out of order at record 0",
                  T.decode(dump(records, 2, state=2, reason=0))["integrity"])

  def test_reject(self):
    with self.assertRaises(ValueError):
      T.decode(b"")
    b = bytearray(self.full())
    struct.pack_into("<I", b, 0, 0)
    with self.assertRaises(ValueError):
      T.decode(bytes(b))
    b = bytearray(self.full())
    struct.pack_into("<I", b, 32, 5)
    with self.assertRaises(ValueError):
      T.decode(bytes(b))


if __name__ == "__main__":
  unittest.main()
