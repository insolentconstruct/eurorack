// Copyright 2026 Insolent Construct.
// Author: Insolent Construct.
// MIT licence: see ../static/js/encoder.js.
// Release-readiness regressions use the shipped catalogue and manifest.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, readdirSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { defaults, encodeProfile, equal, cpuUsage, profileKey, modes,
  releaseIdForTag, validateReleaseId, initialEntry } from '../static/js/profile.js';
import { guide } from '../static/js/guide.js';
const text = (svg) => [...svg.matchAll(/<text\b[^>]*>(.*?)<\/text>/g)].map((m) => m[1]).join(' ');
const root = new URL('../static/', import.meta.url);
const read = (p) => readFileSync(new URL(p, root), 'utf8');
const manifest = JSON.parse(read('manifest.json'));
const catalogue = JSON.parse(read('profile-features.json'));
const release = manifest.releases[0], profile = release.profile;
const source = (file) => execFileSync('git', ['show', `${release.development ? release.source_commit : 'refs/tags/' + release.tag}:${file}`], { encoding: 'utf8' });
const table = source('beads/profile.cc').split('kFeatureDefinitions[FEATURE_LAST] = {')[1].split('};')[0];
const definitions = [...table.matchAll(/\{ (\d+), (\w+), (true|false), (true|false), (true|false), (true|false) \}/g)].slice(1);
assert.equal(profile.features.length, 23);
assert.equal(catalogue.features.length, definitions.length);
for (const [i, definition] of definitions.entries()) {
  const f = profile.features[i], c = catalogue.features[i];
  assert.equal(f.id, i + 1);
  assert.equal(c.id, f.id);
  assert.deepEqual(f.values, c.values);
  assert.equal(f.name, c.name);
  assert.equal(f.field, c.field);
  assert.equal(f.field, definition[2] === 'kNoField' ? null : Number(definition[2]));
  assert.deepEqual(f.states, c.states);
  assert.equal(f.synth_path, c.touches_synth);
  assert.equal(f.values?.length || 0, Number(definition[1]));
  for (const [key, index, slots] of [['page', 3, 'page_slots'], ['amount', 4, 'amount_slots'], ['action', 5, 'actions']]) {
    assert.equal(f[key], definition[index] === 'true', `${f.name}: firmware ${key}`);
    assert.equal(f[key], c[slots].length > 0, `${f.name}: catalogue ${key}`);
  }
  assert.equal(f.stateless, definition[6] === 'true');
  assert.deepEqual(modes(f).map(([, label]) => label), c.states);
  if (f.id >= 15) {
    assert.equal(f.default_in_block, false);
    assert.deepEqual(initialEntry(f), [f.id, 0, c.default, 0, 0, 0]);
  }
}
assert.deepEqual(profile.features.find((f) => f.id === 21).values, ['Off', 'Ignore', 'Sync', 'Add']);
assert.deepEqual(profile.features.find((f) => f.id === 18).values, ['100%', '66%', '33%']);
assert.equal(release.cpu.baselines.synth.percent, 87.2);
assert(release.cpu.baselines.synth.provenance.includes('86.1–87.2%'));
const entries = defaults(profile);
assert.equal(entries.length, 14);
const app = readFileSync(new URL(release.app, root));
assert(equal(encodeProfile(entries, profile), app.subarray(1024, 1536)));
// A missing measured engine uses its own estimate and still gates the profile.
for (const engine of ['synth', 'granular']) {
  const r = structuredClone(release);
  r.cpu.combinations = [{ profile: profileKey([], profile), [engine]: 90 }];
  const result = cpuUsage([], r);
  assert(Number.isFinite(result.percent));
  assert.equal(result.kind, 'partly measured');
  assert.equal(result.engines.find((e) => e.engine === engine).percent, 90);
  assert.equal(result.engines.find((e) => e.engine !== engine).measured, false);
  assert.equal(result.engines.find((e) => e.engine !== engine).percent,
    cpuUsage([], release).engines.find((e) => e.engine !== engine).percent);
  const unmeasured = Object.keys(r.cpu.baselines).find((e) => e !== engine);
  r.cpu.baselines[unmeasured].percent = 99;
  assert(cpuUsage([], r).blocked, 'missing engine still enforces its estimated ceiling');
}
// Conservative DSP estimates must exercise the real warning/block controls.
const warning = entries.map((e) => e.slice());
for (const [id, value] of [[2, 1], [8, 2], [12, 255]]) warning.find((e) => e[0] === id)[2] = value;
assert(cpuUsage(warning, release).warning && !cpuUsage(warning, release).blocked);
const heavy = warning.map((e) => e.slice());
for (const [id, value] of [[1, 9], [10, 1], [11, 255]]) heavy.find((e) => e[0] === id)[2] = value;
assert(cpuUsage(heavy, release).blocked);
for (const f of profile.features) assert(f.cpu.placeholder && f.cpu.kind === 'estimate');
for (const mobile of [false, true]) {
  for (const id of [9, 13, 14]) {
    const svg = guide([[id, 2, 1, 0, 0, 3]], release, 'test', mobile);
    assert(text(svg).includes('Both inputs unplugged.'));
  }
  const empty = guide([], release, 'test', mobile);
  assert(text(empty).includes('no options pages'));
  for (const instruction of ['Open:', 'Change page:', 'Close:']) assert(!empty.includes(instruction));
  const latch = guide([[21, 2, 3, 1, 0, 0]], release, 'test', mobile);
  for (const label of ['Off', 'Ignore', 'Sync', 'Add']) {
    assert(latch.includes(`>${label}</text>`));
  }
  assert.equal((latch.match(/stroke-width="8"/g) || []).length, 3, 'four zones have three boundaries');
  const brightness = guide([[18, 2, 2, 1, 0, 0]], release, 'test', mobile);
  for (const label of ['100%', '66%', '33%']) assert(brightness.includes(`>${label}</text>`));
}
assert.equal(releaseIdForTag('drizzlet-2026.11.05'), 0x20261105);
const dated = { tag: 'drizzlet-2026.11.05', profile: { release_id: 0x20261105 } };
validateReleaseId(dated, 'const uint32_t kProfileReleaseId = 0x20261105;');
assert.throws(() => validateReleaseId(dated, 'kProfileReleaseId = 0xa51e8760;'));
assert.throws(() => validateReleaseId({ ...dated, profile: { release_id: 0xa51e8760 } }, 'kProfileReleaseId = 0xa51e8760;'));
validateReleaseId(release, source('beads/profile.h'));
// Follow every static dependency, including worker and nested module imports.
const version = manifest.asset_version;
assert.match(version, /^[a-f0-9]{16}$/);
const unversion = (s) => s.replace(/\?v=[a-f0-9]+/g, '');
const bare = structuredClone(manifest); delete bare.asset_version;
for (const r of bare.releases) r.app = unversion(r.app);
const modules = readdirSync(new URL('js/', root)).filter((p) => p.endsWith('.js')).sort();
assert.equal(version, createHash('sha256').update(JSON.stringify(bare) +
  modules.map((p) => unversion(read('js/' + p))).join('')).digest('hex').slice(0, 16),
  'run version-assets after changing the manifest or modules');

