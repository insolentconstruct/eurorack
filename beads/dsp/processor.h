// Copyright 2017 Emilie Gillet.
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
// The whole FX chain.

#ifndef BEADS_DSP_PROCESSOR_H_
#define BEADS_DSP_PROCESSOR_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/filter.h"

#include "beads/dsp/granular_processor.h"
#include "beads/dsp/modal_resonator.h"
#include "beads/dsp/modulated_parameter.h"
#include "beads/dsp/spectral_processor.h"
#include "beads/dsp/time_scan.h"
#include "beads/dsp/fx/diffuser.h"
#include "beads/dsp/fx/feedback_diffuser.h"
#include "beads/dsp/fx/reverb.h"
#include "beads/dsp/fx/wow_and_flutter.h"

// Setting this flag slows down the codec to 24kHz for the MEDIA_DIRTY_TAPE
// setting. The codec becomes very hissy at this sample rate, but this has
// a certain charm!
// 
// When this flag is not set, the codec is run at 48kHz and the wet signal is
// downsampled by a 0.5 factor before being processed. Low-pass filtering
// is also applied to the output signal within the wow and flutter code.
#define ENABLE_HARDWARE_24K

namespace beads {

// (512 + 256) * 1000 bytes, 48kHz 16-bit stereo = 4 seconds
const size_t kDelayBufferSizeBits = 768 * 1000 * 8;

const float kDelayModeTransitionTime = 1.5f;  // seconds
const float kDelayModeFadeTime = 0.75f;  // seconds

// The grains' mono sum excites the resonator.
const float kResonatorInputGain = 0.5f;
// PITCH at noon tunes it to C3.
const float kResonatorRootNote = 48.0f;

enum ResonatorPlacement {
  RESONATOR_OFF,
  RESONATOR_AFTER_GRAINS,
  RESONATOR_BEFORE_BUFFER,
  // After the grains, each grain striking one voice of a chord.
  RESONATOR_CHORD
};

struct ResonatorControls {
  float structure;
  float brightness;
  float damping;
  float position;
  float amount;
  // While the CV moves damping, FEEDBACK is the knob alone.
  float feedback;
  bool hold_feedback;
  const ResonatorModes* modes;
  float chord_gain;
};

class Processor {
 public:
  Processor() { }
  ~Processor() { }
  
  enum Media {
    MEDIA_DIRTY_TAPE,
    MEDIA_CLEAN_TAPE,
    MEDIA_VINTAGE_DIGITAL,
    MEDIA_DIGITAL
  };
  
  struct MixParameters {
    float feedback;
    float dry_wet;
    float reverb_amount;
    bool output_gate;
  };
  
  void Init(uint8_t* buffers[], uint16_t* reverb_buffer);
  void SetCloudsStyle(bool clouds);
  // Attacks (#58, #47); changing the mode forgets the marks.
  void SetAttacks(int mode, float threshold) {
    attack_input_.Configure(mode, threshold);
    processor_digital_.SetAttacks(mode);
    processor_vintage_digital_.SetAttacks(mode);
    processor_clean_tape_.SetAttacks(mode);
    processor_dirty_tape_.SetAttacks(mode);
  }

  inline void set_time_scan(bool enabled) {
    SetOption(OPTION_FLAG_TIME_SCAN, enabled);
  }
  // Wet rises linearly with DRY/WET; dry and both ends stay stock.
  inline void set_gentle_mix(bool gentle) {
    SetOption(OPTION_FLAG_GENTLE_MIX, gentle);
  }
  // Reverb on the grains before the dry/wet mix, as in Clouds.
  inline void set_reverb_on_grains(bool grains) {
    SetOption(OPTION_FLAG_REVERB_ON_GRAINS, grains);
  }
  // Delay mode: FEEDBACK has a unity notch and the loop keeps its bass.
  inline void set_unity_feedback(bool unity) {
    SetOption(OPTION_FLAG_UNITY_FEEDBACK, unity);
    SetFeedbackCutoff(false);
  }
  // Delay mode: knob timing crossfades between delay times, as when clocked.
  inline void set_delay_crossfade(bool crossfade) {
    SetOption(OPTION_FLAG_DELAY_CROSSFADE, crossfade);
  }
  // Rings' modal resonator on the grains, or on the input before the buffer.
  inline void set_resonator(int placement) {
    // One store: the audio interrupt never sees both placements.
    const uint8_t flag = placement == RESONATOR_AFTER_GRAINS ||
        placement == RESONATOR_CHORD
        ? OPTION_FLAG_RESONATOR_GRAINS
        : (placement == RESONATOR_BEFORE_BUFFER
            ? OPTION_FLAG_RESONATOR_INPUT : 0);
    resonator_sample_rate_ = 0.0f;
    resonator_chord_ = placement == RESONATOR_CHORD;
    options_ = (options_ &
        ~(OPTION_FLAG_RESONATOR_GRAINS | OPTION_FLAG_RESONATOR_INPUT)) | flag;
    resonator_sample_rate_ = 0.0f;
    // A SEED held while it starts does not strike it.
    seed_gate_ = true;
  }
  // Set at the UI rate; until then the resonator keeps its defaults.
  inline void set_resonator_controls(const ResonatorControls& controls) {
    resonator_controls_ = controls;
    resonator_controls_set_ = true;
  }
  // Configure turns it off; the main loop sets it again each pass.
  inline void set_seed_latch_fade(bool enabled) {
    delay_controller_.set_timing_fade(enabled);
  }

