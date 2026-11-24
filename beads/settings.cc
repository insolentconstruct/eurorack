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

#include "beads/settings.h"

#include <algorithm>
#include <cmath>

#include "stmlib/system/storage.h"

namespace beads {

using namespace std;

#define FIX_OUTLIER(destination, expected_value) if (fabsf(destination / expected_value - 1.0f) > 0.25f) { destination = expected_value; }
#define FIX_OUTLIER_ABSOLUTE(destination, expected_value) if (fabsf(destination - expected_value) > 0.1f) { destination = expected_value; }

const uint8_t kRecordOffsets[FEATURE_LAST] = {
  0,  // None.
  8,  // Pitch.
  9,  // Reverb.
  10,  // Delay.
  11,  // Processor.
  12,  // Limiter.
  13,  // Frozen.
  14,  // Synth.
  15,  // Attacks.
  16,  // Synth start.
  17,  // Intervals.
  18,  // Width.
  19,  // Diffusion.
  0,  // Monophonic.
  0,  // Trim.
  0,  // Version.
  0,  // Attenuation.
  0,  // Delay cue.
  20,  // Brightness.
  0,  // Live assign.
  0,  // Save freeze.
  21,  // Seed latch.
  0,  // Pitch notch.
  22,  // Time scan.
  0,  // Synth tail.
  0,  // Idle timeout.
  0,  // Seed latch fade.
  0,  // Manual gain.
  0,  // Attack sensitivity.
  0,  // Seed start.
  0,  // Pitch law.
  23,  // Plaits bank.
  0,  // Mix law.
  0,  // Reverb input.
  0,  // Grain stealing.
  0,  // Unity feedback.
  0,  // Delay crossfade.
  0,  // Page preview.
  0,  // First page.
  0,  // Start quality.
  0,  // Gain knob.
  24,  // Resonator.
  27,  // Rings polyphony.
  25,  // Resonator material.
  26,  // Resonator ring.
  0,  // Resonator structure.
  0,  // Resonator brightness.
  0,  // Resonator damping.
  0,  // Resonator position.
  0,  // Resonator amount.
  0,  // Synth level.
  0,  // Output division.
  28,  // Gain ceiling.
  0,  // Manual delay.
  0,  // Reverb decay.
};

bool Settings::Init(bool recovery_mode) {
  return Init(recovery_mode, kProfileBlock);
}

bool Settings::Init(bool recovery_mode,
    const volatile uint8_t* profile_block) {
  profile_locked_ = !profile_.Init(profile_block);
  ConfigurePitchScales(&profile_.scale(0), profile_.scale_count(),
      profile_.custom_bank());
  AdcCalibrationData calibration;
  calibration.scale = -2.0f;
  calibration.offset = 1.0f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_ASSIGN] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_DENSITY] = calibration;

  calibration.scale = -3.2f;
  calibration.offset = 1.6f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_TIME] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_SIZE] = calibration;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_SHAPE] = calibration;

  calibration.scale = -120.0f;
  calibration.offset = 60.0f;
  persistent_data_.adc_calibration[CV_ADC_CHANNEL_PITCH] = calibration;
  
  state_.media = 3;
  state_.assignable_cv_destination = 0;
  state_.assignable_cv_amount[0] = 255;
  state_.assignable_cv_amount[1] = 0;
  state_.assignable_cv_amount[2] = 128;
  state_.gain = 0;
  state_.gate_output_mode = 0;
  state_.frozen = 0;
  state_.disable_autosave = 0;
  state_.monophonic_mode = 0;
  fill(&state_.padding[0], &state_.padding[sizeof(state_.padding)], 0);
  fill(
      &persistent_data_.padding[0],
      &persistent_data_.padding[sizeof(persistent_data_.padding)],
      0);
  factory_ = persistent_data_;
  const PersistentData& factory = factory_;
  fill(&values_[0], &values_[FEATURE_LAST], 0);
  cv_target_ = 0;
  dormant_destination_ = 0;
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const ProfileEntry& e = profile_.entry(ProfileFeature(i));
    SetFeatureValue(ProfileFeature(i), e.state == PROFILE_OFF ? 0 : e.value);
  }
  // 1 Bright digital to 4 Scorched cassette; 0 keeps the saved quality.
  const uint8_t start_quality = values_[FEATURE_START_QUALITY];
  if (start_quality) {
    state_.media = 4 - start_quality;
  }
  stored_state_ = state_;
  const State defaults = stored_state_;
  OptionsRecord default_record;
  DefaultRecord(&default_record);
  record_ = default_record;
  repair_ = STORAGE_REPAIR_NONE;
  
  Flash::Init();  // Every write checks the supply from here on.
  chunk_storage_.set_calibration_check(&CalibrationPlausible, &factory_);
  StorageStatus status = chunk_storage_.Init(
      &persistent_data_, &stored_state_, &record_, profile_locked_);
  // Another version's record, or none, gives the profile's defaults.
  const bool record_valid = chunk_storage_.record_found() && \
      record_.version == kOptionsRecordVersion;
  if (!record_valid) {
    record_ = default_record;
  }
  stored_record_ = record_;
  if (!chunk_storage_.record_found()) {
    stored_record_.version = 0;  // The first save writes one.
  }
  if (recovery_mode && !profile_locked_) {
    record_ = default_record;  // SEED at power-up resets the options.
  }
  
  // Restore the module's calibration before writing; keep loaded states.
  // A worse calibration must never replace the mirror, and a module's own
  // calibration never reads as exactly the factory values.
  const bool lost = status == STORAGE_CALIBRATION_LOST;
  const bool loaded = status == STORAGE_OK || status == STORAGE_RESTORED;
  const bool implausible = loaded && \
      !CalibrationPlausible(persistent_data_, factory);
  const bool factory_values = loaded && \
      !memcmp(persistent_data_.adc_calibration, factory.adc_calibration,
          sizeof(factory.adc_calibration));
  PersistentData mirror;
  if ((lost || implausible || factory_values) && \
      chunk_storage_.ReadMirror(&mirror) && \
      CalibrationPlausible(mirror, factory) && \
      (!factory_values || memcmp(mirror.adc_calibration,
          factory.adc_calibration, sizeof(mirror.adc_calibration)))) {
    persistent_data_ = mirror;
    if (recovery_mode && !profile_locked_) {
      stored_state_ = defaults;
    }
    status = STORAGE_RESTORED;
  } else if (lost) {
    status = STORAGE_DAMAGED;
  }
  if (status == STORAGE_RESTORED) {
    chunk_storage_.RestorePersistentData();
  }
  
  if (status == STORAGE_DAMAGED) {
    // No usable calibration: play on factory values and default settings,
    // unfrozen, in RAM only. Only SEED at power-up writes.
    chunk_storage_.Lock();
    stored_state_ = defaults;
    record_ = default_record;
    persistent_data_ = factory;
    if (recovery_mode) {
      repair_ = chunk_storage_.Repair()
          ? STORAGE_REPAIR_CALIBRATION_RESET
          : STORAGE_REPAIR_FAILED;
    }
  } else {
    if (recovery_mode && status == STORAGE_OK && !profile_locked_) {
      stored_state_ = defaults;
      chunk_storage_.Recover(&persistent_data_, &stored_state_);
    }
  }
  // Sanitize settings read from flash.
  state_ = stored_state_;
  fill(&state_.padding[0], &state_.padding[sizeof(state_.padding)], 0);
  LoadOptions();
  const bool new_profile = !record_valid || \
      record_.profile_identity != profile_.identity();
  CONSTRAIN(state_.media, 0, 3);
  CONSTRAIN(state_.assignable_cv_destination, 0, 3);

  NormalizeOptions();
  ApplyProfile();
  if (recovery_mode && !profile_locked_) {
    SaveOptions();
  }
  // A newly installed profile starts in its quality once; a frozen recording
  // returning from flash keeps the quality it was made in.
  const bool restores = state_.frozen && !state_.disable_autosave && \
      !spectral();
  if (new_profile && start_quality && !restores) {
    state_.media = 4 - start_quality;
  }
  if (!state_.gain) {
    state_.gain = DefaultGain();
  }
  // A new module runs the factory test, unless SEED is held.
  return status != STORAGE_NEW || recovery_mode;
}

