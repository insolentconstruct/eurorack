#!/usr/bin/env python3
# Copyright 2026 Insolent Construct.
#
# Author: Insolent Construct (insolentconstruct@gmail.com)
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
# The committed feature tables are features.py's output, the frozen IDs hold,
# and each check refuses a deliberate fault in a scratch copy.

import hashlib
import json
import pathlib
import re
import shutil
import sys
import tempfile
import zlib

sys.path.insert(0, 'beads/build/tools')
import features

ROOT = pathlib.Path('.')
FILES = [features.MANIFEST, features.FROZEN, features.CATALOGUE,
    'beads/profile.h', 'beads/profile.cc', 'beads/options.h',
    'beads/options.cc', 'beads/build/tools/options_tables.cc',
    'beads/settings.h', 'beads/settings.cc', features.FEATURES_DOC]


def outcome(directory, path, before, after):
  # Applies one fault to a fresh copy: the generator's refusal, or 'stale'
  # when the committed outputs no longer match.
  for name in FILES:
    (directory / name).parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(ROOT / name, directory / name)
  text = (directory / path).read_text()
  assert text.count(before) == 1, (path, before)
  (directory / path).write_text(text.replace(before, after))
  try:
    return 'stale' if features.stale_outputs(directory, False) else 'clean'
  except AssertionError as error:
    return str(error)


assert not features.stale_outputs(ROOT, False), (
    'Stale feature tables: run make -f beads/build/tools/features.mk')
print('Generated feature tables match features.json: PASS')

# The factory block decodes to the manifest's factory rows, CRC included.
source = (ROOT / 'beads/profile.cc').read_text()
block = source.split('const volatile uint8_t kProfileBlock[kProfileSize]')[1]
block = bytes(int(b, 16) for b in re.findall(
    r'0x([0-9a-f]{2})', block.split('};')[0]))
manifest = json.loads((ROOT / features.MANIFEST).read_text())
rows = list(features.factory_entries(manifest['features']))
size = 16 + 6 * len(rows)
assert len(block) == features.PROFILE_SIZE and block[5] == len(rows)
assert [list(block[16 + 6 * i:22 + 6 * i]) for i in range(len(rows))] == [
    e for _, e in rows]
assert int.from_bytes(block[size:size + 4], 'little') == zlib.crc32(
    block[:size])
assert set(block[size + 4:]) == {0xff}
print('Factory profile block holds the manifest factory rows: PASS')

# Catalogue format 2 names the manifest it came from and its record layout.
catalogue = json.loads((ROOT / features.CATALOGUE).read_text())
assert catalogue['format'] == 2
assert catalogue['manifest_sha256'] == hashlib.sha256(
    (ROOT / features.MANIFEST).read_bytes()).hexdigest()
assert catalogue['record_version'] == manifest['record_version']
assert [c['factory'] for c in catalogue['features'] if c['factory']] == [
    e for _, e in rows]
assert [c.get('saved') for c in catalogue['features']] == [
    f.get('saved') for f in manifest['features']]
print('Catalogue format 2, manifest digest and record bytes: PASS')
DIGEST = '"manifest_sha256": "%s"' % catalogue['manifest_sha256']

frozen = json.loads((ROOT / features.FROZEN).read_text())
assert [r['id'] for r in frozen['features'][:53]] == list(range(1, 54))
assert frozen['features'][53] == {
    'id': 54, 'symbol': 'REVERB_DECAY',
    'values': ['Coupled (stock)', 'Short', 'Medium', 'Long']}
assert frozen['features'][54:] == [{
    'id': 55, 'symbol': 'TIME_CV_RANGE', 'values': ['Stock', 'Fine (1/8)']}]
