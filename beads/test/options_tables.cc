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
// Export the firmware options layout and MEDIA indications as TSV.

#include <cstdio>
#include "beads/options.h"

int main() {
  using namespace beads;
  puts("page\tLED\tknob\tsetting\tvalue\tcolour\tpulse");
  for (int page = 0; page < 3; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      const OptionSetting setting = kOptionPages[page].knobs[knob];
      for (size_t i = 0; i < kNumOptionValues; ++i) {
        const OptionValue& value = kOptionValues[i];
        if (value.setting != setting) continue;
        const char* pulse = value.pulses == 0 ? "steady"
            : value.pulses == 1 ? "slow (0.98 Hz)"
            : value.pulses == 2 ? "fast (3.91 Hz)"
            : "2 yellow pulses / 2.048 s";
        printf("%d\t%s\t%s\t%s\t%s\t%s\t%s\n", page + 1,
            kOptionPages[page].led, kOptionKnobNames[knob],
            kOptionNames[setting], value.name, value.color, pulse);
      }
    }
  }
  return 0;
}
