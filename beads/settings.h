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

#include "beads/drivers/cv_adc.h"
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
  uint8_t padding[14];
  enum { tag = 0x54415453 };  // STAT
};

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

  void SavePersistentData();
  void SaveState();
  
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
  inline bool write_locked() const { return chunk_storage_.locked(); }

  inline StorageRepair repair() const { return repair_; }

 private:
  static bool CalibrationPlausible(
      const PersistentData& data,
      const PersistentData& factory);

  PersistentData persistent_data_;
  State state_;
  // Bank 2: the settings in sector 7, the calibration mirror in sector 6.
  ChunkStorage<15, 14, PersistentData, State> chunk_storage_;
  StorageRepair repair_;
  
  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace beads

#endif  // BEADS_SETTINGS_H_