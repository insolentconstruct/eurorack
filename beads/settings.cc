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

bool Settings::Init(bool recovery_mode) {
  return Init(recovery_mode, kProfileBlock);
}

bool Settings::Init(bool recovery_mode,
    const volatile uint8_t* profile_block) {
  profile_locked_ = !profile_.Init(profile_block);
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
  state_.output_limiter_mode = 0;
  state_.self_play = 0;
  state_.pitch_scale = 0;
  state_.reverb_style = 0;
  state_.stereo_width = 0;
  state_.delay_mode = 0;
  state_.width_assigned = 0;
  dormant_destination_ = 0;
  state_.diffusion = 0;
  state_.frozen_feedback = 0;
  state_.attacks = 0;
  state_.engine = 0;
  state_.synth_engine = 0;
  state_.random_intervals = 0;
  fill(&state_.padding[0], &state_.padding[sizeof(state_.padding)], 0);
  fill(
      &persistent_data_.padding[0],
      &persistent_data_.padding[sizeof(persistent_data_.padding)],
      0);
  const PersistentData factory = persistent_data_;
  for (size_t i = 0; i < sizeof(packed_); ++i) {
    packed_[i] = 0x9f;
  }
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const ProfileEntry& e = profile_.entry(ProfileFeature(i));
    SetFeatureValue(ProfileFeature(i), e.state == PROFILE_OFF ? 0 : e.value);
  }
  stored_state_ = state_;
  if (!profile_locked_) {
    PackOptions();
  }
  const State defaults = stored_state_;
  repair_ = STORAGE_REPAIR_NONE;
  
  Flash::Init();  // Every write checks the supply from here on.
  StorageStatus status = chunk_storage_.Init(
      &persistent_data_, &stored_state_, profile_locked_);
  
  // Restore the module's calibration before writing; keep loaded states.
  // A worse calibration must never replace the mirror.
  const bool lost = status == STORAGE_CALIBRATION_LOST;
  const bool implausible = \
      (status == STORAGE_OK || status == STORAGE_RESTORED) && \
      !CalibrationPlausible(persistent_data_, factory);
  PersistentData mirror;
  if ((lost || implausible) && \
      chunk_storage_.ReadMirror(&mirror) && \
      CalibrationPlausible(mirror, factory)) {
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
    // Neither copy of the calibration is usable: play on the factory values
    // and default settings held in RAM, unfrozen since the stored flag can't
    // be trusted. Only SEED at power-up writes.
    chunk_storage_.Lock();
    stored_state_ = defaults;
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
  LoadOptions();
  state_.padding[0] = 0;
  CONSTRAIN(state_.media, 0, 3);
  CONSTRAIN(state_.assignable_cv_destination, 0, 3);
  
  NormalizeOptions();
  ApplyProfile();
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

void Settings::SavePersistentData() {
  fill(
      &persistent_data_.padding[0],
      &persistent_data_.padding[sizeof(persistent_data_.padding)],
      0);
  PackOptions();
  chunk_storage_.SavePersistentData();
}

void Settings::NormalizeOptions() {
  set_output_limiter_mode(output_limiter_mode());
  set_self_play_on_request(self_play_on_request());
  set_spectral(spectral());
  set_clouds_style(clouds_style());
  set_pitch_scale(pitch_scale());
  set_stereo_width(stereo_width());
  set_delay_mode(delay_mode());
  set_evolving_feedback(evolving_feedback());
  set_attack_mode(attack_mode());
  set_plaits_synth(plaits_synth());
  set_random_intervals(random_intervals());
  // One store, so the audio interrupt never sees the target cleared.
  state_.width_assigned = width_assigned() && stereo_width() &&
      profile_.adjustable(FEATURE_WIDTH) ? 1
      : (diffusion_assigned() && diffusion_assignable() &&
          profile_.adjustable(FEATURE_DIFFUSION) ? 2 : 0);
  if (width_assigned() || diffusion_assigned()) {
    state_.assignable_cv_destination = 0;
  }
}

void Settings::SaveState() {
  NormalizeOptions();
  ApplyProfile();
  PackOptions();
  chunk_storage_.SaveState();
}

namespace {
const uint8_t kFieldBytes[10] = { 10, 11, 12, 13, 15, 18, 19, 20, 21, 22 };
}  // namespace

uint8_t Settings::FeatureValue(ProfileFeature feature) const {
  switch (feature) {
    case FEATURE_PITCH:
      return pitch_scale();
    case FEATURE_REVERB:
      return clouds_style();
    case FEATURE_DELAY:
      return delay_mode();
    case FEATURE_PROCESSOR:
      return spectral();
    case FEATURE_LIMITER:
      return output_limiter_mode();
    case FEATURE_FROZEN:
      return evolving_feedback();
    case FEATURE_SYNTH:
      return plaits_synth();
    case FEATURE_ATTACKS:
      return attack_mode();
    case FEATURE_SYNTH_START:
      return self_play_on_request();
    case FEATURE_INTERVALS:
      return random_intervals();
    case FEATURE_WIDTH:
      return stereo_width();
    case FEATURE_DIFFUSION:
      return state_.diffusion;
    case FEATURE_MONOPHONIC:
      return state_.monophonic_mode;
    default:
      return 0;
  }
}

void Settings::SetFeatureValue(ProfileFeature feature, uint8_t value) {
  switch (feature) {
    case FEATURE_PITCH:
      set_pitch_scale(PitchScale(value));
      break;
    case FEATURE_REVERB:
      set_clouds_style(value);
      break;
    case FEATURE_DELAY:
      set_delay_mode(DelayMode(value));
      break;
    case FEATURE_PROCESSOR:
      set_spectral(value);
      break;
    case FEATURE_LIMITER:
      set_output_limiter_mode(OutputLimiterMode(value));
      break;
    case FEATURE_FROZEN:
      set_evolving_feedback(value);
      break;
    case FEATURE_SYNTH:
      set_plaits_synth(value);
      break;
    case FEATURE_ATTACKS:
      set_attack_mode(AttackMode(value));
      break;
    case FEATURE_SYNTH_START:
      set_self_play_on_request(value);
      break;
    case FEATURE_INTERVALS:
      set_random_intervals(value);
      break;
    case FEATURE_WIDTH:
      state_.stereo_width = value;
      break;
    case FEATURE_DIFFUSION:
      state_.diffusion = value;
      break;
    case FEATURE_MONOPHONIC:
      state_.monophonic_mode = value;
      break;
    default:
      break;
  }
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

void Settings::LoadOptions() {
  dormant_destination_ = stored_state_.width_assigned;
  const uint8_t marker = stored_state_.padding[0];
  if (marker != kSettingsFormat) {
    dormant_destination_ = 0;
    state_.width_assigned = 0;
    // Full-byte dormant options also belong to the new profile.
    stored_state_.stereo_width = profile_.entry(FEATURE_WIDTH).value;
    stored_state_.diffusion = profile_.entry(FEATURE_DIFFUSION).value;
  }
  if (marker == kSettingsFormat) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&stored_state_);
    for (int i = 0; i < 10; ++i) {
      packed_[i] = bytes[kFieldBytes[i]];
      // Keep the downgrade invariant even after a damaged record.
      if ((packed_[i] >> 4) > 9) {
        packed_[i] = (packed_[i] & 15) | 0x90;
      }
    }
  }
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const ProfileFeature feature = ProfileFeature(i);
    const uint8_t field = kFeatureDefinitions[i].field;
    uint8_t value = profile_.entry(feature).value;
    // Monophonic mode belongs to stock State; all other options start fresh.
    if (!marker && feature == FEATURE_MONOPHONIC) {
      value = state_.monophonic_mode;
    } else if (marker == kSettingsFormat) {
      if (field != kNoField) {
        const uint8_t saved = (packed_[field / 2] >> ((field & 1) * 4)) & 15;
        if (saved < kFeatureDefinitions[i].values) {
          value = saved;
        }
      } else if (feature != FEATURE_TRIM) {
        value = FeatureValue(feature);
      }
    }
    SetFeatureValue(feature, value);
  }
}

