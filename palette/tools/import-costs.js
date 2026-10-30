// Copyright 2026 Insolent Construct.
// MIT licence: see ../static/js/encoder.js.
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';

export const tableSha256 = 'dc5f7fef75cb5c0435aa52f5513549a17b48f4daf37d2728ba5debbd281063a3';
export const costSource = 'measured on a module-calibrated development board';
export const tableBytes = readFileSync(new URL('../costs/cost-table.json', import.meta.url));
if (createHash('sha256').update(tableBytes).digest('hex') !== tableSha256) {
  throw new Error('Cost table checksum mismatch.');
}
export const costTable = JSON.parse(tableBytes);

export function importedCost(feature, table = costTable) {
  const matches = table.features.filter((f) => f.id === feature.id);
  if (matches.length !== 1 || matches[0].name !== feature.name) {
    throw new Error(`Cost table feature mismatch: ${feature.id} ${feature.name}`);
  }
  const cpu = matches[0].cpu;
  if (!['measured', 'inspection'].includes(cpu.kind)) throw new Error('Unknown table cost kind.');
  const zero = cpu.kind === 'inspection';
  const values = () => feature.values ? feature.values.map(() => 0) : { zero: 0, maximum: 0 };
  const engines = zero ? Object.fromEntries(['synth', 'granular'].map((engine) =>
    [engine, { off: 0, fixed: values(), adjustable: values() }])) : structuredClone(cpu.engines);
  return { kind: zero ? 'zero' : 'measured',
    source: zero ? 'no per-block work (code inspection)' : costSource,
    engines, uncertainty: structuredClone(cpu.uncertainty), note: cpu.note };
}

export function importCosts(manifest) {
  const releases = manifest.releases.filter((r) => r.development);
  if (releases.length !== 1) throw new Error('Expected one development release.');
  const release = releases[0];
  if (release.profile.features.length !== costTable.features.length ||
      new Set(release.profile.features.map((f) => f.id)).size !== costTable.features.length) {
    throw new Error('Cost table feature set mismatch.');
  }
  const costs = release.profile.features.map((f) => importedCost(f));
  release.profile.features.forEach((f, i) => { f.cpu = costs[i]; });
  release.cpu.costs = { source: costSource, firmware: costTable.firmware,
    table: 'costs/cost-table.json', table_sha256: tableSha256 };
  release.cpu.release_note = 'Historical default-profile measurements include Clouds-style reverb. Per-feature costs were measured on a module-calibrated development board; estimates sum each engine’s costs. Measured module combinations take precedence. The final image still needs measurement. The default profile is always allowed.';
  return manifest;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const path = new URL('../static/manifest.json', import.meta.url);
  writeFileSync(path, JSON.stringify(importCosts(JSON.parse(readFileSync(path))), null, 2) + '\n');
  console.log('Imported per-engine CPU costs.');
}
