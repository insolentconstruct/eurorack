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
# G1: the Rings code the synth blob compiles is Mutable Instruments' vanilla
# source (beads/blob/synths/rings-vanilla.sha256, master c5f910f) except the
# allowed edits below. Each edit is undone exactly once; every file must then
# hash as pinned. With RINGS_BLOB and RINGS_BLOCK_SIZE undefined, the edited
# and vanilla files must also preprocess to the same tokens.
#
#   rings_vanilla.py check [--cpp CXX]
#   rings_vanilla.py vanilla DIR   (writes the checked vanilla tree)
#   rings_vanilla.py self-test [--cpp CXX]   (faults in copies must fail)

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

PINS = 'beads/blob/synths/rings-vanilla.sha256'

# (file, vanilla text, edited text): the block-size guard and the three
# writable tables made const for the blob.
EDITS = [
  ('rings/dsp/dsp.h',
   'const size_t kMaxBlockSize = 24;\n',
   '#ifndef RINGS_BLOCK_SIZE\n'
   '#define RINGS_BLOCK_SIZE 24\n'
   '#endif  // RINGS_BLOCK_SIZE\n'
   'const size_t kMaxBlockSize = RINGS_BLOCK_SIZE;\n'
   '\n'
   '#ifdef RINGS_BLOB\n'
   '#define RINGS_TABLE const\n'
   '#else\n'
   '#define RINGS_TABLE\n'
   '#endif  // RINGS_BLOB\n'),
  ('rings/dsp/part.h',
   '  static float model_gains_[RESONATOR_MODEL_LAST];\n',
   '  static RINGS_TABLE float model_gains_[RESONATOR_MODEL_LAST];\n'),
  ('rings/dsp/part.cc',
   '// Chord table by Bryan Noll:\nfloat chords[',
   '// Chord table by Bryan Noll:\nRINGS_TABLE float chords['),
  ('rings/dsp/part.cc',
   'float Part::model_gains_[] = {\n',
   'RINGS_TABLE float Part::model_gains_[] = {\n'),
]


def pins(root):
  table = {}
  for line in open(os.path.join(root, PINS)):
    digest, path = line.split()
    table[path] = digest
  return table


def vanilla(root):
  """The vanilla text of every pinned file, and any failures."""
  failures = []
  texts = {}
  pinned = pins(root)
  present = set()
  for base, _, names in os.walk(os.path.join(root, 'rings/dsp')):
    for name in names:
      present.add(os.path.relpath(os.path.join(base, name), root))
  for path in ('rings/resources.cc', 'rings/resources.h'):
    if os.path.exists(os.path.join(root, path)):
      present.add(path)
  for path in sorted(present - set(pinned)):
    failures.append('%s is not vanilla Rings' % path)
  for path in sorted(pinned):
    if path not in present:
      failures.append('%s is missing' % path)
      continue
    text = open(os.path.join(root, path)).read()
    for file, before, after in EDITS:
      if file != path:
        continue
      if text.count(after) != 1:
        failures.append('%s: an allowed edit is not there once' % path)
        continue
      text = text.replace(after, before)
    if hashlib.sha256(text.encode()).hexdigest() != pinned[path]:
      failures.append('%s differs from vanilla beyond the allowed edits' %
                      path)
    texts[path] = text
  return texts, failures


def tokens(cxx, root, path):
  out = subprocess.check_output(
      [cxx, '-E', '-P', '-x', 'c++', '-I', root, os.path.join(root, path)],
      stderr=subprocess.STDOUT).decode()
  return re.sub(r'\s+', ' ', out).strip()


def check(root, cxx):
  texts, failures = vanilla(root)
  if failures or not cxx:
    return failures
  # The edited files, preprocessed as a vanilla build would, equal vanilla.
  with tempfile.TemporaryDirectory() as scratch:
    for path, text in texts.items():
      target = os.path.join(scratch, path)
      os.makedirs(os.path.dirname(target), exist_ok=True)
      open(target, 'w').write(text)
    shutil.copytree(os.path.join(root, 'stmlib'),
                    os.path.join(scratch, 'stmlib'))
    for path in sorted(texts):
      if path.endswith('.cc') and \
          tokens(cxx, root, path) != tokens(cxx, scratch, path):
        failures.append('%s preprocesses differently from vanilla' % path)
  return failures


# Faults in a copy of the tree, each of which the check must find.
FAULTS = [
  ('a byte outside the edits', 'rings/dsp/string.cc',
   lambda t: t.replace('0.07f', '0.08f', 1)),
  ('a fifth RINGS_TABLE', 'rings/dsp/string_synth_part.cc',
   lambda t: t.replace('const int32_t kRegistrationTableSize',
                       'RINGS_TABLE int32_t kRegistrationTableSize', 1)),
  ('an edit undone', 'rings/dsp/part.h',
   lambda t: t.replace('static RINGS_TABLE float', 'static float')),
  ('the original chord table made const too', 'rings/dsp/part.cc',
   lambda t: t.replace('// Original chord table\nfloat chords[',
                       '// Original chord table\nRINGS_TABLE float chords[')),
  ('a new file', 'rings/dsp/extra.h', lambda t: '// extra\n'),
]


def self_test(root, cxx):
  failures = []
  for label, path, change in FAULTS:
    with tempfile.TemporaryDirectory() as scratch:
      for folder in ('rings/dsp', 'stmlib', 'beads/blob/synths'):
        shutil.copytree(os.path.join(root, folder),
                        os.path.join(scratch, folder))
      for name in ('resources.cc', 'resources.h'):
        shutil.copy(os.path.join(root, 'rings', name),
                    os.path.join(scratch, 'rings', name))
      target = os.path.join(scratch, path)
      text = open(target).read() if os.path.exists(target) else ''
      changed = change(text)
      if changed == text:
        failures.append('%s: the fault did not apply' % label)
        continue
      open(target, 'w').write(changed)
      found = check(scratch, cxx)
      print('%s: %s' % (label, found[0] if found else 'NOT DETECTED'))
      if not found:
        failures.append('%s: not detected' % label)
  return failures


def main(argv):
  root = os.getcwd()
  cxx = argv[argv.index('--cpp') + 1] if '--cpp' in argv else None
  command = argv[1] if len(argv) > 1 else 'check'
  if command == 'vanilla':
    texts, failures = vanilla(root)
    if not failures:
      for path, text in texts.items():
        target = os.path.join(argv[2], path)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        open(target, 'w').write(text)
  elif command == 'self-test':
    failures = check(root, cxx) or self_test(root, cxx)
  else:
    failures = check(root, cxx)
  for f in failures:
    print('ERROR: %s' % f)
  if not failures:
    print('rings: vanilla except %d allowed edits' % len(EDITS))
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
