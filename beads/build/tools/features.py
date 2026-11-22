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
# Writes the feature tables from features.json into their source files,
# including the options record's layout.
#
# Each output is a whole file or the body between an anchor line and its
# closing line, so the tables stay in the translation units that own them.
# Run from the repository root: python3 beads/build/tools/features.py [--check]

import hashlib
import json
import pathlib
import re
import sys
import zlib

MANIFEST = 'beads/resources/features.json'
FROZEN = 'beads/resources/features-frozen.json'
CATALOGUE = 'beads/build/profile-features.json'
FEATURES_DOC = 'beads/build/docs/FEATURES.md'
SAVED_TABLE = '| Feature ID | Feature | Saved in |'

STATES = ['Off', 'Fixed', 'Adjustable']
STATE_NAMES = ['PROFILE_OFF', 'PROFILE_FIXED', 'PROFILE_ADJUSTABLE']
AMOUNT_NAMES = ['AMOUNT_NONE', 'AMOUNT_DRY_WET', 'AMOUNT_REVERB',
    'AMOUNT_FEEDBACK']
ACTION_NAMES = ['ACTION_NONE', 'ACTION_SEED', 'ACTION_FREEZE', 'ACTION_HOLD']
PULSES = ['steady', 'slow', 'fast', 'double']
PAGE_CONTROLS = ['none', 'halves', 'zones', 'steps', 'scale']
# Values 0-2 of a page feature light these, steady, unless listed.
OPTION_COLORS = [[0, 255, 0], [255, 255, 0], [255, 0, 0]]
NOT_USED = 'Not used yet'
RESERVED = 'Reserved'

# LED mask of each options page (bit 0 FEEDBACK, 1 DRY/WET, 2 REVERB).
PAGE_LEDS = [1, 2, 4, 3, 5, 6, 7]
# OptionsRecord: the header bytes, then one byte per saved feature.
RECORD_FIRST = 8
RECORD_SIZE = 56
FACTORY_PROFILE_VERSION = 2
PROFILE_SIZE = 4096


def label(feature):
  # The source comment: the symbol in sentence case.
  return feature['symbol'].replace('_', ' ').capitalize() + '.'


def header_constant(header, name):
  value = re.search(r'\b%s = (0x[0-9a-f]+|\d+);' % name, header).group(1)
  return int(value, 0)


def check_manifest(features, reserved):
  # IDs run 1, 2, 3... and skip only the reserved ones.
  for feature in features:
    assert feature['id'] not in reserved, 'ID %d is reserved for %s' % (
        feature['id'], reserved[feature['id']])
  ids = [i for i in range(1, len(features) + len(reserved) + 1)
         if i not in reserved][:len(features)]
  for i, feature in zip(ids, features):
    assert feature['id'] == i, 'IDs run 1, 2, 3... without gaps: %d' % i
    assert re.match(r'^[A-Z][A-Z0-9_]*$', feature['symbol']), feature
    assert set(feature['states']) <= set(STATES), feature['symbol']
    values = feature['values']
    assert values is None or values, feature['symbol']
    for value in values or []:
      assert value.get('pulse', 'steady') in PULSES, feature['symbol']
      assert feature['page'] or 'led' not in value, feature['symbol']
    control = feature.get('page_control', 'none')
    assert (control != 'none') == feature['page'], feature['symbol']
    assert control in PAGE_CONTROLS, feature['symbol']
    if control == 'halves':
      assert len(values) == 2, 'Halves: %s' % feature['symbol']
    if control == 'zones':
      assert len(values) == 3, 'Zones: %s' % feature['symbol']
    if feature['page']:
      for value in values[:len(OPTION_COLORS)]:
        assert 'led' in value, 'Page values 0-2 light: %s' % feature['symbol']
    factory = feature.get('factory')
    if factory:
      assert factory['state'] in feature['states'], feature['symbol']
      assert not factory['page_slot'] or feature['page'], feature['symbol']
  symbols = [f['symbol'] for f in features]
  assert len(set(symbols)) == len(symbols), 'Duplicate symbol'


