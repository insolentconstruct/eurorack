// Copyright 2019 Emilie Gillet.
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
// Settings storage.

#ifndef BEADS_SETTINGS_H_
#define BEADS_SETTINGS_H_

#include <stddef.h>

#include "stmlib/stmlib.h"

#include "beads/drivers/cv_adc.h"
#include "beads/dsp/attack_detector.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/pitch_quantizer.h"
#include "beads/dsp/playback_options.h"
#include "beads/profile.h"
#include "beads/settings_storage.h"

namespace beads {

struct AdcCalibrationData {
  float scale;
  float offset;
};

struct PersistentData {
  AdcCalibrationData adc_calibration[CV_ADC_CHANNEL_LAST];
  uint8_t padding[72];
  enum { tag = 0x494C4143 };  // CALI
};

struct State {
  uint8_t media;
  uint8_t assignable_cv_destination;
  uint8_t assignable_cv_amount[3];
  uint8_t gain;  // 0 for auto-gain
  uint8_t gate_output_mode;
  uint8_t frozen;
  uint8_t disable_autosave;
  uint8_t monophonic_mode;
  uint8_t output_limiter_mode;
  uint8_t self_play;
  uint8_t pitch_scale;
  uint8_t reverb_style;
  uint8_t stereo_width;
  uint8_t delay_mode;
  uint8_t width_assigned;
  uint8_t diffusion;
  uint8_t frozen_feedback;
  uint8_t attacks;
  uint8_t engine;
  uint8_t synth_engine;
  uint8_t random_intervals;
  uint8_t padding[1];
  enum { tag = 0x54415453 };  // STAT
};

struct StateLayoutAssertions {
  STATIC_ASSERT(sizeof(State) == 24, state_size);
  STATIC_ASSERT(offsetof(State, media) == 0, state_media_offset);
  STATIC_ASSERT(
      offsetof(State, assignable_cv_destination) == 1,
      state_assignable_cv_destination_offset);
  STATIC_ASSERT(
      offsetof(State, assignable_cv_amount) == 2,
      state_assignable_cv_amount_offset);
  STATIC_ASSERT(offsetof(State, gain) == 5, state_gain_offset);
  STATIC_ASSERT(
      offsetof(State, gate_output_mode) == 6, state_gate_output_mode_offset);
  STATIC_ASSERT(offsetof(State, frozen) == 7, state_frozen_offset);
  STATIC_ASSERT(
      offsetof(State, disable_autosave) == 8, state_disable_autosave_offset);
  STATIC_ASSERT(
      offsetof(State, monophonic_mode) == 9, state_monophonic_mode_offset);
  STATIC_ASSERT(
      offsetof(State, output_limiter_mode) == 10,
      state_output_limiter_mode_offset);
  STATIC_ASSERT(offsetof(State, self_play) == 11, state_self_play_offset);
  STATIC_ASSERT(offsetof(State, pitch_scale) == 12, state_pitch_scale_offset);
  STATIC_ASSERT(offsetof(State, reverb_style) == 13, state_reverb_style_offset);
  STATIC_ASSERT(offsetof(State, stereo_width) == 14, state_stereo_width_offset);
  STATIC_ASSERT(offsetof(State, delay_mode) == 15, state_delay_mode_offset);
  STATIC_ASSERT(
      offsetof(State, width_assigned) == 16, state_width_assigned_offset);
  STATIC_ASSERT(offsetof(State, diffusion) == 17, state_diffusion_offset);
  STATIC_ASSERT(
      offsetof(State, frozen_feedback) == 18, state_frozen_feedback_offset);
  STATIC_ASSERT(offsetof(State, attacks) == 19, state_attacks_offset);
  STATIC_ASSERT(offsetof(State, engine) == 20, state_engine_offset);
  STATIC_ASSERT(
      offsetof(State, synth_engine) == 21, state_synth_engine_offset);
  STATIC_ASSERT(
      offsetof(State, random_intervals) == 22, state_random_intervals_offset);
  STATIC_ASSERT(offsetof(State, padding) == 23, state_padding_offset);
};

const uint8_t kOptionTag = 0xa0;
// Option nibbles packed two to a byte, in the free bytes of State.
const int kNumPackedBytes = 10;

// SEED held at power-up on damaged storage.
enum StorageRepair {
  STORAGE_REPAIR_NONE,
  STORAGE_REPAIR_CALIBRATION_RESET,
  STORAGE_REPAIR_FAILED
};

class Settings {
 public:
  Settings() { }
  ~Settings() { }
  