  inline bool delay_active() const {
    return delay_mode_transition_ > 0.0f && !spectral_;
  }

  inline AttackInput* attack_input() { return &attack_input_; }

  // Attacks are marked only while granular playback records.
  inline bool attack_recording() const {
    return delay_mode_transition_ <= 0.0f && !spectral_;
  }

  // Snap and trigger: a hit this block adds a grain, as SEED latch Add does.
  inline void set_attack_trigger(bool trigger) {
    attack_trigger_ = trigger;
  }

  void SetAttackOnset(bool onset) {
    switch (media_) {
      case MEDIA_DIRTY_TAPE:
        processor_dirty_tape_.SetAttackOnset(onset);
        break;
      case MEDIA_CLEAN_TAPE:
        processor_clean_tape_.SetAttackOnset(onset);
        break;
      case MEDIA_VINTAGE_DIGITAL:
        processor_vintage_digital_.SetAttackOnset(onset);
        break;
      default:
        processor_digital_.SetAttackOnset(onset);
        break;
    }
  }
  void Configure(Media media, bool mono);
  
  inline Media media() const { return media_; }
  inline bool mono() const { return mono_; }

  // Takes effect at the next Configure, which clears the recording.
  inline void set_spectral(bool spectral) { spectral_ = spectral; }
  inline bool spectral() const { return spectral_; }

  // Main loop: the spectral engine's frames.
  inline void Buffer() {
    if (spectral_) {
      spectral_processor_.Buffer();
    }
  }
  
  inline float sample_rate() const {
#ifdef ENABLE_HARDWARE_24K
    float sr[4] = { 24000.0f, 48000.0f, 32000.0f, 48000.0f };
#else
    float sr[4] = { 48000.0f, 48000.0f, 32000.0f, 48000.0f };
#endif  // ENABLE_HARDWARE_24K
    return sr[media_];
  }
  
  inline int ratio() const {
#ifdef ENABLE_HARDWARE_24K
    int ratio[4] = { 1, 2, 1, 1 };
#else
    int ratio[4] = { 2, 2, 1, 1 };
#endif  // ENABLE_HARDWARE_24K
    return ratio[media_];
  }
  
  inline uint8_t* delay_buffer_raw_bytes(size_t offset) {
    // Independently of the quality settings, we're writing in the same
    // bytes anyway!
    return processor_vintage_digital_.delay_buffer_raw_bytes(mono_, offset);
  }
  
  inline int* mutable_delay_write_ptr() {
    switch (media_) {
      case MEDIA_DIRTY_TAPE:
        return processor_dirty_tape_.mutable_delay_write_ptr(mono_);
      case MEDIA_CLEAN_TAPE:
        return processor_clean_tape_.mutable_delay_write_ptr(mono_);
      case MEDIA_VINTAGE_DIGITAL:
        return processor_vintage_digital_.mutable_delay_write_ptr(mono_);
      case MEDIA_DIGITAL:
      default:
        return processor_digital_.mutable_delay_write_ptr(mono_);
    }
  }
  
  inline int* mutable_delay_duration() {
    switch (media_) {
      case MEDIA_DIRTY_TAPE:
        return processor_dirty_tape_.mutable_delay_duration(mono_);
      case MEDIA_CLEAN_TAPE:
        return processor_clean_tape_.mutable_delay_duration(mono_);
      case MEDIA_VINTAGE_DIGITAL:
        return processor_vintage_digital_.mutable_delay_duration(mono_);
      case MEDIA_DIGITAL:
      default:
        return processor_digital_.mutable_delay_duration(mono_);
    }
  }

  inline float overwrite_reserve() const {
    float reserve = float(remaining());
    float fade_duration = sample_rate() * 0.15f / float(ratio());
    return reserve >= fade_duration ? 1.0f : reserve / fade_duration;
  }
  