/* static */
bool Settings::CalibrationPlausible(
    const PersistentData& data,
    const PersistentData& factory) {
  // Within 25% of the factory values, as FIX_OUTLIER checks.
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    const AdcCalibrationData& c = data.adc_calibration[i];
    const AdcCalibrationData& f = factory.adc_calibration[i];
    // Written so that NaN fails.
    if (!(fabsf(c.scale / f.scale - 1.0f) <= 0.25f) || \
        !(fabsf(c.offset / f.offset - 1.0f) <= 0.25f)) {
      return false;
    }
  }
  return true;
}

bool Settings::SavePersistentData() {
  fill(
      &persistent_data_.padding[0],
      &persistent_data_.padding[sizeof(persistent_data_.padding)],
      0);
  PackRecord();
  PackState();
  return chunk_storage_.SavePersistentData();
}

bool Settings::ReadPersistentData(PersistentData* data) {
  if (!chunk_storage_.ReadPersistentData(data)) {
    return false;
  }
  PersistentData mirror;
  if (!memcmp(data->adc_calibration, factory_.adc_calibration,
          sizeof(data->adc_calibration)) &&
      chunk_storage_.ReadMirror(&mirror) &&
      CalibrationPlausible(mirror, factory_) &&
      memcmp(mirror.adc_calibration, factory_.adc_calibration,
          sizeof(mirror.adc_calibration))) {
    *data = mirror;
  }
  return true;
}

