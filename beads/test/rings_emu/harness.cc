// Copyright 2015 Emilie Gillet.
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
// Emulator harness for the Rings synth blob: the same scenarios through the
// blob's entries and through a vanilla build of rings.cc's FillBuffer logic
// (Part, StringSynthPart and Strummer, configured as Rings' UI does). Driven
// by rings_compare.py under Unicorn's Cortex-M7.

#include <algorithm>

#include "stmlib/utils/random.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/synths/rings_synth_core.h"
#include "rings/dsp/part.h"
#include "rings/dsp/string_synth_part.h"
#include "rings/dsp/strummer.h"

using namespace beads;
using namespace rings;

const size_t kBlock = kMaxBlockSize;
// Scenario lengths: multiples of 24 samples, so both block sizes agree.
const size_t kScenarioSamples = 96000;
const size_t kNumScenarios = 40;
const size_t kTransitionPeriod = 24000;

enum HarnessMode {
  HARNESS_VANILLA,
  HARNESS_BLOB
};

// The vanilla driver, as rings.cc: one strummer, both cores sharing the
// reverb buffer, configured as ui.cc does.
Part part;
StringSynthPart string_synth;
Strummer strummer;
uint16_t reverb_buffer[32768];
RingsSynthTarget vanilla_target;
float in_level;

// What the blob writes besides its RAM and the stack.
extern "C" {
RingsSynthControls harness_controls;
float harness_out[kBlock];
float harness_aux[kBlock];
// The generator's seed (stmlib's own by default); G3 renders a second.
uint32_t harness_seed = 0x21;
}

// Triangle in [0, 1] of the given period, at sample t.
float Triangle(size_t t, size_t period) {
  const float phase = float(t % period) / float(period);
  return phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
}

const uint8_t kPolyphony[3] = { 1, 2, 4 };

RingsSynthTarget ScenarioTarget(size_t scenario, size_t t) {
  RingsSynthTarget target;
  target.core = RINGS_CORE_PART;
  target.model = 0;
  target.polyphony = 1;
  target.fx = FX_ENSEMBLE;
  if (scenario < 18) {
    target.model = scenario / 3;
    target.polyphony = kPolyphony[scenario % 3];
  } else if (scenario < 36) {
    target.core = RINGS_CORE_STRING_SYNTH;
    target.fx = (scenario - 18) / 3;
    target.polyphony = kPolyphony[scenario % 3];
  } else if (scenario == 36) {
    // Models and polyphony change in turn.
    const size_t step = t / kTransitionPeriod;
    const uint8_t models[4] = { 2, 1, 3, 4 };
    target.model = models[step % 4];
    target.polyphony = kPolyphony[step % 3];
  } else if (scenario == 37) {
    // Part and Disastrous peace alternate; the FX change clears its buffer.
    const size_t step = t / kTransitionPeriod;
    target.core = step & 1 ? RINGS_CORE_STRING_SYNTH : RINGS_CORE_PART;
    target.model = 5;
    target.fx = step < 2 ? FX_CHORUS : FX_REVERB;
    target.polyphony = step < 2 ? 2 : 4;
  } else if (scenario == 38) {
    target.model = 2;
    target.polyphony = 2;
  } else {
    target.core = RINGS_CORE_STRING_SYNTH;
    target.fx = FX_REVERB_2;
    target.polyphony = 4;
  }
  return target;
}

// The block's controls, as Beads would set them (constant over 24 samples).
void ScenarioControls(size_t scenario, size_t t, RingsSynthControls* c) {
  t = t / 24 * 24;
  Patch* patch = &c->patch;
  patch->structure = 0.05f + 0.9f * Triangle(t, 30000);
  patch->brightness = 0.25f + 0.75f * Triangle(t + 5000, 20000);
  patch->damping = 0.55f + 0.4f * Triangle(t, 45000);
  patch->position = Triangle(t + 1000, 17000);
  PerformanceState* s = &c->performance_state;
  const float notes[5] = { 0.0f, 7.0f, 12.0f, 5.0f, -12.0f };
  s->internal_exciter = true;
  s->tonic = 48.0f;
  s->fm = 0.0f;
  s->note = notes[(t / 7200) % 5];
  s->chord = static_cast<int32_t>(patch->structure * 10.0f + 0.5f);
  if (scenario == 38) {
    // SEED unpatched, PITCH patched: Rings' strummer follows note steps.
    s->strum = false;
    s->internal_strum = true;
    s->internal_note = false;
  } else {
    s->strum = t % 4800 == 0;
    s->internal_strum = false;
    s->internal_note = true;
  }
  if (scenario == 39) {
    // Held: the longest decay, a long quiet tail after two strums.
    patch->damping = 1.0f;
    s->strum = t == 0 || t == 9600;
  }
}

void VanillaPrime(uint8_t core, const RingsSynthControls& controls) {
  float in[kBlock];
  float out[kBlock];
  float aux[kBlock];
  std::fill(&in[0], &in[kBlock], 0.0f);
  PerformanceState performance_state = controls.performance_state;
  performance_state.strum = false;
  if (core == RINGS_CORE_STRING_SYNTH) {
    string_synth.Process(
        performance_state, controls.patch, in, out, aux, kBlock);
  } else {
    part.Process(performance_state, controls.patch, in, out, aux, kBlock);
  }
}

