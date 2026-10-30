// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Decode the generated PCM, independently demodulating and checking each packet.
// Timing is the encoder's fixed framing, not a decoder for recorded audio.
import { crc32 } from './profile.js?v=974dc91abfff0f91';
import { sampleCount, MAX_APP_SIZE } from './encoder.js?v=974dc91abfff0f91';

export function decode(wav, size) {
  if (!Number.isInteger(size) || size < 1 || size > MAX_APP_SIZE) {
    throw new Error('Invalid decoded image size.');
  }
  const view = new DataView(wav.buffer, wav.byteOffset, wav.byteLength);
  const text = (offset, length) => String.fromCharCode(...wav.subarray(offset, offset + length));
  if (wav.length !== 44 + sampleCount(size) * 2 || text(0, 4) !== 'RIFF' ||
      text(8, 8) !== 'WAVEfmt ' || view.getUint32(16, true) !== 16 ||
      view.getUint16(20, true) !== 1 || view.getUint16(22, true) !== 1 ||
      view.getUint32(24, true) !== 48000 || view.getUint32(28, true) !== 96000 ||
      view.getUint16(32, true) !== 2 || view.getUint16(34, true) !== 16 ||
      text(36, 4) !== 'data' || view.getUint32(4, true) !== wav.length - 8 ||
      view.getUint32(40, true) !== wav.length - 44) throw new Error('Invalid update WAV.');
  let position = 48000;
  let modulated = 0;
  let scrambler = 0;
  const skip = (seconds) => {
    const samples = Math.trunc(seconds * 12000 / 8) * 32;
    position += samples;
    modulated += samples;
  };
  const byte = () => {
    let result = 0;
    for (let s = 0; s < 4; ++s) {
      let q = 0;
      let i = 0;
      for (let n = 0; n < 8; ++n) {
        const phase = 2 * Math.PI * 6000 * (modulated++ / 48000);
        const sample = view.getInt16(44 + 2 * position++, true);
        q += sample * Math.sin(phase);
        i += sample * Math.cos(phase);
      }
      result = (result << 2) | (i > 0 ? 2 : 0) | (q > 0 ? 1 : 0);
    }
    return result;
  };
  const blocks = Math.ceil(size / 65536);
  const image = new Uint8Array(blocks * 65536);
  skip(1);
  for (let b = 0; b < blocks; ++b) {
    for (let p = 0; p < 256; ++p) {
      const packet = Uint8Array.from({ length: 276 }, byte);
      if (!packet.subarray(0, 8).every((v) => v === 0) ||
          !packet.subarray(8, 12).every((v) => v === 0x99) ||
          !packet.subarray(12, 16).every((v) => v === 0xcc)) {
        throw new Error('Update packet header failed.');
      }
      const data = packet.subarray(16, 272);
      if (crc32(data) !== new DataView(packet.buffer).getUint32(272, false)) {
        throw new Error('Update packet checksum failed.');
      }
      for (let n = 0; n < 256; ++n) {
        image[b * 65536 + p * 256 + n] = data[n] ^ (scrambler >>> 24);
        scrambler = (Math.imul(scrambler, 1664525) + 1013904223) >>> 0;
      }
    }
    skip(b % 2 === 0 ? 3 : .2);
  }
  if (!image.subarray(size).every((v) => v === 255)) throw new Error('Update padding failed.');
  return image.slice(0, size);
}
