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
// Six Plaits voices, played in place of the wavetable synth.

#ifndef BEADS_DSP_PLAITS_SYNTH_H_
#define BEADS_DSP_PLAITS_SYNTH_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/hysteresis_quantizer.h"

#include "beads/dsp/plaits_voice.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dsp/fx/reverb.h"

namespace beads {

#ifndef PLAITS_SYNTH_VOICES
#define PLAITS_SYNTH_VOICES 6
#endif  // PLAITS_SYNTH_VOICES
const int kNumPlaitsVoices = PLAITS_SYNTH_VOICES;

// C3, as the wavetable synth. Plaits tunes for 47872 Hz; Beads plays at
// 47991 Hz, 4.3 cents higher.
const float kPlaitsRootNote = 48.0f - 0.0429f;

// Engine RAM for all voices and the banks they share, lent by the caller
// while the synth plays.
const size_t kPlaitsSynthRamSize = kNumPlaitsVoices * kPlaitsVoiceRamSize +
    kPlaitsSpeechRamSize + kPlaitsFmBanksRamSize;

// FEEDBACK selects one of the eight models of the current bank.
const int kPlaitsBankSize = 8;
// Banks in Plaits' order: green, red, yellow.
const int kNumPlaitsBanks = 3;

class PlaitsSynth {
 public:
  PlaitsSynth() { }
  ~PlaitsSynth() { }

  void Init(uint16_t* reverb_buffer);
  // Builds the voices in kPlaitsSynthRamSize bytes of ram.
  void InitVoices(uint8_t* ram);
  void SetCloudsStyle(bool clouds, bool clear) {
    clouds_style_ = clouds;
    reverb_.Reset(kSynthSampleRate, clear);
  }

  void Render(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      Synth::Resolution resolution,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const Processor::MixParameters& mix_parameters,
      FloatFrame* out,
      size_t size);

  inline void set_output_limiter_mode(OutputLimiterMode mode) {
    output_limiter_mode_ = mode;
  }

  inline void set_stereo_width(float width, bool enabled) {
    stereo_width_ = width;
    width_enabled_ = enabled;
  }

  inline void set_pitch_scale(PitchScale scale) {
    pitch_scale_ = scale;
  }

  // Voices that play Plaits' AUX, on the right; the others play OUT, on the
  // left. Spread over the note rotation, as Rings' odd and even voices.
  void set_aux_voices(int count);

  void set_bank(int bank);

 private:
  void StartNote(const Parameters& p, int model);

  TriggerGenerator trigger_generator_;

  Reverb reverb_;
  Limiter pre_reverb_limiter_;
  Limiter output_limiter_;
  OutputLimiterMode output_limiter_mode_;

  PlaitsVoice voice_[kNumPlaitsVoices];
  PlaitsPatch patch_[kNumPlaitsVoices];
  float gain_l_[kNumPlaitsVoices];
  float gain_r_[kNumPlaitsVoices];
  bool aux_[kNumPlaitsVoices];
  int num_aux_voices_;
  int active_voice_;
  bool noon_;
  bool drone_;

  RandomWalkModulation harmonics_modulation_;
  RandomWalkModulation timbre_modulation_;
  RandomWalkModulation morph_modulation_;

  stmlib::HysteresisQuantizer2 model_quantizer_;
  int first_model_;
  float stereo_width_;
  bool width_enabled_;
  PitchScale pitch_scale_;
  bool clouds_style_;

  DISALLOW_COPY_AND_ASSIGN(PlaitsSynth);
};

}  // namespace beads

#endif  // BEADS_DSP_PLAITS_SYNTH_H_
