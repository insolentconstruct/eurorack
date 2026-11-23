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
// Fades, blanking and codec restarts between engines, synths and styles.

#include "beads/engine_transition.h"

namespace beads {

/* static */
int EngineTransition::codec_transition_frames_;

/* static */
bool EngineTransition::clouds_style_;

/* static */
float EngineTransition::reverb_style_gain_;

/* static */
volatile bool EngineTransition::codec_fade_;

/* static */
bool EngineTransition::codec_running_;

/* static */
int EngineTransition::codec_sample_rate_;

/* static */
float EngineTransition::reverb_style_previous_gain_;

/* static */
SynthEngine EngineTransition::playing_synth_;

/* static */
SynthEngine EngineTransition::bank_owner_;

}  // namespace beads