def check_frozen(features, frozen):
  # Frozen IDs keep their symbol and value meanings; rows and values may only
  # be appended, and a reserved ID waits for the feature it was kept for.
  by_id = dict((f['id'], f) for f in features)
  for row in frozen['features']:
    f = by_id.get(row['id'])
    assert f and f['symbol'] == row['symbol'], 'Frozen ID %d' % row['id']
    values = [v['name'] for v in f['values']] if f['values'] else None
    if row['values'] is None:
      assert values is None, 'Frozen ID %d: values' % row['id']
    else:
      assert values[:len(row['values'])] == row['values'], (
          'Frozen ID %d: value meanings' % row['id'])
  for row in frozen['reserved']:
    assert row['id'] not in by_id, 'ID %d is reserved for %s' % (
        row['id'], row['for'])


def check_record(manifest, frozen):
  # Each record version's layout is frozen; within one, only enumerated
  # features may be added, in bytes no earlier layout used.
  version = manifest['record_version']
  saved = dict((f['id'], f['saved']) for f in manifest['features']
      if 'saved' in f)
  for f in manifest['features']:
    assert f['field'] is None or 'saved' in f, 'Saved: %s' % f['symbol']
  offsets = sorted(saved.values())
  assert len(set(offsets)) == len(offsets), 'Record offsets overlap'
  assert all(RECORD_FIRST <= o < RECORD_SIZE for o in offsets), offsets
  layouts = dict((r['version'], dict(r['saved'])) for r in frozen['records'])
  assert version in layouts, 'Freeze record layout %d' % version
  layout = layouts[version]
  by_id = dict((f['id'], f) for f in manifest['features'])
  for feature_id, offset in layout.items():
    assert saved.get(feature_id) == offset, (
        'Record layout %d: ID %d' % (version, feature_id))
  for feature_id, offset in saved.items():
    if feature_id not in layout:
      assert by_id[feature_id]['values'], (
          'Record layout %d: a full byte needs a new version' % version)
      assert offset > max(layout.values()), (
          'Record layout %d: append after the frozen bytes' % version)


def check_docs(features, doc):
  # FEATURES.md's "Saved data" table gives each saved feature its record byte.
  lines = doc.split('\n')
  begin = lines.index(SAVED_TABLE) + 2
  end = lines.index('', begin)
  by_id = dict((f['id'], f) for f in features)
  rows = {}
  for line in lines[begin:end]:
    feature_id, name, where = [c.strip() for c in line.strip('|').split('|')]
    f = by_id[int(feature_id)]
    assert f['id'] not in rows, 'Saved data: ID %d twice' % f['id']
    assert re.sub(r' \(#\d+\)$', '', name) == f['name'], (
        'Saved data: ID %s name' % feature_id)
    rows[f['id']] = where
  for f in features:
    where = rows.get(f['id'], '')
    match = re.match(r'^(?:Record byte )?(\d+)(, full byte)?$', where)
    if 'saved' in f:
      assert match and int(match.group(1)) == f['saved'] and (
          bool(match.group(2)) == (not f['values'])), (
          'Saved data: ID %d' % f['id'])
    else:
      assert not match, 'Saved data: ID %d is not in the record' % f['id']
  assert list(rows) == sorted(rows), 'Saved data: ID order'


def page_features(features):
  return [f for f in features if f['page']]


def indicated(feature):
  return [(i, v) for i, v in enumerate(feature['values'] or []) if 'led' in v]


def listed(feature):
  # Indications that differ from the shared colours.
  return [(i, v) for i, v in indicated(feature)
          if i >= len(OPTION_COLORS) or v['led'] != OPTION_COLORS[i] or
          v.get('pulse', 'steady') != 'steady']


