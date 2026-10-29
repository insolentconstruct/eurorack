# Drizzlet Hygrostat

The Drizzlet Hygrostat: set your firmware's climate. (A hygrostat is the industrial humidity controller; this one controls drizzle.)

A static page that makes the Beads firmware update file (`.wav`) in the browser, from a release's app image. Nothing is uploaded; no server or build step.

The profile editor targets profile-capable Beta 2 and later firmware. It offers
Off, Fixed and Adjustable features, defaults, page/amount/action placements,
templates, share links and desktop/phone SVG guides. Conflicts and invalid
firmware profiles are refused. Older plain-WAV releases are not offered.

The current entry is an **unreleased development build** pinned in the manifest. All
feature CPU costs and per-engine baselines are placeholder estimates. Replace them with
module measurements before publishing a release. The provisional baselines use
86% for synth (measured range 86–87%) and 83% for granular (81–83%).
Synth-path costs apply to synth; all active costs apply conservatively to granular.
Estimates are rounded to 0.1% before each engine is checked. The provisional stock ceiling
is 92%; unmeasured combinations warn from 86% and are blocked above 87%.
A measured combination at or below 92% is allowed. A measurement is tied to the
complete encoded 512-byte profile block (`cpu.combinations[].profile`, hex), with
its worst-case `synth` and `granular` percentages; placement or default changes require another match.

Layout: everything served as-is is under `static/`, where a Hugo site expects it, so moving to Hugo later only adds configuration and templates. Tests are outside it.

- `static/js/profile.js`: wire-format parser/encoder, validation, templates,
  share links, fingerprints, names and CPU rules. Metadata mirrors
  `beads/profile.cc`; a differential test checks the real parser.
- `static/js/guide.js`: custom layout drawing, with knob-card geometry ported
  from `options_guide.py`. `release_guides.js` preserves the canonical release
  drawings for the untouched default. Update these two SVG strings when the
  release guides change. Normalisation removes only the added fingerprint
  `<text data-profile-fingerprint="true">` element: every remaining byte of
  the default guide must equal the release SVG. Edited layouts reuse the
  release's to-scale panel geometry and draw their own pages and gestures.
- `static/js/decoder.js`: generated-PCM QPSK demodulator, with packet headers,
  CRCs, descrambling and padding checks. This checks generated WAVs, not audio
  recorded from a player.
- `static/js/encoder.js`: Mutable Instruments' QPSK encoder (`stm_audio_bootloader/qpsk/encoder.py`) as Beads' release build runs it. One unminified module, used by both the page and the Node tests.
- `static/js/glibc_sincos.js`: sine and cosine ported from the GNU C Library 2.36, LGPL 2.1 or later (`static/js/COPYING.LIB`). The release file is encoded by numpy on glibc; one last bit of a sine decides between samples 32766 and 32767, and the browser's own `Math.sin` differs in about 636,000 samples of the Beta 2 file.
- `static/index.html`, `static/js/app.js`, `static/js/worker.js`: the page; encoding and decode-back verification run in a worker.
- `static/manifest.json` and `static/images/`: the releases offered, with each app image and update file pinned by SHA-256. GitHub release downloads cannot be fetched from another site, so the images are served with the page.

Tests (Node 18 or later, g++, and the pinned stmlib submodule):

```sh
node palette/test/manifest.js
node palette/test/profile.js
node palette/test/golden.js APP.bin EXPECTED_WAV_SHA256
```

The profile test compiles a small oracle from the actual firmware parser and
compares more than 26,000 candidate blocks. With stmlib already available in a
separate checkout, `STMLIB_INCLUDE` can name that checkout's root. No firmware
source or Git metadata needs changing.

Manifest format 2 stores the profile offset, size, version, release ID and
feature table per release. `values: null` means a full byte. The `page`,
`amount`, `action` and `stateless` fields mirror the firmware definition table.
The page verifies the pinned base app first, patches only bytes 1024–1535, then
encodes and decodes the WAV. The untouched default also checks the release WAV
hash. Custom WAVs and guides carry the first eight hex digits of SHA-256 of the
512-byte profile; share links carry all 512 bytes and the release tag.

Development images are pinned by the manifest and independently encoded with
the release Python encoder. Released images must also match their tag's checked-in
hash files. The development build currently differs from phase 2's checked-in
app and WAV hashes; the default block agrees with the firmware, and the newly
built image's WAV agrees with the independent encoder. Resolve those stale
hashes before turning this into a release entry.

CI also encodes every build's `app.bin` and checks it against `beads/build/expected-wav.sha256`.

**Deploy:** `.github/workflows/pages.yml` runs on every push to `drizzlet` that touches `palette/`. It reruns the manifest test, builds with a pinned Hugo (version and SHA-256 in the workflow), checks that the output is `static/` byte for byte, and publishes it with GitHub Pages. `hugo.toml` only tells Hugo to copy `static/`; there are no templates yet.

**Counting:** each update file made sends one request to GoatCounter with the release tag only (`/render/<tag>`), from `app.js`, with no cookies and no GoatCounter script; it is skipped when the browser sends "do not track". The page says so.

To try the page locally, serve `static/` over HTTP (`python3 -m http.server -d palette/static`); the hash check needs a secure context, which `localhost` is.
