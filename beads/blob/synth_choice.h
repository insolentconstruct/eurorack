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
// Which synth plays: the saved selection (feature 7) against the synths the
// region can play, before any blob code runs (ABI.md, faults).

#ifndef BEADS_BLOB_SYNTH_CHOICE_H_
#define BEADS_BLOB_SYNTH_CHOICE_H_

#include "stmlib/stmlib.h"

namespace beads {

// Feature 7's values; their meanings never change.
enum SynthEngine {
  SYNTH_BEADS,
  SYNTH_PLAITS,
  SYNTH_RINGS,
  SYNTH_SILENT,
  SYNTH_LAST = SYNTH_SILENT
};

// The playable synths as bits: 1 << SYNTH_BEADS and so on.
inline uint8_t SynthMask(bool beads, bool plaits, bool rings) {
  return (beads ? 1 << SYNTH_BEADS : 0) | (plaits ? 1 << SYNTH_PLAITS : 0) |
      (rings ? 1 << SYNTH_RINGS : 0);
}

// The selection if playable; otherwise Beads' synth, else Plaits, else
// silence. A fallback never picks Rings: its knobs mean other things.
inline SynthEngine EffectiveSynth(uint8_t selected, uint8_t playable) {
  if (selected < SYNTH_LAST && (playable & (1 << selected))) {
    return SynthEngine(selected);
  } else if (playable & (1 << SYNTH_BEADS)) {
    return SYNTH_BEADS;
  } else if (playable & (1 << SYNTH_PLAITS)) {
    return SYNTH_PLAITS;
  }
  return SYNTH_SILENT;
}

// The next playable synth after value, in value order (actions and pages).
inline uint8_t NextSynth(uint8_t value, uint8_t playable) {
  for (int i = 1; i <= SYNTH_LAST; ++i) {
    const uint8_t next = (value + i) % SYNTH_LAST;
    if (playable & (1 << next)) {
      return next;
    }
  }
  return value;
}

}  // namespace beads

#endif  // BEADS_BLOB_SYNTH_CHOICE_H_
