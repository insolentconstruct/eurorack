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

// A calibration write refusal must reach the caller.
#define main StorageOriginalMain
#include "beads/test/storage_test.cc"
#undef main

#define private public
#include "beads/calibrator.h"
#undef private

int main() {
  if (!CHECK(MapBank2())) {
    return 2;
  }
  TestLowSupply();
  test_checker.Start("ReviewCalibrationRefusal");
  const PersistentData calibration = Calibration(0.1f);
  const PersistentData changed = Calibration(-0.1f);
  Setup(calibration, 4);
  Settings s;
  CHECK(s.Init(false));
  memcpy(s.mutable_persistent_data()->adc_calibration,
      changed.adc_calibration, sizeof(changed.adc_calibration));
  TestFlash().supply_low = true;
  const bool saved = s.SavePersistentData();
  Settings loaded;
  CHECK(loaded.Init(false));
  const bool old_loaded = SameCalibration(loaded, calibration);
  printf("REPRO saved=%d old_calibration_after_reboot=%d\n", saved, old_loaded);
  CHECK(old_loaded);
  CHECK(!saved);
  TestFlash().supply_low = false;
  test_checker.Start("CalibrationPartialSave");
  for (int drop = 1; drop <= 12; ++drop) {
    Setup(calibration, 6);
    Settings partial;
    CHECK(partial.Init(false));
    memcpy(partial.mutable_persistent_data()->adc_calibration,
        changed.adc_calibration, sizeof(changed.adc_calibration));
    TestFlash().supply_budget = drop;
    Calibrator calibrator;
    calibrator.settings_ = &partial;
    memcpy(calibrator.calibration_data_, changed.adc_calibration,
        sizeof(changed.adc_calibration));
    CHECK(!calibrator.Save());
    PersistentData durable;
    CHECK(partial.ReadPersistentData(&durable));
    TestFlash().supply_low = false;
    TestFlash().supply_budget = 0;
    Settings rebooted;
    CHECK(rebooted.Init(false));
    CHECK(SameCalibration(rebooted, durable));
    CHECK(SameCalibration(rebooted, partial.persistent_data()));
  }
  test_checker.Start("CalibrationNoDurableCopy");
  Wipe();
  TestFlash().supply_low = true;
  Settings blank;
  blank.Init(false);
  const PersistentData previous = blank.persistent_data();
  Calibrator calibrator;
  calibrator.settings_ = &blank;
  memcpy(calibrator.calibration_data_, changed.adc_calibration,
      sizeof(changed.adc_calibration));
  CHECK(!calibrator.Save());
  CHECK(!memcmp(&previous, &blank.persistent_data(), sizeof(previous)));
  TestFlash().supply_low = false;
  test_checker.Start("CalibrationFactoryPrimary");
  Setup(calibration, 4);
  Settings factory;
  CHECK(factory.Init(false));
  PokeChunk(kSector, PersistentData::tag, &factory.factory_,
      sizeof(PersistentData));
  TestFlash().supply_low = true;
  Settings restored;
  CHECK(restored.Init(false));
  PersistentData selected;
  CHECK(restored.ReadPersistentData(&selected));
  CHECK(SameCalibration(restored, selected));
  CHECK(SameCalibration(restored, calibration));
  TestFlash().supply_low = false;
  return test_checker.failures() ? 1 : 0;
}
