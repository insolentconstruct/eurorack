// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see static/js/encoder.js.
//
// All network access is stubbed; never contact analytics from tests.
import assert from 'node:assert/strict';
import { downloadCounts, countLine, eventName, recordDownload } from '../static/js/downloads.js';
globalThis.fetch = () => { throw new Error('Live network forbidden'); };
const release = { name: 'Beta 2 (Fog)', tag: 'drizzlet-test', slug: 'beta-2-fog', date: '2026-10-29' };
assert.equal(eventName(release), 'download/beta-2-fog');
const response = (data) => ({ ok: true, json: async () => data });
const assets = { assets: [{ name: 'a-wav.zip', download_count: 3 }, { name: 'b-wav.zip', download_count: 2 }, { name: 'app.zip', download_count: 1000 }] };
for (const [goat, github, total] of [
  [{ count: '5' }, assets, 10], [{ count: '1,234' }, assets, 1239],
  [null, assets, 5], [{ count: '10' }, null, 10], [null, null, null],
  [{ count: 'bad' }, assets, 5], [{ count: -1 }, {}, null],
  [{ count: '4' }, { assets: [{ name: 'a-wav.zip', download_count: -1 }] }, 4]
]) {
  const urls = [];
  const get = downloadCounts(async (url) => {
    urls.push(url);
    const data = url.includes('goatcounter') ? goat : github;
    if (data === null) throw new Error('offline');
    return response(data);
  });
  const [a, b] = await Promise.all([get(release), get(release)]);
  assert.equal(a, total); assert.equal(b, total); assert.equal(urls.length, 2);
  assert(urls[0].endsWith('/counter/download%2Fbeta-2-fog.json'));
  assert(urls[1].endsWith('/releases/tags/drizzlet-test'));
  assert.equal(await get(release), total); assert.equal(urls.length, 2);
}
assert.equal(await downloadCounts(async () => ({ ok: false, status: 429 }))(release), null);
assert.equal(await downloadCounts(async () => ({ ok: true, json: async () => { throw new Error('invalid JSON'); } }))(release), null);
assert.equal(countLine(release, 9), release.date);
assert.equal(countLine(release, 10), 'Beta 2 (Fog): downloaded 10 times');
assert.equal(countLine(release, null), release.date);
assert.equal(countLine({ name: 'Unknown' }, null), '');
const events = [];
recordDownload(release, { count: (event) => events.push(event) }, '1');
assert.equal(events.length, 0);
recordDownload(release, { count: (event) => events.push(event) }, '0');
assert.deepEqual(events, [{ path: 'download/beta-2-fog', event: true, title: '', referrer: '' }]);
recordDownload(release, undefined, '0');
recordDownload(release, { count: () => { throw new Error('blocked'); } }, '0');
const stored = new Map();
const storage = { getItem: (key) => stored.has(key) ? stored.get(key) : null,
  setItem: (key, value) => stored.set(key, value) };
assert.equal(await downloadCounts(async () => response({ count: '10' }), new Map(), storage)(release), 10);
assert.equal(await downloadCounts(() => { throw new Error('cached session must not fetch'); }, new Map(), storage)(release), 10);
const blockedStorage = { getItem: () => { throw new Error('blocked'); }, setItem: () => { throw new Error('blocked'); } };
assert.equal(await downloadCounts(async () => response({ count: '10' }), new Map(), blockedStorage)(release), 10);
console.log('PASS download event, public count combination, threshold, failures and session cache (stubbed fetches only)');
