// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Reverb.

#ifndef BEADS_DSP_FX_REVERB_H_
#define BEADS_DSP_FX_REVERB_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/fx/fx_engine.h"

namespace beads {

enum ReverbCharacter {
  REVERB_STOCK,
  REVERB_CLOUDS,
  REVERB_PLATE,
  REVERB_BLOOM
};

class Reverb {
 public:
  struct CharacterState {
    void Init(uint8_t new_character, float sample_rate);
    void Reset(float sample_rate);
    float Density(float level);

    uint8_t character;
    bool onset;
    float peak;
    float baseline;
    float progress;
    float density;
    float tick;
  };

  Reverb() { }
  ~Reverb() { }
  
  void Init(uint16_t* buffer, float sample_rate) {
    engine_.Init(buffer);
    Reset(sample_rate);
  }
  
  void Reset(float sample_rate) {
    engine_.SetLFOFrequency(LFO_1, 0.5f / sample_rate);
    engine_.SetLFOFrequency(LFO_2, 0.3f / sample_rate);
    lp_ = 0.7f;
    lp_1_ = 0.0f;
    lp_2_ = 0.0f;
    mix_ = 0.0f;
    dry_ = 0.0f;
    wet_ = 0.0f;
    send_ = 0.0f;
  }

  void RenderCharacter(CharacterState& state, FloatFrame* in_out, size_t size);

  void Process(FloatFrame* in_out, size_t size) {
#include "beads/dsp/fx/reverb_tank.inc"
  }

  void ProcessClouds(FloatFrame* in_out, size_t size);

  void Reset(float sample_rate, bool clear) {
    if (clear) {
      Clear();
    }
    Reset(sample_rate);
  }

  // Initialize the profile's decay before selecting a codec callback.
  static bool InitFixedDecay(uint8_t choice) {
    static const float feedback[] = { 0.0f, 0.3f, 0.6f, 0.85f };
    fixed_decay_ = choice < sizeof(feedback) / sizeof(feedback[0])
        ? feedback[choice] : 0.0f;
    return fixed_decay_ != 0.0f;
  }

  static inline float fixed_decay() { return fixed_decay_; }

  inline void set_mix(float mix) { mix_ = mix; }
  inline void set_time(float reverb_time) { reverb_time_ = reverb_time; }
  inline void set_lp(float lp) { lp_ = lp; }
  inline void Clear() { engine_.Clear(); }

 private:
  void ProcessPlate(FloatFrame* in_out, size_t size);

  void ProcessBloom(CharacterState& state, FloatFrame* in_out, size_t size);

  typedef FxEngine<32768, FORMAT_16_BIT> E;
  static float fixed_decay_;
  E engine_;
  
  float mix_;
  float wet_;
  float dry_;
  float send_;
  float reverb_time_;
  float lp_;
  
  float lp_1_;
  float lp_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Reverb);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_REVERB_H_
