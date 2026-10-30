// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see static/js/encoder.js.
//
// All network access is stubbed; never contact analytics from tests.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { downloadCounts, countLine, eventName, recordDownload, recordBuild, afterInteraction } from '../static/js/downloads.js';
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

for (const dnt of ['0', undefined, '1']) {
  const builds = [];
  recordBuild(release, (...args) => builds.push(args), dnt);
  assert.equal(builds.length, dnt === '1' ? 0 : 1);
  if (builds.length) {
    const [url, options] = builds[0];
    assert.deepEqual([...new URL(url).searchParams], [['p', '/render/drizzlet-test'], ['e', 'true']]);
    assert.deepEqual(options, { mode: 'no-cors', credentials: 'omit', referrerPolicy: 'no-referrer', cache: 'no-store' });
  }
}
recordBuild(release, () => { throw new Error('blocked'); }, '0');
recordBuild(release, async () => { throw new Error('offline'); }, '0');
const app = readFileSync(new URL('../static/js/app.js', import.meta.url), 'utf8');
assert.equal((app.match(/recordBuild\(release\)/g) || []).length, 1);
assert(app.indexOf('recordBuild(release)') > app.indexOf('await updateGuides(release, chosen'));
assert(!app.includes('goatcounter.com/count'));
console.log('PASS successful-build event, DNT, private parameters and quiet failures');

// Read-only counters: downloads remain ahead of visitors even when visitors finish first.
const { visitorCounts, mountCounts } = await import('../static/js/downloads.js');
for (const [data, expected] of [[{ count: '1,234' }, 1234], [{ count: 0 }, 0], [{ count: 'bad' }, null]]) {
  const requests = [];
  const visitors = visitorCounts(async (url, options) => {
    requests.push(url);
    assert.equal(options.credentials, 'omit');
    assert.equal(options.referrerPolicy, 'no-referrer');
    assert(options.signal instanceof AbortSignal);
    return response(data);
  }, blockedStorage);
  assert.equal(await visitors(), expected);
  assert.equal(await visitors(), expected);
  assert.deepEqual(requests, ['https://insolentconstruct.goatcounter.com/counter/TOTAL.json']);
}
assert.equal(await visitorCounts(async () => { throw new Error('offline'); }, blockedStorage)(), null);
assert.equal(await visitorCounts(async () => ({ ok: false }), blockedStorage)(), null);
assert.equal(await visitorCounts(async () => ({ ok: true, json: async () => { throw new Error('bad JSON'); } }), blockedStorage)(), null);
assert.equal(await visitorCounts(async () => response({ count: '12' }), storage)(), 12);
assert.equal(await visitorCounts(() => { throw new Error('session must be cached'); }, storage)(), 12);
const container = () => ({ children: [], ownerDocument: { createElement: () => ({}) }, append(p) { this.children.push(p); } });
const panel = container();
let finishDownload;
let reads = 0;
const refreshCounts = mountCounts(panel, [release], () => {
  reads++; return new Promise((resolve) => { finishDownload = resolve; });
}, async () => { reads++; return 400; });
assert.equal(reads, 0, 'mounting counters does not fetch');
assert.equal(panel.children[0].textContent, release.date);
assert(panel.children[1].hidden);
const updating = refreshCounts();
await Promise.resolve();
assert.equal(panel.children[1].textContent, 'Site visitors: 400');
finishDownload(1234);
await updating;
assert.equal(panel.children[0].textContent, 'Beta 2 (Fog): downloaded 1,234 times');
assert.equal(panel.children[0].className, 'download-total');
assert.equal(panel.children[1].className, 'note');
const two = container();
await mountCounts(two, [release, { ...release, tag: 'other', name: 'Beta 3' }], async (r) => r.tag === release.tag ? 12 : 20, async () => null)();
assert.deepEqual(two.children.map((p) => p.textContent), ['Beta 2 (Fog): downloaded 12 times', 'Beta 3: downloaded 20 times', '']);
assert(two.children[2].hidden);
const offline = container();
await mountCounts(offline, [release], async () => { throw new Error('offline'); }, async () => { throw new Error('offline'); })();
assert.equal(offline.children[0].textContent, release.date);
assert(offline.children[1].hidden);
assert(app.includes("afterInteraction(document, mountCounts($('download-counts'), manifest.releases))"));
console.log('PASS deferred downloads-first release totals, secondary visitors, public endpoint, independent failures and cache');
