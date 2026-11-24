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
// Beads' controls for the Rings synth: FEEDBACK's twelve zones, the knobs as
// Rings' parameters, and Beads' triggers as strums.

#ifndef BEADS_DSP_RINGS_SYNTH_H_
#define BEADS_DSP_RINGS_SYNTH_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/hysteresis_quantizer.h"

#include "beads/blob/synths/rings_synth_controls.h"
#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dsp/fx/reverb.h"

namespace beads {

// Six resonator models, then Disastrous peace with each of its six FX.
const int kNumRingsZones = 12;
// C3, as both Beads synths. Rings tunes for 48 kHz: Beads plays 0.33 cents
// flat, left as is so the core stays vanilla.
const float kRingsRootNote = 48.0f;
// Rings' own reach: TUNE 12 to 72 plus V/OCT at its factory calibration.
const float kRingsLowestNote = 12.0f + 66.67f - 84.26f;
const float kRingsHighestNote = 72.0f + 66.67f;
// Rings' STRUCTURE range (cv_scaler.cc).
const float kRingsMaxStructure = 0.9995f;
// Rings' out and aux into Beads: its median peak and RMS then fall within
// about 1 dB of the Plaits synth's (whose voices are scaled to 10 Vpp).
const float kRingsGain = 1.12f;
// A FEEDBACK zone or polyphony is applied once held this many blocks
// (30 ms), so a sweep reconfigures once.
const int kRingsSettleBlocks = 120;

class RingsSynth {
 public:
  RingsSynth() { }
  ~RingsSynth() { }

  void Init();
  // The Beads-side chain, as the Plaits synth's, in its reverb buffer.
  void Init(uint16_t* reverb_buffer);
  void SetCloudsStyle(bool clouds, bool clear) {
    SetReverbCharacter(clouds, clear);
  }
  void SetReverbCharacter(uint8_t character, bool clear) {
    clouds_style_ = character == REVERB_CLOUDS;
    reverb_.set_character(character);
    reverb_.Reset(kSynthSampleRate, clear);
  }

  // A blob the region checked (NULL: silent until one is bound).
  void set_blob(const uint8_t* blob);
  // The blob's entries, or the host tests' core.
  void set_entries(
      RingsSynthInitFn init,
      RingsSynthConfigureFn configure,
      RingsSynthRenderFn render);
  // Whether a blob fits: its synth ID, RAM, control block and block size.
  static bool Fits(const uint8_t* blob, size_t ram_size);

  // Main loop, codec paused: builds Rings in ram (zeroed first) for
  // FEEDBACK's zone and the polyphony, priming both cores.
  void InitVoices(
      uint8_t* ram,
      size_t ram_size,
      const PlaitsBlobServices* services,
      float feedback);
  // Main loop, codec paused: applies a settled zone or polyphony change.
  void Configure();
  inline bool configure_pending() const { return pending_; }

  // Audio interrupt.
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
  void RenderFixedDecay(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      Synth::Resolution resolution,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const Processor::MixParameters& mix_parameters,
      FloatFrame* out,
      size_t size);

  // Polyphony 1, 2 or 4 (feature 42).
  inline void set_polyphony(int polyphony) { polyphony_ = polyphony; }
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

  // FEEDBACK's zone and the saved polyphony, as the core's target. The core
  // that does not play keeps its last model or FX.
  void ComputeTarget(float feedback, int polyphony, RingsSynthTarget* target);

  // One block's controls. pitch: semitones from C3 after Pitch scale.
  // trigger: SEED, its gate or the DENSITY clock fired in this block.
  void ComputeControls(
      float pitch,
      float time,
      float size,
      float shape,
      float dry_wet,
      bool sustain,
      bool trigger,
      bool seed_patched,
      bool pitch_patched,
      RingsSynthControls* controls);

 private:
  stmlib::HysteresisQuantizer2 zone_quantizer_;
  RingsSynthTarget target_;
  int32_t chord_;

  TriggerGenerator trigger_generator_;
  Reverb reverb_;
  Limiter pre_reverb_limiter_;
  Limiter output_limiter_;
  OutputLimiterMode output_limiter_mode_;
  float stereo_width_;
  bool width_enabled_;
  PitchScale pitch_scale_;
  bool clouds_style_;
  int polyphony_;

  RingsSynthInitFn init_;
  RingsSynthConfigureFn configure_;
  RingsSynthRenderFn render_;
  void* context_;
  // The configured target, the wanted one and how long it has been held.
  RingsSynthTarget configured_;
  RingsSynthTarget wanted_;
  int settle_;
  volatile bool pending_;
  RingsSynthControls controls_;
  // FEEDBACK as last rendered.
  float feedback_;

  DISALLOW_COPY_AND_ASSIGN(RingsSynth);
};

}  // namespace beads

#endif  // BEADS_DSP_RINGS_SYNTH_H_
