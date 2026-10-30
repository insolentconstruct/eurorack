// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see encoder.js.
//
// The page: pick a release, check its app image against the manifest, encode
// the update .wav in the browser, check the .wav, offer it for download.

import { defaults, decodeProfile, encodeProfile, equal, sha256, fingerprint, filenames, share, unshare, templates, validate, cpuUsage, featureDelta, modes, initialEntry, normalise } from './profile.js?v=6b546fe5d6cc5671';
import { guide, pageName, AMOUNTS, ACTIONS } from './guide.js?v=6b546fe5d6cc5671';
import { downloadCounts, countLine, recordDownload, recordBuild, afterInteraction } from './downloads.js?v=6b546fe5d6cc5671';

const $ = (id) => document.getElementById(id);

function status(text, kind = '') {
  $('status').textContent = text;
  $('status').className = kind;
}

function encodeInWorker(app) {
  return new Promise((resolve, reject) => {
    const worker = new Worker('js/worker.js?v=6b546fe5d6cc5671', { type: 'module' });
    worker.onmessage = ({ data }) => {
      if (data.progress !== undefined) {
        $('progress').value = data.progress;
      } else {
        worker.terminate();
        data.error ? reject(new Error(data.error)) : resolve(data.wav);
      }
    };
    worker.onerror = (event) => {
      worker.terminate();
      reject(new Error(event.message || 'encoder failed'));
    };
    worker.postMessage(app.buffer, [app.buffer]);
  });
}

let manifest;
let download;
let entries;
let revision = 0;
const guideURLs = [];

async function build(release, app) {
  if (download) URL.revokeObjectURL(download);
  $('result').hidden = true;
  $('progress').value = 0;
  $('progress').hidden = false;
  const appHash = await sha256(app);
  if (app.length !== release.app_size || appHash !== release.app_sha256) {
    throw new Error(`the app image does not match ${release.tag}: ${appHash}`);
  }
  const chosen = entries.map((e) => e.slice());
  validate(chosen, release.profile);
  if (cpuUsage(chosen, release).blocked) throw new Error('This combination exceeds the CPU allowance.');
  const block = encodeProfile(chosen, release.profile);
  const original = app.slice(release.profile.offset, release.profile.offset + 512);
  if (!equal(original, encodeProfile(defaults(release.profile), release.profile))) {
    throw new Error('The release profile does not match the manifest.');
  }
  const isDefault = equal(block, original);
  const base = app.slice();
  app.set(block, release.profile.offset);
  const offset = release.profile.offset;
  if (!equal(app.subarray(0, offset), base.subarray(0, offset)) ||
      !equal(app.subarray(offset + 512), base.subarray(offset + 512)) ||
      JSON.stringify(decodeProfile(app.subarray(offset, offset + 512), release.profile)) !== JSON.stringify(chosen)) {
    throw new Error('Patched image verification failed.');
  }
  const fp = await fingerprint(block);
  const names = filenames(release, fp, isDefault);
  status('Encoding and checking the update file…');
  const wav = await encodeInWorker(app.slice());
  const wavHash = await sha256(wav);
  $('app-hash').textContent = appHash;
  $('wav-hash').textContent = wavHash;
  $('expected-hash').textContent = release.wav_sha256;
  if (isDefault && wavHash !== release.wav_sha256) {
    throw new Error('the encoded .wav does not match the release: do not install it');
  }
  download = URL.createObjectURL(new Blob([wav], { type: 'audio/wav' }));
  $('download').href = download;
  $('download').download = names.wav;
  $('download').textContent = `Download ${names.wav} (${(wav.length / 1e6).toFixed(1)} MB)`;
  $('result').hidden = false;
  status(isDefault ? 'Done: identical to the release update file. WAV decoded and checked.' : `Done: profile ${fp}. WAV decoded and checked.`, 'ok');
  await updateGuides(release, chosen, fp, names);
  $('download').onclick = () => recordDownload(release);
  recordBuild(release);
}

function selected() {
  return manifest.releases[$('release').selectedIndex];
}

let busy = false;

async function run(getApp) {
  if (busy) return;
  busy = true;
  $('editor').disabled = true;
  $('build').disabled = $('release').disabled = $('local').disabled = true;
  $('result').hidden = true;
  try {
    const release = selected();
    await build(release, await getApp(release));
  } catch (error) {
    status(`Stopped: ${error.message}`, 'error');
  } finally {
    $('progress').hidden = true;
    $('local').value = '';
    $('build').disabled = $('release').disabled = $('local').disabled = false;
    busy = false;
    if (pendingShare) { pendingShare = false; loadSharedProfile(); }
    $('editor').disabled = false;
    const blocked = cpuUsage(entries, selected()).blocked;
    $('build').disabled = $('local').disabled = blocked;
  }
}