def feature_enum(features):
  lines = ['  FEATURE_NONE = 0,']
  for f in features:
    lines.append('  FEATURE_%s = %d,' % (f['symbol'], f['id']))
  lines.append('  FEATURE_LAST')
  return lines


def definition(values, f):
  flags = [f['page'], f['amount'], bool(f['actions']), f['stateless']]
  return '  { %s, %s, %s },  // %s' % (
      values, 'kNoField' if f['field'] is None else f['field'],
      ', '.join('true' if flag else 'false' for flag in flags), label(f))


def by_id(features, reserved):
  # Each ID up to the last feature's: the feature, or None while reserved.
  rows = dict((f['id'], f) for f in features)
  return [rows.get(i) for i in range(1, features[-1]['id'] + 1)]


def definitions(features, reserved):
  lines = ['  { 0, kNoField, false, false, false, false },  // None.']
  for f in by_id(features, reserved):
    if f is None:
      # A reserved ID parses only as Off or Fixed at 0, and does nothing.
      lines.append('  { 1, kNoField, false, false, false, false },'
          '  // Reserved.')
      continue
    count = len(f['values']) if f['values'] else 0
    if f.get('values_from') == 'banks':
      lines += [
          '#ifdef PLAITS_BLOBS',
          '  // Up to 15 banks, the field\'s width; Parse checks the profile\'s'
          ' count.',
          definition('kMaxProfileBanks', f),
          '#else',
          definition(count, f),
          '#endif  // PLAITS_BLOBS']
    else:
      lines.append(definition(count, f))
  return lines


def factory_entries(features):
  for f in features:
    factory = f.get('factory')
    if factory:
      yield f, [f['id'], STATES.index(factory['state']), factory['value'],
          factory['page_slot'], factory['amount_slot'], factory['action']]


def profile_block(features, magic, profile_format):
  entries = b''.join(bytes(e) for _, e in factory_entries(features))
  count = len(entries) // 6
  header = (magic.to_bytes(4, 'little') +
      bytes([FACTORY_PROFILE_VERSION, count]) +
      len(entries).to_bytes(2, 'little') +
      profile_format.to_bytes(4, 'little') +
      bytes(4))
  data = header + entries
  data += zlib.crc32(data).to_bytes(4, 'little')
  return data + b'\xff' * (PROFILE_SIZE - len(data))


def block_lines(data):
  return ['  ' + ' '.join('0x%02x,' % b for b in data[i:i + 8])
          for i in range(0, len(data), 8)]


def defaults(features):
  lines = []
  for f, e in factory_entries(features):
    first = '    { FEATURE_%s, %s, %d, %d,' % (
        f['symbol'], STATE_NAMES[e[1]], e[2], e[3])
    second = '%s, %s },  // %s' % (AMOUNT_NAMES[e[4]], ACTION_NAMES[e[5]],
        label(f))
    if len(first) + 1 + len(second) <= 80:
      lines.append(first + ' ' + second)
    else:
      lines += [first, '      ' + second]
  return lines


def record_offsets(features, reserved):
  lines = ['  0,  // None.']
  for f in by_id(features, reserved):
    if f is None:
      lines.append('  0,  // Reserved.')
      continue
    lines.append('  %d,  // %s' % (f.get('saved', 0), label(f)))
  return lines


def page_values(features):
  return ['  PAGE_VALUES_%s = %d,' % (f['symbol'], len(f['values']))
          for f in page_features(features)]


def num_option_values(features):
  count = 1 + sum(len(listed(f)) for f in page_features(features))
  return ['const size_t kNumOptionValues = %d;' % count]


