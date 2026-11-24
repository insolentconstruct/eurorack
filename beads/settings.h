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
  bool SameCalibration(const PersistentData& other) const {
    return !memcmp(adc_calibration, other.adc_calibration,
        sizeof(adc_calibration));
  }

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
  uint8_t padding[14];
  enum { tag = 0x54415453 };  // STAT
};

// Drizzlet's options, in the calibration mirror sector: one byte per saved
// feature, at the offsets features.json gives for this version.
struct OptionsRecord {
  uint8_t version;
  uint8_t cv_target;  // 0 none, 1 width, 2 diffusion.
  uint8_t reserved[2];
  uint32_t profile_identity;
  uint8_t values[48];
  enum { tag = 0x4C5A5244 };  // DRZL
};

struct StateLayoutAssertions {
  STATIC_ASSERT(sizeof(State) == 24, state_size);
  STATIC_ASSERT(
      offsetof(State, monophonic_mode) == 9, state_monophonic_mode_offset);
  STATIC_ASSERT(offsetof(State, padding) == 10, state_padding_offset);
  STATIC_ASSERT(sizeof(OptionsRecord) == 56, record_size);
  STATIC_ASSERT(offsetof(OptionsRecord, values) == 8, record_values_offset);
};

const uint8_t kOptionsRecordVersion = 1;
// Payload offset of each saved feature; 0 for one that isn't saved.
extern const uint8_t kRecordOffsets[FEATURE_LAST];

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

  // False when the requested calibration rewrite did not complete.
  bool SavePersistentData();
  bool ReadPersistentData(PersistentData* data);
  void SaveState();

  // Saves only the options, if they changed.
  void SaveOptions();

  inline OutputLimiterMode output_limiter_mode() const {
    return OutputLimiterMode(values_[FEATURE_LIMITER]);
  }

  inline void set_output_limiter_mode(OutputLimiterMode mode) {
    SetFeatureValue(FEATURE_LIMITER, mode);
  }

  inline bool self_play_on_request() const {
    return values_[FEATURE_SYNTH_START];
  }

  inline void set_self_play_on_request(bool on_request) {
    values_[FEATURE_SYNTH_START] = on_request;
  }

  inline ProcessorMode processor_mode() const {
    return ProcessorMode(values_[FEATURE_PROCESSOR]);
  }

  inline bool spectral() const {
    return processor_mode() == PROCESSOR_SPECTRAL;
  }

  inline void set_spectral(bool spectral) {
    values_[FEATURE_PROCESSOR] = spectral;
  }

  inline bool clouds_style() const { return values_[FEATURE_REVERB]; }

  inline void set_clouds_style(bool clouds) {
    values_[FEATURE_REVERB] = clouds;
  }

  inline PitchScale pitch_scale() const {
    return PitchScale(values_[FEATURE_PITCH]);
  }

  inline void set_pitch_scale(PitchScale scale) {
    SetFeatureValue(FEATURE_PITCH, scale);
  }

  inline StereoWidth stereo_width() const {
    return values_[FEATURE_WIDTH];
  }

  inline void set_stereo_width(StereoWidth width) {
    values_[FEATURE_WIDTH] = width;
    if (!width && width_assigned()) {
      set_width_assigned(false);
    }
  }

  inline DelayMode delay_mode() const {
    return DelayMode(values_[FEATURE_DELAY]);
  }

  inline void set_delay_mode(DelayMode mode) {
    SetFeatureValue(FEATURE_DELAY, mode);
  }

  // A SynthEngine value; EffectiveSynth gives the one that plays.
  inline uint8_t synth() const { return values_[FEATURE_SYNTH]; }

  inline void set_synth(uint8_t synth) {
    SetFeatureValue(FEATURE_SYNTH, synth);
  }

  // One, two or four voices.
  inline uint8_t rings_polyphony() const {
    return 1 << values_[FEATURE_RINGS_POLYPHONY];
  }

  inline RandomIntervals random_intervals() const {
    return RandomIntervals(values_[FEATURE_INTERVALS]);
  }

  inline void set_random_intervals(RandomIntervals intervals) {
    SetFeatureValue(FEATURE_INTERVALS, intervals);
  }

  inline bool width_assigned() const { return cv_target_ == 1; }

  inline void set_width_assigned(bool assigned) {
    cv_target_ = assigned && profile_.adjustable(FEATURE_WIDTH) &&
        stereo_width() ? 1 : 0;
    if (assigned) {
      state_.assignable_cv_destination = 0;
    }
  }

  inline AttackMode attack_mode() const {
    return AttackMode(values_[FEATURE_ATTACKS]);
  }

  inline void set_attack_mode(AttackMode mode) {
    SetFeatureValue(FEATURE_ATTACKS, mode);
  }

  inline bool evolving_feedback() const { return values_[FEATURE_FROZEN]; }

  inline void set_evolving_feedback(bool evolving) {
    values_[FEATURE_FROZEN] = evolving;
  }

  // Zero follows the reverb character: none with Stock, Beta 1's fixed
  // amount with Clouds-style. Other values are an amount set with [A]+REVERB.
  inline bool diffusion_set() const { return values_[FEATURE_DIFFUSION] != 0; }

  inline float diffusion() const {
    return diffusion_set()
        ? float(values_[FEATURE_DIFFUSION] - 1) / 254.0f
        : (clouds_style() ? kCloudsDiffusion : 0.0f);
  }

  inline void set_diffusion(float amount) {
    CONSTRAIN(amount, 0.0f, 1.0f);
    values_[FEATURE_DIFFUSION] = 1 + static_cast<uint8_t>(
        amount * 254.0f + 0.5f);
    if (values_[FEATURE_DIFFUSION] == 1) {
      set_diffusion_assigned(false);
    }
  }

  inline bool diffusion_assigned() const { return cv_target_ == 2; }

  // Only an amount set with [A]+REVERB can be modulated.
  inline bool diffusion_assignable() const {
    return values_[FEATURE_DIFFUSION] > 1;
  }

  inline void set_diffusion_assigned(bool assigned) {
    if (assigned && profile_.adjustable(FEATURE_DIFFUSION) &&
        diffusion_assignable()) {
      cv_target_ = 2;
      state_.assignable_cv_destination = 0;
    } else if (diffusion_assigned()) {
      cv_target_ = 0;
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

  inline uint8_t FeatureValue(ProfileFeature feature) const {
    return feature == FEATURE_MONOPHONIC
        ? state_.monophonic_mode : values_[feature];
  }

  void SetFeatureValue(ProfileFeature feature, uint8_t value);

  // Any LED feature on: Ui::Poll skips their code when none is.
  inline bool led_features() const { return led_features_; }

 private:
  void NormalizeOptions();
  // A record holding the profile's defaults; no value set.
  void DefaultRecord(OptionsRecord* record) const;
  void LoadOptions();
  void PackRecord();
  void PackState();
  void ApplyProfile();
  uint8_t DefaultGain() const;
  static bool CalibrationPlausible(
      const PersistentData& data,
      const PersistentData& factory);

  PersistentData persistent_data_;
  PersistentData factory_;
  State state_;
  // Read by the audio callback, next to state_ as its bytes were.
  uint8_t cv_target_;
  // Every feature's value in use but Monophonic's, which is in State.
  uint8_t values_[FEATURE_LAST];
  State stored_state_;
  OptionsRecord record_;
  OptionsRecord stored_record_;
  Profile profile_;
  bool profile_locked_;
  bool led_features_;
  uint8_t dormant_destination_;
  // Bank 2: the settings in sector 7, the calibration mirror and the
  // options in sector 6.
  ChunkStorage<15, 14, PersistentData, State, OptionsRecord> chunk_storage_;
  StorageRepair repair_;
  
  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_H_