  inline int remaining() const {
    switch (media_) {
      case MEDIA_DIRTY_TAPE:
        return processor_dirty_tape_.remaining(mono_);
      case MEDIA_CLEAN_TAPE:
        return processor_clean_tape_.remaining(mono_);
      case MEDIA_VINTAGE_DIGITAL:
        return processor_vintage_digital_.remaining(mono_);
      case MEDIA_DIGITAL:
      default:
        return processor_digital_.remaining(mono_);
    }
  }

  inline void Trim() {
    switch (media_) {
      case MEDIA_DIRTY_TAPE:
        processor_dirty_tape_.Trim(mono_);
        break;
      case MEDIA_CLEAN_TAPE:
        processor_clean_tape_.Trim(mono_);
        break;
      case MEDIA_VINTAGE_DIGITAL:
        processor_vintage_digital_.Trim(mono_);
        break;
      case MEDIA_DIGITAL:
      default:
        processor_digital_.Trim(mono_);
        break;
    }
  }
  
  void WasteCPUCycles(size_t size) {
    // The one with the most convoluted memory access!
    processor_vintage_digital_.WasteCPUCycles(size);
  }

  void Process(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const MixParameters& mix_parameters,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size);
  
  inline void set_output_limiter_mode(OutputLimiterMode mode) {
    output_limiter_mode_ = mode;
  }

  inline void set_delay_mode(DelayMode mode) {
    delay_mode_ = mode;
    processor_digital_.set_delay_mode(mode);
    processor_vintage_digital_.set_delay_mode(mode);
    processor_clean_tape_.set_delay_mode(mode);
    processor_dirty_tape_.set_delay_mode(mode);
  }

  // Clears the reverb's memory, which the synths share.
  inline void ClearReverb() {
    reverb_.Reset(sample_rate(), true);
  }

  // Frozen feedback evolves through an all-pass loop (#4). Turning it off
  // lets the loop fade out first.
  inline void set_evolving_feedback(bool evolving) {
    evolving_feedback_ = evolving;
  }

  // Grain diffusion; zero skips the diffuser, which restarts clean.
  inline void set_diffusion(float amount) {
    if (amount > 0.0f && diffusion_ == 0.0f) {
      diffuser_.Init(diffuser_buffer_);
    }
    diffusion_ = amount;
    diffuser_.set_amount(amount);
  }

  inline void set_stereo_width(float width, bool enabled) {
    processor_digital_.set_stereo_width(width, enabled);
    processor_vintage_digital_.set_stereo_width(width, enabled);
    processor_clean_tape_.set_stereo_width(width, enabled);
    processor_dirty_tape_.set_stereo_width(width, enabled);
  }

  inline void set_pitch_scale(PitchScale scale) {
    processor_digital_.set_pitch_scale(scale);
    processor_vintage_digital_.set_pitch_scale(scale);
    processor_clean_tape_.set_pitch_scale(scale);
    processor_dirty_tape_.set_pitch_scale(scale);
    spectral_processor_.set_pitch_scale(scale);
  }

  // Grains only; spectral has no random grain pitch.
  inline void set_random_intervals(bool intervals) {
    processor_digital_.set_random_intervals(intervals);
    processor_vintage_digital_.set_random_intervals(intervals);
    processor_clean_tape_.set_random_intervals(intervals);
    processor_dirty_tape_.set_random_intervals(intervals);
  }

  // Chosen at power-on: the grain stealing of the 2024 firmware.
  inline void set_legacy_stealing(bool legacy) {
    processor_digital_.set_legacy_stealing(legacy);
    processor_vintage_digital_.set_legacy_stealing(legacy);
    processor_clean_tape_.set_legacy_stealing(legacy);
    processor_dirty_tape_.set_legacy_stealing(legacy);
  }

#ifdef DWT_CALIBRATION
  void PrepareCalibration(uint32_t block) {
    processor_digital_.PrepareCalibration(block);
  }
#endif  // DWT_CALIBRATION

 private:
  enum OptionFlags {
    OPTION_FLAG_TIME_SCAN = 1,
    OPTION_FLAG_GENTLE_MIX = 2,
    OPTION_FLAG_REVERB_ON_GRAINS = 4,
    OPTION_FLAG_UNITY_FEEDBACK = 8,
    OPTION_FLAG_DELAY_CROSSFADE = 16,
    // State, not a choice: the unity loop's 10 Hz cutoff is in place.
    OPTION_FLAG_LOW_CUTOFF = 32,
    OPTION_FLAG_RESONATOR_GRAINS = 64,
    OPTION_FLAG_RESONATOR_INPUT = 128
  };

