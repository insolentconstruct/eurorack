// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Plaits engines whose AUX Beads skips, built apart from the synth test.

#include "beads/test/plaits_aux.h"

#include <algorithm>

#include "stmlib/utils/buffer_allocator.h"

#define private public
#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine2/phase_distortion_engine.h"
#undef private

using namespace plaits;

const size_t kAuxBlockSize = 12;

static VirtualAnalogEngine virtual_analog_engine;
static PhaseDistortionEngine phase_distortion_engine;
static GrainEngine grain_engine;
static AdditiveEngine additive_engine;
static WaveshapingEngine waveshaping_engine;
static uint8_t engine_ram[4096];

// An engine and the oscillator that only AUX uses.
struct AuxOnlyState {
  Engine* engine;
  const uint8_t* bytes;
  size_t size;
};

template<typename T>
AuxOnlyState AuxOnly(Engine* engine, const T& state) {
  AuxOnlyState s = {
    engine, reinterpret_cast<const uint8_t*>(&state), sizeof(T)
  };
  return s;
}

bool AuxUntouched(int engine) {
  const AuxOnlyState states[kNumAuxSkippingEngines] = {
    AuxOnly(&virtual_analog_engine, virtual_analog_engine.auxiliary_),
    AuxOnly(&phase_distortion_engine, phase_distortion_engine.modulator_),
    AuxOnly(&grain_engine, grain_engine.z_oscillator_),
    AuxOnly(&additive_engine, additive_engine.harmonic_oscillator_[2]),
    AuxOnly(&waveshaping_engine, waveshaping_engine.triangle_)
  };
  const AuxOnlyState& s = states[engine];
  uint8_t before[256];
  if (s.size > sizeof(before)) {
    return false;
  }
  stmlib::BufferAllocator allocator(engine_ram, sizeof(engine_ram));
  s.engine->Init(&allocator);
  s.engine->Reset();
  std::copy(&s.bytes[0], &s.bytes[s.size], &before[0]);

  EngineParameters p;
  p.trigger = TRIGGER_UNPATCHED;
  p.note = 48.0f;
  p.timbre = p.morph = p.harmonics = 0.5f;
  p.accent = 0.8f;
  float out[kAuxBlockSize];
  float aux[kAuxBlockSize];
  for (int block = 0; block < 4; ++block) {
    bool already_enveloped = false;
    s.engine->Render(p, out, aux, kAuxBlockSize, &already_enveloped);
  }
  return std::equal(&s.bytes[0], &s.bytes[s.size], &before[0]);
}
