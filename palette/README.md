# Drizzlet builder

A static page that makes the Beads firmware update file (`.wav`) in the browser, from a release's app image. Nothing is uploaded; no server or build step.

**Phase 1:** it makes exactly the update file published with each release, byte for byte, so anyone can check it. Choosing features per firmware comes later.

Layout: everything served as-is is under `static/`, where a Hugo site expects it, so moving to Hugo later only adds configuration and templates. Tests are outside it.

- `static/js/encoder.js`: Mutable Instruments' QPSK encoder (`stm_audio_bootloader/qpsk/encoder.py`) as Beads' release build runs it. One unminified module, used by both the page and the Node tests.
- `static/js/glibc_sincos.js`: sine and cosine ported from the GNU C Library 2.36, LGPL 2.1 or later (`static/js/COPYING.LIB`). The release file is encoded by numpy on glibc; one last bit of a sine decides between samples 32766 and 32767, and the browser's own `Math.sin` differs in about 636,000 samples of the Beta 2 file.
- `static/index.html`, `static/js/app.js`, `static/js/worker.js`: the page; the encoder runs in a worker.
- `static/manifest.json` and `static/images/`: the releases offered, with each app image and update file pinned by SHA-256. GitHub release downloads cannot be fetched from another site, so the images are served with the page.

Tests (Node 18 or later):

```sh
node palette/test/manifest.js
node palette/test/golden.js APP.bin EXPECTED_WAV_SHA256
```

CI also encodes every build's `app.bin` and checks it against `beads/build/expected-wav.sha256`.

To try the page locally, serve `static/` over HTTP (`python3 -m http.server -d palette/static`); the hash check needs a secure context, which `localhost` is.
