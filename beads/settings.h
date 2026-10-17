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

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "beads/drivers/cv_adc.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/playback_options.h"
#include "beads/dsp/pitch_quantizer.h"

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
  uint8_t padding[14];
  enum { tag = 0x54415453 };  // STAT
};

class Settings {
 public:
  Settings() { }
  ~Settings() { }
  
  bool Init(bool recovery_mode);

  void SavePersistentData();
  void SaveState();

  // Stored in the first padding byte. Unknown values, including the zero
  // left by stock firmware, read as Stock.
  inline OutputLimiterMode output_limiter_mode() const {
    const uint8_t value = state_.padding[0];
    return value == 0xa1 ? OUTPUT_LIMITER_FAST
        : value == 0xa2 ? OUTPUT_LIMITER_OFF : OUTPUT_LIMITER_STOCK;
  }

  inline void set_output_limiter_mode(OutputLimiterMode mode) {
    state_.padding[0] = mode == OUTPUT_LIMITER_FAST ? 0xa1
        : mode == OUTPUT_LIMITER_OFF ? 0xa2 : 0;
  }
  
  inline bool self_play_on_request() const {
    return state_.padding[1] == 0xa1;
  }

  inline void set_self_play_on_request(bool on_request) {
    state_.padding[1] = on_request ? 0xa1 : 0;
  }

  // Byte 12 belongs to pitch scale; 0xa1 in byte 13 is reserved.
  inline bool clouds_style() const {
    return state_.padding[3] == 0xa2;
  }

  inline void set_clouds_style(bool clouds) {
    state_.padding[3] = clouds ? 0xa2 : 0;
  }

  inline PitchScale pitch_scale() const {
    const uint8_t value = state_.padding[2];
    return value >= 0xa1 && value <= 0xa9
        ? PitchScale(value - 0xa0) : PITCH_SCALE_OFF;
  }

  inline void set_pitch_scale(PitchScale scale) {
    state_.padding[2] = scale > PITCH_SCALE_OFF && scale < PITCH_SCALE_LAST
        ? 0xa0 + scale : 0;
  }

  // Byte 14: zero is stock; 1..255 is enabled width.
  inline StereoWidth stereo_width() const {
    return state_.padding[4];
  }

  inline void set_stereo_width(StereoWidth width) {
    state_.padding[4] = width;
    if (!width) set_width_assigned(false);
  }

  // Byte 15: the automatic SIZE transition is the default.
  inline DelayMode delay_mode() const {
    const uint8_t value = state_.padding[5];
    return value == 0xa1 ? DELAY_MODE_NEVER
        : value == 0xa2 ? DELAY_MODE_WIDE_DETENT : DELAY_MODE_AUTO;
  }

  inline void set_delay_mode(DelayMode mode) {
    state_.padding[5] = mode == DELAY_MODE_NEVER ? 0xa1
        : mode == DELAY_MODE_WIDE_DETENT ? 0xa2 : 0;
  }

  // Byte 16 overlays WIDTH while FEEDBACK remains the saved destination.
  inline bool width_assigned() const { return state_.padding[6] == 0xa1; }

  inline void set_width_assigned(bool assigned) {
    state_.padding[6] = assigned && stereo_width() ? 0xa1 : 0;
    if (assigned) state_.assignable_cv_destination = 0;
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

 private:
  PersistentData persistent_data_;
  State state_;
  stmlib::ChunkStorage<15, PersistentData, State> chunk_storage_;
  
  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_H_