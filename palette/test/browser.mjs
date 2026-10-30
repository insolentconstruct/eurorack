// Copyright 2026 Insolent Construct. MIT licence: see ../static/js/encoder.js.
// Run with Playwright available, against a local server in an offline container.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { chromium, firefox, webkit } from 'playwright';
const base = process.env.BASE || 'http://127.0.0.1:8080/';
const site = process.env.SITE || '/site';
const release = JSON.parse(readFileSync(`${site}/manifest.json`)).releases[0];
const { encodeProfile, defaults, share, fingerprint, templates, profileKey } = await import(
  'data:text/javascript;base64,' + readFileSync(`${site}/js/profile.js`).toString('base64'));
for (const [name, engine] of [['chromium', chromium], ['firefox', firefox], ['webkit', webkit]]) {
  const browser = await engine.launch();
  try {
    const context = await browser.newContext();
    await context.route('**/*', (route) => new URL(route.request().url()).origin === new URL(base).origin ?
      route.continue() : route.abort('internetdisconnected'));
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(String(error)));
    await page.goto(base);
    const until = async (predicate) => {
      const deadline = Date.now() + 30000;
      while (!await predicate()) {
        assert(Date.now() < deadline, 'timed out waiting for page state');
        await page.waitForTimeout(50);
      }
    };
    const waitFingerprint = (fp) => until(async () =>
      await page.textContent('#fingerprint') === fp && await page.locator('#guides').isVisible());
    const releaseBlock = encodeProfile(defaults(release.profile), release.profile);
    const releaseFP = await fingerprint(releaseBlock);
    await waitFingerprint(releaseFP);
    for (const f of release.profile.features) {
      const link = page.getByRole('link', { name: `${f.name} documentation`, exact: true });
      assert.equal(await link.count(), 1);
      const href = await link.getAttribute('href');
      assert(href.includes(`/blob/${release.development ? release.source_commit : release.tag}/beads/build/docs/FEATURES.md#`));
      assert((await link.locator('..').innerText()).length > 'Documentation'.length);
    }
    assert((await page.getByRole('group', { name: 'Monophonic grains', exact: true }).innerText()).includes('no page knob'));
    // Keyboard focus survives rebuilding the controls; names identify the feature.
    await page.locator('#release').focus();
    await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'template');
    await page.keyboard.press('Tab'); // CPU measurement details summary.
    assert.equal(await page.evaluate(() => document.activeElement.tagName), 'SUMMARY');
    await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'Pitch scale documentation');
    await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'feature-1-control-0');
    await page.keyboard.press('Home');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'feature-1-control-0');
    assert.equal(await page.getByRole('combobox', { name: 'Pitch scale: Mode', exact: true }).count(), 1);
    await page.keyboard.press('Tab'); // Off removes the feature's other controls.
    assert.equal(await page.evaluate(() => document.activeElement.getAttribute('aria-label')), 'Reverb character documentation');
    await page.keyboard.press('Tab');
    assert.equal(await page.evaluate(() => document.activeElement.id), 'feature-2-control-0');
    await page.selectOption('#template', 'Drizzlet');
    await waitFingerprint(releaseFP);
    assert.equal(await page.locator('#cpu').getAttribute('role'), null);
    assert.equal(await page.locator('#cpu-status').getAttribute('role'), 'status');
    assert((await page.textContent('#cpu-status')).length < 100);
    for (const f of release.profile.features.filter((f) => !f.values)) {
      const input = page.getByRole('spinbutton', { name: `${f.name}: Default (0–255)`, exact: true });
      const previous = await input.inputValue();
      const previousShare = await page.getAttribute('#share', 'href');
      for (const invalid of ['', '-1', '256', '1.5']) {
        await input.fill(invalid);
        await input.press('Tab');
        assert.equal(await input.inputValue(), previous);
        assert.equal(await page.getAttribute('#share', 'href'), previousShare);
        assert((await page.textContent('#status')).includes('previous value has been kept'));
      }
    }
    await page.emulateMedia({ media: 'print' });
    assert.equal(await page.locator('#features').isVisible(), false);
    assert.equal(await page.locator('.print-note').isVisible(), true);
    assert.equal(await page.locator('#install').isVisible(), true);
    await page.emulateMedia({ media: 'screen' });
    const row = (f) => page.locator('.feature').filter({ has: page.getByRole('heading', { name: f.name, exact: true }) });
    const mode = (f, value) => row(f).locator('select').first().selectOption(String(value));
    // Every feature returns to byte-identical Off state, including value/slots.
    await page.selectOption('#template', 'Stock plus fixes');
    const stockFP = await fingerprint(encodeProfile(templates(release.profile)['Stock plus fixes'], release.profile));
    await waitFingerprint(stockFP);
    assert.match(await page.textContent('#cpu'), /^Within the measured default\./);
    assert(!/%|estimated/.test(await page.textContent('#cpu')));
    const stockShare = await page.getAttribute('#share', 'href');
    // Preserve the enabled/unknown regression using an explicit Off-default fixture.
    const offManifest = JSON.parse(readFileSync(`${site}/manifest.json`));
    for (const f of offManifest.releases[0].profile.features) f.default = [f.id, 0, 0, 0, 0, 0];
    const manifestURL = /\/manifest\.json(?:\?|$)/;
    const offRoute = (route) => route.fulfill({ json: offManifest });
    await context.route(manifestURL, offRoute);
    await page.reload();
    await waitFingerprint(stockFP);
    for (const f of release.profile.features) {
      await mode(f, 1);
      await until(async () => (await page.textContent('#cpu')).startsWith('CPU cost not yet measured for:'));
      assert((await page.textContent('#cpu')).includes(f.name));
      assert(!/%/.test(await page.textContent('#cpu')));
      assert.equal(await page.isDisabled('#build'), false);
      assert.equal(await page.isDisabled('#local'), false);
      assert.equal(await page.locator('body').evaluate((b) => b.classList.contains('cpu-warning')), false);
      await mode(f, 0);
      await waitFingerprint(stockFP);
      assert.equal(await page.getAttribute('#share', 'href'), stockShare, `${f.name}: exact Off bytes`);
    }
    await context.unroute(manifestURL, offRoute);
    await page.reload();
    await waitFingerprint(releaseFP);
    await page.selectOption('#template', 'Drizzlet');
    await waitFingerprint(releaseFP);
    for (const f of release.profile.features.filter((f) => f.default_in_block === false)) {
      await mode(f, 1);
      await mode(f, 0);
      await waitFingerprint(releaseFP);
      assert.equal(await page.inputValue('#template'), 'Drizzlet');
      assert.equal(await page.getAttribute('#guide', 'download'), 'options-guide-dev.svg');
      assert.equal(await page.getAttribute('#guide-mobile', 'download'), 'options-guide-dev-mobile.svg');
    }
    // Change only the fragment in an already open document: no reload.
    const chosen = defaults(release.profile);
    chosen.find((e) => e[0] === 1)[2] = 3;
    const customBlock = encodeProfile(chosen, release.profile);
    const customFP = await fingerprint(customBlock);
    await page.evaluate(() => { window.sameDocument = true; });
    await page.evaluate((hash) => { location.hash = hash; }, share(release, customBlock));
    await waitFingerprint(customFP);
    assert.equal(await page.evaluate(() => window.sameDocument), true);
    assert.equal(await row(release.profile.features[0]).locator('label').filter({ hasText: /^Default/ }).locator('select').inputValue(), '3');
    assert.equal(await page.inputValue('#template'), 'Custom');
    assert((await page.getAttribute('#guide', 'download')).includes(customFP));
    assert((await page.getAttribute('#share', 'href')).includes(share(release, customBlock)));
    assert.equal(await page.isDisabled('#build'), false);
    assert.equal(await page.textContent('#cpu'), 'CPU cost not yet measured for: Pitch scale.');
    // Invalid input reports its error and leaves the valid profile intact.
    await page.evaluate(() => { location.hash = 'release=missing&profile=00'; });
    await until(async () => await page.getAttribute('#status', 'class') === 'error');
    assert.equal(await page.textContent('#fingerprint'), customFP);
    await page.evaluate((hash) => { location.hash = hash; }, share(release, releaseBlock));
    await waitFingerprint(releaseFP);
    assert.equal(await page.inputValue('#template'), 'Drizzlet');
    assert((await page.textContent('#cpu')).includes('87.2%'));
    assert.equal(await page.isDisabled('#build'), false);
    // Exact measured combinations take precedence with shipped null costs.
    const measuredManifest = JSON.parse(readFileSync(`${site}/manifest.json`));
    const measured = measuredManifest.releases[0];
    measured.cpu.combinations = [{ profile: profileKey(chosen, release.profile), synth: 90.4, granular: 89 }];
    const measuredRoute = (route) => route.fulfill({ json: measuredManifest });
    await context.route(manifestURL, measuredRoute);
    for (const [percent, blocked] of [[90.4, false], [92, false], [92.1, true]]) {
      measured.cpu.combinations[0].synth = percent;
      await page.goto(base + '#' + share(release, customBlock));
      await page.reload();
      await waitFingerprint(customFP);
      assert((await page.textContent('#cpu')).startsWith(`${percent.toFixed(1)}% measured.`));
      assert.equal(await page.isDisabled('#build'), blocked);
      assert.equal(await page.isDisabled('#local'), blocked);
      assert.equal(await page.locator('body').evaluate((b) => b.classList.contains('cpu-warning')), percent >= 91);
    }
    const counters = await browser.newContext();
    const counterRequests = [];
    await counters.route('**/*', (route) => {
      const url = new URL(route.request().url());
      if (url.origin === new URL(base).origin) return route.continue();
      counterRequests.push(url.href);
      if (url.hostname === 'insolentconstruct.goatcounter.com' && url.pathname.startsWith('/counter/')) {
        return route.fulfill({ headers: { 'access-control-allow-origin': '*' }, json: { count: url.pathname.endsWith('/TOTAL.json') ? '400' : '12' } });
      }
      if (url.hostname === 'api.github.com') return route.fulfill({ headers: { 'access-control-allow-origin': '*' }, json: { assets: [{ name: 'release-wav.zip', download_count: 8 }] } });
      return route.abort('internetdisconnected');
    });
    const counterPage = await counters.newPage();
    await counterPage.goto(base);
    await counterPage.waitForFunction(() => document.querySelector('#fingerprint').textContent.length > 0);
    assert.deepEqual(counterRequests, [], 'counters remain deferred');
    await counterPage.keyboard.press('Tab');
    await counterPage.waitForFunction(() => document.querySelector('#download-counts').textContent.includes('Site visitors: 400'));
    const lines = counterPage.locator('#download-counts p');
    assert.equal(await lines.count(), JSON.parse(readFileSync(`${site}/manifest.json`)).releases.length + 1);
    assert((await lines.first().textContent()).includes(`${release.name}: downloaded 20 times`));
    assert.equal(await lines.last().textContent(), 'Site visitors: 400');
    await counters.close();
    // Capability failure gives an actionable message before enabling builds.
    await page.addInitScript(() => { window.Worker = undefined; });
    await page.goto(base);
    await until(async () => (await page.textContent('#status')).includes('current Chrome'));
    assert.equal(await page.isDisabled('#build'), true);
    const noJS = await browser.newContext({ javaScriptEnabled: false });
    const noJSPage = await noJS.newPage();
    await noJSPage.goto(base);
    assert((await noJSPage.locator('noscript').innerText()).includes('Enable JavaScript'));
    await noJS.close();
    assert.deepEqual(errors, []);
    console.log(`PASS ${name}: every feature Fixed/Off exact bytes, default names, unknown CPU costs, same-document share links and invalid-link recovery`);
  } finally { await browser.close(); }
}
