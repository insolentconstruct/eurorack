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
// The Rings core for the host tests, in its own translation unit: Rings'
// tables and Beads' share macro names.

#ifndef BEADS_TEST_RINGS_CORE_HOST_H_
#define BEADS_TEST_RINGS_CORE_HOST_H_

#include "beads/blob/synths/rings_synth_controls.h"

namespace beads {

// The blob's entries on the host; configure and render are counted.
extern uint32_t configure_calls;
extern uint32_t render_calls;
// The core's last block, before the app's quality, gain and chain.
extern float last_out[kRingsBlockSize];
extern float last_aux[kRingsBlockSize];
void* HostInit(uint8_t* ram, size_t ram_size, const PlaitsBlobServices* s);
bool HostConfigure(void* context, const RingsSynthTarget* target,
                   const RingsSynthControls* controls, bool entry);
void HostRender(void* context, RingsSynthControls* controls, float* out,
                float* aux, size_t size);

size_t CoreSize();
size_t CoreBlockSize();
// The core holds Part, StringSynthPart, Strummer and Rings' reverb.
bool CoreSizesAddUp();
bool CoreValid(const RingsSynthTarget& target);
const RingsSynthTarget& CoreTarget(void* context);

}  // namespace beads

#endif  // BEADS_TEST_RINGS_CORE_HOST_H_
