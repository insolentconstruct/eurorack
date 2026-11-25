// Copyright 2015 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Rings' modal resonator, rendered by Plaits' batched SVFs. Coefficients and
// pickup position are updated once per block.

#ifndef BEADS_DSP_MODAL_RESONATOR_H_
#define BEADS_DSP_MODAL_RESONATOR_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "beads/dsp/frame.h"
#include "beads/dsp/limiter.h"
#include "plaits/dsp/physical_modelling/resonator.h"

namespace beads {

// A multiple of 8, sized so that all 32 grains still play beside it.
const int kNumResonatorModes = 24;
// Gain staging ahead of the mix: Rings' bank reaches +20 dBFS on noise.
const float kResonatorPeak = 0.5f;
// Scaled down, a sharp mode stays under the limiter's detector clamp.
const float kResonatorHeadroom = 1.0f / 16.0f;
// The default object: a harmonic, medium-bright string.
const float kResonatorStructure = 0.25f;
const float kResonatorBrightness = 0.5f;
const float kResonatorDamping = 0.5f;
const float kResonatorPosition = 0.3f;

// Drizzlet's own objects: modes in left-right pairs, each with its ring time
// over the fundamental's and its level; a strike bends the pitch.
struct ResonatorModes {
  float ratio[kNumResonatorModes];
  float decay[kNumResonatorModes];
  float gain[kNumResonatorModes];
  float glide;  // Frequency offset at a strike, falling to 0.
  float glide_rate;  // Per second.
};

struct ResonatorMaterial {
  float structure;
  float brightness;
  const ResonatorModes* modes;  // NULL: Rings' stiffness curve.
  float chord_gain;  // A grain's strike over the grains' level.
};

// Per side, each mode's |level| a, ratio r and Q factor d over the base Q,
// for the feedback path's bound; worked out off the audio path.
struct ResonatorBound {
  float structure;
  float brightness;
  float position;
  const ResonatorModes* modes;
  float peak;  // Largest a, and a x r x d.
  float peak_slope;
  float sum;  // Sum of a, and of a x r x d.
  float sum_slope;
  float spill;  // The most the other modes add off a mode's peak.
  float lowest_ratio;  // With the largest d: the slowest decay.
  float highest_ratio;
  float largest_decay;
};

// Two modes of a side on one pitch: no spill bound, the sum of peaks holds.
const float kResonatorNoSpill = 1.0e9f;
// Outward rounding of the bound.
const float kResonatorBoundMargin = 1.001f;

// Chord: each voice is one batch, the Material's first eight modes.
const int kNumChordVoices = kNumResonatorModes / 8;
// A voice is retuned only 42 dB under its loudest, so that a tail whose
// strikes partly cancelled is still 40 dB down.
const float kChordRetuneLevel = 0.00794328f;
const float kChordSilence = 1.0e-4f;
// A grain within half a semitone of a voice strikes it as it is.
const float kChordNoteTolerance = 0.9715319f;
// A grain's strike follows the grains' level, held for half a second.
const float kChordLevelRelease = 0.5f;

const int kNumResonatorMaterials = 8;
const int kNumResonatorRingTimes = 4;

// The options page's curated values; Hygrostat can set exact ones instead.
extern const ResonatorMaterial kResonatorMaterials[kNumResonatorMaterials];
extern const float kResonatorRingTimes[kNumResonatorRingTimes];

class ModalResonator {
 public:
  ModalResonator() { }
  ~ModalResonator() { }

  void Init(float sample_rate);
  void InitChord(float sample_rate);
  // Odd modes go to out, even modes to aux; aux == out gives the mono sum.
  void Process(const float* in, float* out, float* aux, size_t size);
  // Adds the limited stereo resonance to the frames.
  void Render(const float* in, FloatFrame* frames, size_t size);
  // 1 over Amount and the bank's gain bound: the loop copy's gain is at most
  // 1 at every frequency; 0 for an object other than the bound's.
  float LoopGain(const ResonatorBound& bound, size_t size);
  // The object's bound, worked out off the audio path.
  static void Bound(
      float structure,
      float brightness,
      float position,
      const ResonatorModes* modes,
      ResonatorBound* bound);

  inline void set_frequency(float frequency) {
    frequency_ = frequency;
  }

  inline void set_structure(float structure) {
    structure_ = structure;
  }

  inline void set_brightness(float brightness) {
    brightness_ = brightness;
  }

  inline float brightness() const { return brightness_; }
  inline float structure() const { return structure_; }
  inline float position() const { return position_; }
  inline const ResonatorModes* modes() const { return modes_; }

  inline void set_damping(float damping) {
    damping_ = damping;
  }

  inline void set_position(float position) {
    position_ = position;
  }

  // The resonance's level in the mix; 1 is unity.
  inline void set_amount(float amount) {
    amount_ = amount;
  }

  inline void set_modes(const ResonatorModes* modes) {
    // Another object starts unbent.
    if (modes != modes_) {
      glide_ = 0.0f;
    }
    modes_ = modes;
  }

  inline void Strike() {
    glide_ = modes_ ? modes_->glide : 0.0f;
  }

  // Chord: strikes[v * size + i] excites voice v at sample i. A grain at
  // this pitch ratio strikes its nearest voice, at gain times Rings' strum;
  // only a quiet voice is retuned.
  void StrikeVoice(
      float ratio,
      size_t offset,
      float gain,
      float* strikes,
      size_t size);
  // SEED and hits strike every tuned voice, as Rings' strum.
  void StrikeChord(float* strikes, size_t size);
  void RenderChord(const float* strikes, FloatFrame* frames, size_t size);

 private:
  void ProcessModes(const float* in, float* out, float* aux, size_t size);
  void Strike(
      int voice,
      size_t offset,
      float gain,
      float* strikes,
      size_t size);
  // Another object starts the voices unbent.
  inline void UpdateChordModes() {
    if (modes_ != chord_modes_) {
      std::fill(&chord_glide_[0], &chord_glide_[kNumChordVoices], 0.0f);
      chord_modes_ = modes_;
    }
  }

  float frequency_;
  float structure_;
  float brightness_;
  float damping_;
  float position_;
  float amount_;
  const ResonatorModes* modes_;
  float glide_;
  float sample_period_;
  // Pitch ratio (0 before the first strike), estimated level and peak.
  float chord_ratio_[kNumChordVoices];
  float chord_level_[kNumChordVoices];
  float chord_peak_[kNumChordVoices];
  float chord_glide_[kNumChordVoices];
  const ResonatorModes* chord_modes_;
  Limiter limiter_;

  plaits::ResonatorSvf<plaits::kModeBatchSize> odd_[kNumResonatorModes / 8];
  plaits::ResonatorSvf<plaits::kModeBatchSize> even_[kNumResonatorModes / 8];
  // The loop's bound: it rises at once and falls as the slowest mode decays.
  float peak_gain_;

  DISALLOW_COPY_AND_ASSIGN(ModalResonator);
};

}  // namespace beads

#endif  // BEADS_DSP_MODAL_RESONATOR_H_