async function fetchApp(release) {
  status('Downloading the app image…');
  const response = await fetch(release.app, { cache: 'no-cache' });
  if (!response.ok) throw new Error(`could not download the app image (${response.status})`);
  return new Uint8Array(await response.arrayBuffer());
}

async function updateGuides(release, chosen, fp, names) {
  guideURLs.splice(0).forEach((url) => URL.revokeObjectURL(url));
  for (const mobile of [false, true]) {
    const url = URL.createObjectURL(new Blob([guide(chosen, release, fp, mobile)], { type: 'image/svg+xml' }));
    guideURLs.push(url);
    const a = $(mobile ? 'guide-mobile' : 'guide');
    a.href = url; a.download = names[mobile ? 'mobile' : 'desktop'];
  }
  $('guides').hidden = false;
}

function selectControl(label, values, value, change) {
  const wrapper = document.createElement('label');
  wrapper.textContent = label;
  const select = document.createElement('select');
  values.forEach(([v, text]) => select.add(new Option(text, v)));
  select.value = value;
  select.onchange = () => change(Number(select.value));
  wrapper.append(select);
  return wrapper;
}

function setupEditor() {
  const release = selected();
  $('template').replaceChildren();
  Object.keys(templates(release.profile)).forEach((name) => $('template').add(new Option(name)));
  $('template').add(new Option('Custom', 'Custom'));
  $('template').value = Object.entries(templates(release.profile)).find(([, candidate]) =>
    equal(encodeProfile(candidate, release.profile), encodeProfile(entries, release.profile)))?.[0] || 'Custom';
  $('template').onchange = () => {
    if ($('template').value === 'Custom') return;
    entries = templates(release.profile)[$('template').value]; renderEditor(); refresh();
  };
  $('development').hidden = !release.development;
  renderEditor(); refresh();
}

function renderEditor() {
  $('features').replaceChildren();
  const profile = selected().profile;
  // Omitted entries in a shared profile are Off, as in the firmware.
  for (const f of profile.features) {
    let e = entries.find((e) => e[0] === f.id);
    const missing = !e;
    if (missing) e = initialEntry(f);
    const row = document.createElement('div'); row.className = 'feature';
    const title = document.createElement('h3'); title.textContent = f.name; row.append(title);
    const change = (k, value) => {
      const candidate = entries.map((e) => e.slice());
      let edited = candidate.find((e) => e[0] === f.id);
      if (!edited) { edited = e.slice(); candidate.push(edited); }
      edited[k] = value;
      if (k === 1 && value === 1 && f.values?.[0] === 'Off' && !edited[2]) edited[2] = 1;
      if (k === 1 && value !== 2) {
        edited[3] = edited[4] = 0;
        if (!value || !edited[2] || !f.stateless) edited[5] = 0;
      }
      try {
        entries = normalise(candidate, profile);
        $('template').value = Object.entries(templates(profile)).find(([, value]) =>
          equal(encodeProfile(entries, profile), encodeProfile(value, profile)))?.[0] || 'Custom';
        status('');
      } catch (error) { status(error.message, 'error'); }
      renderEditor(); refresh();
    };
    row.append(selectControl('Mode', modes(f), e[1], (v) => change(1, v)));
    if (e[1]) {
      if (f.values) row.append(selectControl('Default', f.values.map((v, i) => [i, v]).filter(([i]) => !(e[1] === 1 && i === 0 && f.values[0] === 'Off')), e[2], (v) => change(2, v)));
      else {
        const label = document.createElement('label'); label.textContent = 'Default (0–255)';
        const input = document.createElement('input'); input.type = 'number'; input.min = 0; input.max = 255; input.value = e[2];
        input.onchange = () => change(2, Number(input.value)); label.append(input); row.append(label);
      }
      for (const [k, enabled, labels] of [[3, f.page && e[1] === 2, Array.from({ length: 13 }, (_, i) => pageName(i))],
          [4, f.amount && e[1] === 2, AMOUNTS], [5, f.action && (e[1] === 2 || (f.stateless && e[2])), ACTIONS]]) {
        if (!enabled) continue;
        const control = selectControl(['', '', '', 'Page slot', 'Amount slot', 'Action'][k], labels.map((v, i) => [i, v]), e[k], (v) => change(k, v));
        // Conflicting placements cannot be chosen. The validator remains the final gate.
        Array.from(control.querySelector('select').options).forEach((o) => {
          o.disabled = Number(o.value) !== 0 && entries.some((other) => other[0] !== f.id && other[k] === Number(o.value));
        });
        row.append(control);
      }
    }
    const cost = document.createElement('small');
    const engines = ['synth', 'granular'].filter((engine) => f.cpu?.engines?.[engine]);
    cost.textContent = !f.cpu ? 'CPU cost not yet measured' : f.cpu.kind === 'zero' ?
      'No CPU cost (no per-block work)' : 'CPU ' + engines.map((engine) => {
        const delta = featureDelta(e, f, engine);
        return `${engine}${engines.length === 1 ? ' only' : ''} ${delta >= 0 ? '+' : ''}${delta.toFixed(1)}`;
      }).join(', ') + ' points versus stock · measured on a module-calibrated development board';
    row.append(cost); $('features').append(row);
  }
}

