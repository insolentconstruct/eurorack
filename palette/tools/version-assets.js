// Copyright 2026 Insolent Construct.
// Author: Insolent Construct.
// MIT licence: see ../static/js/encoder.js.
// Run after editing the manifest or browser modules. The version binds both.
import { readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
const root = new URL('../static/', import.meta.url);
const read = (p) => readFileSync(new URL(p, root), 'utf8');
const clean = (s) => s.replace(/\?v=[a-f0-9]+/g, '');
const manifest = JSON.parse(read('manifest.json'));
delete manifest.asset_version;
for (const release of manifest.releases) release.app = clean(release.app);
const files = readdirSync(new URL('js/', root)).filter((p) => p.endsWith('.js')).sort();
const version = createHash('sha256').update(JSON.stringify(manifest) +
    files.map((p) => clean(read('js/' + p))).join('')).digest('hex').slice(0, 16);
manifest.asset_version = version;
for (const release of manifest.releases) release.app += '?v=' + release.app_sha256;
writeFileSync(new URL('manifest.json', root), JSON.stringify(manifest, null, 2) + '\n');
for (const file of files) {
  const source = clean(read('js/' + file)).replace(/(['"])((?:\.\/|js\/)[^'"\n]+\.js|manifest\.json)\1/g,
      (_, q, path) => `${q}${path}?v=${version}${q}`);
  writeFileSync(new URL('js/' + file, root), source);
}
writeFileSync(new URL('index.html', root), clean(read('index.html')).replace('src="js/app.js"', `src="js/app.js?v=${version}"`));
console.log(`Versioned browser assets: ${version}`);
