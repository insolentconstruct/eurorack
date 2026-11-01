// Copyright 2026 Insolent Construct.
//
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
// Bit-exact capture of default audio for comparison.

#include "beads/test/test_fpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#define private public
#include "beads/dsp/processor.h"
#include "beads/settings.h"
#include "beads/ui.h"
#undef private

#include "beads/dsp/synth.h"
#include "beads/background_buffer_backup.h"
#include "beads/calibrator.h"
#include "beads/gain_control.h"

using namespace beads;

bool test_down[4] = { };
uint32_t test_writes = 0;
namespace stmlib {
SystemClock system_clock;
}  // namespace stmlib

// Compare these native-host samples byte for byte against the preserved
// pre-optimization DSP, compiled with the same compiler and floating point flags.
static Processor processor;
static Synth synth;
static uint8_t banks[3][262144];
static uint16_t reverb[32768];

enum Mode {
  kStereoGrains, kMonoGrains, kDelay, kSynth, kSynthUnpatched, kNumModes
};

int CaptureError(int media, int mode, int block, const char* message) {
  fprintf(stderr, "ERROR: audio media=%d mode=%d block=%d: %s\n",
      media, mode, block, message);
  return 1;
}

int main(int argc, char** argv) {
  InitTestFpu();
  if (argc != 2) return CaptureError(-1, -1, -1, "expected output path");
  FILE* file = fopen(argv[1], "wb");
  if (!file) return CaptureError(-1, -1, -1, "cannot open output");
  for (int media = 0; media < 4; ++media) {
    for (int mode = kStereoGrains; mode < kNumModes; ++mode) {
      memset(banks, 0, sizeof(banks));
      memset(reverb, 0, sizeof(reverb));
      memset(static_cast<void*>(&processor), 0, sizeof(processor));
      uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
      stmlib::Random::Seed(0x12345678);
      processor.Init(buffers, reverb);
      memset(static_cast<void*>(&synth), 0, sizeof(synth));
      synth.Init(reverb);
      processor.Configure(static_cast<Processor::Media>(media), mode == kMonoGrains);
      Parameters p = { };
      p.time.base = 0.37f;
      p.time.attenuverter = 0.17f;
      p.size.base = mode == kDelay ? 1.0f : mode >= kSynth ? 0.0f : 0.48f;
      p.shape.base = 0.61f;
      p.shape.attenuverter = 0.13f;
      p.pitch.attenuverter = 0.21f;
      ModulatedParameter density = { };
      density.base = 0.73f;
      Processor::MixParameters mix = { };
      mix.feedback = 0.21f;
      mix.dry_wet = 0.85f;
      mix.reverb_amount = 0.31f;
      unsigned sample = 0;
      bool nonzero = false;
      for (int block = 0; block < 4400; ++block) {
        FloatFrame input[12], output[12];
        bool freeze[12];
        stmlib::GateFlags seed[12];
        const int size = mode >= kSynth ? 12 : int(processor.sample_rate()) / 4000;
        for (int i = 0; i < size; ++i, ++sample) {
          // Integer-generated stereo input is stable across compilers.
          input[i].l = float(int((sample * 73) % 8192) - 4096) / 16384.0f;
          input[i].r = float(int((sample * 131) % 8192) - 4096) / 16384.0f;
          freeze[i] = block > 1800 && block < 3300;
          seed[i] = sample % 401 == 0 ? stmlib::GATE_FLAG_RISING : stmlib::GATE_FLAG_LOW;
        }
        if (mode >= kSynth) {
          synth.Render(mode == kSynthUnpatched ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_ON,
              mode == kSynthUnpatched ? NULL : seed, freeze, static_cast<Synth::Resolution>(media),
              density, p, mix, output, size);
        } else {
          processor.Process(SEED_BUTTON_STATE_ON, seed, freeze, density, p, mix,
              input, output, size);
        }
        if (block < 2400) continue;
        for (int i = 0; i < size; ++i) {
          if (!std::isfinite(output[i].l) || !std::isfinite(output[i].r)) {
            return CaptureError(media, mode, block, "non-finite output");
          }
          nonzero |= output[i].l != 0.0f || output[i].r != 0.0f;
        }
        if (fwrite(output, sizeof(FloatFrame), size, file) != size_t(size)) {
          return CaptureError(media, mode, block, "write failed");
        }
      }
      if (!nonzero) return CaptureError(media, mode, 4400, "silent output");
    }
  }
  return fclose(file) ? CaptureError(-1, -1, -1, "close failed") : 0;
}
