// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see encoder.js.
//
// Public, per-release counts. Failures never affect the WAV builder.
export const eventName = (release) => `download/${release.slug || release.tag.replace(/^drizzlet-/, '')}`;

function recordEvent(path, fetcher, dnt) {
  if (dnt === '1') return;
  try {
    const query = new URLSearchParams({ p: path, e: 'true' });
    Promise.resolve(fetcher(`https://insolentconstruct.goatcounter.com/count?${query}`, {
      mode: 'no-cors', credentials: 'omit', referrerPolicy: 'no-referrer', cache: 'no-store'
    })).catch(() => {});
  } catch {} // A blocked or unavailable counter must not interrupt a download.
}

export function recordDownload(release, fetcher = globalThis.fetch, dnt = globalThis.navigator?.doNotTrack) {
  recordEvent(eventName(release), fetcher, dnt);
}

export function recordBuild(release, fetcher = globalThis.fetch, dnt = globalThis.navigator?.doNotTrack) {
  recordEvent(`/render/${release.tag}`, fetcher, dnt);
}

// Defer optional third-party count requests until the page is being used.
export function afterInteraction(target, callback) {
  const events = ['pointerdown', 'keydown'];
  const start = () => {
    for (const event of events) target.removeEventListener(event, start);
    callback();
  };
  for (const event of events) target.addEventListener(event, start, { once: true });
}

function number(value) {
  if (typeof value !== 'number' && typeof value !== 'string') return null;
  const text = String(value).replace(/,/g, '');
  if (!/^\d+$/.test(text)) return null;
  const n = Number(text);
  return Number.isSafeInteger(n) ? n : null;
}

export function countLine(release, total) {
  return total !== null && total >= 10 ?
    `${release.name}: downloaded ${total.toLocaleString('en-US')} times` : (release.date || '');
}

export function downloadCounts(fetcher = globalThis.fetch, cache = new Map(), storage) {
  try { if (storage === undefined) storage = globalThis.sessionStorage; } catch {}

  return (release) => {
    const key = `${release.tag}/${eventName(release)}`;
    const storageKey = `hygrostat-downloads:${key}`;
    if (!cache.has(key)) {
      try {
        const saved = storage?.getItem(storageKey);
        if (saved !== null && saved !== undefined) {
          const value = JSON.parse(saved);
          if (value === null || (Number.isSafeInteger(value) && value >= 0)) cache.set(key, Promise.resolve(value));
        }
      } catch {}
    }
    if (!cache.has(key)) cache.set(key, (async () => {
      const get = async (url, read) => {
        try {
          const response = await fetcher(url, { credentials: 'omit', referrerPolicy: 'no-referrer', signal: AbortSignal.timeout(5000) });
          return response.ok ? read(await response.json()) : null;
        } catch { return null; }
      };
      const counts = await Promise.all([
        get(`https://insolentconstruct.goatcounter.com/counter/${encodeURIComponent(eventName(release))}.json`, (data) => number(data.count)),
        get(`https://api.github.com/repos/insolentconstruct/eurorack/releases/tags/${encodeURIComponent(release.tag)}`, (data) => {
          if (!Array.isArray(data.assets)) return null;
          const values = data.assets.filter((a) => typeof a.name === 'string' && a.name.endsWith('-wav.zip')).map((a) => number(a.download_count));
          return values.some((n) => n === null) ? null : values.reduce((sum, n) => sum + n, 0);
        })
      ]);
      const total = counts.every((n) => n === null) ? null : counts.reduce((sum, n) => sum + (n || 0), 0);
      try { storage?.setItem(storageKey, JSON.stringify(total)); } catch {}
      return total;
    })());
    return cache.get(key);
  };
}

// TOTAL is GoatCounter's public site-wide visitor count, not a render event.
export function visitorCounts(fetcher = globalThis.fetch, storage) {
  try { if (storage === undefined) storage = globalThis.sessionStorage; } catch {}
  let pending;
  return () => {
    if (!pending) pending = (async () => {
      const key = 'hygrostat-visitors:TOTAL';
      try {
        const saved = storage?.getItem(key);
        if (saved !== null && saved !== undefined) {
          const value = JSON.parse(saved);
          if (value === null || (Number.isSafeInteger(value) && value >= 0)) return value;
        }
      } catch {}
      let total = null;
      try {
        const response = await fetcher('https://insolentconstruct.goatcounter.com/counter/TOTAL.json', {
          credentials: 'omit', referrerPolicy: 'no-referrer', signal: AbortSignal.timeout(5000)
        });
        if (response.ok) total = number((await response.json()).count);
      } catch {}
      try { storage?.setItem(key, JSON.stringify(total)); } catch {}
      return total;
    })();
    return pending;
  };
}

// Mount downloads first and visitors second; completion order cannot reorder them.
// Return the refresh callback so the caller can defer every request to interaction.
export function mountCounts(container, releases, counts = downloadCounts(), visitors = visitorCounts()) {
  const line = (text, className) => {
    const p = container.ownerDocument.createElement('p');
    p.className = className; p.textContent = text; p.hidden = !text;
    container.append(p);
    return p;
  };
  const downloads = releases.map((release) => line(release.date || '', 'download-total'));
  const visitor = line('', 'note');
  const update = (p, text) => { p.textContent = text; p.hidden = !text; };
  return () => Promise.all([
    ...releases.map((release, i) => counts(release).then((total) => update(downloads[i], countLine(release, total))).catch(() => {})),
    visitors().then((total) => update(visitor, total === null ? '' : `Site visitors: ${total.toLocaleString('en-US')}`)).catch(() => {})
  ]);
}
