// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see static/js/encoder.js.
//
// All network access is stubbed; never contact analytics from tests.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { downloadCounts, countLine, eventName, recordDownload, afterInteraction } from '../static/js/downloads.js';
const html = readFileSync(new URL('../static/index.html', import.meta.url), 'utf8');
function checkScripts(source) {
  for (const tag of source.matchAll(/<script\b[^>]*>/gi)) {
    const src = tag[0].match(/\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+))/i);
    if (src) assert.equal(new URL(src[1] ?? src[2] ?? src[3], 'https://local.invalid/').origin,
      'https://local.invalid', 'scripts must be local');
  }
  const csp = source.match(/http-equiv="Content-Security-Policy" content="([^"]+)"/i)[1];
  const directives = csp.split(';').map((d) => d.trim().split(/\s+/)).filter((d) => d[0] === 'script-src');
  assert.deepEqual(directives, [['script-src', "'self'"]]);
}
checkScripts(html);
for (const script of ['<script src="https://external.invalid/a.js"></script>', '<script src=//external.invalid/a.js></script>']) {
  assert.throws(() => checkScripts(html + script));
}
assert.throws(() => checkScripts(html.replace("script-src 'self'", "script-src 'self' https://external.invalid")));
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
const send = async (...args) => events.push(args);
recordDownload(release, send, '1');
assert.equal(events.length, 0);
recordDownload(release, send, '0');
assert.equal(events.length, 1);
const [url, options] = events[0];
const query = new URL(url).searchParams;
assert.deepEqual([...query], [['p', 'download/beta-2-fog'], ['e', 'true']]);
assert.deepEqual(options, { mode: 'no-cors', credentials: 'omit', referrerPolicy: 'no-referrer', cache: 'no-store' });
recordDownload(release, () => { throw new Error('blocked'); }, '0');
recordDownload(release, async () => { throw new Error('offline'); }, '0');
const target = new EventTarget();
let starts = 0;
afterInteraction(target, () => starts++);
assert.equal(starts, 0, 'no count request during load');
target.dispatchEvent(new Event('pointerdown'));
target.dispatchEvent(new Event('keydown'));
assert.equal(starts, 1, 'count requests start once on first interaction');
const keyboard = new EventTarget();
afterInteraction(keyboard, () => starts++);
keyboard.dispatchEvent(new Event('keydown'));
assert.equal(starts, 2, 'keyboard interaction also starts counts');
const stored = new Map();
const storage = { getItem: (key) => stored.has(key) ? stored.get(key) : null,
  setItem: (key, value) => stored.set(key, value) };
assert.equal(await downloadCounts(async () => response({ count: '10' }), new Map(), storage)(release), 10);
assert.equal(await downloadCounts(() => { throw new Error('cached session must not fetch'); }, new Map(), storage)(release), 10);
const blockedStorage = { getItem: () => { throw new Error('blocked'); }, setItem: () => { throw new Error('blocked'); } };
assert.equal(await downloadCounts(async () => response({ count: '10' }), new Map(), blockedStorage)(release), 10);
console.log('PASS download event, public count combination, threshold, failures and session cache (stubbed fetches only)');
