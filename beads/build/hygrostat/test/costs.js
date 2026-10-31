// Copyright 2026 Insolent Construct.
// MIT licence: see ../static/js/encoder.js.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { costTable, costSource } from '../tools/import-costs.js';
import { defaults, templates, modes, featureDelta, cpuUsage, profileKey,
  encodeProfile, decodeProfile } from '../static/js/profile.js';
const release = JSON.parse(readFileSync(new URL('../static/manifest.json', import.meta.url))).releases[0];
const profile = release.profile;
const engines = ['synth', 'granular'];
const round = (v) => Math.round(v * 10) / 10;
const tableFeature = (f) => costTable.features.find((row) => row.id === f.id);
// Independent oracle: use only each engine's table fields, never its top-level maxima.
function tableCost(entry, f, engine) {
  const cpu = tableFeature(f).cpu;
  if (cpu.kind === 'inspection') return 0;
  const c = cpu.engines[engine];
  if (!c) return 0;
  if (!entry || entry[1] === 0) return c.off;
  const v = c[entry[1] === 1 ? 'fixed' : 'adjustable'];
  if (Array.isArray(v)) return v[entry[2]];
  return (v.zero * (255 - entry[2]) + v.maximum * entry[2]) / 255;
}
function tableDelta(entry, f, engine) {
  return tableCost(entry, f, engine) - tableCost(f.default, f, engine);
}
const sum = (entries, engine) => profile.features.reduce((total, f) =>
  total + tableDelta(entries.find((e) => e[0] === f.id), f, engine), 0);
