// Copyright 2026 Insolent Construct. MIT licence: see ../static/js/encoder.js.
// Run with Playwright available, against a local server in an offline container.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { chromium, firefox, webkit } from 'playwright';
const base = process.env.BASE || 'http://127.0.0.1:8080/';
const site = process.env.SITE || '/site';
const release = JSON.parse(readFileSync(`${site}/manifest.json`)).releases[0];
const { encodeProfile, defaults, share, fingerprint, templates } = await import(
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
    const row = (f) => page.locator('.feature').filter({ has: page.getByRole('heading', { name: f.name, exact: true }) });
    const mode = (f, value) => row(f).locator('select').first().selectOption(String(value));
    // Every feature returns to byte-identical Off state, including value/slots.
    await page.selectOption('#template', 'Stock plus fixes');
    const stockFP = await fingerprint(encodeProfile(templates(release.profile)['Stock plus fixes'], release.profile));
    await waitFingerprint(stockFP);
    const stockShare = await page.getAttribute('#share', 'href');
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
    // Invalid input reports its error and leaves the valid profile intact.
    await page.evaluate(() => { location.hash = 'release=missing&profile=00'; });
    await until(async () => await page.getAttribute('#status', 'class') === 'error');
    assert.equal(await page.textContent('#fingerprint'), customFP);
    await page.evaluate((hash) => { location.hash = hash; }, share(release, releaseBlock));
    await waitFingerprint(releaseFP);
    assert.equal(await page.inputValue('#template'), 'Drizzlet');
    assert((await page.textContent('#cpu')).includes('87.2%'));
    assert.equal(await page.isDisabled('#build'), false);
    assert.deepEqual(errors, []);
    console.log(`PASS ${name}: every feature Fixed/Off exact bytes, default names, unknown CPU costs, same-document share links and invalid-link recovery`);
  } finally { await browser.close(); }
}
