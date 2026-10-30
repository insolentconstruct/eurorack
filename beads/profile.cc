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
// Profile validation. Byte reads also support patched flash images.

#include "beads/profile.h"

#include <cstring>

namespace beads {

const FeatureDefinition kFeatureDefinitions[FEATURE_LAST] = {
  { 0, kNoField, false, false, false, false },  // None.
  { 10, 0, true, false, false, false },  // Pitch.
  { 2, 1, true, false, false, false },  // Reverb.
  { 3, 2, true, false, false, false },  // Delay.
  { 2, 3, true, false, false, false },  // Processor.
  { 3, 4, true, false, false, false },  // Limiter.
  { 2, 5, true, false, false, false },  // Frozen.
  { 2, 6, true, false, false, false },  // Synth.
  { 3, 7, true, false, false, false },  // Attacks.
  { 2, 8, true, false, true, true },  // Synth start.
  { 2, 9, true, false, false, false },  // Intervals.
  { 0, kNoField, false, true, false, false },  // Width.
  { 0, kNoField, false, true, false, false },  // Diffusion.
  { 2, kNoField, false, false, true, false },  // Monophonic.
  { 2, kNoField, false, false, true, true },  // Trim.
  { 2, kNoField, false, false, false, false },  // Version.
  { 2, kNoField, false, false, false, false },  // Attenuation.
  { 2, kNoField, false, false, false, false },  // Delay cue.
  { 3, 10, true, false, false, false },  // Brightness.
  { 2, kNoField, false, false, false, false },  // Live assignment.
  { 2, kNoField, false, false, false, false },  // Save freeze.
  { 4, 11, true, false, false, false },  // Seed latch.
  { 2, kNoField, false, false, false, false },  // Pitch notch.
};

const volatile uint8_t kProfileBlock[kProfileSize]
    __attribute__((section(".profile"), used, aligned(32))) = {
  0x44, 0x5a, 0x50, 0x46, 0x01, 0x0e, 0x54, 0x00, 0x60, 0x87, 0x1e, 0xa5, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x00, 0x05, 0x00, 0x00, 0x03, 0x02, 0x00, 0x04,
  0x00, 0x00, 0x04, 0x02, 0x00, 0x07, 0x00, 0x00, 0x05, 0x02, 0x00, 0x0c, 0x00, 0x00, 0x06, 0x02,
  0x00, 0x06, 0x00, 0x00, 0x07, 0x02, 0x00, 0x08, 0x00, 0x00, 0x08, 0x02, 0x00, 0x03, 0x00, 0x00,
  0x09, 0x02, 0x00, 0x09, 0x00, 0x03, 0x0a, 0x02, 0x00, 0x02, 0x00, 0x00, 0x0b, 0x02, 0x00, 0x00,
  0x01, 0x00, 0x0c, 0x02, 0x00, 0x00, 0x02, 0x00, 0x0d, 0x02, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x01,
  0x01, 0x00, 0x00, 0x02, 0x84, 0x9f, 0x1a, 0x5c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

static uint32_t Read32(const volatile uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
      uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

/* static */
uint32_t Profile::Crc32(const volatile uint8_t* data, size_t size) {
  uint32_t crc = 0xffffffff;
  while (size--) {
    crc ^= *data++;
    for (int i = 0; i < 8; ++i) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return ~crc;
}

void Profile::Defaults() {
  memset(entries_, 0, sizeof(entries_));
  const ProfileEntry defaults[] = {
    { FEATURE_PITCH, PROFILE_ADJUSTABLE, 0, 1,
      AMOUNT_NONE, ACTION_NONE },  // Pitch.
    { FEATURE_REVERB, PROFILE_ADJUSTABLE, 0, 5,
      AMOUNT_NONE, ACTION_NONE },  // Reverb.
    { FEATURE_DELAY, PROFILE_ADJUSTABLE, 0, 4,
      AMOUNT_NONE, ACTION_NONE },  // Delay.
    { FEATURE_PROCESSOR, PROFILE_ADJUSTABLE, 0, 7,
      AMOUNT_NONE, ACTION_NONE },  // Processor.
    { FEATURE_LIMITER, PROFILE_ADJUSTABLE, 0, 12,
      AMOUNT_NONE, ACTION_NONE },  // Limiter.
    { FEATURE_FROZEN, PROFILE_ADJUSTABLE, 0, 6,
      AMOUNT_NONE, ACTION_NONE },  // Frozen.
    { FEATURE_SYNTH, PROFILE_ADJUSTABLE, 0, 8,
      AMOUNT_NONE, ACTION_NONE },  // Synth.
    { FEATURE_ATTACKS, PROFILE_ADJUSTABLE, 0, 3,
      AMOUNT_NONE, ACTION_NONE },  // Attacks.
    { FEATURE_SYNTH_START, PROFILE_ADJUSTABLE, 0, 9,
      AMOUNT_NONE, ACTION_HOLD },  // Synth start.
    { FEATURE_INTERVALS, PROFILE_ADJUSTABLE, 0, 2,
      AMOUNT_NONE, ACTION_NONE },  // Intervals.
    { FEATURE_WIDTH, PROFILE_ADJUSTABLE, 0, 0,
      AMOUNT_DRY_WET, ACTION_NONE },  // Width.
    { FEATURE_DIFFUSION, PROFILE_ADJUSTABLE, 0, 0,
      AMOUNT_REVERB, ACTION_NONE },  // Diffusion.
    { FEATURE_MONOPHONIC, PROFILE_ADJUSTABLE, 0, 0,
      AMOUNT_NONE, ACTION_SEED },  // Monophonic.
    { FEATURE_TRIM, PROFILE_FIXED, 1, 0, AMOUNT_NONE, ACTION_FREEZE },  // Trim.
  };
  for (size_t i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i) {
    entries_[defaults[i].feature] = defaults[i];
  }
}

bool Profile::Init(const volatile uint8_t* block) {
  if (Parse(block)) {
    return true;
  }
  Defaults();
  return false;
}

bool Profile::Parse(const volatile uint8_t* b) {
  const uint8_t count = b[5];
  const uint16_t bytes = uint16_t(b[6]) | uint16_t(b[7]) << 8;
  if (count > kMaxProfileEntries || bytes != 6 * count) {
    return false;
  }
  if (Read32(b) != kProfileMagic || b[4] != 1 || Read32(b + 12) != 0 ||
      Crc32(b, 16 + bytes) != Read32(b + 16 + bytes) ||
      Read32(b + 8) != kProfileReleaseId) {
    return false;
  }
  memset(entries_, 0, sizeof(entries_));
  bool seen[FEATURE_LAST] = { false };
  bool pages[13] = { false };
  bool amounts[3] = { false };
  bool actions[4] = { false };
  for (size_t i = 0; i < count; ++i) {
    const volatile uint8_t* p = b + 16 + 6 * i;
    const uint8_t id = p[0];
    if (!id || id >= FEATURE_LAST || seen[id]) {
      return false;
    }
    seen[id] = true;
    const FeatureDefinition& d = kFeatureDefinitions[id];
    ProfileEntry e = { id, p[1], p[2], p[3], p[4], p[5] };
    if (e.state > PROFILE_ADJUSTABLE || (d.values && e.value >= d.values) ||
        e.page_slot > 12 || e.amount_slot > 2 || e.action > 3) {
      return false;
    }
    if (id >= FEATURE_VERSION && e.state == PROFILE_ADJUSTABLE &&
        !d.page) {
      return false;
    }
    if ((e.page_slot && (!d.page || e.state != PROFILE_ADJUSTABLE)) ||
        (e.amount_slot && (!d.amount || e.state != PROFILE_ADJUSTABLE)) ||
        (e.action && (!d.action || e.state == PROFILE_OFF ||
            (e.state == PROFILE_FIXED && (!e.value || !d.stateless))))) {
      return false;
    }
    if ((e.page_slot && pages[e.page_slot]) ||
        (e.amount_slot && amounts[e.amount_slot]) ||
        (e.action && actions[e.action])) {
      return false;
    }
    if (id == FEATURE_SYNTH_START && e.state == PROFILE_FIXED &&
        e.value && !e.action) {
      return false;
    }
    pages[e.page_slot] = amounts[e.amount_slot] = actions[e.action] = true;
    entries_[id] = e;
  }
  return true;
}

ProfileFeature Profile::Page(uint8_t slot) const {
  for (int i = 1; i < FEATURE_LAST; ++i) {
    if (slot && entries_[i].page_slot == slot) {
      return ProfileFeature(i);
    }
  }
  return FEATURE_NONE;
}

ProfileFeature Profile::Amount(uint8_t slot) const {
  for (int i = 1; i < FEATURE_LAST; ++i) {
    if (slot && entries_[i].amount_slot == slot) {
      return ProfileFeature(i);
    }
  }
  return FEATURE_NONE;
}

ProfileFeature Profile::Action(uint8_t slot) const {
  for (int i = 1; i < FEATURE_LAST; ++i) {
    if (slot && entries_[i].action == slot) {
      return ProfileFeature(i);
    }
  }
  return FEATURE_NONE;
}

}  // namespace beads