  // False on a new module's blank storage: the factory test runs.
  bool Init(bool recovery_mode);
  bool Init(bool recovery_mode, const volatile uint8_t* profile_block);

  inline const Profile& profile() const { return profile_; }
  inline bool profile_locked() const { return profile_locked_; }

  void SavePersistentData();
  void SaveState();

  // Unknown values, including stock firmware's zero, read as Stock.
  inline OutputLimiterMode output_limiter_mode() const {
    const uint8_t value = state_.output_limiter_mode;
    return value == (kOptionTag + 1) ? OUTPUT_LIMITER_FAST
        : value == (kOptionTag + 2) ? OUTPUT_LIMITER_OFF : OUTPUT_LIMITER_STOCK;
  }

  inline void set_output_limiter_mode(OutputLimiterMode mode) {
    state_.output_limiter_mode = mode == OUTPUT_LIMITER_FAST ? (kOptionTag + 1)
        : mode == OUTPUT_LIMITER_OFF ? (kOptionTag + 2) : 0;
  }
  
  inline bool self_play_on_request() const {
    return state_.self_play == (kOptionTag + 1);
  }

  inline void set_self_play_on_request(bool on_request) {
    state_.self_play = on_request ? (kOptionTag + 1) : 0;
  }

  inline bool spectral() const {
    return state_.engine == (kOptionTag + 1);
  }

  inline void set_spectral(bool spectral) {
    state_.engine = spectral ? (kOptionTag + 1) : 0;
  }

  inline bool clouds_style() const {
    return state_.reverb_style == (kOptionTag + 2);
  }

  inline void set_clouds_style(bool clouds) {
    state_.reverb_style = clouds ? (kOptionTag + 2) : 0;
  }

  inline PitchScale pitch_scale() const {
    const uint8_t value = state_.pitch_scale;
    return value >= (kOptionTag + 1) && value <= (kOptionTag + kMaxPitchScaleId)
        && profile_.HasScale(value - kOptionTag)
        ? PitchScale(value - kOptionTag) : PITCH_SCALE_OFF;
  }

  inline void set_pitch_scale(PitchScale scale) {
    state_.pitch_scale = scale > PITCH_SCALE_OFF && scale <= kMaxPitchScaleId &&
        profile_.HasScale(scale)
        ? kOptionTag + scale : 0;
  }

  inline StereoWidth stereo_width() const {
    return state_.stereo_width;
  }

  inline void set_stereo_width(StereoWidth width) {
    state_.stereo_width = width;
    if (!width && width_assigned()) {
      set_width_assigned(false);
    }
  }

  inline DelayMode delay_mode() const {
    const uint8_t value = state_.delay_mode;
    return value == (kOptionTag + 1) ? DELAY_MODE_NEVER
        : value == (kOptionTag + 2) ? DELAY_MODE_WIDE_DETENT : DELAY_MODE_AUTO;
  }

  inline void set_delay_mode(DelayMode mode) {
    state_.delay_mode = mode == DELAY_MODE_NEVER ? (kOptionTag + 1)
        : mode == DELAY_MODE_WIDE_DETENT ? (kOptionTag + 2) : 0;
  }

  inline bool plaits_synth() const {
    return state_.synth_engine == (kOptionTag + 1);
  }

  inline void set_plaits_synth(bool plaits) {
    state_.synth_engine = plaits ? (kOptionTag + 1) : 0;
  }

  inline bool random_intervals() const {
    return state_.random_intervals == (kOptionTag + 1);
  }

  inline void set_random_intervals(bool intervals) {
    state_.random_intervals = intervals ? (kOptionTag + 1) : 0;
  }

