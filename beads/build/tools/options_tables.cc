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

// By the page's mix LED mask.
const char* const kPageNames[] = {
  "none", "FEEDBACK", "DRY/WET", "FEEDBACK and DRY/WET", "REVERB",
  "FEEDBACK and REVERB", "DRY/WET and REVERB", "all three"
};
// Names come from beads/resources/features.json (build/tools/features.py).
const char* const kOptionNames[] = {
  "Not used yet",
  "Pitch scale",
  "Reverb character",
  "Delay mode",
  "Processor",
  "Limiter",
  "Frozen feedback",
  "Synth",
  "Attacks",
  "Synth start",
  "Random pitch steps",
  "Stereo width",
  "Diffusion",
  "Monophonic grains",
  "Buffer trim",
  "Version at boot",
  "Input attenuation",
  "Delay cue",
  "LED brightness",
  "Live assignment",
  "Save FREEZE",
  "SEED latch",
  "Pitch notch",
  "TIME scan",
  "Synth reverb tail",
  "Options page idle timeout",
  "Delay SEED latch fade",
  "Input gain at power-on",
  "Attack sensitivity",
  "SEED at power-on",
  "PITCH knob",
  "Plaits bank",
  "Mix law",
  "Reverb input",
  "Grain stealing",
  "Delay feedback",
  "Delay time",
  "Options page preview",
  "Options page on opening",
  "Start in quality",
  "Manual gain knob",
  "Resonator",
  "Reserved",
  "Resonator material",
  "Resonator ring time",
  "Resonator structure",
  "Resonator brightness",
  "Resonator damping",
  "Resonator position",
  "Resonator amount",
};
const char* const kOptionKnobNames[] = { "FEEDBACK", "DRY/WET", "REVERB" };
// Each indicated page value, in page order.
struct NamedValue {
  beads::ProfileFeature feature;
  uint8_t value;
  const char* name;
};
const NamedValue kValueNames[] = {
  { beads::FEATURE_PITCH, 0, "Off" },
  { beads::FEATURE_PITCH, 1, "Chromatic" },
  { beads::FEATURE_PITCH, 2, "Fifths" },
  { beads::FEATURE_PITCH, 3, "Major" },
  { beads::FEATURE_PITCH, 4, "Minor" },
  { beads::FEATURE_PITCH, 5, "Pentatonic major" },
  { beads::FEATURE_PITCH, 6, "Pentatonic minor" },
  { beads::FEATURE_PITCH, 7, "Pelog" },
  { beads::FEATURE_PITCH, 8, "Raag Bhairav" },
  { beads::FEATURE_PITCH, 9, "Raag Shri" },
  { beads::FEATURE_REVERB, 0, "Stock" },
  { beads::FEATURE_REVERB, 1, "Clouds-style" },
  { beads::FEATURE_DELAY, 0, "Auto (Stock)" },
  { beads::FEATURE_DELAY, 1, "Disabled" },
  { beads::FEATURE_DELAY, 2, "Wide/smooth filter" },
  { beads::FEATURE_PROCESSOR, 0, "Granular (Stock)" },
  { beads::FEATURE_PROCESSOR, 1, "Spectral" },
  { beads::FEATURE_LIMITER, 0, "Stock" },
  { beads::FEATURE_LIMITER, 1, "Fast release" },
  { beads::FEATURE_LIMITER, 2, "Off" },
  { beads::FEATURE_FROZEN, 0, "Stock" },
  { beads::FEATURE_FROZEN, 1, "Evolving" },
  { beads::FEATURE_SYNTH, 0, "Beads (Stock)" },
  { beads::FEATURE_SYNTH, 1, "Plaits" },
  { beads::FEATURE_ATTACKS, 0, "Off" },
  { beads::FEATURE_ATTACKS, 1, "Snap" },
  { beads::FEATURE_ATTACKS, 2, "Snap and trigger" },
  { beads::FEATURE_SYNTH_START, 0, "Automatic (Stock)" },
  { beads::FEATURE_SYNTH_START, 1, "Manual" },
  { beads::FEATURE_INTERVALS, 0, "Free (Stock)" },
  { beads::FEATURE_INTERVALS, 1, "Fifths and octaves" },
  { beads::FEATURE_INTERVALS, 2, "Recent notes" },
  { beads::FEATURE_BRIGHTNESS, 0, "100%" },
  { beads::FEATURE_BRIGHTNESS, 1, "66%" },
  { beads::FEATURE_BRIGHTNESS, 2, "33%" },
  { beads::FEATURE_SEED_LATCH, 0, "Off" },
  { beads::FEATURE_SEED_LATCH, 1, "Ignore" },
  { beads::FEATURE_SEED_LATCH, 2, "Sync" },
  { beads::FEATURE_SEED_LATCH, 3, "Add" },
  { beads::FEATURE_TIME_SCAN, 0, "Random" },
  { beads::FEATURE_TIME_SCAN, 1, "Scan" },
  { beads::FEATURE_RESONATOR, 0, "Off (stock)" },
  { beads::FEATURE_RESONATOR, 1, "After the grains" },
  { beads::FEATURE_RESONATOR, 2, "Before the buffer" },
  { beads::FEATURE_RESONATOR_MATERIAL, 0, "Wood" },
  { beads::FEATURE_RESONATOR_MATERIAL, 1, "String" },
  { beads::FEATURE_RESONATOR_MATERIAL, 2, "Glass" },
  { beads::FEATURE_RESONATOR_MATERIAL, 3, "Bell" },
  { beads::FEATURE_RESONATOR_RING, 0, "Short" },
  { beads::FEATURE_RESONATOR_RING, 1, "Medium" },
  { beads::FEATURE_RESONATOR_RING, 2, "Long" },
  { beads::FEATURE_RESONATOR_RING, 3, "Very long" },
};