async function refresh() {
  const token = ++revision;
  $('result').hidden = true; $('guides').hidden = true;
  const release = selected();
  const cpu = cpuUsage(entries, release);
  document.body.classList.toggle('cpu-warning', cpu.warning);
  $('cpu').textContent = cpu.withinMeasuredDefault ? 'Within the measured default. Savings from Off or lighter settings are not yet measured.' : cpu.percent === null ? `CPU cost not yet measured for: ${cpu.unknown.join(', ')}.` :
      `${cpu.percent.toFixed(1)}% ${cpu.kind}. ` +
      cpu.engines.map((e) => `${e.engine} ${e.percent.toFixed(1)}%${e.measured ? ' measured on the module' : ' estimated from costs measured on a module-calibrated development board'}`).join('; ') +
      `. Provisional stock ceiling ${release.cpu.ceiling}%. ` +
      (cpu.blocked ? 'This combination is blocked.' : cpu.warning ? 'Near the CPU limit.' : 'Within the CPU allowance.');
  $('cpu-provenance').textContent = `Contributing settings: ${cpu.names}. ` +
      Object.entries(release.cpu.baselines).map(([engine, baseline]) => ({ engine, baseline })).map((e) => `${e.engine} baseline ${e.baseline.percent}% (${e.baseline.kind}, provisional): ${e.baseline.provenance.replace(/[.]+$/, '')}.`).join(' ') +
      ' Estimates sum per-engine costs measured on a module-calibrated development board. Final-image measurements are pending.';
  $('build').disabled = $('local').disabled = busy || cpu.blocked;
  const block = encodeProfile(entries, release.profile);
  const fp = await fingerprint(block);
  if (token !== revision) return;
  $('fingerprint').textContent = fp;
  $('share').href = '#' + share(release, block);
  $('share').onclick = () => { history.replaceState(null, '', $('share').href); status('Share this address to keep the profile.'); };
  await updateGuides(release, entries, fp, filenames(release, fp,
      equal(block, encodeProfile(defaults(release.profile), release.profile))));
}

let pendingShare = false;
function loadSharedProfile() {
  try {
    const shared = location.hash ? unshare(location.hash, manifest.releases) :
      { release: selected(), entries: defaults(selected().profile) };
    const chosen = normalise(shared.entries, shared.release.profile);
    $('release').value = shared.release.tag;
    entries = chosen;
    status('');
    setupEditor();
  } catch (error) { status(error.message, 'error'); }
}

async function init() {
  try {
    const response = await fetch('manifest.json?v=6b546fe5d6cc5671', { cache: 'no-cache' });
    manifest = await response.json();
  } catch (error) {
    status('Could not load the release list.', 'error');
    return;
  }
  if (manifest.format !== 2 || !manifest.releases.length ||
      manifest.releases.some((r) => !r.profile || r.profile.version !== 1 || r.profile.offset !== 1024 || r.profile.size !== 512)) {
    status('Unsupported release list.', 'error'); return;
  }
  const counts = downloadCounts();
  for (const release of manifest.releases) {
    $('release').add(new Option(release.name, release.tag));
    const line = document.createElement('p'); line.className = 'note';
    line.textContent = release.date || ''; line.hidden = !line.textContent;
    $('download-counts').append(line);
    afterInteraction(document, () => counts(release).then((total) => {
      line.textContent = countLine(release, total); line.hidden = !line.textContent;
    }).catch(() => {}));
  }
  $('release').selectedIndex = manifest.releases.length - 1;
  entries = defaults(selected().profile);
  if (location.hash) loadSharedProfile();
  window.addEventListener('hashchange', () => {
    if (busy) pendingShare = true;
    else loadSharedProfile();
  });
  $('release').onchange = () => { entries = defaults(selected().profile); setupEditor(); };
  setupEditor();
  $('build').onclick = () => run(fetchApp);
  $('local').onchange = () => {
    const file = $('local').files[0];
    if (file) run(async () => new Uint8Array(await file.arrayBuffer()));
  };
}

init();