  inline bool width_assigned() const { return state_.width_assigned == 1; }

  inline void set_width_assigned(bool assigned) {
    state_.width_assigned = assigned && profile_.adjustable(FEATURE_WIDTH) &&
        stereo_width() ? 1 : 0;
    if (assigned) {
      state_.assignable_cv_destination = 0;
    }
  }

  inline AttackMode attack_mode() const {
    const uint8_t value = state_.attacks;
    return value == (kOptionTag + 1) ? ATTACK_MODE_SNAP
        : value == (kOptionTag + 2) ? ATTACK_MODE_SNAP_AND_TRIGGER
        : ATTACK_MODE_OFF;
  }

  inline void set_attack_mode(AttackMode mode) {
    state_.attacks = mode == ATTACK_MODE_SNAP ? (kOptionTag + 1)
        : mode == ATTACK_MODE_SNAP_AND_TRIGGER ? (kOptionTag + 2) : 0;
  }

  inline bool evolving_feedback() const {
    return state_.frozen_feedback == (kOptionTag + 1);
  }

  inline void set_evolving_feedback(bool evolving) {
    state_.frozen_feedback = evolving ? (kOptionTag + 1) : 0;
  }

  // Zero follows the reverb character: none with Stock, Beta 1's fixed
  // amount with Clouds-style. Other values are an amount set with [A]+REVERB.
  inline bool diffusion_set() const { return state_.diffusion != 0; }

  inline float diffusion() const {
    return diffusion_set()
        ? float(state_.diffusion - 1) / 254.0f
        : (clouds_style() ? kCloudsDiffusion : 0.0f);
  }

  inline void set_diffusion(float amount) {
    CONSTRAIN(amount, 0.0f, 1.0f);
    state_.diffusion = 1 + static_cast<uint8_t>(amount * 254.0f + 0.5f);
    if (state_.diffusion == 1) {
      set_diffusion_assigned(false);
    }
  }

  inline bool diffusion_assigned() const {
    return state_.width_assigned == 2;
  }

  // Only an amount set with [A]+REVERB can be modulated.
  inline bool diffusion_assignable() const { return state_.diffusion > 1; }

  inline void set_diffusion_assigned(bool assigned) {
    if (assigned && profile_.adjustable(FEATURE_DIFFUSION) &&
        diffusion_assignable()) {
      state_.width_assigned = 2;
      state_.assignable_cv_destination = 0;
    } else if (diffusion_assigned()) {
      state_.width_assigned = 0;
    }
  }

  inline State* mutable_state() {
    return &state_;
  }
  
  inline const State& state() const {
    return state_;
  }
  
  inline PersistentData* mutable_persistent_data() {
    return &persistent_data_;
  }

  inline const PersistentData& persistent_data() const {
    return persistent_data_;
  }

  // Damaged storage found at power-up: settings change in RAM only.
  inline bool storage_damaged() const {
    return chunk_storage_.storage_damaged();
  }

  inline bool write_locked() const { return chunk_storage_.locked(); }

  inline StorageRepair repair() const { return repair_; }

  uint8_t FeatureValue(ProfileFeature feature) const;
  void SetFeatureValue(ProfileFeature feature, uint8_t value);

  // Any LED feature on: Ui::Poll skips their code when none is.
  inline bool led_features() const { return led_features_; }

 private:
  void NormalizeOptions();
  void LoadOptions();
  void PackOptions();
  void ApplyProfile();
  static bool CalibrationPlausible(
      const PersistentData& data,
      const PersistentData& factory);

  PersistentData persistent_data_;
  State state_;
  State stored_state_;
  Profile profile_;
  bool profile_locked_;
  uint8_t packed_[kNumPackedBytes];
  uint8_t profile_values_[FEATURE_LAST];
  bool led_features_;
  uint8_t dormant_destination_;
  // Bank 2: the settings in sector 7, the calibration mirror in sector 6.
  ChunkStorage<15, 14, PersistentData, State> chunk_storage_;
  StorageRepair repair_;
  
  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_H_
