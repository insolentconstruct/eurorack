// Copyright 2026 Lyle Mills.
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
// NaturalSpeech: Palette 0390557b, mono, no FM, fourth macro 0.5.

#ifndef BEADS_BLOB_MODELS_NATURAL_SPEECH_ENGINE_H_
#define BEADS_BLOB_MODELS_NATURAL_SPEECH_ENGINE_H_

#include "plaits/dsp/engine/engine.h"
#include "stmlib/dsp/hysteresis_quantizer.h"

namespace plaits {

class NaturalSpeech {
 public:
  NaturalSpeech() { }
  ~NaturalSpeech() { }
  void Init(stmlib::BufferAllocator* allocator);
  void Reset();

  void Render(const EngineParameters& parameters, float* out, float* aux,
      size_t size, bool* already_enveloped);

 private:
  enum { kOrder = 18, kBands = 5 };

  struct Biquad {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float x1;
    float x2;
    float y1;
    float y2;
    void Set(float b0_, float b1_, float b2_, float a1_, float a2_) {
      b0 = b0_;
      b1 = b1_;
      b2 = b2_;
      a1 = a1_;
      a2 = a2_;
      x1 = x2 = y1 = y2 = 0.0f;
    }
    inline float Process(float x) {
      float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
      return y;
    }
  };

  void DesignBands();
  void DecodeFrame(int frame_index, float gamma);
  void UpdateBandWeights(int band);
  float InternalTick(float f0_phase_inc, float* whisper);

  float k_target_[kOrder];
  float v_target_[kBands];
  float gain_target_;
  float f0_st_target_;
  bool voiced_;

  float k_[kOrder];
  float v_[kBands];
  float gain_;
  float f0_st_;
  int smooth_countdown_;

  float wp_comp_[kBands];
  float wn_cal_[kBands];

  float lattice_[kOrder + 1];
  float whisper_lattice_[kOrder + 1];

  Biquad pulse_lp_[kBands - 1];
  Biquad noise_lp_[kBands - 1];
  float noise_cal_[kBands];
  float period_phase_;
  float period_samples_;
  int wavelet_pos_;
  float period_amp_;
  float denormal_guard_;
  float pole_damp_;
  float jitter_mul_;
  float flutter_phase_[3];
  int flutter_countdown_;
  float flutter_value_;

  float prosody_amount_;
  float prosody_now_;
  float speed_bipolar_;
  int bank_;
  int words_in_bank_;
  int word_;
  int playback_frame_;
  float frame_phase_;
  bool word_done_;
  int last_decoded_frame_;
  float last_decoded_gamma_;
  stmlib::HysteresisQuantizer2 bank_quantizer_;
  stmlib::HysteresisQuantizer2 word_quantizer_;

  float clock_phase_;
  float sample_[2];
  float next_sample_[2];

  DISALLOW_COPY_AND_ASSIGN(NaturalSpeech);
};

class NaturalSpeechEngine : public Engine {
 public:
  NaturalSpeechEngine() { }
  ~NaturalSpeechEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    natural_speech_.Init(allocator);
  }

  virtual void Reset() {
    natural_speech_.Reset();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    natural_speech_.Render(parameters, out, aux, size, already_enveloped);
  }

 private:
  NaturalSpeech natural_speech_;

  DISALLOW_COPY_AND_ASSIGN(NaturalSpeechEngine);
};

}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_NATURAL_SPEECH_ENGINE_H_