void Settings::NormalizeOptions() {
  // One store, so the audio interrupt never sees the target cleared.
  cv_target_ = width_assigned() && stereo_width() &&
      profile_.adjustable(FEATURE_WIDTH) ? 1
      : (diffusion_assigned() && diffusion_assignable() &&
          profile_.adjustable(FEATURE_DIFFUSION) ? 2 : 0);
  if (cv_target_) {
    state_.assignable_cv_destination = 0;
  }
}

// The options first: a cut before the state leaves a pair stock reads.
void Settings::SaveState() {
  SaveOptions();
  PackState();
  chunk_storage_.SaveState();
}

void Settings::SaveOptions() {
  NormalizeOptions();
  ApplyProfile();
  PackRecord();
  if ((chunk_storage_.record_pending() || \
          memcmp(&record_, &stored_record_, sizeof(OptionsRecord))) && \
      chunk_storage_.SaveRecord()) {
    stored_record_ = record_;
  }
}

void Settings::SetFeatureValue(ProfileFeature feature, uint8_t value) {
  const FeatureDefinition& d = kFeatureDefinitions[feature];
  if (feature == FEATURE_MONOPHONIC) {
    state_.monophonic_mode = value;  // Stock's byte, as stock reads it.
    return;
  } else if (feature == FEATURE_PITCH) {
    value = value <= kMaxPitchScaleId && profile_.HasScale(value) ? value : 0;
  } else if (d.values && value >= d.values) {
    value = 0;
  }
  values_[feature] = value;
  led_features_ = values_[FEATURE_VERSION] ||
      values_[FEATURE_DELAY_CUE] ||
      values_[FEATURE_BRIGHTNESS] ||
      values_[FEATURE_LIVE_ASSIGN] ||
      values_[FEATURE_PITCH_NOTCH];
}

void Settings::ApplyProfile() {
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const ProfileFeature feature = ProfileFeature(i);
    const ProfileEntry& e = profile_.entry(feature);
    if (e.state != PROFILE_ADJUSTABLE) {
      const uint8_t value = e.state == PROFILE_FIXED ? e.value
          : feature == FEATURE_DIFFUSION ? 1 : 0;
      SetFeatureValue(feature, value);
    }
  }
}

void Settings::DefaultRecord(OptionsRecord* record) const {
  memset(record, 0xff, sizeof(OptionsRecord));
  record->version = kOptionsRecordVersion;
  record->cv_target = 0;
  record->profile_identity = profile_.identity();
  // Full-byte options belong to the profile until they are set.
  uint8_t* bytes = reinterpret_cast<uint8_t*>(record);
  for (int i = 1; i < FEATURE_LAST; ++i) {
    if (kRecordOffsets[i] && !kFeatureDefinitions[i].values) {
      bytes[kRecordOffsets[i]] = profile_.entry(ProfileFeature(i)).value;
    }
  }
}

void Settings::LoadOptions() {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record_);
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const ProfileFeature feature = ProfileFeature(i);
    const uint8_t offset = kRecordOffsets[i];
    uint8_t value = profile_.entry(feature).value;
    if (feature == FEATURE_MONOPHONIC) {
      value = state_.monophonic_mode;  // Stock State keeps it.
    } else if (offset && (!kFeatureDefinitions[i].values ||
#ifdef PLAITS_BLOBS
        // A saved bank past this profile's banks takes its start bank.
        bytes[offset] < profile_.Values(feature))) {
#else
        bytes[offset] < kFeatureDefinitions[i].values)) {
#endif  // PLAITS_BLOBS
      value = bytes[offset];  // A byte past the values is not set.
    }
    SetFeatureValue(feature, value);
  }
  cv_target_ = dormant_destination_ = record_.cv_target;
}

void Settings::PackRecord() {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&record_);
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const uint8_t offset = kRecordOffsets[i];
    if (offset && profile_.adjustable(ProfileFeature(i))) {
      bytes[offset] = FeatureValue(ProfileFeature(i));
    }
  }
  record_.version = kOptionsRecordVersion;
  record_.profile_identity = profile_.identity();
  // Any new CV assignment replaces the dormant one.
  if (cv_target_ || state_.assignable_cv_destination) {
    dormant_destination_ = 0;
  }
  record_.cv_target = \
      (dormant_destination_ == 1 && !profile_.adjustable(FEATURE_WIDTH)) ||
      (dormant_destination_ == 2 && !profile_.adjustable(FEATURE_DIFFUSION))
      ? dormant_destination_ : cv_target_;
}

// As stock writes it; a monophonic mode the profile fixes stays dormant.
void Settings::PackState() {
  const uint8_t mono = stored_state_.monophonic_mode;
  stored_state_ = state_;
  if (!profile_.adjustable(FEATURE_MONOPHONIC)) {
    stored_state_.monophonic_mode = mono;
  }
}

// Blank gain starts automatic, or at unity manual gain.
uint8_t Settings::DefaultGain() const {
  if (!values_[FEATURE_MANUAL_GAIN]) {
    return 0;
  }
  return values_[FEATURE_ATTENUATION] ? 128 : 1;
}

}  // namespace beads
