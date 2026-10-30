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
// Profile format and permanent feature identities.

#ifndef BEADS_PROFILE_H_
#define BEADS_PROFILE_H_

#include <stddef.h>
#include <stdint.h>

#include "stmlib/stmlib.h"

namespace beads {

const size_t kProfileSize = 512;
const uint32_t kProfileMagic = 0x46505a44;
const uint8_t kMaxProfileEntries = 82;
const uint8_t kNoField = 255;
const uint32_t kProfileReleaseId = 0xa51e8760;
const uint8_t kSettingsFormat = 0xc1;

enum ActionSlot {
  ACTION_NONE,
  ACTION_SEED,
  ACTION_FREEZE,
  ACTION_HOLD
};

enum AmountSlot {
  AMOUNT_NONE,
  AMOUNT_DRY_WET,
  AMOUNT_REVERB
};

enum ProfileFeature {
  FEATURE_NONE,
  FEATURE_PITCH,
  FEATURE_REVERB,
  FEATURE_DELAY,
  FEATURE_PROCESSOR,
  FEATURE_LIMITER,
  FEATURE_FROZEN,
  FEATURE_SYNTH,
  FEATURE_ATTACKS,
  FEATURE_SYNTH_START,
  FEATURE_INTERVALS,
  FEATURE_WIDTH,
  FEATURE_DIFFUSION,
  FEATURE_MONOPHONIC,
  FEATURE_TRIM,
  FEATURE_VERSION,
  FEATURE_ATTENUATION,
  FEATURE_DELAY_CUE,
  FEATURE_BRIGHTNESS,
  FEATURE_LIVE_ASSIGN,
  FEATURE_LAST
};

enum ProfileState {
  PROFILE_OFF,
  PROFILE_FIXED,
  PROFILE_ADJUSTABLE
};

struct ProfileEntry {
  uint8_t feature;
  uint8_t state;
  uint8_t value;
  uint8_t page_slot;
  uint8_t amount_slot;
  uint8_t action;
};

STATIC_ASSERT(sizeof(ProfileEntry) == 6, profile_entry_size);

struct FeatureDefinition {
  uint8_t values;  // Zero means a full byte.
  uint8_t field;  // Permanent nibble, or 255 for no packed value.
  bool page;
  bool amount;
  bool action;
  bool stateless;
};

extern const FeatureDefinition kFeatureDefinitions[FEATURE_LAST];
extern const volatile uint8_t kProfileBlock[kProfileSize];

class Profile {
 public:
  Profile() { }
  ~Profile() { }

  bool Init(const volatile uint8_t* block);
  void Defaults();
  static uint32_t Crc32(const volatile uint8_t* data, size_t size);

  inline const ProfileEntry& entry(ProfileFeature feature) const {
    return entries_[feature];
  }

  inline bool adjustable(ProfileFeature feature) const {
    return entries_[feature].state == PROFILE_ADJUSTABLE;
  }

  ProfileFeature Page(uint8_t slot) const;
  ProfileFeature Amount(uint8_t slot) const;
  ProfileFeature Action(uint8_t slot) const;

 private:
  bool Parse(const volatile uint8_t* block);
  ProfileEntry entries_[FEATURE_LAST];

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace beads

#endif  // BEADS_PROFILE_H_
