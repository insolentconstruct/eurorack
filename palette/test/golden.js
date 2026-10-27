// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// MIT licence: see static/js/encoder.js.
//
// Encodes an app image with the page's encoder and checks the .wav against
// the released file's SHA-256; on a mismatch, counts the samples that differ.
//
// node palette/test/golden.js APP.bin EXPECTED_WAV_SHA256 [REFERENCE.wav]

import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { encode } from '../static/js/encoder.js';

const [app, expected, reference] = process.argv.slice(2);
const started = Date.now();
const wav = encode(readFileSync(app));
const digest = createHash('sha256').update(wav).digest('hex');
console.log(`${digest}  ${app} (${wav.length} bytes, ${(Date.now() - started) / 1000} s)`);
if (digest === expected) {
  console.log('PASS: byte-identical to the release .wav');
  process.exit(0);
}
console.log(`FAIL: expected ${expected}`);
if (reference) {
  const ref = readFileSync(reference);
  if (ref.length !== wav.length) console.log(`length ${wav.length}, reference ${ref.length}`);
  const a = new DataView(wav.buffer), b = new DataView(ref.buffer, ref.byteOffset);
  let differ = 0, worst = 0, first = -1;
  for (let n = 44; n + 1 < Math.min(wav.length, ref.length); n += 2) {
    const d = Math.abs(a.getInt16(n, true) - b.getInt16(n, true));
    if (d) {
      ++differ;
      worst = Math.max(worst, d);
      if (first < 0) first = (n - 44) / 2;
    }
  }
  console.log(`${differ} samples differ, at most by ${worst}, first at sample ${first}`);
}
process.exit(1);
