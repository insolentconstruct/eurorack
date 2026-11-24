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

// Bound construction waits for main; audio keeps the previously published pair.
#define main FirmwareOriginalMain
#include "beads/test/firmware_test.cc"
#undef main

void TestDeferredResonatorBound() {
  test_checker.Start("TestDeferredResonatorBound");
  Rig r(false, false);
  r.Settle();
  settings.SetFeatureValue(FEATURE_RESONATOR, RESONATOR_FEEDBACK);
  r.Tick(2, true);
  const uint8_t first = processor.resonator_bound_;
  const ResonatorControls previous = processor.resonator_controls_;
  settings.SetFeatureValue(FEATURE_RESONATOR_MATERIAL, 4);
  SysTick_Handler();
  CHECK(processor.resonator_bound_ == first);
  CHECK(processor.resonator_controls_.modes == previous.modes);
  CHECK(processor.resonator_controls_.structure == previous.structure);
  r.Block();
  MainLoopStep();
  CHECK(processor.resonator_bound_ != first);
  const ResonatorControls& controls = processor.resonator_controls_;
  const ResonatorBound& bound =
      processor.resonator_bounds_[processor.resonator_bound_];
  CHECK(controls.modes == kResonatorMaterials[4].modes);
  CHECK(bound.modes == controls.modes);
  CHECK(bound.structure == controls.structure);
  CHECK(bound.brightness == controls.brightness);
  CHECK(bound.position == controls.position);
  settings.SetFeatureValue(FEATURE_RESONATOR_RING, 3);
  SysTick_Handler();
  MainLoopStep();
  CHECK(processor.resonator_bound_ != first);
  CHECK(processor.resonator_controls_.damping == kResonatorRingTimes[3]);
}

int main() {
  setbuf(stdout, NULL);
  void* flash = mmap(reinterpret_cast<void*>(0x08100000), 0x100000,
      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  if (flash == MAP_FAILED) {
    return 2;
  }
  TestDeferredResonatorBound();
  return test_checker.failures() ? 1 : 0;
}
