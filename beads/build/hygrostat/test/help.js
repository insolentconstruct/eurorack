// Copyright 2026 Insolent Construct. MIT licence: see ../static/js/encoder.js.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { featureHelp, actionHelp } from '../static/js/help.js';
const read = (file) => readFileSync(new URL(file, import.meta.url), 'utf8');
const manifest = JSON.parse(read('../static/manifest.json'));
const docs = read('../../docs/FEATURES.md');
const headings = [...docs.matchAll(/^#{2,3} (.+)$/gm)].map((m) => m[1].toLowerCase().replace(/[^\w\s-]/g, '').replace(/\s/g, '-'));
for (const release of manifest.releases) {
  for (const feature of release.profile.features) {
    const help = featureHelp(feature, release);
    assert(help.description.length > 15 && help.description.length < 180, feature.name);
    assert(!help.description.includes('\n'), 'one line of plain language');
    const url = new URL(help.url);
    assert.equal(url.origin, 'https://github.com');
    assert.equal(url.pathname, `/insolentconstruct/eurorack/blob/${release.development ? release.source_commit : release.tag}/beads/build/docs/FEATURES.md`);
    assert(headings.includes(url.hash.slice(1)), `${feature.name}: real FEATURES.md section`);
    const tagged = { ...release, development: false, tag: 'drizzlet-2026.11.05' };
    assert(featureHelp(feature, tagged).url.includes('/blob/drizzlet-2026.11.05/'));
    assert(!featureHelp(feature, tagged).url.includes(release.source_commit));
  }
}
assert.match(actionHelp({ id: 13 }), /action to toggle.*no page knob/);
assert.match(actionHelp({ id: 13 }), /Fixed.*without a toggle/);
assert.match(actionHelp({ id: 14 }), /Fixed On or Adjustable.*gesture/);
assert.match(actionHelp({ id: 9 }), /Manual.*action/);
assert.equal(actionHelp({ id: 18 }), '');
assert.match(featureHelp({ id: 16 }, manifest.releases[0]).description, /automatic gain unchanged/);
assert.match(featureHelp({ id: 22 }, manifest.releases[0]).description, /ignoring CV/);
assert.match(featureHelp({ id: 23 }, manifest.releases[0]).description, /granular.*TIME CV is unplugged/);
const app = read('../static/js/app.js');
assert(app.includes('featureHelp(f, selected())') && app.includes('link.href = help.url'));
assert(app.includes('action.textContent = actionHelp(f)'));
const html = read('../static/index.html');
for (const text of ['Off removes', 'Fixed keeps', 'Adjustable starts', 'Diffusion Off removes grain diffusion']) assert(html.includes(text));
console.log('PASS feature descriptions, real documentation anchors, release-tag resolution and action-only help');
