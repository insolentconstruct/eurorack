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
// A stream of 512-byte candidates goes through the firmware's own parser.
#include <cstdio>
#include <cstring>
#include "beads/profile.h"

int main() {
  uint8_t block[beads::kProfileSize];
  beads::Profile defaults;
  defaults.Defaults();
  for (size_t i = 0; i < sizeof(block); ++i) {
    block[i] = beads::kProfileBlock[i];
  }
  beads::Profile p;
  if (!p.Init(block)) return 1;
  for (int id = 1; id < beads::FEATURE_LAST; ++id) {
    if (memcmp(&defaults.entry(beads::ProfileFeature(id)),
        &p.entry(beads::ProfileFeature(id)), sizeof(beads::ProfileEntry))) {
      return 1;
    }
  }
  while (fread(block, 1, sizeof(block), stdin) == sizeof(block)) {
    const bool accepted = p.Init(block);
    putchar(accepted ? 1 : 0);
    for (int i = 0; i < 82; ++i) {
      uint8_t empty[6] = {0, 0, 0, 0, 0, 0};
      const void* entry = empty;
      if (accepted && i < block[5]) {
        entry = &p.entry(beads::ProfileFeature(block[16 + i * 6]));
      }
      fwrite(entry, 1, 6, stdout);
    }
  }
  return ferror(stdin) ? 1 : 0;
}