const paths = [...read('index.html').matchAll(/src="([^"]+)"/g)].map((m) => m[1]);
for (const file of readdirSync(new URL('js/', root)).filter((f) => f.endsWith('.js'))) {
  paths.push(...[...read('js/' + file).matchAll(/['"]((?:\.\/|js\/)[^'"\n]+\.js[^'"\n]*|manifest\.json[^'"\n]*)['"]/g)].map((m) => m[1]));
}
assert(paths.length > 10);
for (const path of paths) assert.equal(new URL(path, 'https://local.invalid/').searchParams.get('v'), version, path);
for (const r of manifest.releases) assert.equal(new URL(r.app, 'https://local.invalid/').searchParams.get('v'), r.app_sha256);
const workflow = readFileSync('.github/workflows/pages.yml', 'utf8');
assert(workflow.includes('  pull_request:'));
assert(workflow.includes("github.event_name != 'pull_request' && github.ref == 'refs/heads/drizzlet'"));
const html = read('index.html');
for (const text of ['How to install', 'storage-recovery-2026.10.27', ':focus-visible', 'min-height: 44px', 'Made by Insolent Construct']) assert(html.includes(text));
console.log(`PASS readiness catalogue, default bytes, partial measurements, real CPU thresholds (${cpuUsage(warning, release).percent}/${cpuUsage(heavy, release).percent}), guides, release IDs, assets and PR CI`);
