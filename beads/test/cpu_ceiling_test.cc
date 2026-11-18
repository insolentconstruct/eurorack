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
// CPU ceiling hold: commands and the store-run classification.

#include "beads/cpu_ceiling.h"
#include "beads/test/test_checker.h"

using namespace beads::cpu_ceiling;

const uint32_t kSequence = 1 << 24;

void TestCommands() {
  CHECK(kVersion == 2);
  CHECK(sizeof(Hold) == 64);
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
  CHECK(Relative(48, 0, words) == 0);
  CHECK(Relative(46, 0, words) == 2);
  CHECK(Relative(24, 0, words) == 24);
  CHECK(Relative(24, 24, words) == 0);
  CHECK(Relative(48, 24, words) == 24);
  // Word 0: fetched once the stream is in the half.
  CHECK(Fetched(48, 0, words, 0));
  CHECK(Fetched(25, 0, words, 0));
  CHECK(!Fetched(24, 0, words, 0));
  CHECK(!Fetched(1, 0, words, 0));
  // Word 2 (the callback's first left word): two slots later.
  CHECK(!Fetched(48, 0, words, 2));
  CHECK(!Fetched(47, 0, words, 2));
  CHECK(Fetched(46, 0, words, 2));
  CHECK(Fetched(25, 0, words, 2));
  CHECK(!Fetched(24, 0, words, 2));
  CHECK(!Fetched(24, 24, words, 2));
  CHECK(Fetched(22, 24, words, 2));
  CHECK(!Fetched(48, 24, words, 2));
  // 32 and 24 kHz: 8 and 6 frames, 32- and 24-word rings.
  CHECK(Fetched(30, 0, 32, 2));
  CHECK(!Fetched(31, 0, 32, 2));
  CHECK(!Fetched(16, 0, 32, 2));
  CHECK(Fetched(10, 12, 24, 2));
  CHECK(!Fetched(11, 12, 24, 2));
  CHECK(!Fetched(24, 12, 24, 2));
}

void TestClassify() {
  const uint32_t words = 48;
  // The carried frame (word 0), stored before the callback.
  CHECK(Classify(3, 2, 0, words, 0) == STORE_ON_TIME);
  CHECK(Classify(1, 48, 0, words, 0) == STORE_AMBIGUOUS);
  CHECK(Classify(48, 47, 0, words, 0) == STORE_LATE);
  // The callback's run from word 2.
  CHECK(Classify(48, 48, 0, words, 2) == STORE_ON_TIME);
  CHECK(Classify(48, 47, 0, words, 2) == STORE_ON_TIME);
  CHECK(Classify(47, 46, 0, words, 2) == STORE_AMBIGUOUS);
  CHECK(Classify(46, 45, 0, words, 2) == STORE_LATE);
  CHECK(Classify(25, 24, 0, words, 2) == STORE_LATE);
  CHECK(Classify(26, 24, 24, words, 2) == STORE_ON_TIME);
  CHECK(Classify(23, 22, 24, words, 2) == STORE_AMBIGUOUS);
  CHECK(Classify(22, 21, 24, words, 2) == STORE_LATE);
  // Past the half: the stream is already in the other half, not late.
  CHECK(Classify(24, 23, 0, words, 2) == STORE_ON_TIME);
}

int main() {
  TestCommands();
  TestRingPosition();
  TestClassify();
  return test_checker.failures() ? 1 : 0;
}
