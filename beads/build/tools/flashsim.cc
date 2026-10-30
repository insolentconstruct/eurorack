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
// -----------------------------------------------------------------------------
//
// Runs stm_audio_bootloader's own QPSK demodulator and packet decoder
// (unchanged) on a simulated 48 kHz ADC stream, to check that a .wav decodes
// to the expected firmware image.
//
//   flashsim SAMPLES.u16 PAYLOAD.bin [max_sync_duration]

#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include "stm_audio_bootloader/qpsk/demodulator.h"
#include "stm_audio_bootloader/qpsk/packet_decoder.h"

using namespace stm_audio_bootloader;

static Demodulator demodulator;
static PacketDecoder decoder;

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: flashsim SAMPLES.u16 PAYLOAD.bin [max_sync]\n");
    return 2;
  }
  FILE* in = fopen(argv[1], "rb");
  FILE* out = fopen(argv[2], "wb");
  if (!in || !out) {
    return 2;
  }
  uint16_t max_sync = argc > 3 ? atoi(argv[3]) : 20000;
  const double kSampleRate = 48000.0;
  const double kModulationRate = 6000.0;
  const double kBitRate = 12000.0;
  decoder.Init(max_sync, true);
  demodulator.Init(
      kModulationRate / kSampleRate * 4294967296.0,
      kSampleRate / kModulationRate,
      2.0 * kSampleRate / kBitRate);
  demodulator.SyncCarrier(true);
  decoder.Reset();
  long packets = 0, errors = 0, overflow = 0, symbols = 0, first_error = -1;
  uint16_t buf[32];
  size_t n;
  bool stop = false;
  bool complete = false;
  while (!stop &&
      (n = fread(buf, sizeof(buf[0]), sizeof(buf) / sizeof(buf[0]), in)) > 0) {
    for (size_t i = 0; i < n; ++i) {
      demodulator.PushSample(static_cast<int16_t>(buf[i]));
    }
    if (demodulator.state() == DEMODULATOR_STATE_OVERFLOW) {
      ++overflow;
      stop = true;
      break;
    }
    demodulator.ProcessAtLeast(n);
    while (demodulator.available()) {
      uint8_t symbol = demodulator.NextSymbol();
      ++symbols;
      PacketDecoderState state = decoder.ProcessSymbol(symbol);
      if (state == PACKET_DECODER_STATE_OK) {
        fwrite(decoder.packet_data(), 1, kPacketSize, out);
        ++packets;
        decoder.Reset();
        if (packets % 256 == 0) {
          demodulator.SyncCarrier(false);
        } else {
          demodulator.SyncDecision();
        }
      } else if (state == PACKET_DECODER_STATE_END_OF_TRANSMISSION) {
        complete = true;
        stop = true;
        break;
      } else if (state == PACKET_DECODER_STATE_ERROR_SYNC ||
                 state == PACKET_DECODER_STATE_ERROR_CRC) {
        ++errors;
        if (first_error < 0) {
          first_error = symbols;
        }
        // The real bootloader shows the error and waits for a button.
        stop = true;
        break;
      }
    }
  }
  fclose(out);
  printf("packets=%ld blocks=%ld errors=%ld first_error_symbol=%ld "
         "overflow=%ld complete=%d\n",
         packets, packets / 256, errors, first_error, overflow,
         complete ? 1 : 0);
  return 0;
}
