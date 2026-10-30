# Drizzlet Hygrostat

The Drizzlet Hygrostat: set your firmware's climate. (A hygrostat is the industrial humidity controller; this one controls drizzle.)

A static page that makes the Beads firmware update file (`.wav`) in the browser, from a release's app image. Nothing is uploaded; no server or build step.

The profile editor targets profile-capable Beta 2 and later firmware. It offers
Off, Fixed and Adjustable features, defaults, page/amount/action placements,
templates, share links and desktop/phone SVG guides. Conflicts and invalid
firmware profiles are refused. Older plain-WAV releases are not offered.

The current entry is an **unreleased development build** pinned in the manifest.
It pins `drizzlet-dev-db46c9a`, built from the merged Drizzlet firmware.
Each feature has either a measured CPU cost or **not yet measured** (`cpu: null`);
none has a measured cost yet. The synth baseline is the measured default-profile
worst case, **87.2% with Clouds-style reverb**, width 255 and Raag Shri; it already
includes Clouds-style reverb. Granular's historical baseline is 83.0%.

Measurement provenance: measured on the module, October 2026. Clouds-style
synth reverb ranged from 86.1–87.2%; Plaits with Clouds-style reverb measured
69.4%, and vanilla granular 92.1% supplies the provisional ceiling. These builds
predate profiles and paired grains; invalid boot-guard measurements are excluded.

A custom profile enabling any feature without a measured cost shows
“CPU cost not yet measured for: …” instead of a computed percentage. It does
not warn or block on unknown costs. The exact release default is always allowed.
Once all enabled features have measured costs, each engine rounds to 0.1%:
warn from 91% and block above the provisional 92% ceiling. Exact measured
combinations key on the complete encoded 512-byte profile
(`cpu.combinations[].profile`, hex). Missing engine measurements fall back to
that engine's baseline plus measured feature deltas; placement changes require
another exact match. Unknown enabled feature costs still suppress a computed
percentage, even if a combination measurement is present.

Maintainer release gates:

- **OPEN — release blocking:** supply the per-feature cost table, referenced to
  the measured default profile, and module checks before release. No invented
  allowances; do not add Clouds-style reverb twice. Generate feature CPU fields
  from that table and validate their provenance.
- **OPEN — release blocking:** measure the final image's default and heaviest
  supported combinations. Historical baselines remain provisional.

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
node palette/test/downloads.js
node palette/test/readiness.js
node palette/test/golden.js APP.bin EXPECTED_WAV_SHA256
```

`palette/test/browser.mjs` adds every-feature Fixed/Off byte round trips and
same-document share-link regressions in Chromium, Firefox and WebKit. Run it
with Playwright available in an offline Docker container serving the page;
`SITE` names the static directory and `BASE` the local HTTP URL. Browser test
dependencies belong to the test harness and are never served by the page.

The profile test compiles a small oracle from the manifest source commit or release tag’s firmware parser and
compares more than 26,000 candidate blocks. With stmlib already available in a
separate checkout, `STMLIB_INCLUDE` can name that checkout's root. No firmware
source or Git metadata needs changing. When the pinned firmware is an ancestor
of HEAD, the working parser source and header must also match it byte for byte.

Manifest format 2 stores the profile offset, size, version, release ID and
feature table per release. `default_in_block: false` omits a feature from the
untouched wire block while its catalogue default remains available in the editor.
The 23-feature catalogue must match the pinned firmware file byte for byte and
is checked against its definition table. `FIRMWARE_CATALOGUE_REF` adds a second
byte-identity check against a prepared firmware revision. The default block still
contains 14 entries. `states` controls editor modes.
Released `release_id` values are the tag date as hexadecimal YYYYMMDD (for example,
2026-11-05 becomes `0x20261105`), and must equal that tag's firmware
`kProfileReleaseId`; development entries retain the development constant. `values: null` means a full byte. The `page`,
`amount`, `action` and `stateless` fields mirror the firmware definition table.
The page verifies the pinned base app first, patches only bytes 1024–1535, then
encodes and decodes the WAV. The untouched default also checks the release WAV
hash. Custom WAVs and guides carry the first eight hex digits of SHA-256 of the
512-byte profile; share links carry all 512 bytes and the release tag.

Development images are pinned by the manifest and independently encoded with
the release Python encoder. Released images must also match their tag's checked-in
hash files. The current development image and independently encoded WAV match
the pinned commit's checked-in app and WAV hashes. Its default block fingerprint
remains `a0540fdd`.

CI also encodes every build's `app.bin` and checks it against `beads/build/expected-wav.sha256`.

**Deploy:** `.github/workflows/pages.yml` runs on every push to `drizzlet` that touches `palette/`. Pull requests run all palette suites too; only the drizzlet branch can deploy. It reruns the manifest test, builds with a pinned Hugo (version and SHA-256 in the workflow), checks that the output is `static/` byte for byte, and publishes it with GitHub Pages. `hugo.toml` only tells Hugo to copy `static/`; there are no templates yet.

**Counting:** Successful builds send `/render/<tag>` events; WAV download clicks send one
`download/<release-slug>` event through GoatCounter's scriptless `/count` endpoint (`p` is the event
path and `e=true` marks an event). The event name is
the only application data sent: no profile, fingerprint, custom/default marker,
page URL, referrer or personal data. The request necessarily exposes the browser
and IP address to the service. “Do not track” is respected. Guide downloads are not counted. Both event types use the same tested transport. Custom and default downloads share one event per version because
GoatCounter has no separate event properties that preserve that count page.
See the [GoatCounter endpoint parameters](https://www.goatcounter.com/help/pixel).

The owner must enable **Allow adding visitor counts** in GoatCounter settings.
After the first pointer or keyboard interaction, the page reads `/counter/<encoded-event-name>.json` and adds GitHub release asset
`download_count` values from api.github.com for names ending `-wav.zip`.
Both services receive the browser and IP address. Counts may merge repeat
downloads by the same visitor on the same day. Totals of at least 10 are shown;
otherwise the manifest date is shown, or the line is hidden if no date exists.
Either source can fail independently; results (including failures) are cached
in sessionStorage for the browser tab session, with an in-memory fallback.
Requests time out after five seconds and never delay or block building. Tests use only stubbed fetches; the Docker test container runs
without network access. See [public counter setup](https://www.goatcounter.com/help/visitor-counter).

To try the page locally, serve `static/` over HTTP (`python3 -m http.server -d palette/static`); the hash check needs a secure context, which `localhost` is.

Browser modules, worker dependencies and the manifest URL carry `?v=` versions
bound to the manifest and module contents; image URLs carry their SHA-256.
After changing browser code or manifest data, run
`node palette/tools/version-assets.js`. The readiness suite verifies every
referenced browser asset has the matching version or hash.
