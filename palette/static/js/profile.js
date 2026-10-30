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
// Six-byte entries are kept in wire order: id, state, value, page, amount, action.
export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let i = 0; i < 8; ++i) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

export function validate(entries, profile) {
  if (!Array.isArray(entries) || entries.length > 82) throw new Error('Too many features.');
  const seen = new Set();
  const used = [new Set(), new Set(), new Set()];
  for (const entry of entries) {
    if (!Array.isArray(entry) || entry.length !== 6 ||
        entry.some((v) => !Number.isInteger(v) || v < 0 || v > 255)) {
      throw new Error('Invalid feature entry.');
    }
    const [id, state, value, page, amount, action] = entry;
    const f = profile.features.find((f) => f.id === id);
    if (!f || seen.has(id)) throw new Error('Unknown or repeated feature.');
    seen.add(id);
    if (state > 2 || (state === 2 && f.states && !f.states.includes('Adjustable')) || (f.values && value >= f.values.length) ||
        page > 12 || amount > 2 || action > 3 ||
        (page && (!f.page || state !== 2)) ||
        (amount && (!f.amount || state !== 2)) ||
        (action && (!f.action || !state || (state === 1 && (!value || !f.stateless)))) ||
        (id === 9 && state === 1 && value && !action)) {
      throw new Error(`${f.name}: this setting or placement is not allowed.`);
    }
    [page, amount, action].forEach((slot, k) => {
      if (slot && used[k].has(slot)) throw new Error(`${f.name}: control already used.`);
      if (slot) used[k].add(slot);
    });
  }
  return entries;
}

export function encodeProfile(entries, profile) {
  validate(entries, profile);
  const b = new Uint8Array(512).fill(255);
  const v = new DataView(b.buffer);
  v.setUint32(0, 0x46505a44, true);
  b[4] = 1;
  b[5] = entries.length;
  v.setUint16(6, entries.length * 6, true);
  v.setUint32(8, profile.release_id, true);
  v.setUint32(12, 0, true);
  entries.forEach((e, i) => b.set(e, 16 + i * 6));
  const end = 16 + entries.length * 6;
  v.setUint32(end, crc32(b.subarray(0, end)), true);
  return b;
}

export function decodeProfile(b, profile) {
  if (!(b instanceof Uint8Array) || b.length !== 512) throw new Error('Invalid profile size.');
  const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const count = b[5];
  const end = 16 + count * 6;
  if (count > 82 || v.getUint16(6, true) !== count * 6 ||
      v.getUint32(0, true) !== 0x46505a44 || b[4] !== 1 ||
      v.getUint32(8, true) !== profile.release_id || v.getUint32(12, true) !== 0 ||
      v.getUint32(end, true) !== crc32(b.subarray(0, end))) {
    throw new Error('Invalid profile header or checksum.');
  }
  return validate(Array.from({ length: count }, (_, i) =>
    Array.from(b.subarray(16 + i * 6, 22 + i * 6))), profile);
}

