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

#include "beads/dsp/pitch_quantizer.h"
#ifdef PLAITS_BLOBS
#include "beads/blob/plaits_region.h"
#endif  // PLAITS_BLOBS

namespace beads {

const size_t kProfileSize = 4096;
const uint32_t kProfileMagic = 0x46505a44;
const uint8_t kMaxProfileEntries = 82;
const uint8_t kNoField = 255;
// Changes only for incompatible profile meanings (SCALE-PROFILES.md).
const uint32_t kProfileFormat = 1;
const uint8_t kSettingsFormat = 0xc1;
// Field 0 keeps 15 as not-set; field 13 explicitly selects custom scale 15.
const uint8_t kPitchExtensionField = 13;
const uint8_t kPitchExtensionScale = 15;
const uint8_t kPitchExtensionValue = 1;
// Fields 16 and 18 (low nibbles 0-14 each) keep the identity of the profile
// that last saved; 15 is not set.
const uint8_t kIdentityField = 16;
const uint8_t kNumIdentities = 225;
// Page slot 0 is none; 1 to 21 are the options pages' knobs, in order.
const uint8_t kNumPageSlots = 22;
#ifdef PLAITS_BLOBS
// Version 3 adds the Plaits banks (8 permanent model IDs each) and the
// region directory after the scales; 0 banks means the stock three.
const uint8_t kProfileVersionBlobs = 3;
const uint8_t kMaxProfileBanks = 15;
const uint8_t kMaxProfileSections = PLAITS_SECTION_LAST;
const uint8_t kNumStockPlaitsBanks = 3;
#else
const uint8_t kMaxProfileBanks = 0;
const uint8_t kMaxProfileSections = 0;
#endif  // PLAITS_BLOBS
const uint8_t kProfileBankSize = 8;

enum ActionSlot {
  ACTION_NONE,
  ACTION_SEED,
  ACTION_FREEZE,
  ACTION_HOLD,
  ACTION_LAST
};

enum AmountSlot {
  AMOUNT_NONE,
  AMOUNT_DRY_WET,
  AMOUNT_REVERB,
  AMOUNT_FEEDBACK,
  AMOUNT_LAST
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
  FEATURE_SAVE_FREEZE,
  FEATURE_SEED_LATCH,
  FEATURE_PITCH_NOTCH,
  FEATURE_TIME_SCAN,
  FEATURE_SYNTH_TAIL,
  FEATURE_IDLE_TIMEOUT,
  FEATURE_SEED_LATCH_FADE,
  FEATURE_MANUAL_GAIN,
  FEATURE_ATTACK_SENSITIVITY,
  FEATURE_SEED_START,
  FEATURE_PITCH_LAW,
  FEATURE_PLAITS_BANK,
  FEATURE_MIX_LAW,
  FEATURE_REVERB_INPUT,
  FEATURE_GRAIN_STEALING,
  FEATURE_UNITY_FEEDBACK,
  FEATURE_DELAY_CROSSFADE,
  FEATURE_PAGE_PREVIEW,
  FEATURE_FIRST_PAGE,
  FEATURE_START_QUALITY,
  FEATURE_GAIN_KNOB,
  FEATURE_RESONATOR,
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

  inline bool custom_bank() const { return custom_bank_; }
  inline uint8_t identity() const { return identity_; }
  inline uint8_t scale_count() const { return scale_count_; }
  inline const PitchScaleDefinition& scale(size_t index) const {
    return scales_[index];
  }
  const PitchScaleDefinition* FindScale(uint8_t id) const;
  bool HasScale(uint8_t id) const;
  PitchScale FirstScale() const;
  PitchScale NextScale(PitchScale current) const;

  ProfileFeature Page(uint8_t slot) const;
  ProfileFeature Amount(uint8_t slot) const;
  ProfileFeature Action(uint8_t slot) const;

#ifdef PLAITS_BLOBS
  // How many values a feature takes: its definition's, or the bank count.
  uint8_t Values(ProfileFeature feature) const;

  // The banks' model IDs: the profile's, or the stock 3 x 8.
  inline uint8_t num_plaits_banks() const { return num_plaits_banks_; }
  inline const uint8_t* plaits_bank(uint8_t bank) const {
    return plaits_banks_[bank];
  }
  inline const uint8_t (*plaits_banks() const)[kProfileBankSize] {
    return plaits_banks_;
  }
  // NULL without a directory: the release's default layout applies.
  inline const PlaitsDirectoryEntry* directory() const {
    return num_sections_ ? directory_ : NULL;
  }
  inline uint8_t num_sections() const { return num_sections_; }
  static bool KnownModel(uint8_t id);
#endif  // PLAITS_BLOBS

 private:
  bool Parse(const volatile uint8_t* block);
  void FactoryScales();
  bool ParseScales(const volatile uint8_t* block, uint8_t count);
#ifdef PLAITS_BLOBS
  void StockBanks();
  bool ParseBanks(const volatile uint8_t* block, uint8_t count);
  void ParseDirectory(const volatile uint8_t* block, uint8_t count);
#endif  // PLAITS_BLOBS
  ProfileEntry entries_[FEATURE_LAST];
  bool custom_bank_;
  uint8_t identity_;
  uint8_t scale_count_;
  PitchScaleDefinition scales_[kMaxCustomScales];
#ifdef PLAITS_BLOBS
  uint8_t num_plaits_banks_;
  uint8_t plaits_banks_[kMaxProfileBanks][kProfileBankSize];
  uint8_t num_sections_;
  PlaitsDirectoryEntry directory_[PLAITS_SECTION_LAST];
#endif  // PLAITS_BLOBS

  DISALLOW_COPY_AND_ASSIGN(Profile);
};

}  // namespace beads

#endif  // BEADS_PROFILE_H_
