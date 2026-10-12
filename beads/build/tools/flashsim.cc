// Copyright 2026 insolentconstruct.
//
// Author: insolentconstruct
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

// Adapted from the MIT-licensed RE tools/bela/flash/flashsim.cc, with EOT proof.
// flashsim -- the MI QPSK audio bootloader's OWN demodulator and packet decoder
// (stm-audio-bootloader d391c6f71eaeaa40ffd5d3d761a6843f675bc2b1, qpsk/demodulator.cc + packet_decoder.cc, compiled unchanged) driven
// the way a Clouds-style bootloader drives them, fed with a simulated ADC stream.
//
//   flashsim SAMPLES.u16 PAYLOAD.bin [max_sync_duration]
// SAMPLES.u16: little-endian uint16, the values the bootloader would PushSample() (48 kHz, 12-bit
// centred at 2048).  Writes every CRC-valid packet's 256 payload bytes, in order, to PAYLOAD.bin and
// prints one summary line:  "packets=N blocks=B errors=E first_error_symbol=S overflow=O".
// Parameters: 48 kHz sample rate, 6 kHz carrier, 12 kbit/s (encoder.py's CLI defaults, the same as
// RE tools/pipeline/wav2bin.py's Protocol), so Init(6000/48000*2^32, 8, 8).  After every 256 packets
// (one 64 KiB STM32H7 block) the decoder resets and the carrier re-syncs, as the block write does.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include "stm_audio_bootloader/qpsk/demodulator.h"
#include "stm_audio_bootloader/qpsk/packet_decoder.h"

using namespace stm_audio_bootloader;

static Demodulator demodulator;
static PacketDecoder decoder;

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "usage: flashsim SAMPLES.u16 PAYLOAD.bin [max_sync]\n"); return 2; }
  FILE* in = fopen(argv[1], "rb");
  FILE* out = fopen(argv[2], "wb");
  if (!in || !out) return 2;
  uint16_t max_sync = argc > 3 ? atoi(argv[3]) : 20000;
  const double kSampleRate = 48000.0, kModulationRate = 6000.0, kBitRate = 12000.0;
  decoder.Init(max_sync, true);
  demodulator.Init(kModulationRate / kSampleRate * 4294967296.0, kSampleRate / kModulationRate,
                   2.0 * kSampleRate / kBitRate);
  demodulator.SyncCarrier(true);
  decoder.Reset();
  long packets = 0, errors = 0, overflow = 0, symbols = 0, first_error = -1;
  std::vector<uint16_t> buf(32);
  size_t n;
  bool stop = false;
  bool complete = false;
  while (!stop && (n = fread(buf.data(), 2, buf.size(), in)) > 0) {
    for (size_t i = 0; i < n; ++i) demodulator.PushSample(static_cast<int16_t>(buf[i]));
    if (demodulator.state() == DEMODULATOR_STATE_OVERFLOW) { ++overflow; stop = true; break; }
    demodulator.ProcessAtLeast(n);
    while (demodulator.available()) {
      uint8_t symbol = demodulator.NextSymbol();
      ++symbols;
      PacketDecoderState state = decoder.ProcessSymbol(symbol);
      if (state == PACKET_DECODER_STATE_OK) {
        fwrite(decoder.packet_data(), 1, kPacketSize, out);
        ++packets;
        decoder.Reset();
        if (packets % 256 == 0) demodulator.SyncCarrier(false); else demodulator.SyncDecision();
      } else if (state == PACKET_DECODER_STATE_END_OF_TRANSMISSION) {
        complete = true;
        stop = true;
        break;
      } else if (state == PACKET_DECODER_STATE_ERROR_SYNC || state == PACKET_DECODER_STATE_ERROR_CRC) {
        ++errors;
        if (first_error < 0) first_error = symbols;
        stop = true;    // the real bootloader shows the error and waits for a button
        break;
      }
    }
  }
  fclose(out);
  printf("packets=%ld blocks=%ld errors=%ld first_error_symbol=%ld overflow=%ld complete=%d\n", packets, packets / 256,
         errors, first_error, overflow, complete ? 1 : 0);
  return 0;
}