void VanillaConfigure(
    const RingsSynthTarget& target,
    const RingsSynthControls& controls,
    bool entry) {
  if (target.polyphony != vanilla_target.polyphony) {
    part.set_polyphony(target.polyphony);
    string_synth.set_polyphony(target.polyphony);
  }
  part.set_model(static_cast<ResonatorModel>(target.model));
  string_synth.set_fx(static_cast<FxType>(target.fx));
  vanilla_target = target;
  if (entry) {
    VanillaPrime(RINGS_CORE_PART, controls);
    VanillaPrime(RINGS_CORE_STRING_SYNTH, controls);
  } else {
    VanillaPrime(target.core, controls);
  }
}

// rings.cc's FillBuffer with silent input, before the conversion to 16 bits.
void VanillaFillBuffer(RingsSynthControls* c, float* out, float* aux) {
  float in[kBlock];
  if (vanilla_target.core == RINGS_CORE_STRING_SYNTH) {
    for (size_t i = 0; i < kBlock; ++i) {
      in[i] = 0.0f;
    }
    strummer.Process(NULL, kBlock, &c->performance_state);
    string_synth.Process(c->performance_state, c->patch, in, out, aux, kBlock);
  } else {
    for (size_t i = 0; i < kBlock; ++i) {
      const float kNoiseGateThreshold = 0.00003f;
      float in_sample = 0.0f;
      float error, gain;
      error = in_sample * in_sample - in_level;
      in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
      gain = in_level <= kNoiseGateThreshold
            ? (1.0f / kNoiseGateThreshold) * in_level : 1.0f;
      in[i] = gain * in_sample;
    }
    strummer.Process(in, kBlock, &c->performance_state);
    part.Process(c->performance_state, c->patch, in, out, aux, kBlock);
  }
}

extern "C" uint32_t harness_block_size() {
  return kBlock;
}

// Renders one scenario into out (out and aux interleaved per sample);
// returns the number of samples, or 0 if the blob refused a target.
extern "C" uint32_t harness_run(
    uint32_t mode,
    uint32_t scenario,
    const uint8_t* blob,
    uint8_t* ram,
    float* out) {
  stmlib::Random::Seed(harness_seed);
  const PlaitsBlobSynthHeader* h =
      reinterpret_cast<const PlaitsBlobSynthHeader*>(blob);
  RingsSynthInitFn init = reinterpret_cast<RingsSynthInitFn>(
      blob + h->init_offset);
  RingsSynthConfigureFn configure = reinterpret_cast<RingsSynthConfigureFn>(
      blob + h->configure_offset);
  RingsSynthRenderFn render = reinterpret_cast<RingsSynthRenderFn>(
      blob + h->render_offset);
  void* context = NULL;

  RingsSynthTarget target = ScenarioTarget(scenario, 0);
  ScenarioControls(scenario, 0, &harness_controls);
  if (mode == HARNESS_BLOB) {
    // The loader zeroes the declared RAM first.
    std::fill(&ram[0], &ram[h->ram_size], 0);
    context = init(ram, h->ram_size, NULL);
    if (!configure(context, &target, &harness_controls, true)) {
      return 0;
    }
  } else {
    // rings.cc's globals start zeroed, as at boot.
    std::fill(reinterpret_cast<uint8_t*>(&part),
              reinterpret_cast<uint8_t*>(&part + 1), 0);
    std::fill(reinterpret_cast<uint8_t*>(&string_synth),
              reinterpret_cast<uint8_t*>(&string_synth + 1), 0);
    std::fill(reinterpret_cast<uint8_t*>(&strummer),
              reinterpret_cast<uint8_t*>(&strummer + 1), 0);
    std::fill(&reverb_buffer[0], &reverb_buffer[32768], 0);
    strummer.Init(0.01f, kSampleRate / kMaxBlockSize);
    part.Init(reverb_buffer);
    string_synth.Init(reverb_buffer);
    in_level = 0.0f;
    vanilla_target.core = RINGS_CORE_PART;
    vanilla_target.model = RESONATOR_MODEL_MODAL;
    vanilla_target.polyphony = 1;
    vanilla_target.fx = FX_ENSEMBLE;
    VanillaConfigure(target, harness_controls, true);
  }
  for (size_t t = 0; t < kScenarioSamples; t += kBlock) {
    ScenarioControls(scenario, t, &harness_controls);
    if (t && t % kTransitionPeriod == 0) {
      const RingsSynthTarget next = ScenarioTarget(scenario, t);
      if (next.core != target.core || next.model != target.model ||
          next.polyphony != target.polyphony || next.fx != target.fx) {
        target = next;
        if (mode == HARNESS_BLOB) {
          if (!configure(context, &target, &harness_controls, false)) {
            return 0;
          }
        } else {
          VanillaConfigure(target, harness_controls, false);
        }
      }
    }
    if (mode == HARNESS_BLOB) {
      render(context, &harness_controls, harness_out, harness_aux, kBlock);
    } else {
      VanillaFillBuffer(&harness_controls, harness_out, harness_aux);
    }
    for (size_t i = 0; i < kBlock; ++i) {
      *out++ = harness_out[i];
      *out++ = harness_aux[i];
    }
  }
  return kScenarioSamples;
}