  inline void SetOption(OptionFlags flag, bool enabled) {
    options_ = enabled ? (options_ | flag) : (options_ & ~flag);
  }

  void ProcessWithOptions(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const MixParameters& mix_parameters,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size);
  // One source for both blocks, each inlined in ProcessWithOptions.
  template<bool resonator>
  inline void ProcessOptions(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      const ModulatedParameter& density,
      const Parameters& parameters,
      const MixParameters& mix_parameters,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size) __attribute__((always_inline));
  // A flat notch at unity loop gain; both ends of the knob stay stock.
  static float UnityFeedback(float feedback, float tap_gain);
  // 10 Hz in the unity delay loop, stock 40 Hz everywhere else.
  void SetFeedbackCutoff(bool low);
  void MixDryWet(
      const FloatFrame* dry_signal,
      float dry,
      float wet,
      FloatFrame* out,
      size_t size);
  void ProcessReverb(float reverb, FloatFrame* out, size_t size);
  void QuantizeDrySignal(const FloatFrame* in, FloatFrame* out, size_t size);
  // The page's controls, once the UI has set them.
  void ApplyResonatorControls();
  // Struck by input hits, tuned by PITCH; adds its resonance to the frames.
  void ProcessResonator(
      const RandomizedParameter& pitch,
      bool strike,
      FloatFrame* frames,
      size_t size);
  // Each grain started in the block strikes one voice of the chord.
  void ProcessChord(
      bool strike,
      const TriggerGenerator::Output* triggers,
      FloatFrame* frames,
      size_t size);
  template<typename GrainSource>
  void StrikeChord(
      const GrainSource& source,
      const TriggerGenerator::Output* triggers,
      float gain,
      float* strikes,
      size_t size);
  // A hit marked for this block's grains, read before they take it.
  bool AttackOnset() const;
  // A rising SEED, button and jack merged as the grains see them.
  bool SeedStrike(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      size_t size);
  inline void AddAttackTrigger(TriggerGenerator::Output* triggers) {
    if (attack_trigger_) {
      triggers[0].trigger = true;
      triggers[0].gate = true;
      triggers[0].fractional_sample = 0.0f;
    }
  }

  GranularProcessor<
      RESOLUTION_16_BIT,
      kDelayBufferSizeBits / (16 * 2),
      1> processor_digital_;
  GranularProcessor<
      RESOLUTION_12_BIT,
      kDelayBufferSizeBits / (12 * 2),
      1> processor_vintage_digital_;
  GranularProcessor<
      RESOLUTION_12_BIT,
      kDelayBufferSizeBits / (12 * 2),
      2> processor_clean_tape_;
#ifdef ENABLE_HARDWARE_24K
  GranularProcessor<
      RESOLUTION_8_BIT,
      kDelayBufferSizeBits / (8 * 2),
      1> processor_dirty_tape_;
#else
  GranularProcessor<
      RESOLUTION_8_BIT,
      kDelayBufferSizeBits / (8 * 2),
      2> processor_dirty_tape_;
#endif  // ENABLE_HARDWARE_24K

  SpectralProcessor spectral_processor_;
  bool spectral_;

  TriggerGenerator trigger_generator_;
  DelayController delay_controller_;
  
  Media media_;
  bool process_dry_;
  bool mono_;
  
  float delay_mode_transition_;
  DelayMode delay_mode_;
  uint8_t options_;
  TimeScan time_scan_;

  float dry_;
  float wet_;
  
  WowAndFlutter wow_and_flutter_;
  Reverb reverb_;
  Limiter pre_reverb_limiter_;
  Limiter output_limiter_;
  OutputLimiterMode output_limiter_mode_;
  Diffuser diffuser_;
  uint16_t diffuser_buffer_[kDiffuserBufferSize];
  bool clouds_style_;
  float diffusion_;
  FeedbackDiffuser feedback_diffuser_;
  bool evolving_feedback_;
  AttackInput attack_input_;
  bool attack_trigger_;
  ModalResonator resonator_;
  // It starts afresh at each new rate.
  float resonator_sample_rate_;
  ResonatorControls resonator_controls_;
  bool resonator_controls_set_;
  bool resonator_chord_;
  // The grains' level, which a grain's strike in a chord follows.
  float grain_level_;
  bool seed_gate_;
  // SEED's pulse, low-passed as Rings' strum, while it still sounds.
  stmlib::Svf strike_filter_;
  bool strike_sounding_;
  
  DISALLOW_COPY_AND_ASSIGN(Processor);
};

}  // namespace beads

#endif  // BEADS_DSP_PROCESSOR_H_
