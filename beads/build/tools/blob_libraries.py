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
# Check every built blob against the catalogue Hygrostat reads: each library's
# ID is catalogued, each of Plaits' 24 models is built, and each model lists
# exactly the catalogue's libraries for its ID, in order, each the library
# blob built here.
#
#   blob_libraries.py OUT [CATALOGUE.json]

import glob
import json
import sys

from blob_sections import blob_ids, blob_kind, KIND_LIBRARY, KIND_SYNTH

NUM_PLAITS_MODELS = 24


def main(argv):
  out = argv[1]
  catalogue = json.load(open(
      argv[2] if len(argv) > 2 else 'beads/build/profile-features.json'))
  models = dict((m['id'], m.get('libraries', []))
                for m in catalogue['plaits_models'])
  known = set(library['id'] for library in catalogue['plaits_libraries'])
  blobs = []
  for path in sorted(glob.glob(out + '/*.bin')):
    data = open(path, 'rb').read()
    if blob_kind(data) == KIND_SYNTH:
      continue  # A synth serves no Plaits model and calls no library.
    evidence = json.load(open(path + '.json'))
    blobs.append((path[len(out) + 1:-len('.bin')], blob_kind(data),
                  blob_ids(data), evidence))
  built = dict((ids[0], evidence['sha256'])
               for _, kind, ids, evidence in blobs if kind == KIND_LIBRARY)
  failures = []
  served = set()
  for name, kind, ids, evidence in blobs:
    if kind == KIND_LIBRARY:
      if ids[0] not in known:
        failures.append('library %s has ID %d, not in the catalogue' % (
            name, ids[0]))
      continue
    listed = [d['id'] for d in evidence['dependencies']]
    for model in ids:
      served.add(model)
      if model not in models:
        failures.append('%s serves model %d, not in the catalogue' % (
            name, model))
      elif listed != models[model]:
        failures.append('%s lists libraries %s, the catalogue %s for %d' % (
            name, listed, models[model], model))
    for d in evidence['dependencies']:
      if built.get(d['id']) != d['sha256']:
        failures.append('%s depends on library %d, not built here' % (
            name, d['id']))
  for model in sorted(set(range(NUM_PLAITS_MODELS)) - served):
    failures.append('model %d has no blob in %s' % (model, out))
  for failure in failures:
    print('LIBRARIES: ' + failure, file=sys.stderr)
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
