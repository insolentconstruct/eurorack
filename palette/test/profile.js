// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Node tests use the compiled firmware as the profile oracle.
import assert from 'node:assert/strict';
import { readFileSync, mkdtempSync, rmSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { webcrypto } from 'node:crypto';
import { defaults, encodeProfile, decodeProfile, crc32, equal, fingerprint,
  profileKey, filenames, share, unshare, cpuUsage, templates } from '../static/js/profile.js';
import { guide } from '../static/js/guide.js';
import { encode } from '../static/js/encoder.js';
import { decode } from '../static/js/decoder.js';
if (!globalThis.crypto) globalThis.crypto = webcrypto;
const manifest = JSON.parse(readFileSync(new URL('../static/manifest.json', import.meta.url)));
const release = manifest.releases[0];
const profile = release.profile;
const entries = defaults(profile);
const block = encodeProfile(entries, profile);
const reseal = (b) => {
  const end = 16 + b[5] * 6;
  new DataView(b.buffer).setUint32(end, crc32(b.subarray(0, end)), true);
  return b;
};
const cases = [];
const reject = (b) => { cases.push(b); assert.throws(() => decodeProfile(b, profile)); };
assert.equal(crc32(new TextEncoder().encode('123456789')), 0xcbf43926);
assert.deepEqual(decodeProfile(block, profile), entries);
const app = readFileSync(new URL('../static/' + release.app, import.meta.url));
assert(equal(block, app.subarray(1024, 1536)), 'default equals firmware build block');
for (let i = 0; i < 104; ++i) {
  const b = block.slice(); b[i] ^= 0x80; reject(b);
}
for (const offset of [0, 4, 5, 6, 7, 8, 12, 13, 14, 15]) {
  const b = block.slice(); b[offset] ^= 1;
  reject([5, 6, 7].includes(offset) ? b : reseal(b));
}
// The refusal table from beads/test/profile_test.cc, parsed so it cannot drift.
const firmwareTests = readFileSync(new URL('../../beads/test/profile_test.cc', import.meta.url), 'utf8');
const bad = firmwareTests.split('const uint8_t bad[][2] = {')[1].split('};')[0];
const constants = { FEATURE_LAST: 15, FEATURE_PITCH: 1, PROFILE_OFF: 0, PROFILE_FIXED: 1 };
for (const match of bad.matchAll(/\{\s*(\d+),\s*(\w+)\s*\}/g)) {
  const b = block.slice(); b[Number(match[1])] = constants[match[2]] ?? Number(match[2]); reject(reseal(b));
}
for (const f of profile.features) {
  for (let kind = 0; kind < 3; ++kind) {
    for (let slot = 1; slot <= [12, 2, 3][kind]; ++slot) {
      const e = [f.id, 2, 0, 0, 0, 0]; e[3 + kind] = slot;
      if (f[['page', 'amount', 'action'][kind]]) {
        assert.deepEqual(decodeProfile(encodeProfile([e], profile), profile), [e]);
      } else assert.throws(() => encodeProfile([e], profile));
    }
  }
  // Exhaust all legal and illegal states, defaults and action placements.
  for (let state = 0; state <= 3; ++state) {
    for (let value = 0; value < (f.values ? f.values.length + 1 : 256); ++value) {
      for (let action = 0; action <= 4; ++action) {
        const e = [f.id, state, value, 0, 0, action];
        const raw = block.slice(); raw[5] = 1; raw[6] = 6; raw[7] = 0;
        raw.set(e, 16); cases.push(reseal(raw));
        const allowed = state <= 2 && (!f.values || value < f.values.length) && action <= 3 &&
          (!action || (f.action && state !== 0 && (state !== 1 || (value && f.stateless)))) &&
          !(f.id === 9 && state === 1 && value && !action);
        if (allowed) assert.deepEqual(decodeProfile(encodeProfile([e], profile), profile), [e]);
        else assert.throws(() => encodeProfile([e], profile));
      }
    }
  }
}
assert.deepEqual(decodeProfile(encodeProfile([], profile), profile), []);
for (const count of [82, 83]) {
  const b = block.slice(); b[5] = count;
  new DataView(b.buffer).setUint16(6, 6 * count, true);
  reject(count === 82 ? reseal(b) : b);
}
for (const chosen of Object.values(templates(profile))) encodeProfile(chosen, profile);
const fp = await fingerprint(block);
assert.equal(fp.length, 8);
assert.match(fp, /^[0-9a-f]{8}$/);
assert.deepEqual(unshare(share(release, block), manifest.releases).entries, entries);
assert.throws(() => unshare('release=missing&profile=00', manifest.releases));
assert.equal(filenames(release, fp, true).wav, release.tag + '.wav');
assert.equal(filenames(release, fp, true).desktop, 'options-guide-dev.svg');
assert.equal(filenames(release, fp, false).wav, `${release.tag}-${fp}.wav`);
assert.equal(filenames(release, fp, false).mobile, `options-guide-dev-a70e669-${fp}-mobile.svg`);
for (const [percent, warning, blocked] of [[85.94, false, false], [85.99, true, false], [87.04, true, false], [87.06, true, true]]) {
  const r = structuredClone(release);
  for (const baseline of Object.values(r.cpu.baselines)) baseline.percent = percent;
  const result = cpuUsage([], r); assert.equal(result.warning, warning); assert.equal(result.blocked, blocked);
}
for (const [percent, blocked] of [[92, false], [92.1, true]]) {
  const r = structuredClone(release); r.cpu.combinations = [{ profile: profileKey([], profile), synth: percent, granular: percent }];
  const result = cpuUsage([], r); assert.equal(result.kind, 'measured'); assert.equal(result.blocked, blocked);
}
for (const engine of ['synth', 'granular']) {
  const r = structuredClone(release);
  for (const baseline of Object.values(r.cpu.baselines)) baseline.percent = 80;
  r.cpu.baselines[engine].percent = 87.1;
  const usage = cpuUsage([], r);
  assert(usage.blocked && usage.warning, `${engine} independently gates the profile`);
  assert(usage.engines.find((e) => e.engine === engine).blocked);
}
const measuredRelease = structuredClone(release);
measuredRelease.cpu.combinations = [{ profile: profileKey(entries, profile), synth: 90, granular: 89 }];
assert.equal(cpuUsage(entries, measuredRelease).kind, 'measured');
const changedPlacement = entries.map((e) => e.slice()); changedPlacement[0][3] = 10;
assert.equal(cpuUsage(changedPlacement, measuredRelease).kind, 'estimated');
const warningProfile = [[7, 1, 1, 0, 0, 0]];
assert(cpuUsage(warningProfile, release).warning);
assert(!cpuUsage(warningProfile, release).blocked);
assert(cpuUsage(entries, release).blocked, 'real manifest heavy combination blocks');
assert.deepEqual(unshare(share(release, block).replace(/profile=([0-9a-f]+)/, (_, hex) => 'profile=' + hex.toUpperCase()), manifest.releases).entries, entries);
// Documented guide normalisation: compare page titles, setting names, value
// labels and LED indications; omit geometry, wrapping and added defaults/fp.
const unescape = (s) => s.replace(/&amp;/g, '&').replace(/&gt;/g, '>').replace(/&lt;/g, '<');
for (const mobile of [false, true]) {
  const svg = guide(entries, release, fp, mobile);
  const golden = readFileSync(new URL(`../../beads/build/docs/options-guide${mobile ? '-mobile' : ''}.svg`, import.meta.url), 'utf8');
  assert.equal(svg.replace(/<text data-profile-fingerprint="true"[^>]*>.*?<\/text>\n/, ''), golden);
  const text = (s) => [...s.matchAll(/<text\b[^>]*>(.*?)<\/text>/g)].map((m) => unescape(m[1]));
  const actual = text(svg), expected = text(golden);
  for (const f of profile.features.filter((f) => f.page)) {
    const e = entries.find((e) => e[0] === f.id);
    const title = `${['FEEDBACK', 'DRY/WET', 'REVERB'][(e[3] - 1) % 3]}: ${f.name}`;
    assert(actual.includes(title) && expected.includes(title), title);
    for (const value of f.values) assert(actual.includes(value) && expected.includes(value), value);
  }
  assert(svg.includes(fp));
  const moved = entries.map((e) => e.slice()); moved[0][3] = 10;
  const custom = guide(moved, release, fp, mobile);
  assert(custom.includes('FEEDBACK: Pitch scale'));
  assert(custom.includes('CC BY-SA 3.0'));
  assert(!custom.includes('MIT licence'));
  assert(svg.includes('y="52" font-size="12"'));
  assert(text(guide([[1, 2, 3, 0, 0, 0]], release, fp, mobile)).join(' ').includes('set on the page: not placed'));
  if (mobile) assert(custom.includes('scale(0.85)'));
  assert(!guide([], release, fp, mobile).includes('PAGE 1'));
}
// Differential coverage: byte mutations of every entry field, all headers,
// every placement kind and sparse profiles, against the actual C++ parser.
for (let offset = 0; offset < 100; ++offset) {
  for (let value = 0; value < 256; ++value) {
    const b = block.slice(); b[offset] = value;
    if (offset !== 5 && offset !== 6 && offset !== 7) reseal(b);
    cases.push(b);
  }
}
for (const f of profile.features) {
  for (let k = 3; k <= 5; ++k) {
    for (let slot = 0; slot <= [12, 2, 3][k - 3]; ++slot) {
      const b = block.slice(); b[5] = 1; b[6] = 6; b[7] = 0;
      b.set([f.id, 2, 0, 0, 0, 0], 16); b[16 + k] = slot;
      cases.push(reseal(b));
    }
  }
}
const scratch = mkdtempSync(join(tmpdir(), 'hygrostat-oracle-'));
try {
  const oracle = join(scratch, 'profile-oracle');
  execFileSync('g++', ['-std=c++98', '-Wall', '-Werror', '-I.',
    ...(process.env.STMLIB_INCLUDE ? ['-I' + process.env.STMLIB_INCLUDE] : []),
    'palette/test/profile_oracle.cc', 'beads/profile.cc', '-o', oracle]);
  const answers = execFileSync(oracle, [], { input: Buffer.concat(cases.map((b) => Buffer.from(b))), maxBuffer: 40000000 });
  const stride = 493;
  assert.equal(answers.length, cases.length * stride);
  const compare = (candidateProfile) => cases.forEach((b, i) => {
    let decoded;
    try { decoded = decodeProfile(b, candidateProfile); } catch {}
    assert.equal(!!decoded, answers[i * stride] === 1, `firmware differential case ${i}`);
    if (decoded) assert.deepEqual(decoded.flat(), Array.from(answers.subarray(i * stride + 1, i * stride + 1 + decoded.length * 6)), `decoded entries case ${i}`);
  });
  compare(profile);
  const mutated = structuredClone(profile);
  mutated.features.find((f) => f.name === 'Monophonic grains').stateless = true;
  assert.throws(() => compare(mutated), /firmware differential/, 'stateless metadata mutation must be detected');
  console.log(`PASS ${cases.length} candidates agree with the compiled firmware parser`);
} finally {
  rmSync(scratch, { recursive: true, force: true });
}
// Exercise packets across a flash-sector boundary, padding and a corrupted WAV.
const image = Uint8Array.from({ length: 131073 }, (_, i) => (i * 71) & 255);
const wav = encode(image);
assert(equal(decode(wav, image.length), image));
const corrupt = wav.slice(); corrupt.fill(0, 192044, 192044 + 1000);
assert.throws(() => decode(corrupt, image.length));
const customEntries = entries.map((e) => e.slice());
customEntries[0] = [1, 1, 3, 0, 0, 0];
const customBlock = encodeProfile(customEntries, profile);
const customApp = new Uint8Array(app);
customApp.set(customBlock, profile.offset);
assert(equal(decode(encode(customApp), customApp.length), customApp));
assert(!equal(customBlock, block));
assert.notEqual(await fingerprint(customBlock), fp);
assert(!equal(customApp, app));
assert(equal(customApp.subarray(0, 1024), app.subarray(0, 1024)));
assert(equal(customApp.subarray(1536), app.subarray(1536)));
console.log('PASS profile defaults, firmware refusals, placements, share, names, CPU, guide normalisation and WAV decode');