function choices(f, extremes = false) {
  const values = f.values ? f.values.map((_, i) => i) :
    extremes ? [0, 255] : Array.from({ length: 256 }, (_, i) => i);
  return modes(f).flatMap(([state]) => (state === 0 ? [0] : values).map((value) =>
    [f.id, state, value, 0, 0, f.id === 9 && state === 1 && value ? 1 : 0]));
}
for (const f of profile.features) {
  const cpu = tableFeature(f).cpu;
  assert(!('off' in f.cpu) && !('fixed' in f.cpu) && !('adjustable' in f.cpu));
  for (const engine of engines) {
    if (cpu.kind === 'measured' && !cpu.engines[engine]) {
      assert.match(cpu.note, new RegExp(`${engine}: (0 by construction|not applicable)`));
      assert.equal(f.cpu.engines[engine], undefined);
    }
    for (const entry of choices(f)) {
      assert(Math.abs(featureDelta(entry, f, engine) - tableDelta(entry, f, engine)) < 1e-12,
        `${f.name}, ${engine}, state ${entry[1]}, value ${entry[2]}`);
    }
    assert.equal(featureDelta(undefined, { ...f, cpu: null }, engine), null);
    assert.equal(featureDelta(undefined, { ...f, cpu: { kind: 'unknown' } }, engine), null);
  }
  assert.equal(f.cpu.source, cpu.kind === 'measured' ? costSource : 'no per-block work (code inspection)');
  assert(!JSON.stringify(f.cpu).includes('measured on the module'));
}
assert.equal(featureDelta([2, 1, 1], profile.features[1], 'synth'), 0);
assert.equal(featureDelta([2, 1, 1], profile.features[1], 'granular'), 0);
assert.equal(featureDelta([5, 1, 1], profile.features[4], 'synth'), -0.062);
assert.equal(featureDelta([5, 1, 1], profile.features[4], 'granular'), 0);
// Both the exact default and a valid placement-only edit use the same baseline.
const moved = defaults(profile);
moved.find((e) => e[0] === 1)[3] = 10;
assert.notEqual(profileKey(moved, profile), profileKey(defaults(profile), profile));
assert.deepEqual(decodeProfile(encodeProfile(moved, profile), profile), moved);
for (const entries of [defaults(profile), moved]) {
  for (const result of cpuUsage(entries, release).engines) {
    assert.equal(result.percent, release.cpu.baselines[result.engine].percent);
  }
}
for (const f of profile.features) for (const engine of engines) {
  assert.equal(featureDelta(f.default, f, engine), 0, `${f.name}: default ${engine}`);
}
// A real setting change adds only the measured difference from its default.
const changed = structuredClone(moved);
changed.push([18, 1, 2, 0, 0, 0]);
assert.deepEqual(decodeProfile(encodeProfile(changed, profile), profile), changed);
for (const result of cpuUsage(changed, release).engines) {
  const delta = tableDelta(changed.find((e) => e[0] === 18), profile.features[17], result.engine);
  assert.equal(delta, 0.147);
  assert.equal(result.percent, round(result.baseline.percent + delta));
}
const fixedPitch = structuredClone(moved);
fixedPitch[0] = [1, 1, 0, 0, 0, 0];
assert.equal(featureDelta(fixedPitch[0], profile.features[0], 'granular'), -0.16);
assert.equal(cpuUsage(fixedPitch, release).engines.find((e) => e.engine === 'granular').percent, 82.8);
for (const [name, entries] of Object.entries({ Default: defaults(profile), ...templates(profile) })) {
  assert.deepEqual(decodeProfile(encodeProfile(entries, profile), profile), entries, `${name} builds`);
  const usage = cpuUsage(entries, release);
  assert(!usage.blocked, name);
  for (const result of usage.engines) {
    // The exact default uses the measured baseline, ahead of estimated costs.
    const isDefault = profileKey(entries, profile) === profileKey(defaults(profile), profile);
    assert.equal(result.percent, round(result.baseline.percent + (isDefault ? 0 : sum(entries, result.engine))), name);
    if (!isDefault) assert.equal(result.measured, false);
  }
}
const maxima = [];
for (const engine of engines) {
  const entries = profile.features.map((f) => choices(f, true).reduce((best, e) =>
    tableDelta(e, f, engine) > tableDelta(best, f, engine) ? e : best));
  assert.deepEqual(decodeProfile(encodeProfile(entries, profile), profile), entries);
  const maximum = sum(entries, engine);
  const estimate = round(release.cpu.baselines[engine].percent + maximum);
  maxima.push(`${engine} ${estimate.toFixed(1)}% (delta ${maximum.toFixed(3)} points)`);
  const result = cpuUsage(entries, release).engines.find((e) => e.engine === engine);
  assert.equal(result.percent, estimate);
  assert.equal(result.warning, estimate >= 91);
  assert.equal(result.blocked, estimate > 92);
  for (const percent of [90, 91, 91.5, 92, 92.5]) {
    const measured = structuredClone(release);
    measured.cpu.combinations = [{ profile: profileKey(entries, profile), synth: percent, granular: percent }];
    const usage = cpuUsage(entries, measured);
    assert.equal(usage.percent, percent);
    assert.equal(usage.kind, 'measured');
    assert.equal(usage.warning, percent >= 91);
    assert.equal(usage.blocked, percent > 92);
    assert(usage.engines.every((e) => e.measured && e.percent === percent));
    // Exercise an override below a higher estimate on either engine.
    measured.cpu.baselines[engine].percent = 99;
    assert(cpuUsage(entries, { ...measured, cpu: { ...measured.cpu, combinations: [] } }).percent > percent);
    assert.equal(cpuUsage(entries, measured).percent, percent);
  }
}
let seed = 0x321;
const random = (n) => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed % n; };
for (let i = 0; i < 100; ++i) {
  const entries = profile.features.map((f) => { const options = choices(f); return options[random(options.length)]; });
  const usage = cpuUsage(entries, release);
  for (const result of usage.engines) {
    const delta = sum(entries, result.engine);
    const individual = profile.features.reduce((total, f) =>
      total + featureDelta(entries.find((e) => e[0] === f.id), f, result.engine), 0);
    assert(Math.abs(individual - delta) < 1e-10);
    assert.equal(result.percent, round(result.baseline.percent + delta));
  }
}
const app = readFileSync(new URL('../static/js/app.js', import.meta.url), 'utf8');
assert(app.includes('points versus release default · ' + costSource));
assert(app.includes('estimated from costs ' + costSource));
assert(app.includes("e.measured ? ' measured on the module'"));
assert.equal((app.match(/measured on the module/g) || []).length, 1);
assert(app.includes('No CPU cost (no per-block work)'));
assert(app.includes("engines.length === 1 ? ' only'"));
assert.equal(release.cpu.costs.source, costSource);
console.log(`PASS real costs: templates, exhaustive deltas, random sums, measured precedence, labels; maxima ${maxima.join('; ')}`);
