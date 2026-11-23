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
// Applies recovery to a complete simulated bank 2 image.

#include "beads/test/recovery_compat/runtime.h"
#include "beads/recovery/settings_recovery.h"

int main(int argc, char** argv) {
  Require(argc == 4, "usage: recovery_image INPUT OUTPUT KEPT|RESET");
  MapBank();
  Image(argv[1], false);
  beads::SettingsRecovery recovery;
  const beads::RecoveryResult result = recovery.Run();
  const bool kept = !strcmp(argv[3], "KEPT");
  Require(kept || !strcmp(argv[3], "RESET"), "unknown expected result");
  Require(result == (kept ? beads::RECOVERY_CALIBRATION_KEPT
      : beads::RECOVERY_CALIBRATION_RESET), "recovery result differs");
  Image(argv[2], true);
  return 0;
}