export const defaults = (profile) => profile.features.filter((f) => f.default_in_block !== false).map((f) => f.default.slice());
// Canonical editor entries: omitted catalogue defaults stay absent when Off.
// Keep the codec lossless for independently supplied firmware blocks.
export function normalise(entries, profile) {
  validate(entries, profile);
  return entries.filter((e) => e[1] !== 0 ||
    profile.features.find((f) => f.id === e[0]).default_in_block !== false)
    .map((e) => e[1] === 0 ? [e[0], 0, 0, 0, 0, 0] : e.slice());
}
export const equal = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
export async function sha256(b) {
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', b)),
      (v) => v.toString(16).padStart(2, '0')).join('');
}
export const fingerprint = async (block) => (await sha256(block)).slice(0, 8);
export function filenames(release, fp, isDefault) {
  const suffix = isDefault ? '' : `-${fp}`;
  const date = release.tag.replace(/^drizzlet-/, '');
  return { wav: `${release.tag}${suffix}.wav`,
    desktop: isDefault ? `options-guide${release.development ? '-dev' : ''}.svg` : `options-guide-${date}${suffix}.svg`,
    mobile: isDefault ? `options-guide${release.development ? '-dev' : ''}-mobile.svg` : `options-guide-${date}${suffix}-mobile.svg` };
}
export function share(release, block) {
  return new URLSearchParams({ release: release.tag,
    profile: Array.from(block, (v) => v.toString(16).padStart(2, '0')).join('') }).toString();
}
export function unshare(text, releases) {
  const params = new URLSearchParams(text.replace(/^#/, ''));
  const release = releases.find((r) => r.tag === params.get('release'));
  const hex = params.get('profile');
  if (!params.get('release') || !hex || !/^[0-9a-f]{1024}$/i.test(hex)) {
    throw new Error('Invalid shared profile: copy the complete share address and try again.');
  }
  if (!release) throw new Error('This share link was made for another release that this page no longer offers. Choose a release and make a new profile.');
  const block = Uint8Array.from(hex.match(/../g), (v) => parseInt(v, 16));
  const header = new DataView(block.buffer);
  if (header.getUint32(0, true) === 0x46505a44 &&
      (block[4] !== release.profile.version || header.getUint32(8, true) !== release.profile.release_id)) {
    throw new Error('This share link was made for another profile version or release. Open it with the matching page, or make a new profile here.');
  }
  try {
    return { release, entries: decodeProfile(block, release.profile) };
  } catch (error) {
    throw new Error('This shared profile is damaged or uses settings from another page version. Copy the complete share address, or make a new profile here.');
  }
}
export function templates(profile) {
  const stock = defaults(profile).map((e) => [e[0], 0, 0, 0, 0, 0]);
  const themed = (changes) => defaults(profile).map((e) =>
    changes[e[0]] ? [e[0], ...changes[e[0]]] : e);
  return { Drizzlet: defaults(profile), 'Stock plus fixes': stock,
    'Cloud textures': themed({ 2: [2, 1, 5, 0, 0], 12: [2, 96, 0, 2, 0] }),
    'Pitched grains': themed({ 1: [2, 3, 1, 0, 0], 10: [2, 1, 2, 0, 0] }),
    'Manual synth': themed({ 9: [2, 1, 9, 0, 3] }) };
}
export const profileKey = (entries, profile) => Array.from(encodeProfile(entries, profile),
    (v) => v.toString(16).padStart(2, '0')).join('');
// One engine's cost of a setting, in points, relative to the release default; an
// engine the feature cannot run on costs 0, an unmeasured feature is null.
export function featureDelta(entry, feature, engine) {
  if (!feature.cpu || !['measured', 'zero'].includes(feature.cpu.kind)) return null;
  const cpu = feature.cpu.engines[engine];
  if (!cpu) return 0;
  const cost = (e) => {
    if (!e || e[1] === 0) return cpu.off;
    const costs = cpu[e[1] === 1 ? 'fixed' : 'adjustable'];
    return Array.isArray(costs) ? costs[e[2]] : costs.zero + (costs.maximum - costs.zero) * e[2] / 255;
  };
  // The measured engine baseline already includes every default setting.
  // Subtract its cost even for omitted entries (Off); placement has no cost.
  return cost(entry) - cost(feature.default);
}
export function cpuUsage(entries, release) {
  const key = profileKey(entries, release.profile);
  const isDefault = key === profileKey(defaults(release.profile), release.profile);
  const combination = release.cpu.combinations.find((c) => c.profile === key);
  const measured = combination || (isDefault ?
    Object.fromEntries(Object.entries(release.cpu.baselines).map(([e, b]) => [e, b.percent])) :
    null);
  const deltas = release.profile.features.map((f) => ({ feature: f,
    deltas: Object.fromEntries(Object.keys(release.cpu.baselines).map((engine) =>
      [engine, featureDelta(entries.find((e) => e[0] === f.id), f, engine)])) }));
  // Placement does not add DSP work. Off, Fixed Off, and the same value in
  // a lighter mode stay within the default; other enabled values need a cost.
  const withinDefault = (f) => {
    const e = entries.find((e) => e[0] === f.id);
    const d = f.default;
    return !e || e[1] === 0 || (e[1] === 1 && f.values?.[e[2]] === 'Off') ||
      (d[1] !== 0 && e[1] <= d[1] &&
        (f.amount ? e[2] <= d[2] : e[2] === d[2]));
  };
  const unknown = deltas.filter(({ feature: f, deltas }) => Object.values(deltas).some((d) => d === null) &&
    !withinDefault(f)).map(({ feature: f }) => f.name);
  if (!measured && unknown.length) {
    return { engines: [], percent: null, blocked: false, warning: false,
      kind: 'not yet measured', unknown, names: unknown.join(', ') };
  }
  const engines = Object.entries(release.cpu.baselines).map(([engine, baseline]) => {
    const hasMeasurement = Number.isFinite(measured?.[engine]);
    const percent = Math.round((hasMeasurement ? measured[engine] : baseline.percent +
        deltas.reduce((sum, f) => sum + f.deltas[engine], 0)) * 10) / 10;
    return { engine, baseline, percent, measured: hasMeasurement,
      blocked: (!isDefault || Boolean(combination)) && percent > release.cpu.ceiling - (hasMeasurement ? 0 : release.cpu.margin),
      warning: percent >= release.cpu.warning };
  });
  const withinMeasuredDefault = !measured && deltas.some((f) => Object.values(f.deltas).some((d) => d === null)) &&
    deltas.every((f) => Object.values(f.deltas).every((d) => d === null || d <= 0));
  return { engines, withinMeasuredDefault, percent: Math.max(...engines.map((e) => e.percent)),
    blocked: engines.some((e) => e.blocked), warning: engines.some((e) => e.warning),
    kind: withinMeasuredDefault ? 'within measured default' : isDefault && !combination ? 'partly measured' : engines.every((e) => e.measured) ? 'measured' : engines.some((e) => e.measured) ? 'partly measured' : 'estimated',
    names: deltas.filter((f) => Object.values(f.deltas).some((d) => d)).map((f) => f.feature.name).join(', ') || 'Release default settings' };
}

// State choices are catalogue capabilities; Fixed Off means disabled.
export const modes = (feature) => (feature.states || ['Off', 'Fixed', 'Adjustable'])
  .map((name) => [['Off', 'Fixed', 'Adjustable'].indexOf(name), name]);
export const initialEntry = (feature) => feature.default_in_block === false ?
  feature.default.slice() : [feature.id, 0, 0, 0, 0, 0];

// Release tags encode a BCD-like hexadecimal calendar date, not epoch time.
export function releaseIdForTag(tag) {
  const match = /^drizzlet-(\d{4})\.(\d{2})\.(\d{2})$/.exec(tag);
  if (!match) throw new Error('Expected a dated release tag.');
  const [, year, month, day] = match;
  const date = `${year}-${month}-${day}`;
  if (new Date(`${date}T00:00:00Z`).toISOString().slice(0, 10) !== date) {
    throw new Error('Invalid release date.');
  }
  return parseInt(year + month + day, 16) >>> 0;
}
export function validateReleaseId(release, source) {
  const match = /kProfileReleaseId\s*=\s*(0x[0-9a-f]+)/i.exec(source);
  if (!match || Number(match[1]) !== release.profile.release_id ||
      (!release.development && release.profile.release_id !== releaseIdForTag(release.tag))) {
    throw new Error('Release ID must match the firmware and release tag date.');
  }
}