def option_pages(features):
  knobs = [['FEATURE_NONE'] * 3 for _ in PAGE_LEDS]
  for f in page_features(features):
    slot = f.get('factory', {}).get('page_slot', 0)
    if slot:
      page, knob = (slot - 1) // 3, (slot - 1) % 3
      assert knobs[page][knob] == 'FEATURE_NONE', 'Page slot %d' % slot
      knobs[page][knob] = 'FEATURE_' + f['symbol']
  lines = []
  for leds, k in zip(PAGE_LEDS, knobs):
    line = '  { %d, { %s } },' % (leds, ', '.join(k))
    if len(line) > 80:
      line = '  { %d, { %s,\n      %s } },' % (leds, ', '.join(k[:2]), k[2])
    lines.append(line)
  lines[-1] = lines[-1].rstrip(',')
  return lines


def num_page_features(features):
  return ['const int kNumPageFeatures = %d;' % len(page_features(features))]


def page_feature_list(features):
  return ['  FEATURE_%s,' % f['symbol'] for f in page_features(features)]


def page_controls(features, reserved):
  lines = ['  PAGE_CONTROL_NONE,  // None.']
  for f in by_id(features, reserved):
    if f is None:
      lines.append('  PAGE_CONTROL_NONE,  // Reserved.')
      continue
    lines.append('  PAGE_CONTROL_%s,  // %s' % (
        f.get('page_control', 'none').upper(), label(f)))
  return lines


def option_colors():
  lines = ['  { %d, %d, %d },' % tuple(c) for c in OPTION_COLORS]
  lines[-1] = lines[-1].rstrip(',')
  return lines


def option_values(features):
  lines = ['  { FEATURE_NONE, 0, { 0, 0, 0 }, PULSE_STEADY },']
  for f in page_features(features):
    for i, v in listed(f):
      lines.append('  { FEATURE_%s, %d, { %d, %d, %d }, PULSE_%s },' % (
          f['symbol'], i, v['led'][0], v['led'][1], v['led'][2],
          v.get('pulse', 'steady').upper()))
  return lines


def option_names(features, reserved):
  names = [NOT_USED] + [f.get('label', f['name']) if f else RESERVED
      for f in by_id(features, reserved)]
  return ['  %s,' % json.dumps(n) for n in names]


def value_names(features):
  return ['  { beads::FEATURE_%s, %d, %s },' % (
      f['symbol'], i, json.dumps(v['name']))
      for f in page_features(features) for i, v in indicated(f)]


def catalogue(manifest, digest, page_slots):
  rows = []
  factory = dict((f['id'], e) for f, e in factory_entries(manifest['features']))
  for f in manifest['features']:
    default = f.get('factory', {'state': 'Off', 'value': 0})
    rows.append({
      'id': f['id'],
      'name': f['name'],
      'values': [v['name'] for v in f['values']] if f['values'] else None,
      'states': f['states'],
      'page_slots': list(range(1, page_slots + 1)) if f['page'] else [],
      'amount_slots': list(range(1, len(AMOUNT_NAMES))) if f['amount'] else [],
      'actions': f['actions'],
      'stateless': f['stateless'],
      'default_state': default['state'],
      'default': default['value'],
      'factory': factory.get(f['id']),
      'touches_synth': f['touches_synth'],
      'cpu_cost': None,
      'saved': f.get('saved'),
    })
  # Format 2: one row layout; the record byte replaces format 1's field.
  data = {
    'format': 2,
    'manifest_sha256': digest,
    'record_version': manifest['record_version'],
    'features': rows,
    'plaits_models': manifest['plaits_models'],
    'plaits_libraries': manifest['plaits_libraries'],
    'copyright': manifest['catalogue_copyright'],
  }
  return json.dumps(data, indent=2, ensure_ascii=False) + '\n'


def replace(text, start, end, lines, path):
  source = text.split('\n')
  # A region without a closing line is the one line that starts with start.
  first = [i for i, line in enumerate(source)
           if line == start or (end is None and line.startswith(start))]
  assert len(first) == 1, '%s: anchor %r' % (path, start)
  begin = first[0]
  if end is None:
    return '\n'.join(source[:begin] + lines + source[begin + 1:])
  close = source.index(end, begin + 1)
  return '\n'.join(source[:begin + 1] + lines + source[close:])


