#!/usr/bin/env python3
#
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
"""swap_release.py in a scratch repository with a fake release tag.

The page's current releases keep their real tags; the fake release pins a
small made-up image. Needs the current releases' tags.

  python3 palette/test/swap_release_test.py
"""

import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PALETTE = Path(__file__).resolve().parent.parent
FAKE = 'drizzlet-2001.01.01'
APP = bytes((i * 7 + 3) & 0xff for i in range(4096))

failures = 0


def check(ok, text):
  global failures
  print('%s %s' % ('PASS' if ok else 'FAIL', text))
  failures += not ok


def git(repo, *args, **kwargs):
  return subprocess.run(
      ['git', '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid']
      + list(args), cwd=repo, check=True, capture_output=True, **kwargs).stdout


def tag(repo, name, hashes, wav):
  """Commits the two checksum files and tags the commit."""
  build = repo / 'beads' / 'build'
  build.mkdir(parents=True, exist_ok=True)
  (build / 'expected-hashes.sha256').write_bytes(hashes)
  (build / 'expected-wav.sha256').write_bytes(wav)
  git(repo, 'add', '-A')
  git(repo, 'commit', '-q', '-m', name)
  git(repo, 'tag', name)


def snapshot(static):
  return {path.relative_to(static): path.read_bytes()
          for path in static.rglob('*') if path.is_file()}


def swap(repo, *args):
  result = subprocess.run(
      [sys.executable, str(repo / 'palette' / 'tools' / 'swap_release.py')]
      + list(args), capture_output=True, text=True)
  return result.returncode, result.stdout + result.stderr


def main():
  sys.path.insert(0, str(PALETTE / 'tools'))
  import swap_release

  with tempfile.TemporaryDirectory() as scratch:
    repo = Path(scratch)
    git(repo, 'init', '-q')
    offered = json.loads((PALETTE / 'static' / 'manifest.json').read_text())
    for release in offered['releases']:
      pins = ['refs/tags/%s:beads/build/expected-%s.sha256' % (release['tag'],
                                                               name)
              for name in ('hashes', 'wav')]
      tag(repo, release['tag'], git(PALETTE, 'show', pins[0]),
          git(PALETTE, 'show', pins[1]))
    app_sha256 = hashlib.sha256(APP).hexdigest()
    wav = swap_release.wav_sha256(APP, PALETTE / 'static')
    tag(repo, FAKE, ('%s  app.bin\n' % app_sha256).encode(),
        ('%s  app.wav\n' % wav).encode())
    for folder in ('static', 'test', 'tools'):
      shutil.copytree(PALETTE / folder, repo / 'palette' / folder)
    shutil.copy(PALETTE / 'package.json', repo / 'palette')
    static = repo / 'palette' / 'static'
    image = repo / 'app.bin'
    image.write_bytes(APP)
    old = offered['releases'][0]['tag']
    before = snapshot(static)

    code, log = swap(repo, FAKE, str(image), '--replace', old)
    check(code == 0 and 'manifest test: PASS' in log, 'dry run passes')
    check(snapshot(static) == before, 'dry run changes nothing')

    image.write_bytes(APP[:-1] + b'\0')
    code, log = swap(repo, FAKE, str(image), '--replace', old, '--write')
    check(code == 1 and 'Refused: app.bin' in log, 'unpinned image refused')
    check(snapshot(static) == before, 'refusal changes nothing')
    image.write_bytes(APP)

    code, log = swap(repo, FAKE, str(image), '--replace', 'drizzlet-1999.01.01')
    check(code == 1 and 'is not offered' in log, 'unknown --replace refused')

    code, log = swap(repo, FAKE, str(image), '--label', 'Beta 9 "Haze"',
                     '--replace', old, '--write')
    check(code == 0 and 'manifest test: PASS' in log, 'swap passes')
    manifest = json.loads((static / 'manifest.json').read_text())
    check(manifest['releases'] == [{
        'tag': FAKE,
        'name': 'Drizzlet 2001.01.01 (Beta 9 "Haze")',
        'app': 'images/%s/app.bin' % FAKE,
        'app_size': len(APP),
        'app_sha256': app_sha256,
        'wav_sha256': wav,
        'profile': None,
    }], 'manifest entry replaced, with no profile')
    check((static / 'images' / FAKE / 'app.bin').read_bytes() == APP,
          'image added')
    check(not (static / 'images' / old).exists(), 'old image removed')

    code, log = swap(repo, FAKE, str(image))
    check(code == 1 and 'already offered' in log, 'offered tag refused')
  return 1 if failures else 0


if __name__ == '__main__':
  sys.exit(main())
