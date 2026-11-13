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
// CPU ceiling hold: commands and the L0 store classification.

#include "beads/cpu_ceiling.h"
#include "beads/test/test_checker.h"

using namespace beads::cpu_ceiling;

const uint32_t kSequence = 1 << 24;

void TestCommands() {
  CHECK(!Valid(0));
  CHECK(!Valid(kMaxTarget));
  CHECK(Valid(kSequence));
  CHECK(Valid(kSequence | kMaxTarget));
  CHECK(!Valid(kSequence | (kMaxTarget + 1)));
  CHECK(!Valid(kSequence | (1 << 17)));
  CHECK(!Valid(kSequence | (1 << 20)));
  CHECK(Valid(kSequence | kObserve));
  CHECK(Valid((255u << 24) | kSine | kSkipDsp | 110000));
  CHECK(!Holds(kSequence));
  CHECK(Holds(kSequence | kSine));
  CHECK(Holds(kSequence | kSkipDsp));
  CHECK(Holds(kSequence | 1));
  CHECK(Holds(kSequence | kObserve));
}

// 12 frames per block: a 48-word ring, halves at words 0 and 24. NDTR counts
// the words left in the lap; a reload to 48 means the lap's last word went.
void TestRingPosition() {
  const uint32_t words = 48;
  CHECK(Sent(48, 0, words) == 0);
  CHECK(Reached(48, 0, words));
  CHECK(Reached(25, 0, words));
  CHECK(!Reached(24, 0, words));
  CHECK(!Reached(1, 0, words));
  CHECK(Sent(24, 24, words) == 0);
  CHECK(Reached(24, 24, words));
  CHECK(Reached(1, 24, words));
  CHECK(!Reached(48, 24, words));
  CHECK(!Reached(25, 24, words));
  // 24 kHz: 6 frames, a 24-word ring.
  CHECK(Reached(24, 0, 24));
  CHECK(!Reached(12, 0, 24));
  CHECK(Reached(12, 12, 24));
}

void TestClassify() {
  const uint32_t words = 48;
  // Half 0 is written while the stream finishes the other half.
  CHECK(Classify(3, 2, 0, words) == STORE_ON_TIME);
  CHECK(Classify(1, 48, 0, words) == STORE_AMBIGUOUS);
  CHECK(Classify(48, 47, 0, words) == STORE_LATE);
  CHECK(Classify(46, 46, 0, words) == STORE_LATE);
  CHECK(Classify(26, 25, 24, words) == STORE_ON_TIME);
  CHECK(Classify(25, 24, 24, words) == STORE_AMBIGUOUS);
  CHECK(Classify(24, 23, 24, words) == STORE_LATE);
}

int main() {
  TestCommands();
  TestRingPosition();
  TestClassify();
  return test_checker.failures() ? 1 : 0;
}