assert not frozen['reserved']
RESONATOR = '"id": 41,\n      "symbol": "RESONATOR"'
faults = [
  ('Frozen ID 53', features.MANIFEST,
      '"symbol": "MANUAL_DELAY"', '"symbol": "DELAY_TOGGLE"'),
  ('Frozen ID 32: value meanings', features.MANIFEST,
      '{"name": "Send"}', '{"name": "Return"}'),
  ('Frozen ID 50', features.MANIFEST,
      '"symbol": "SYNTH_LEVEL"', '"symbol": "SYNTH_TRIM"'),
  ('Frozen ID 50: value meanings', features.MANIFEST,
      '"name": "Half"', '"name": "Double"'),
  ('Frozen ID 51', features.MANIFEST,
      '"symbol": "OUTPUT_DIVISION"', '"symbol": "OUTPUT_CLOCK"'),
  ('Frozen ID 51: value meanings', features.MANIFEST,
      '"name": "/2"', '"name": "/3"'),
  ('Frozen ID 54: value meanings', features.MANIFEST,
      '{"name": "Coupled (stock)"}', '{"name": "Separate"}'),
  ('Frozen ID 55: value meanings', features.MANIFEST,
      '{"name": "Fine (1/8)"}', '{"name": "Fine (1/4)"}'),
  ('Frozen ID 29', features.MANIFEST,
      '"symbol": "SEED_START"', '"symbol": "SEED_BEGIN"'),
  ('Frozen ID 5: value meanings', features.MANIFEST,
      '{"name": "Fast release", "led": [255, 255, 0]}',
      '{"name": "Hard clip", "led": [255, 255, 0]}'),
  ('Frozen ID 8: value meanings', features.MANIFEST,
      '{"name": "Snap and trigger", "led": [255, 0, 0]}',
      '{"name": "Snap", "led": [255, 0, 0]}'),
  ('without gaps', features.MANIFEST, RESONATOR,
      RESONATOR.replace('41', '43')),
  ('reserved for Rings polyphony', features.FROZEN, '"reserved": []',
      '"reserved": [{"id": 42, "for": "Rings polyphony"}]'),
  ('Frozen ID 7: value meanings', features.MANIFEST,
      '{"name": "Rings", "led": [255, 0, 0]}',
      '{"name": "Resonator", "led": [255, 0, 0]}'),
  ('Frozen ID 42', features.MANIFEST,
      '"symbol": "RINGS_POLYPHONY"', '"symbol": "POLYPHONY"'),
  # A gap is allowed only at a reserved ID.
  ('without gaps', features.MANIFEST, '"id": 49,\n      "symbol"',
      '"id": 51,\n      "symbol"'),
  ('stale', 'beads/profile.cc',
      '{ 5, kNoField, true, false, false, false },  // Resonator.',
      '{ 6, kNoField, true, false, false, false },  // Resonator.'),
  ('stale', 'beads/profile.cc',
      '{ 3, kNoField, false, false, false, false },  // Rings polyphony.',
      '{ 4, kNoField, false, false, false, false },  // Rings polyphony.'),
  ('stale', 'beads/profile.h',
      'FEATURE_RESONATOR = 41,', 'FEATURE_RESONATOR = 40,'),
  ('stale', 'beads/profile.cc',
      '0x1f, 0x02, 0x00, 0x00', '0x1f, 0x02, 0x01, 0x00'),
  ('stale', 'beads/profile.cc',
      '{ FEATURE_TRIM, PROFILE_FIXED, 1, 0,',
      '{ FEATURE_TRIM, PROFILE_FIXED, 0, 0,'),
  ('stale', 'beads/options.cc',
      'FEATURE_PITCH, FEATURE_INTERVALS, FEATURE_ATTACKS',
      'FEATURE_INTERVALS, FEATURE_PITCH, FEATURE_ATTACKS'),
  ('stale', 'beads/options.cc',
      '{ FEATURE_SEED_LATCH, 3, { 0, 255, 0 }, PULSE_SLOW },',
      '{ FEATURE_SEED_LATCH, 3, { 0, 255, 0 }, PULSE_FAST },'),
  ('stale', 'beads/options.h',
      'kNumOptionValues = 22;', 'kNumOptionValues = 23;'),
  ('stale', 'beads/options.h',
      'PAGE_VALUES_DELAY = 3,', 'PAGE_VALUES_DELAY = 4,'),
  ('stale', 'beads/options.cc',
      'PAGE_CONTROL_STEPS,  // Brightness.',
      'PAGE_CONTROL_ZONES,  // Brightness.'),
  ('stale', 'beads/options.cc', '  { 255, 0, 0 }\n};', '  { 255, 0, 32 }\n};'),
  # A value that leaves the shared colours is listed.
  ('stale', features.MANIFEST,
      '{"name": "Evolving", "led": [255, 255, 0]}',
      '{"name": "Evolving", "led": [255, 0, 0]}'),
  ('Zones: REVERB', features.MANIFEST,
      '"field": 1,\n      "saved": 9,\n'
      '      "states": ["Off", "Fixed", "Adjustable"],\n'
      '      "page": true,\n      "page_control": "steps",',
      '"field": 1,\n      "saved": 9,\n'
      '      "states": ["Off", "Fixed", "Adjustable"],\n'
      '      "page": true,\n      "page_control": "zones",'),
  ('Page values 0-2 light: LIMITER', features.MANIFEST,
      '{"name": "Off", "led": [255, 0, 0]}', '{"name": "Off"}'),
  ('stale', 'beads/build/tools/options_tables.cc',
      '"Synth reverb tail",', '"Synth tail",'),
  ('stale', features.CATALOGUE,
      '"name": "Resonator",', '"name": "Rings resonator",'),
  # The options record: each version's layout is frozen.
  ('Record layout 1: ID 2', features.MANIFEST, '"saved": 9,', '"saved": 30,'),
  ('Record layout 1: ID 42', features.MANIFEST, '"saved": 27,',
      '"saved": 29,'),
  ('Record layout 1: byte 29', features.MANIFEST,
      '"symbol": "START_QUALITY",',
      '"symbol": "START_QUALITY",\n      "saved": 29,'),
  ('Record offsets overlap', features.MANIFEST, '"saved": 9,', '"saved": 8,'),
  ('Saved: PITCH', features.MANIFEST, '"saved": 8,\n', ''),
  ('Freeze record layout 2', features.MANIFEST,
      '"record_version": 1,', '"record_version": 2,'),
  ('a full byte needs a new version', features.FROZEN, '[12, 19], ', ''),
  ('append after the frozen bytes', features.FROZEN, '[1, 8], ', ''),
  ('stale', 'beads/settings.cc', '  9,  // Reverb.', '  10,  // Reverb.'),
  ('stale', 'beads/settings.h',
      'kOptionsRecordVersion = 1;', 'kOptionsRecordVersion = 2;'),
  # The catalogue's format, digest and record bytes come from the manifest.
  ('stale', features.CATALOGUE, '"format": 2,', '"format": 1,'),
  ('stale', features.CATALOGUE, DIGEST, DIGEST[:-1] + 'x'),
  ('stale', features.CATALOGUE, '"saved": 23', '"saved": 24'),
  ('stale', features.MANIFEST, '"record_version": 1,', '"record_version":  1,'),
  # FEATURES.md's saved data table follows the record layout.
  ('Saved data: ID 2', features.FEATURES_DOC,
      '| 2 | Reverb character | 9 |', '| 2 | Reverb character | 10 |'),
  ('Saved data: ID 11', features.FEATURES_DOC,
      '| 11 | Stereo width | 18, full byte |', '| 11 | Stereo width | 18 |'),
  ('Saved data: ID 23', features.FEATURES_DOC,
      '| 23 | TIME modulation (#141) | 22 |\n', ''),
  ('Saved data: ID 13 is not in the record', features.FEATURES_DOC,
      '| 13 | Monophonic grains | State byte 9 |',
      '| 13 | Monophonic grains | 24 |'),
  ('Saved data: ID 2 twice', features.FEATURES_DOC,
      '| 2 | Reverb character | 9 |',
      '| 2 | Reverb character | 8 |\n| 2 | Reverb character | 9 |'),
  ('Saved data: ID 5 name', features.FEATURES_DOC,
      '| 5 | Limiter |', '| 5 | Soft clip |'),
  # Values may be appended after the frozen ones.
  ('stale', features.MANIFEST,
      '{"name": "Low"},\n        {"name": "High"}',
      '{"name": "Low"},\n        {"name": "High"}, {"name": "Highest"}'),
]
with tempfile.TemporaryDirectory(prefix='features-') as directory:
  for expected, path, before, after in faults:
    result = outcome(pathlib.Path(directory), path, before, after)
    assert expected in result, (expected, result)
print('%d deliberate changes caught for the right reason: PASS' % len(faults))
