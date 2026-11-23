// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Deferred gesture actions.

#ifndef BEADS_GESTURE_CONTROLLER_H_
#define BEADS_GESTURE_CONTROLLER_H_

#include "stmlib/stmlib.h"

namespace beads {

class Ui;

// Ui keeps the state in place for its interrupt readers.
class GestureController {
 public:
  GestureController() { }
  ~GestureController() { }
  void Init() { }

  // Returns true when the action is handled.
  static bool DoAction(Ui* ui, uint8_t slot);

 private:
  static void StepPlaitsBank(Ui* ui);
  static void StepRingsPolyphony(Ui* ui);

  DISALLOW_COPY_AND_ASSIGN(GestureController);
};

}  // namespace beads

#endif  // BEADS_GESTURE_CONTROLLER_H_
