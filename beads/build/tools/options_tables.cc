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
// Export the firmware options layout and quality LED [A] indications as TSV.

#include <cstdio>
#include "beads/options.h"
#include "beads/profile.h"

namespace {

const char* const kPageNames[] = {
  "FEEDBACK", "DRY/WET", "REVERB", "all three"
};
const char* const kOptionNames[] = {
  "Not used yet", "Pitch scale", "Reverb character", "Delay mode",
  "Processor", "Limiter", "Frozen feedback", "Synth", "Attacks",
  "Synth start", "Random intervals",
  "Stereo width", "Diffusion", "Monophonic grains", "Buffer trim",
  "Version at boot", "Input attenuation", "Delay cue", "LED brightness",
  "Live assignment", "Save FREEZE", "SEED latch", "Pitch notch", "TIME scan"
};
const char* const kOptionKnobNames[] = { "FEEDBACK", "DRY/WET", "REVERB" };
struct ValueNames {
  const char* name;
  const char* color;
};
const ValueNames kValueNames[] = {
  { "Not used yet", "unchanged" },
  { "Off", "green" },
  { "Chromatic", "yellow" },
  { "Fifths and octaves", "red" },
  { "Major", "green" },
  { "Minor", "yellow" },
  { "Major pentatonic", "red" },
  { "Minor pentatonic", "green" },
  { "Pelog", "yellow" },
  { "Raag Bhairav", "red" },
  { "Raag Shri", "yellow" },
  { "Stock", "green" },
  { "Clouds-style", "yellow" },
  { "Auto (Stock)", "green" },
  { "Disabled", "yellow" },
  { "Wide/smooth filter", "red" },
  { "Granular (Stock)", "green" },
  { "Spectral", "yellow" },
  { "Stock", "green" },
  { "Fast release", "yellow" },
  { "Off", "red" },
  { "Stock", "green" },
  { "Evolving", "yellow" },
  { "Beads (Stock)", "green" },
  { "Plaits", "yellow" },
  { "Off", "green" },
  { "Snap", "yellow" },
  { "Snap and trigger", "red" },
  { "Automatic (Stock)", "green" },
  { "Manual", "yellow" },
  { "Free (Stock)", "green" },
  { "Fifths and octaves", "yellow" },
  { "100%", "green" }, { "66%", "yellow" }, { "33%", "red" },
  { "Off", "green" }, { "Ignore", "yellow" },
  { "Sync", "red" }, { "Add", "green" },
  { "Random", "green" }, { "Scan", "yellow" },
};

}  // namespace

int main(int argc, char** argv) {
  using namespace beads;
  if (argc > 1) {
    for (int id = FEATURE_PITCH; id < FEATURE_LAST; ++id) {
      const FeatureDefinition& d = kFeatureDefinitions[id];
      printf("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\n", id, kOptionNames[id],
          d.values, d.field, d.page, d.amount, d.action, d.stateless);
    }
    return 0;
  }
  puts("page\tLED\tknob\tsetting\tvalue\tcolour\tpulse");
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      const OptionSetting setting = kOptionPages[page].knobs[knob];
      for (size_t i = 0; i < kNumOptionValues; ++i) {
        const OptionValue& value = kOptionValues[i];
        if (value.setting != setting) continue;
        char pulse[64];
        if (!value.pulses) {
          snprintf(pulse, sizeof(pulse), "steady");
        } else if (value.pulses < 3) {
          snprintf(pulse, sizeof(pulse), "%s (%.2f Hz)",
              value.pulses == 1 ? "slow" : "fast",
              1000.0 / (value.pulses == 1
                  ? kOptionsSlowPeriod : kOptionsFastPeriod));
        } else {
          snprintf(pulse, sizeof(pulse), "2 yellow pulses / %.3f s",
              kOptionsPulsePeriod / 1000.0);
        }
        printf("%d\t%s\t%s\t%s\t%s\t%s\t%s\n", page + 1,
            kPageNames[kOptionPages[page].led_index], kOptionKnobNames[knob],
            kOptionNames[setting], kValueNames[i].name,
            kValueNames[i].color, pulse);
      }
    }
  }
  puts("\nconstant\tname\tvalue");
  printf("constant\tkOptionsHoldTime\t%.9g\n", double(kOptionsHoldTime));
  printf("constant\tkOptionsTapTime\t%.9g\n", double(kOptionsTapTime));
  printf("constant\tkOptionsArmTime\t%.9g\n", double(kOptionsArmTime));
  printf("constant\tkOptionsPickupThreshold\t%.9g\n",
      double(kOptionsPickupThreshold));
  printf("constant\tkOptionsMovementThreshold\t%.9g\n",
      double(kOptionsMovementThreshold));
  printf("constant\tkWidthStockThreshold\t%.9g\n",
      double(kWidthStockThreshold));
  printf("constant\tkWidthEnabledThreshold\t%.9g\n",
      double(kWidthEnabledThreshold));
  printf("constant\tkWidthRange\t%.9g\n", double(kWidthRange));
  printf("constant\tkOptionsHalfLow\t%.9g\n", double(kOptionsHalfLow));
  printf("constant\tkOptionsHalfHigh\t%.9g\n", double(kOptionsHalfHigh));
  printf("constant\tkOptionsZoneBand\t%.9g\n", double(kOptionsZoneBand));
  printf("constant\tkOptionsSettleTime\t%.9g\n", double(kOptionsSettleTime));
  printf("constant\tkOptionsFlashTime\t%.9g\n", double(kOptionsFlashTime));
  printf("constant\tkOptionsSlowPeriod\t%.9g\n", double(kOptionsSlowPeriod));
  printf("constant\tkOptionsFastPeriod\t%.9g\n", double(kOptionsFastPeriod));
  printf("constant\tkOptionsPulsePeriod\t%.9g\n", double(kOptionsPulsePeriod));
  printf("constant\tkOptionsPreviewTime\t%.9g\n", double(kOptionsPreviewTime));
  printf("constant\tkSelfPlayHoldTime\t%.9g\n", double(kSelfPlayHoldTime));
  printf("constant\tkDelayDetentLow\t%.9g\n", double(kDelayDetentLow));
  printf("constant\tkDelayDetentHigh\t%.9g\n", double(kDelayDetentHigh));
  printf("constant\tkOptionsZoneLow\t%.9g\n", double(kOptionsZoneLow));
  printf("constant\tkOptionsZoneHigh\t%.9g\n", double(kOptionsZoneHigh));
  return 0;
}
