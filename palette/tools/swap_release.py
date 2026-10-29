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
"""Offer a new release on the Hygrostat page.

Checks the app image against the hashes pinned at the release tag (app.bin
and the .wav it encodes to), optionally removes the release it replaces, adds
the image and the manifest entry, and runs the manifest test, on a temporary
copy. --write then applies it to palette/ if the test passed.

  python3 palette/tools/swap_release.py drizzlet-2026.10.30 APP.bin \\
      --label 'Beta 2 "Fog"' --replace drizzlet-2026.10.18 [--write]

Without APP.bin, the app image is cut from the release's combo download.
"""

import argparse
import hashlib
import io
import json
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

PALETTE = Path(__file__).resolve().parent.parent
DOWNLOAD = 'https://github.com/insolentconstruct/eurorack/releases/download'
BOOTLOADER_SIZE = 0x20000  # The app starts at 0x08020000 in combo.bin.

# Prints the SHA-256 of the .wav that the page's encoder makes from an image.
ENCODE = (
    "const { encode } = await import(process.argv[1]);"
    "const { createHash } = await import('node:crypto');"
    "const { readFileSync } = await import('node:fs');"
    "const wav = encode(readFileSync(process.argv[2]));"
    "console.log(createHash('sha256').update(wav).digest('hex'));")


class Refused(Exception):
  pass


def sha256(data):
  return hashlib.sha256(data).hexdigest()


def pinned(tag, name):
  """Returns {file: sha256} from a checksum file at the release tag."""
  try:
    text = subprocess.run(
        ['git', 'show', 'refs/tags/%s:beads/build/%s' % (tag, name)],
        cwd=PALETTE, check=True, capture_output=True, text=True).stdout
  except subprocess.CalledProcessError:
    raise Refused('no %s at tag %s (fetch the tags)' % (name, tag))
  pins = {}
  for line in text.splitlines():
    digest, _, file = line.partition('  ')
    pins[file] = digest
  return pins


def download_app(tag, pins):
  url = '%s/%s/%s-combo.zip' % (DOWNLOAD, tag, tag)
  with urllib.request.urlopen(url) as response:
    archive = zipfile.ZipFile(io.BytesIO(response.read()))
  combo = archive.read('%s-combo.bin' % tag)
  if sha256(combo) != pins.get('combo.bin'):
    raise Refused('%s: combo.bin is not the one pinned at the tag' % url)
  return combo[BOOTLOADER_SIZE:]


def wav_sha256(app, static):
  with tempfile.NamedTemporaryFile(suffix='.bin') as image:
    image.write(app)
    image.flush()
    encoder = (static / 'js' / 'encoder.js').as_uri()
    result = subprocess.run(
        ['node', '--input-type=module', '-e', ENCODE, encoder, image.name],
        capture_output=True, text=True)
  if result.returncode:
    raise Refused('the encoder refused the image: %s' % result.stderr.strip())
  return result.stdout.strip()


def entry(tag, label, app, static):
  """Returns the manifest entry for an image, if both hashes are pinned."""
  hashes = pinned(tag, 'expected-hashes.sha256')
  wavs = pinned(tag, 'expected-wav.sha256')
  app_sha256 = sha256(app)
  if app_sha256 != hashes.get('app.bin'):
    raise Refused('app.bin %s is not the one pinned at %s' % (app_sha256, tag))
  wav = wav_sha256(app, static)
  if wav != wavs.get('app.wav'):
    raise Refused('the .wav %s is not the one pinned at %s' % (wav, tag))
  name = 'Drizzlet ' + tag.split('-', 1)[1]
  if label:
    name += ' (%s)' % label
  return {
      'tag': tag,
      'name': name,
      'app': 'images/%s/app.bin' % tag,
      'app_size': len(app),
      'app_sha256': app_sha256,
      'wav_sha256': wav,
      'profile': None,  # Plain update file, no profile block.
  }


def swap(static, new, app, replace):
  """Applies the swap to a static/ folder."""
  path = static / 'manifest.json'
  manifest = json.loads(path.read_text())
  releases = manifest['releases']
  tags = [release['tag'] for release in releases]
  image = static / new['app']
  if new['tag'] in tags or image.parent.exists():
    raise Refused('%s is already offered' % new['tag'])
  if replace and replace not in tags:
    raise Refused('%s is not offered' % replace)
  if replace:
    old = static / releases.pop(tags.index(replace))['app']
    old.unlink()
    old.parent.rmdir()
  image.parent.mkdir(parents=True)
  image.write_bytes(app)
  for release in releases:
    release.setdefault('profile', None)
  releases.append(new)
  path.write_text(json.dumps(manifest, indent=2) + '\n')
  return [release['tag'] for release in releases]


def manifest_test(test):
  result = subprocess.run(['node', str(test / 'manifest.js')], cwd=PALETTE,
                          capture_output=True, text=True)
  return result.returncode == 0, result.stdout + result.stderr


def main():
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument('tag', help='release tag, e.g. drizzlet-2026.10.30')
  parser.add_argument('app', nargs='?', type=Path,
                      help='app.bin (default: from the release download)')
  parser.add_argument('--label', help='shown after the date, e.g. Beta 2 "Fog"')
  parser.add_argument('--replace', metavar='TAG', help='release to remove')
  parser.add_argument('--write', action='store_true',
                      help='apply to palette/ if the test passes')
  args = parser.parse_args()

  try:
    if args.app:
      app = args.app.read_bytes()
    else:
      app = download_app(args.tag, pinned(args.tag, 'expected-hashes.sha256'))
    new = entry(args.tag, args.label, app, PALETTE / 'static')
    # Always on a copy first, so a refusal or a failed test changes nothing.
    with tempfile.TemporaryDirectory() as scratch:
      copy = Path(scratch)
      for folder in ('static', 'test'):
        shutil.copytree(PALETTE / folder, copy / folder)
      shutil.copy(PALETTE / 'package.json', copy)
      offered = swap(copy / 'static', new, app, args.replace)
      passed, log = manifest_test(copy / 'test')
    if passed and args.write:
      swap(PALETTE / 'static', new, app, args.replace)
  except Refused as error:
    print('Refused: %s' % error)
    return 1

  print(log, end='')
  written = passed and args.write
  print('%s: %s, %d bytes' % ('Wrote' if written else 'Dry run', new['name'],
                              new['app_size']))
  print('  app.bin %s (pinned at the tag)' % new['app_sha256'])
  print('  app.wav %s (pinned at the tag)' % new['wav_sha256'])
  if args.replace:
    print('  removed %s' % args.replace)
  print('  offered: %s' % ', '.join(offered))
  print('  manifest test: %s' % ('PASS' if passed else 'FAIL'))
  if passed and not written:
    print('Nothing changed; add --write to apply.')
  return 0 if passed else 1


if __name__ == '__main__':
  sys.exit(main())
