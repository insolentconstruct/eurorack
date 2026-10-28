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
  const State defaults = state_;
  repair_ = STORAGE_REPAIR_NONE;
  
  const StorageStatus status = chunk_storage_.Init(&persistent_data_, &state_);
  
  if (chunk_storage_.locked()) {
    // Damaged: play on safe settings held in RAM, unfrozen since the stored
    // flag can't be trusted. Only SEED at power-up writes. The calibration
    // is the stored one if it read clean and plausible, else the mirror's,
    // else the factory values.
    state_ = defaults;
    bool keep = status == STORAGE_DAMAGED_STATE && \
        CalibrationPlausible(factory);
    if (!keep) {
      keep = chunk_storage_.ReadMirror(&persistent_data_) && \
          CalibrationPlausible(factory);
    }
    if (!keep) {
      persistent_data_ = factory;
    }
    if (recovery_mode) {
      repair_ = !chunk_storage_.Repair()
          ? STORAGE_REPAIR_FAILED
          : (keep
              ? STORAGE_REPAIR_CALIBRATION_KEPT
              : STORAGE_REPAIR_CALIBRATION_RESET);
    }
  } else if (status == STORAGE_OK) {
    if (recovery_mode) {
      state_ = defaults;
      chunk_storage_.Recover(&persistent_data_, &state_);
    }
    // Sanitize settings read from flash.
    CONSTRAIN(state_.media, 0, 3);
    CONSTRAIN(state_.assignable_cv_destination, 0, 3);
  }
  
  NormalizeOptions();
  // A new module runs the factory test, unless SEED is held.
  return status != STORAGE_NEW || recovery_mode;
}

bool Settings::CalibrationPlausible(const PersistentData& factory) const {
  // Within 25% of the factory values, as FIX_OUTLIER checks.
  for (int i = 0; i < CV_ADC_CHANNEL_LAST; ++i) {
    const AdcCalibrationData& c = persistent_data_.adc_calibration[i];
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
  state_.width_assigned = width_assigned() && stereo_width() ? 1
      : (diffusion_assigned() && diffusion_assignable() ? 2 : 0);
  if (state_.width_assigned) state_.assignable_cv_destination = 0;
}

void Settings::SaveState() {
  NormalizeOptions();
  fill(&state_.padding[0], &state_.padding[sizeof(state_.padding)], 0);
  chunk_storage_.SaveState();
}

}  // namespace beads
