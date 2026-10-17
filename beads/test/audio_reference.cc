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
// Recording quality and freeze tests through the audio processor.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#include "beads/test/test_checker.h"

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
unsigned test_writes = 0;
namespace stmlib {
SystemClock system_clock;
}  // namespace stmlib
namespace beads {
bool Leds::Init() {
  Clear();
  return true;
}
void Leds::Clear() { std::fill(intensity_, intensity_ + LED_LAST, 0); }
void Leds::Write() { }
}  // namespace beads

// Compare these native-host samples byte for byte against the preserved
// pre-optimization DSP, compiled with the same compiler and floating point flags.
// This is independent of linked-ARM emulator evidence.
static Processor processor;
static Synth synth;
static uint8_t banks[3][262144];
static uint16_t reverb[32768];

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  FILE* file = fopen(argv[1], "wb");
  if (!file) return 2;
  for (int media = 0; media < 4; ++media) {
    for (int mode = 0; mode < 5; ++mode) {
      memset(banks, 0, sizeof(banks));
      memset(reverb, 0, sizeof(reverb));
      memset(static_cast<void*>(&processor), 0, sizeof(processor));
      uint8_t* buffers[] = { banks[0], banks[1], banks[2] };
      stmlib::Random::Seed(0x12345678);
      processor.Init(buffers, reverb);
      memset(static_cast<void*>(&synth), 0, sizeof(synth));
      synth.Init(reverb);
      processor.Configure(static_cast<Processor::Media>(media), mode == 1);
      Parameters p = { };
      p.time.base = 0.37f;
      p.time.attenuverter = 0.17f;
      p.size.base = mode == 2 ? 1.0f : mode >= 3 ? 0.0f : 0.48f;
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
        const int size = mode >= 3 ? 12 : int(processor.sample_rate()) / 4000;
        for (int i = 0; i < size; ++i, ++sample) {
          // Integer-generated stereo input is stable across compilers.
          input[i].l = float(int((sample * 73) % 8192) - 4096) / 16384.0f;
          input[i].r = float(int((sample * 131) % 8192) - 4096) / 16384.0f;
          freeze[i] = block > 1800 && block < 3300;
          seed[i] = sample % 401 == 0 ? stmlib::GATE_FLAG_RISING : stmlib::GATE_FLAG_LOW;
        }
        if (mode >= 3) {
          synth.Render(mode == 4 ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_ON,
              mode == 4 ? NULL : seed, freeze, static_cast<Synth::Resolution>(media),
              density, p, mix, output, size);
        } else {
          processor.Process(SEED_BUTTON_STATE_ON, seed, freeze, density, p, mix,
              input, output, size);
        }
        if (block < 2400) continue;
        for (int i = 0; i < size; ++i) {
          if (!std::isfinite(output[i].l) || !std::isfinite(output[i].r)) return 1;
          nonzero |= output[i].l != 0.0f || output[i].r != 0.0f;
        }
        if (fwrite(output, sizeof(FloatFrame), size, file) != size_t(size)) return 1;
      }
      if (!nonzero) return 1;
    }
  }
  return fclose(file) ? 1 : 0;
}
