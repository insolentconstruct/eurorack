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
// The Rings core for the host tests.

#include "beads/test/rings_core_host.h"

#include <cstdio>
#include <new>

#include "beads/blob/synths/rings_synth_core.h"

namespace beads {

using namespace rings;

uint32_t configure_calls;
uint32_t render_calls;

void* HostInit(uint8_t* ram, size_t ram_size, const PlaitsBlobServices* s) {
  RingsSynthCore* core = new(ram) RingsSynthCore;
  core->Init();
  return core;
}

bool HostConfigure(void* context, const RingsSynthTarget* target,
                   const RingsSynthControls* controls, bool entry) {
  ++configure_calls;
  return static_cast<RingsSynthCore*>(context)->Configure(
      *target, *controls, entry);
}

void HostRender(void* context, RingsSynthControls* controls, float* out,
                float* aux, size_t size) {
  ++render_calls;
  static_cast<RingsSynthCore*>(context)->Render(controls, out, aux, size);
}

size_t CoreSize() {
  return sizeof(RingsSynthCore);
}

size_t CoreBlockSize() {
  return kMaxBlockSize;
}

bool CoreSizesAddUp() {
  printf("block %d: Part %d, StringSynthPart %d, Strummer %d, core %d B "
         "(host; the blob header has the target's)\n",
         int(kMaxBlockSize), int(sizeof(Part)), int(sizeof(StringSynthPart)),
         int(sizeof(Strummer)), int(sizeof(RingsSynthCore)));
  return sizeof(RingsSynthCore) >= sizeof(Part) + sizeof(StringSynthPart) +
      sizeof(Strummer) + 2 * kRingsReverbSize;
}

bool CoreValid(const RingsSynthTarget& target) {
  return RingsSynthCore::Valid(target);
}

const RingsSynthTarget& CoreTarget(void* context) {
  return static_cast<RingsSynthCore*>(context)->target();
}

}  // namespace beads
