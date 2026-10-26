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
// Rebuilds the bank 2 storage (frozen buffer and settings) without trusting
// it: a flash word with an uncorrectable ECC error faults when read, so every
// read of bank 2 is made with bus faults ignored and the ECC flag checked.

#ifndef BEADS_RECOVERY_SETTINGS_RECOVERY_H_
#define BEADS_RECOVERY_SETTINGS_RECOVERY_H_

#include <stddef.h>

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "beads/settings.h"

namespace beads {

// Bank 2 sector numbers: the frozen buffer is in 0-6 (0x08100000-0x081dffff),
// the settings in 7 (0x081e0000).
const uint32_t kBank2Start = 0x08100000;
const uint32_t kBank2SectorSize = 0x20000;
const uint32_t kFirstBufferSector = 0;
const uint32_t kLastBufferSector = 6;
const uint32_t kSettingsSector = 7;

enum RecoveryResult {
  RECOVERY_RUNNING,
  RECOVERY_CALIBRATION_KEPT,
  RECOVERY_CALIBRATION_RESET,
  RECOVERY_FAILED
};

class SettingsRecovery {
 public:
  SettingsRecovery() { }
  ~SettingsRecovery() { }

  // Erases the frozen buffer, keeps the calibration if it reads back clean
  // and plausible, and rewrites the settings sector with stmlib's storage.
  RecoveryResult Run();

  // Erases one bank 2 sector, checks that it reads back blank, retries once.
  bool EraseSector(uint32_t sector);

  // Copies size bytes (a multiple of 4) from bank 2 with bus faults ignored.
  // Returns false if the read hit an uncorrectable ECC error.
  bool ReadMasked(uint32_t address, void* destination, size_t size);

  bool IsBlank(uint32_t address, size_t size);

  // True if the calibration chunk at the start of the settings sector is
  // ECC-clean and its tag, size and checksum match.
  bool ReadCalibration(PersistentData* data);

  // True if every scale and offset is within 25% of the default.
  bool IsPlausible(const PersistentData& data) const;

  // Reads the sector back, masked, then as the firmware loads it.
  bool Verify();

  // The values Settings::Init starts from.
  static void LoadDefaults(PersistentData* data, State* state);

  inline const PersistentData& persistent_data() const {
    return persistent_data_;
  }

  inline const State& state() const { return state_; }

 private:
  bool ReadChunk(size_t offset, void* data, uint32_t tag, size_t size);

  PersistentData persistent_data_;
  State state_;
  stmlib::ChunkStorage<15, PersistentData, State> chunk_storage_;

  DISALLOW_COPY_AND_ASSIGN(SettingsRecovery);
};

}  // namespace beads

#endif  // BEADS_RECOVERY_SETTINGS_RECOVERY_H_