void Settings::PackOptions() {
  const uint8_t mono = stored_state_.monophonic_mode;
  const uint8_t width = stored_state_.stereo_width;
  const uint8_t diffusion = stored_state_.diffusion;
  stored_state_ = state_;
  if (!profile_.adjustable(FEATURE_MONOPHONIC)) {
    stored_state_.monophonic_mode = mono;
  }
  if (!profile_.adjustable(FEATURE_WIDTH)) {
    stored_state_.stereo_width = width;
  }
  if (!profile_.adjustable(FEATURE_DIFFUSION)) {
    stored_state_.diffusion = diffusion;
  }
  for (int i = 1; i < FEATURE_LAST; ++i) {
    const uint8_t field = kFeatureDefinitions[i].field;
    if (field != kNoField && profile_.adjustable(ProfileFeature(i))) {
      const uint8_t shift = (field & 1) * 4;
      packed_[field / 2] = (packed_[field / 2] & ~(15 << shift)) |
          (FeatureValue(ProfileFeature(i)) << shift);
    }
  }
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&stored_state_);
  for (int i = 0; i < 10; ++i) {
    bytes[kFieldBytes[i]] = packed_[i];
  }
  // Any new CV assignment replaces the dormant one.
  if (state_.width_assigned || state_.assignable_cv_destination) {
    dormant_destination_ = 0;
  }
  if ((dormant_destination_ == 1 && !profile_.adjustable(FEATURE_WIDTH)) ||
      (dormant_destination_ == 2 && !profile_.adjustable(FEATURE_DIFFUSION))) {
    stored_state_.width_assigned = dormant_destination_;
  }
  stored_state_.padding[0] = kSettingsFormat;
}

}  // namespace beads
