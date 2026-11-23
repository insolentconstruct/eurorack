// Copyright 2016 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Feeds frames to the LPC10 speech synth.

#ifndef BEADS_BLOB_MODELS_LPC_WORDS_CONTROLLER_H_
#define BEADS_BLOB_MODELS_LPC_WORDS_CONTROLLER_H_

#include "beads/blob/models/lpc_words_synth.h"
#include "plaits/dsp/speech/lpc_speech_synth_controller.h"

namespace plaits {

class LPCWordsController {
 public:
  LPCWordsController() { }
  ~LPCWordsController() { }

  void Init(LPCSpeechSynthWordBank* word_bank);

  void Render(
      bool free_running,
      bool trigger,
      int bank,
      float frequency,
      float prosody_amount,
      float speed,
      float address,
      float formant_shift,
      float gain,
      float* excitation,
      float* output,
      size_t size
      );

 private:
  float clock_phase_;
  float sample_[2];
  float next_sample_[2];
  float gain_;
  LPCWordsSynth synth_;

  int playback_frame_;
  int last_playback_frame_;
  size_t remaining_frame_samples_;

  LPCSpeechSynthWordBank* word_bank_;


  DISALLOW_COPY_AND_ASSIGN(LPCWordsController);
};


}  // namespace plaits

#endif  // BEADS_BLOB_MODELS_LPC_WORDS_CONTROLLER_H_
