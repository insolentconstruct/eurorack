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
// Version 3 profile blocks for development-build tests: the factory entries,
// then a bank table and a region directory.

#ifndef BEADS_TEST_PROFILE_V3_H_
#define BEADS_TEST_PROFILE_V3_H_

#include <cstring>

#include "beads/profile.h"

namespace beads {

inline void ProfileV3Checksum(uint8_t* b) {
  const size_t size = 16 + uint16_t(b[6]) + (uint16_t(b[7]) << 8);
  const uint32_t crc = Profile::Crc32(b, size);
  for (int i = 0; i < 4; ++i) {
    b[size + i] = crc >> (8 * i);
  }
}

inline void ProfileV3Write32(uint8_t* p, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    p[i] = value >> (8 * i);
  }
}

// The factory entries with banks (8 IDs each, NULL: none) and directory
// entries; start_bank sets feature 31's value (255 leaves it out).
inline void ProfileV3(
    uint8_t* b,
    const uint8_t* banks,
    uint8_t bank_count,
    const PlaitsDirectoryEntry* directory,
    uint8_t section_count,
    uint8_t start_bank) {
  for (size_t i = 0; i < kProfileSize; ++i) {
    b[i] = kProfileBlock[i];
  }
  uint8_t count = b[5];
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t* e = b + 16 + 6 * i;
    if (e[0] == FEATURE_PLAITS_BANK) {
      if (start_bank == 255) {
        memmove(e, e + 6, 6 * (count - i - 1));
        --count;
      } else {
        e[2] = start_bank;
      }
      break;
    }
  }
  uint8_t* p = b + 16 + 6 * count;
  memcpy(p, banks, 8 * bank_count);
  p += 8 * bank_count;
  for (uint8_t i = 0; i < section_count; ++i, p += 16) {
    ProfileV3Write32(p, directory[i].kind);
    ProfileV3Write32(p + 4, directory[i].offset);
    ProfileV3Write32(p + 8, directory[i].size);
    ProfileV3Write32(p + 12, directory[i].crc);
  }
  const uint16_t bytes = p - (b + 16);
  b[4] = kProfileVersionBlobs;
  b[5] = count;
  b[6] = bytes;
  b[7] = bytes >> 8;
  b[12] = 0;
  b[13] = 0;
  b[14] = bank_count;
  b[15] = section_count;
  ProfileV3Checksum(b);
}

}  // namespace beads

#endif  // BEADS_TEST_PROFILE_V3_H_
