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
// Host render: the FM drum test, then every processor function in both
// control modes, with all four parameters swept end to end and gates from
// slow clocks down to one-sample triggers.

#define main desktop_test_main
#include "peaks/test/peaks_test.cc"
#undef main

const size_t kBlockSize = 4;
const uint32_t kPotPeriod = 32;
const uint32_t kGatePeriods[] = { kSampleRate / 2, kSampleRate / 9, 211, 2 };

uint16_t Sweep(uint32_t i, uint32_t rate) {
  const uint32_t phase = (i * rate) & 0x1ffff;
  return phase > 65535 ? 131071 - phase : phase;
}

void TestProcessorFunction(ProcessorFunction function, ControlMode mode) {
  char name[64];
  sprintf(name, "peaks_%d_%s.wav", function,
      mode == CONTROL_MODE_FULL ? "full" : "half");
  WavWriter wav_writer(1, kSampleRate, 8);
  wav_writer.Open(name);

  processors[0].Init(0);
  processors[0].set_control_mode(mode);
  processors[0].set_function(function);

  for (uint32_t i = 0; i < kSampleRate * 8; i += kBlockSize) {
    if (i % kPotPeriod == 0) {
      processors[0].set_parameter(0, Sweep(i, 1));
      processors[0].set_parameter(1, Sweep(i, 3));
      processors[0].set_parameter(2, Sweep(i, 7));
      processors[0].set_parameter(3, Sweep(i, 13));
    }
    GateFlags gate_flags[kBlockSize];
    for (size_t j = 0; j < kBlockSize; ++j) {
      const uint32_t t = i + j;
      const uint32_t period = kGatePeriods[(t / kSampleRate) % 4];
      const bool high = t % period < (period + 3) / 4;
      const bool previous = t && (t - 1) % period < (period + 3) / 4;
      gate_flags[j] = peaks::ExtractGateFlags(previous, high);
    }
    int16_t out[kBlockSize];
    processors[0].Process(gate_flags, out, kBlockSize);
    wav_writer.WriteFrames(out, kBlockSize);
  }
}

int main(void) {
  TestFMDrum();
  for (int f = 0; f < PROCESSOR_FUNCTION_LAST; ++f) {
    TestProcessorFunction(ProcessorFunction(f), CONTROL_MODE_FULL);
    TestProcessorFunction(ProcessorFunction(f), CONTROL_MODE_HALF);
  }
}
