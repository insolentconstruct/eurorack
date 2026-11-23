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
# Run every emulator assertion in overlapping profiles that fit bank 1.
# Usage: run_profiles.py OUT [JOBS]

import json
import pathlib
import shutil
import subprocess
import sys

import render_compare as compare

sys.path.insert(0, '.')
from beads.build.tools import blob_sections as sections


def closure(models, switches):
  models = set(models)
  while True:
    targets = {second for first, second in switches if first in models}
    if targets <= models:
      return models
    models.update(targets)


def build_region(path, blobs, extra):
  subprocess.run([sys.executable, 'beads/build/tools/blob_sections.py',
                  str(path), str(compare.RELEASE), '--wave',
                  'beads/resources.cc'] + extra + [str(p) for p in blobs],
                 check=True, stdout=subprocess.DEVNULL)
  return path.stat().st_size


def main(argv):
  out = pathlib.Path(argv[1])
  jobs = argv[2] if len(argv) > 2 else str(compare.multiprocessing.cpu_count())
  pairs = dict(compare.PAIRS)
  switches = list(compare.SWITCHES)
  dependents = tuple(compare.DEPENDENTS)
  blobs = []
  for path in sorted((out / 'blob').glob('*.bin')):
    data = path.read_bytes()
    kind = sections.blob_kind(data)
    ids = (set(sections.blob_ids(data))
           if kind == sections.KIND_MODEL else set())
    blobs.append((path, kind, ids))
  # Keep the stock and original proof models in every fault/control fixture.
  core = set(range(24)) | {29, 31, 32, 36, 37, 39, 40, 41, 43, 254}
  room = compare.REGION_END - max(compare.PLACEMENTS) - 0x2fe0
  profiles = []
  pending = set(pairs) - core
  while pending:
    selected = closure(core, switches)
    for model in sorted(pending):
      candidate = closure(selected | {model}, switches)
      paths = [p for p, kind, ids in blobs
               if kind != sections.KIND_MODEL or ids & candidate]
      size = build_region(out / 'trial.bin', paths, [])
      if size <= room:
        selected = candidate
    if not selected & pending:
      raise RuntimeError('a model and its switch targets do not fit')
    profiles.append(selected)
    pending.difference_update(selected)
  covered_models = set()
  covered_switches = set()
  summaries = []
  result = 0
  for index, models in enumerate(profiles):
    fixture = out / ('profile-%d' % index)
    fixture.mkdir(exist_ok=True)
    for name in ('harness.elf', 'harness.nm'):
      shutil.copyfile(out / name, fixture / name)
    paths = [p for p, kind, ids in blobs
             if kind != sections.KIND_MODEL or ids & models]
    size = build_region(fixture / 'region.bin', paths, [])
    gap_size = build_region(fixture / 'region-gap.bin', paths,
                            ['--gap', '0x2fe0'])
    terrain_size = build_region(fixture / 'region-terr.bin', paths,
                                ['--terrain', str(out / 'terrain.bin')])
    if max(size, gap_size, terrain_size) > (
        compare.REGION_END - max(compare.PLACEMENTS)):
      raise RuntimeError('profile exceeds the existing placement bound')
    for base in compare.PLACEMENTS:
      for address, data in compare.segments(str(fixture / 'harness.elf')):
        if address < base + max(size, gap_size, terrain_size) and (
            base < address + len(data)):
          raise RuntimeError('reference code overlaps a region fixture')
    compare.PAIRS = {m: engine for m, engine in pairs.items() if m in models}
    compare.SWITCHES = [(a, b) for a, b in switches if a in models]
    if any(b not in models for a, b in compare.SWITCHES):
      raise RuntimeError('profile omits a switch target')
    compare.DEPENDENTS = tuple(m for m in dependents if m in models)
    compare.load(str(fixture))
    reference = compare.machine()
    if reference.setup(compare.CTX['layout'], compare.CTX['digests']) != 'OK':
      raise RuntimeError('profile setup failed')
    seed = reference.word(compare.RANDOM_STATE)
    reference.rebuild_compiled(compare.PAIRS[29], 29)
    if reference.word(compare.RANDOM_STATE) != seed:
      raise RuntimeError('unselected reference Init consumed Random state')
    print('profile %d: %d models, %d B, gap %d B, terrain %d B' % (
        index, len(models), size, gap_size, terrain_size), flush=True)
    status = compare.main([argv[0], str(fixture), jobs])
    result = result or status
    covered_models.update(compare.PAIRS)
    covered_switches.update(compare.SWITCHES)
    summaries.append({'profile': index, 'models': sorted(models),
                      'bytes': size, 'gap_bytes': gap_size,
                      'terrain_bytes': terrain_size, 'status': status})
  if covered_models != set(pairs) or covered_switches != set(switches):
    raise RuntimeError('profiles did not cover every model and switch')
  summary = {'profiles': summaries, 'models': sorted(covered_models),
             'switches': sorted(covered_switches), 'status': result}
  (out / 'profiles.json').write_text(json.dumps(summary, indent=2) + '\n')
  return result


if __name__ == '__main__':
  sys.exit(main(sys.argv))
