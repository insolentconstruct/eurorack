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
# Writes the feature tables from features.json into their source files.
#
# Each output is a whole file or the body between an anchor line and its
# closing line, so the tables stay in the translation units that own them.
# Run from the repository root: python3 beads/build/tools/features.py [--check]

import json
import pathlib
import re
import sys
import zlib

MANIFEST = 'beads/resources/features.json'
FROZEN = 'beads/resources/features-frozen.json'
CATALOGUE = 'beads/build/profile-features.json'

STATES = ['Off', 'Fixed', 'Adjustable']
STATE_NAMES = ['PROFILE_OFF', 'PROFILE_FIXED', 'PROFILE_ADJUSTABLE']
AMOUNT_NAMES = ['AMOUNT_NONE', 'AMOUNT_DRY_WET', 'AMOUNT_REVERB',
    'AMOUNT_FEEDBACK']
ACTION_NAMES = ['ACTION_NONE', 'ACTION_SEED', 'ACTION_FREEZE', 'ACTION_HOLD']
PULSES = ['steady', 'slow', 'fast', 'double']
NOT_USED = 'Not used yet'

# LED mask of each options page (bit 0 FEEDBACK, 1 DRY/WET, 2 REVERB).
PAGE_LEDS = [1, 2, 4, 3, 5, 6, 7]
FACTORY_PROFILE_VERSION = 2
PROFILE_SIZE = 4096


def label(feature):
  # The source comment: the symbol in sentence case.
  return feature['symbol'].replace('_', ' ').capitalize() + '.'


def header_constant(header, name):
  value = re.search(r'\b%s = (0x[0-9a-f]+|\d+);' % name, header).group(1)
  return int(value, 0)


def check_manifest(features):
  for i, feature in enumerate(features, 1):
    assert feature['id'] == i, 'IDs run 1, 2, 3... without gaps: %d' % i
    assert re.match(r'^[A-Z][A-Z0-9_]*$', feature['symbol']), feature
    assert set(feature['states']) <= set(STATES), feature['symbol']
    values = feature['values']
    assert values is None or values, feature['symbol']
    for value in values or []:
      assert value.get('pulse', 'steady') in PULSES, feature['symbol']
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


def page_features(features):
  return [f for f in features if f['page']]


def indicated(feature):
  return [(i, v) for i, v in enumerate(feature['values'] or []) if 'led' in v]


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


def definitions(features):
  lines = ['  { 0, kNoField, false, false, false, false },  // None.']
  for f in features:
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


def option_enum(features):
  lines = ['  OPTION_RESERVED = 0,']
  for f in page_features(features):
    lines.append('  OPTION_%s = %d,' % (f['symbol'], f['id']))
  lines.append('  OPTION_LAST,')
  return lines


def num_option_values(features):
  count = 1 + sum(len(indicated(f)) for f in page_features(features))
  return ['const size_t kNumOptionValues = %d;' % count]


def option_pages(features):
  knobs = [['OPTION_RESERVED'] * 3 for _ in PAGE_LEDS]
  for f in page_features(features):
    slot = f.get('factory', {}).get('page_slot', 0)
    if slot:
      page, knob = (slot - 1) // 3, (slot - 1) % 3
      assert knobs[page][knob] == 'OPTION_RESERVED', 'Page slot %d' % slot
      knobs[page][knob] = 'OPTION_' + f['symbol']
  lines = ['  { %d, { %s } },' % (leds, ', '.join(k))
           for leds, k in zip(PAGE_LEDS, knobs)]
  lines[-1] = lines[-1].rstrip(',')
  return lines


def option_values(features):
  lines = ['  { OPTION_RESERVED, 0, { 0, 0, 0 }, PULSE_STEADY },']
  for f in page_features(features):
    for i, v in indicated(f):
      lines.append('  { OPTION_%s, %d, { %d, %d, %d }, PULSE_%s },' % (
          f['symbol'], i, v['led'][0], v['led'][1], v['led'][2],
          v.get('pulse', 'steady').upper()))
  return lines


def option_names(features):
  names = [NOT_USED] + [f.get('label', f['name']) for f in features]
  return ['  %s,' % json.dumps(n) for n in names]


def value_names(features):
  names = [NOT_USED] + [v['name'] for f in page_features(features)
                        for _, v in indicated(f)]
  return ['  %s,' % json.dumps(n) for n in names]


def catalogue(manifest, page_slots):
  rows = []
  for f in manifest['features']:
    factory = f.get('factory', {'state': 'Off', 'value': 0})
    values = [v['name'] for v in f['values']] if f['values'] else None
    slots = {
      'page_slots': list(range(1, page_slots + 1)) if f['page'] else [],
      'amount_slots': list(range(1, len(AMOUNT_NAMES))) if f['amount'] else [],
      'actions': f['actions'],
    }
    # Format 1 keeps both of its historical row layouts byte for byte.
    if 'issue' in f:
      assert not f['stateless'], f['symbol']
      row = [('id', f['id']), ('issue', f['issue']), ('name', f['name']),
          ('values', values), ('field', f['field']),
          ('states', f['states']), ('default_state', factory['state']),
          ('default', factory['value'])] + list(slots.items()) + [
          ('cpu_cost', None), ('touches_synth', f['touches_synth'])]
    else:
      row = [('id', f['id']), ('name', f['name']), ('values', values),
          ('states', f['states'])] + list(slots.items()) + [
          ('stateless', f['stateless']), ('default_state', factory['state']),
          ('default', factory['value']),
          ('touches_synth', f['touches_synth']), ('cpu_cost', None),
          ('field', f['field'])]
    rows.append(dict(row))
  data = {
    'format': 1,
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
  manifest = json.loads((root / MANIFEST).read_text())
  features = manifest['features']
  check_manifest(features)
  check_frozen(features, json.loads((root / FROZEN).read_text()))
  header = (root / 'beads/profile.h').read_text()
  magic = header_constant(header, 'kProfileMagic')
  profile_format = header_constant(header, 'kProfileFormat')
  page_slots = header_constant(header, 'kNumPageSlots') - 1
  block = profile_block(features, magic, profile_format)
  regions = [
    ('beads/profile.h', 'enum ProfileFeature {', '};',
        feature_enum(features)),
    ('beads/profile.cc',
        'const FeatureDefinition kFeatureDefinitions[FEATURE_LAST] = {', '};',
        definitions(features)),
    ('beads/profile.cc',
        '    __attribute__((section(".profile"), used, aligned(32))) = {',
        '};', block_lines(block)),
    ('beads/profile.cc', '  const ProfileEntry defaults[] = {', '  };',
        defaults(features)),
    ('beads/options.h', 'enum OptionSetting {', '};', option_enum(features)),
    ('beads/options.h', 'const size_t kNumOptionValues = ', None,
        num_option_values(features)),
    ('beads/options.cc',
        'const OptionPage kOptionPages[kNumOptionPages] = {', '};',
        option_pages(features)),
    ('beads/options.cc', 'const OptionValue kOptionValues[] = {', '};',
        option_values(features)),
    ('beads/build/tools/options_tables.cc',
        'const char* const kOptionNames[] = {', '};', option_names(features)),
    ('beads/build/tools/options_tables.cc',
        'const char* const kValueNames[] = {', '};', value_names(features)),
  ]
  files = {}
  for path, start, end, lines in regions:
    text = files.get(path) or (root / path).read_text()
    files[path] = replace(text, start, end, lines, path)
  files[CATALOGUE] = catalogue(manifest, page_slots)
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
