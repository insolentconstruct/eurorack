// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see encoder.js.
//
// The page: pick a release, check its app image against the manifest, encode
// the update .wav in the browser, check the .wav, offer it for download.

const $ = (id) => document.getElementById(id);

async function sha256(bytes) {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return Array.from(new Uint8Array(digest), (b) => b.toString(16).padStart(2, '0')).join('');
}

function status(text, kind = '') {
  $('status').textContent = text;
  $('status').className = kind;
}

function encodeInWorker(app) {
  return new Promise((resolve, reject) => {
    const worker = new Worker('js/worker.js', { type: 'module' });
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

// One count per update file made, with the release tag only (GoatCounter: no
// cookies, no script of theirs). Skipped when the browser asks not to be
// tracked; a failed count changes nothing.
const COUNTER = 'https://insolentconstruct.goatcounter.com/count';

function count(tag) {
  if (navigator.doNotTrack === '1') return;
  const query = new URLSearchParams({ p: `/render/${tag}`, t: tag,
      rnd: Math.random().toString(36).slice(2) });
  fetch(`${COUNTER}?${query}`, { mode: 'no-cors', credentials: 'omit',
      referrerPolicy: 'no-referrer' }).catch(() => {});
}

async function build(release, app) {
  if (download) URL.revokeObjectURL(download);
  $('result').hidden = true;
  $('progress').value = 0;
  $('progress').hidden = false;
  const appHash = await sha256(app);
  if (app.length !== release.app_size || appHash !== release.app_sha256) {
    throw new Error(`the app image does not match ${release.tag}: ${appHash}`);
  }
  status('Encoding… halcyon and on and on.');  // Orbital, 1993
  const wav = await encodeInWorker(app.slice());
  const wavHash = await sha256(wav);
  $('app-hash').textContent = appHash;
  $('wav-hash').textContent = wavHash;
  $('expected-hash').textContent = release.wav_sha256;
  if (wavHash !== release.wav_sha256) {
    throw new Error('the encoded .wav does not match the release: do not install it');
  }
  download = URL.createObjectURL(new Blob([wav], { type: 'audio/wav' }));
  $('download').href = download;
  $('download').download = `${release.tag}.wav`;
  $('download').textContent = `Download ${release.tag}.wav (${(wav.length / 1e6).toFixed(1)} MB)`;
  $('result').hidden = false;
  status('Done: identical to the release update file.', 'ok');
  count(release.tag);
}

function selected() {
  return manifest.releases[$('release').selectedIndex];
}

let busy = false;

async function run(getApp) {
  if (busy) return;
  busy = true;
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
  }
}

async function fetchApp(release) {
  status('Downloading the app image…');
  const response = await fetch(release.app, { cache: 'no-cache' });
  if (!response.ok) throw new Error(`could not download the app image (${response.status})`);
  return new Uint8Array(await response.arrayBuffer());
}

async function init() {
  try {
    const response = await fetch('manifest.json', { cache: 'no-cache' });
    manifest = await response.json();
  } catch (error) {
    status('Could not load the release list.', 'error');
    return;
  }
  for (const release of manifest.releases) {
    $('release').add(new Option(release.name, release.tag));
  }
  $('release').selectedIndex = manifest.releases.length - 1;
  $('build').disabled = false;
  $('build').onclick = () => run(fetchApp);
  $('local').onchange = () => {
    const file = $('local').files[0];
    if (file) run(async () => new Uint8Array(await file.arrayBuffer()));
  };
}

init();
