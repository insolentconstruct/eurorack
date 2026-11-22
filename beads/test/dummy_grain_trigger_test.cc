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
// Dummy grain trigger: WasteCPUCycles must not read uninitialised memory.
// Valgrind (or MemorySanitizer) finds it; ASan and UBSan cannot.
//
// g++ -DTEST -O0 -g -I. beads/test/dummy_grain_trigger_test.cc \
//   beads/resources.cc stmlib/dsp/units.cc stmlib/utils/random.cc \
//   -o dummy_grain_trigger_test && valgrind --error-exitcode=1 \
//   --track-origins=yes ./dummy_grain_trigger_test

#include <cstdio>

#include "beads/dsp/granular_processor.h"

using namespace beads;

// As processor_vintage_digital_, the processor beads.cc wastes cycles on.
const size_t kBankSize = 262144;
const int kMaxDelay = 768 * 1000 * 8 / (12 * 2);
static uint8_t buffer[3][kBankSize];
static GranularProcessor<RESOLUTION_12_BIT, kMaxDelay, 1> processor;

int main() {
  uint8_t* buffers[3] = { buffer[0], buffer[1], buffer[2] };
  processor.Init(32000.0f, buffers);
  processor.Reset();
  for (int i = 0; i < 64; ++i) {
    processor.WasteCPUCycles(16);
  }
  printf("Rendered 64 dummy blocks\n");
  return 0;
}
