// Copyright 2026 Dirk Hoppmann.
// Copyright 2016 Emilie Gillet.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// ChordsSupersaw: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_CHORDS_SUPERSAW_ENGINE_H_
#define BEADS_BLOB_MODELS_CHORDS_SUPERSAW_ENGINE_H_

#include "beads/blob/models/palette_chord_bank.h"
#include "plaits/dsp/engine/engine.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/polyblep.h"

// The builder compiles a recipe's unused stereo paths out by setting this to 0.

namespace plaits {

// Saws per chord tone. The chord bank's four notes run this many each, so the
const int kSupersawUnison = 3;

// A single band-limited sawtooth. One polyBLEP correction per wrap, carried
enum SupersawShape {
  // Plain saw: no shape arithmetic at all beyond the ramp itself. The cheapest
  SUPERSAW_SHAPE_SAW,
  SUPERSAW_SHAPE_SINE_TO_SAW,
  SUPERSAW_SHAPE_SAW_TO_SQUARE
};

class SupersawVoice {
 public:
  SupersawVoice() { }
  ~SupersawVoice() { }

  inline void Init() {
    phase_ = 0.0f;
    next_sample_ = 0.0f;
    frequency_ = 0.001f;
    gain_ = 0.0f;
  }

  inline void set_phase(float phase) {
    phase_ = phase;
  }

  inline void Render(
      SupersawShape shape,
      float amount,
      float frequency,
      float gain,
      float* out,
      size_t size) {
    CONSTRAIN(frequency, 1.0e-7f, 0.49f);

    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    stmlib::ParameterInterpolator gain_modulation(&gain_, gain, size);

    float phase = phase_;
    float next_sample = next_sample_;

    if (shape == SUPERSAW_SHAPE_SAW) {
      while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;

        const float f = fm.Next();
        const float g = gain_modulation.Next();

        phase += f;
        if (phase >= 1.0f) {
          phase -= 1.0f;
          // The ramp falls from +g to -g at the wrap: a step of -2g.
          const float t = phase / f;
          const float discontinuity = -2.0f * g;
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
        }
        next_sample += (2.0f * phase - 1.0f) * g;
        *out++ += this_sample;
      }
    } else if (shape == SUPERSAW_SHAPE_SINE_TO_SAW) {
      const float sine_amount = 1.0f - amount;
      while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;

        const float f = fm.Next();
        const float g = gain_modulation.Next();

        phase += f;
        if (phase >= 1.0f) {
          phase -= 1.0f;
          // Only the saw half of the mix steps at the wrap; the sine reaches
          const float t = phase / f;
          const float discontinuity = -2.0f * g * amount;
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
        }
        const float x = 2.0f * phase - 1.0f;
        const float folded = x < 0.0f ? -x : x;
        // 4x(1-|x|) is one cycle of a parabolic sine: about 3% third harmonic,
        const float sine = 4.0f * x * (1.0f - folded);
        next_sample += (x * amount + sine * sine_amount) * g;
        *out++ += this_sample;
      }
    } else {
      // A square is one saw minus the same saw shifted half a cycle, so this
      float aux_phase = phase + 0.5f;
      if (aux_phase >= 1.0f) {
        aux_phase -= 1.0f;
      }
      while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;

        const float f = fm.Next();
        const float g = gain_modulation.Next();

        phase += f;
        if (phase >= 1.0f) {
          phase -= 1.0f;
          const float t = phase / f;
          const float discontinuity = -2.0f * g;
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
        }
        aux_phase += f;
        if (aux_phase >= 1.0f) {
          aux_phase -= 1.0f;
          // The subtracted ramp steps the other way, scaled by the morph.
          const float t = aux_phase / f;
          const float discontinuity = 2.0f * g * amount;
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
        }
        next_sample += ((2.0f * phase - 1.0f)
            - amount * (2.0f * aux_phase - 1.0f)) * g;
        *out++ += this_sample;
      }
    }

    phase_ = phase;
    next_sample_ = next_sample;
  }


 private:
  float phase_;
  float next_sample_;
  float frequency_;
  float gain_;
};

class ChordsSupersaw {
 public:
  ChordsSupersaw() { }
  ~ChordsSupersaw() { }

  void Init(stmlib::BufferAllocator* allocator);
  void Reset();
  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);
  // No HardSync() hook: the bounded sync fallback follows its reset with a

 private:
  void ScatterPhases();

  SupersawVoice voice_[kPaletteChordNumNotes][kSupersawUnison];
  PaletteChordBank chords_;

  float morph_lp_;
  float timbre_lp_;
  int active_unison_;

  DISALLOW_COPY_AND_ASSIGN(ChordsSupersaw);
};

class ChordsSupersawEngine : public Engine {
 public:
  ChordsSupersawEngine() { }
  ~ChordsSupersawEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    chords_supersaw_.Init(allocator);
  }

  virtual void Reset() {
    chords_supersaw_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    chords_supersaw_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  ChordsSupersaw chords_supersaw_;

  DISALLOW_COPY_AND_ASSIGN(ChordsSupersawEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_CHORDS_SUPERSAW_ENGINE_H_
