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
// The 6-op blob's algorithms against Plaits': every algorithm compiles to the
// same render calls, each renderer index naming the renderer Plaits' function
// pointer does, and RunRenderer runs that renderer.

#include <cstdio>
#include <cstring>

#include "beads/blob/libraries/six_op_algorithms.h"
#include "beads/blob/models/six_op_voice.h"
#include "plaits/dsp/fm/algorithms.h"
#include "beads/test/test_checker.h"

using namespace plaits;
using namespace plaits::fm;

const int kNumRenderers = 10;
const size_t kSize = 24;

// Plaits' renderers in its table's order (plaits/dsp/fm/algorithms.cc).
const RenderFn renderers[kNumRenderers] = {
  &RenderOperators<1, -2, false>,
  &RenderOperators<1, -2, true>,
  &RenderOperators<1, -1, false>,
  &RenderOperators<1, -1, true>,
  &RenderOperators<1, 0, false>,
  &RenderOperators<1, 0, true>,
  &RenderOperators<3, 2, true>,
  &RenderOperators<2, 1, true>,
  &RenderOperators<2, 0, false>,
  &RenderOperators<2, 0, true>
};

Algorithms<6> plaits_algorithms;
IndexedAlgorithms<6> indexed_algorithms;

void TestCompiledCalls() {
  test_checker.Start("TestCompiledCalls");
  plaits_algorithms.Init();
  indexed_algorithms.Init();
  int calls = 0;
  for (int a = 0; a < Algorithms<6>::NUM_ALGORITHMS; ++a) {
    for (int op = 0; op < 6; op += plaits_algorithms.render_call(a, op).n) {
      const Algorithms<6>::RenderCall& p = plaits_algorithms.render_call(a, op);
      const IndexedAlgorithms<6>::RenderCall& i =
          indexed_algorithms.render_call(a, op);
      CHECK(i.renderer >= 0 && i.renderer < kNumRenderers);
      CHECK(i.renderer >= 0 && i.renderer < kNumRenderers &&
            renderers[i.renderer] == p.render_fn);
      CHECK(i.n == p.n);
      CHECK(i.input_index == p.input_index);
      CHECK(i.output_index == p.output_index);
      CHECK(indexed_algorithms.is_modulator(a, op) ==
            plaits_algorithms.is_modulator(a, op));
      ++calls;
    }
  }
  // Every renderer is used by some algorithm.
  for (int r = 0; r < kNumRenderers; ++r) {
    bool used = false;
    for (int a = 0; a < Algorithms<6>::NUM_ALGORITHMS; ++a) {
      for (int op = 0; op < 6; op += plaits_algorithms.render_call(a, op).n) {
        used = used || indexed_algorithms.render_call(a, op).renderer == r;
      }
    }
    CHECK(used);
  }
  printf("%d render calls\n", calls);
}

void Fill(Operator* ops, float* f, float* a, float* fb, float* modulation,
    float* out, int seed) {
  for (int i = 0; i < 6; ++i) {
    ops[i].phase = 0x12345678u * (i + seed);
    ops[i].amplitude = 0.1f * float(i + 1);
    f[i] = 0.01f * float(i + seed);
    a[i] = 0.2f * float(i + 2);
  }
  fb[0] = 0.25f;
  fb[1] = -0.5f;
  for (size_t i = 0; i < kSize; ++i) {
    modulation[i] = float(i) / kSize - 0.5f;
    out[i] = 0.125f * float(i % 5);
  }
}

void TestRunRenderer() {
  test_checker.Start("TestRunRenderer");
  for (int r = 0; r < kNumRenderers; ++r) {
    Operator ops[2][6];
    float f[6], a[6], fb[2][2], modulation[kSize], out[2][kSize];
    for (int k = 0; k < 2; ++k) {
      Fill(ops[k], f, a, fb[k], modulation, out[k], r + 1);
    }
    (*renderers[r])(ops[0], f, a, fb[0], 3, modulation, out[0], kSize);
    RunRenderer(r, ops[1], f, a, fb[1], 3, modulation, out[1], kSize);
    CHECK(!memcmp(out[0], out[1], sizeof(out[0])));
    CHECK(!memcmp(ops[0], ops[1], sizeof(ops[0])));
    CHECK(!memcmp(fb[0], fb[1], sizeof(fb[0])));
  }
}

int main() {
  TestCompiledCalls();
  TestRunRenderer();
  puts(test_checker.failures() ? "6-op blob checks: FAIL"
                               : "6-op blob checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
