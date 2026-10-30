// Copyright 2013 Emilie Gillet (encoder.py and audio_stream_writer.py).
// Copyright 2026 Insolent Construct (this JavaScript port).
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
// -----------------------------------------------------------------------------
//
// Mutable Instruments' QPSK firmware encoder (stm_audio_bootloader/qpsk/
// encoder.py, commit 39d95a8) for Beads: encoder.py -t stm32h7 -k -s 48000
// -b 12000 -c 6000 -p 256, written as its 16-bit mono .wav. Each step follows
// the Python and numpy arithmetic, so the result is byte-identical.

import { sin, cos } from './glibc_sincos.js?v=56964915dd627c31';

const SAMPLE_RATE = 48000;
const CARRIER = 6000;
const BIT_RATE = 12000;
const PACKET_SIZE = 256;
const BLOCK_SIZE = 0x10000;
const APPLICATION_START = 0x08020000;
const ERASE_PAUSE = 3.0;  // seconds, at the start of an H7 flash sector
const PAUSE = 0.2;
const OUTRO = 5.0;
const SAMPLES_PER_SYMBOL = SAMPLE_RATE / BIT_RATE * 2;  // 8
// The updater writes the app from 0x08020000; 0x08100000 holds the saved
// frozen buffer and the settings, and the updater has no end-address guard.
export const MAX_APP_SIZE = 0x08100000 - APPLICATION_START;

const H7_SECTOR_BASE = new Set([
  0x08000000, 0x08020000, 0x08040000, 0x08060000,
  0x08080000, 0x080a0000, 0x080c0000, 0x080e0000,
  0x08100000, 0x08120000, 0x08140000, 0x08160000,
  0x08180000, 0x081a0000, 0x081c0000, 0x081e0000,
]);

// numpy: 2 * numpy.pi * carrier * t, evaluated left to right.
const OMEGA = 2 * Math.PI * CARRIER;
const LEVEL = 1 / Math.SQRT2;  // (+-1) / numpy.sqrt(2.0)

const CRC_TABLE = new Uint32Array(256);
for (let n = 0; n < 256; ++n) {
  let c = n;
  for (let k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
  CRC_TABLE[n] = c >>> 0;
}

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) crc = CRC_TABLE[(crc ^ b) & 0xff] ^ (crc >>> 8);
  return (crc ^ 0xffffffff) >>> 0;
}

function blankSymbols(duration) {
  // int(duration * bit_rate / 8) * 4 zero symbols.
  return Math.trunc(duration * BIT_RATE / 8) * 4;
}

const PACKET_SYMBOLS = (16 + PACKET_SIZE + 4) * 4;

// The number of samples encode() writes for an app of this size.
export function sampleCount(size) {
  const blocks = Math.ceil(size / BLOCK_SIZE);
  let symbols = blankSymbols(1.0) + blankSymbols(OUTRO);
  for (let b = 0; b < blocks; ++b) {
    const pause = H7_SECTOR_BASE.has(APPLICATION_START + b * BLOCK_SIZE)
        ? ERASE_PAUSE : PAUSE;
    symbols += (BLOCK_SIZE / PACKET_SIZE) * PACKET_SYMBOLS + blankSymbols(pause);
  }
  return SAMPLE_RATE + symbols * SAMPLES_PER_SYMBOL;
}

class Writer {
  constructor(samples) {
    this.out = new Int16Array(samples);
    this.position = 0;
    this.modulated = 0;  // QpskEncoder._sample_index
  }

  zeros(count) {
    this.position += count;  // the array starts at zero
  }

  // QpskEncoder._encode, then AudioStreamWriter.quantize for 16 bits.
  symbols(stream) {
    for (const symbol of stream) {
      const q = (symbol & 1) ? LEVEL : -LEVEL;
      const i = (symbol >> 1) ? LEVEL : -LEVEL;
      for (let n = 0; n < SAMPLES_PER_SYMBOL; ++n) {
        const phase = OMEGA * (this.modulated++ / SAMPLE_RATE);
        let value = q * sin(phase) + i * cos(phase);
        // Clipped to +-1 when the block exceeds it; numpy then truncates.
        if (value > 1) value = 1;
        else if (value < -1) value = -1;
        this.out[this.position++] = Math.trunc(value * 32767);
      }
    }
  }

  blank(duration) {
    this.symbols(new Uint8Array(blankSymbols(duration)));
  }
}

function* packetSymbols(data) {
  for (const byte of data) {
    yield (byte >> 6) & 3;
    yield (byte >> 4) & 3;
    yield (byte >> 2) & 3;
    yield byte & 3;
  }
}

// Encodes an app image into the update .wav's bytes. progress(fraction) is
// called once per 64 KiB block.
export function encode(app, progress = () => {}) {
  if (!(app instanceof Uint8Array)) app = new Uint8Array(app);
  if (app.length === 0 || app.length > MAX_APP_SIZE) {
    throw new RangeError('app image size out of range: ' + app.length);
  }
  const blocks = Math.ceil(app.length / BLOCK_SIZE);
  const writer = new Writer(sampleCount(app.length));
  let scrambler = 0;  // Scrambler(0): one state across every packet

  writer.zeros(SAMPLE_RATE);
  writer.blank(1.0);
  for (let b = 0; b < blocks; ++b) {
    // Each block is padded with 0xff to the block size.
    const block = new Uint8Array(BLOCK_SIZE).fill(0xff);
    block.set(app.subarray(b * BLOCK_SIZE, (b + 1) * BLOCK_SIZE));
    for (let offset = 0; offset < BLOCK_SIZE; offset += PACKET_SIZE) {
      const data = block.slice(offset, offset + PACKET_SIZE);
      for (let n = 0; n < data.length; ++n) {
        data[n] ^= scrambler >>> 24;
        scrambler = (Math.imul(scrambler, 1664525) + 1013904223) >>> 0;
      }
      const crc = crc32(data);
      const packet = new Uint8Array(16 + PACKET_SIZE + 4);
      packet.fill(0x99, 8, 12);
      packet.fill(0xcc, 12, 16);
      packet.set(data, 16);
      packet.set([crc >>> 24, (crc >>> 16) & 0xff, (crc >>> 8) & 0xff, crc & 0xff],
          16 + PACKET_SIZE);
      writer.symbols(packetSymbols(packet));
    }
    const address = APPLICATION_START + b * BLOCK_SIZE;
    writer.blank(H7_SECTOR_BASE.has(address) ? ERASE_PAUSE : PAUSE);
    progress((b + 1) / blocks);
  }
  writer.blank(OUTRO);
  if (writer.position !== writer.out.length) throw new Error('length mismatch');
  return wav(writer.out);
}

// AudioStreamWriter's header: 16-bit mono PCM at 48 kHz.
function wav(samples) {
  const bytes = new Uint8Array(44 + samples.length * 2);
  const view = new DataView(bytes.buffer);
  const ascii = (offset, text) => {
    for (let n = 0; n < text.length; ++n) bytes[offset + n] = text.charCodeAt(n);
  };
  ascii(0, 'RIFF');
  view.setUint32(4, 36 + samples.length * 2, true);
  ascii(8, 'WAVEfmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, 1, true);
  view.setUint32(24, SAMPLE_RATE, true);
  view.setUint32(28, SAMPLE_RATE * 2, true);
  view.setUint16(32, 2, true);
  view.setUint16(34, 16, true);
  ascii(36, 'data');
  view.setUint32(40, samples.length * 2, true);
  for (let n = 0; n < samples.length; ++n) view.setInt16(44 + 2 * n, samples[n], true);
  return bytes;
}