const char* ColorName(const beads::Leds::Color& color) {
  struct NamedColor {
    beads::Leds::Color color;
    const char* name;
  };
  static const NamedColor colors[] = {
    { { 0, 0, 0 }, "unchanged" },
    { { 0, 255, 0 }, "green" },
    { { 255, 255, 0 }, "yellow" },
    { { 255, 0, 0 }, "red" },
    { { 255, 64, 32 }, "coral" },
    { { 255, 160, 0 }, "amber" },
    { { 32, 96, 255 }, "blue" },
    { { 0, 220, 160 }, "teal" },
    { { 160, 48, 255 }, "violet" },
    { { 255, 32, 144 }, "pink" },
  };
  for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
    const beads::Leds::Color& candidate = colors[i].color;
    if (candidate.r == color.r && candidate.g == color.g &&
        candidate.b == color.b) {
      return colors[i].name;
    }
  }
  return "unknown";
}

}  // namespace

int main(int argc, char** argv) {
  using namespace beads;
  if (argc > 1) {
    Profile factory;
    factory.Defaults();
    for (int id = FEATURE_PITCH; id < FEATURE_LAST; ++id) {
      const FeatureDefinition& d = kFeatureDefinitions[id];
      const ProfileEntry& e = factory.entry(ProfileFeature(id));
      printf("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", id,
          kOptionNames[id], d.values, d.field, d.page, d.amount, d.action,
          d.stateless, e.state, e.value);
    }
    return 0;
  }
  puts("page\tLED\tknob\tsetting\tvalue\tcolour\tpulse");
  for (int page = 0; page < kNumOptionPages; ++page) {
    for (int knob = 0; knob < 3; ++knob) {
      const ProfileFeature setting = kOptionPages[page].knobs[knob];
      if (setting == FEATURE_NONE) {
        continue;
      }
      for (size_t i = 0; i < sizeof(kValueNames) / sizeof(kValueNames[0]);
           ++i) {
        if (kValueNames[i].feature != setting) {
          continue;
        }
        const OptionValue value = FindOptionValue(
            setting, kValueNames[i].value);
        char pulse[64];
        if (!value.pulses) {
          snprintf(pulse, sizeof(pulse), "steady");
        } else if (value.pulses < 3) {
          snprintf(pulse, sizeof(pulse), "%s (%.2f Hz)",
              value.pulses == 1 ? "slow" : "fast",
              1000.0 / (value.pulses == 1
                  ? kOptionsSlowPeriod : kOptionsFastPeriod));
        } else {
          snprintf(pulse, sizeof(pulse), "2 pulses / %.3f s",
              kOptionsPulsePeriod / 1000.0);
        }
        printf("%d\t%s\t%s\t%s\t%s\t%s\t%s\n", page + 1,
            kPageNames[kOptionPages[page].leds], kOptionKnobNames[knob],
            kOptionNames[setting], kValueNames[i].name,
            ColorName(value.color), pulse);
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
  printf("constant\tkOptionsProfileZoneBand\t%.9g\n",
      double(kOptionsProfileZoneBand));
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
