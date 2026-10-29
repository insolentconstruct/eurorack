// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see static/js/encoder.js.
//
// Every release the page offers: the pinned app image matches its size and
// SHA-256, those hashes are the ones pinned at the release tag, and the image
// encodes to the release's .wav byte for byte. Needs the release tags.
//
// node palette/test/manifest.js

import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { encode, MAX_APP_SIZE } from '../static/js/encoder.js';

const root = new URL('../static/', import.meta.url);
const hash = (bytes) => createHash('sha256').update(bytes).digest('hex');
const manifest = JSON.parse(readFileSync(new URL('manifest.json', root)));
let failures = 0;
const check = (ok, text) => {
  console.log(`${ok ? 'PASS' : 'FAIL'} ${text}`);
  failures += !ok;
};

check(manifest.format === 1 && manifest.releases.length > 0, 'manifest format 1 with releases');
for (const release of manifest.releases) {
  const app = readFileSync(new URL(release.app, root));
  check(app.length === release.app_size && app.length <= MAX_APP_SIZE,
      `${release.tag}: app size ${app.length}`);
  check(hash(app) === release.app_sha256, `${release.tag}: app SHA-256`);
  // Every release so far is a plain update file, made without a profile.
  check(release.profile === null, `${release.tag}: no profile`);
  check(hash(encode(app)) === release.wav_sha256, `${release.tag}: .wav byte-identical`);
  // The hashes the release was built against, from the tag itself.
  const pinned = (file) => execFileSync('git',
      ['show', `refs/tags/${release.tag}:beads/build/${file}`],
      { encoding: 'utf8' }).split('\n');
  check(pinned('expected-hashes.sha256').includes(`${release.app_sha256}  app.bin`),
      `${release.tag}: app SHA-256 pinned at the tag`);
  check(pinned('expected-wav.sha256').includes(`${release.wav_sha256}  app.wav`),
      `${release.tag}: .wav SHA-256 pinned at the tag`);
}
let refused = false;
try {
  encode(new Uint8Array(MAX_APP_SIZE + 1));
} catch (error) {
  refused = error instanceof RangeError;
}
check(refused, 'an image reaching the frozen-buffer area is refused');
process.exit(failures ? 1 : 0);
