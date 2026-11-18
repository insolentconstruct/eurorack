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
// Plaits' 6-op banks as each engine unpacks them, built apart from the synth
// test.

#include "beads/test/plaits_fm_banks.h"

#include <algorithm>

#include "stmlib/utils/buffer_allocator.h"

#include "beads/dsp/plaits_voice.h"
#define private public
#include "plaits/dsp/engine2/six_op_engine.h"
#undef private
#include "plaits/resources.h"

using namespace beads;
using namespace plaits;

static SixOpEngine six_op_engine;
static uint8_t engine_ram[8192];

bool FmBanksUnpacked(const void* fm_banks) {
  const uint8_t* banks = static_cast<const uint8_t*>(fm_banks);
  const size_t bank_size = kPlaitsFmBankSize * sizeof(fm::Patch);
  stmlib::BufferAllocator allocator(engine_ram, sizeof(engine_ram));
  six_op_engine.Init(&allocator);
  for (int i = 0; i < kNumPlaitsFmBanks; ++i) {
    six_op_engine.LoadUserData(fm_patches_table[i]);
    const uint8_t* unpacked = six_op_engine.patches_[0].bytes();
    if (!std::equal(&unpacked[0], &unpacked[bank_size],
            &banks[i * bank_size])) {
      return false;
    }
  }
  return true;
}