def outputs(root):
  manifest_bytes = (root / MANIFEST).read_bytes()
  manifest = json.loads(manifest_bytes)
  features = manifest['features']
  frozen = json.loads((root / FROZEN).read_text())
  reserved = dict((row['id'], row['for']) for row in frozen['reserved'])
  check_manifest(features, reserved)
  check_frozen(features, frozen)
  check_record(manifest, frozen)
  check_docs(features, (root / FEATURES_DOC).read_text())
  header = (root / 'beads/profile.h').read_text()
  magic = header_constant(header, 'kProfileMagic')
  profile_format = header_constant(header, 'kProfileFormat')
  page_slots = header_constant(header, 'kNumPageSlots') - 1
  options = (root / 'beads/options.h').read_text()
  assert header_constant(options, 'kNumOptionColors') == len(OPTION_COLORS)
  block = profile_block(features, magic, profile_format)
  regions = [
    ('beads/profile.h', 'enum ProfileFeature {', '};',
        feature_enum(features)),
    ('beads/profile.cc',
        'const FeatureDefinition kFeatureDefinitions[FEATURE_LAST] = {', '};',
        definitions(features, reserved)),
    ('beads/profile.cc',
        '    __attribute__((section(".profile"), used, aligned(32))) = {',
        '};', block_lines(block)),
    ('beads/profile.cc', '  const ProfileEntry defaults[] = {', '  };',
        defaults(features)),
    ('beads/settings.h', 'const uint8_t kOptionsRecordVersion = ', None,
        ['const uint8_t kOptionsRecordVersion = %d;' %
            manifest['record_version']]),
    ('beads/settings.cc',
        'const uint8_t kRecordOffsets[FEATURE_LAST] = {', '};',
        record_offsets(features, reserved)),
    ('beads/options.h', 'enum PageValues {', '};', page_values(features)),
    ('beads/options.h', 'const size_t kNumOptionValues = ', None,
        num_option_values(features)),
    ('beads/options.cc',
        'const OptionPage kOptionPages[kNumOptionPages] = {', '};',
        option_pages(features)),
    ('beads/options.h', 'const int kNumPageFeatures = ', None,
        num_page_features(features)),
    ('beads/options.cc',
        'const ProfileFeature kPageFeatures[kNumPageFeatures] = {', '};',
        page_feature_list(features)),
    ('beads/options.cc',
        'const uint8_t kPageControls[FEATURE_LAST] = {', '};',
        page_controls(features, reserved)),
    ('beads/options.cc',
        'const Leds::Color kOptionColors[kNumOptionColors] = {', '};',
        option_colors()),
    ('beads/options.cc', 'const OptionValue kOptionValues[] = {', '};',
        option_values(features)),
    ('beads/build/tools/options_tables.cc',
        'const char* const kOptionNames[] = {', '};',
        option_names(features, reserved)),
    ('beads/build/tools/options_tables.cc',
        'const NamedValue kValueNames[] = {', '};', value_names(features)),
  ]
  files = {}
  for path, start, end, lines in regions:
    text = files.get(path) or (root / path).read_text()
    files[path] = replace(text, start, end, lines, path)
  files[CATALOGUE] = catalogue(
      manifest, hashlib.sha256(manifest_bytes).hexdigest(), page_slots)
  return files


def stale_outputs(root, write):
  stale = []
  for path, text in sorted(outputs(root).items()):
    if (root / path).read_text() != text:
      stale.append(path)
      if write:
        (root / path).write_text(text)
  return stale


def main(argv):
  check = '--check' in argv
  stale = stale_outputs(pathlib.Path('.'), not check)
  if check and stale:
    print('Stale feature tables (edit %s, then run %s): %s' % (
        MANIFEST, 'make -f beads/build/tools/features.mk', ', '.join(stale)))
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
