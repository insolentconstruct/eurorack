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

#include "beads/ui.h"

namespace beads {

void EngineTransition::Init(Codec* codec, PlaitsSynth* plaits_synth,
    RingsSynth* rings_synth, Ui* ui, uint8_t* bank, size_t bank_size) {
  codec_ = codec;
  plaits_synth_ = plaits_synth;
  rings_synth_ = rings_synth;
  ui_ = ui;
  bank_ = bank;
  bank_size_ = bank_size;
  bank_owner_ = SYNTH_SILENT;
  clouds_style_ = false;
  reverb_style_gain_ = 1.0f;
  reverb_style_previous_gain_ = 1.0f;
  codec_fade_ = false;
  codec_running_ = false;
  codec_sample_rate_ = 0;
}

void EngineTransition::Start(
    int sample_rate, Codec::FillBufferCallback callback) {
  codec_transition_frames_ = kBlockRate / 4;
  codec_fade_ = false;
  if (codec_running_ && sample_rate == codec_sample_rate_) {
    // Same clocks: the converter played silence while paused.
    codec_->Resume(callback);
    return;
  }
  if (codec_sample_rate_) {
    codec_->Restart(
        codec_sample_rate_, sample_rate, sample_rate / kBlockRate, callback);
  } else {
    codec_->Start(sample_rate, sample_rate / kBlockRate, callback);
  }
  codec_running_ = true;
  codec_sample_rate_ = sample_rate;
}

void EngineTransition::Pause() {
  codec_->Pause();
}

// Codec paused: one synth at a time owns delay bank 0, built from zero.
void EngineTransition::TakeBank(SynthEngine owner) {
  if (owner == SYNTH_RINGS) {
#ifdef PLAITS_BLOBS
    rings_synth_->InitVoices(bank_, bank_size_, &plaits_blob_services,
        ui_->mix_parameter(Ui::MIX_PARAMETER_FEEDBACK));
#else
    rings_synth_->InitVoices(bank_, bank_size_, NULL,
        ui_->mix_parameter(Ui::MIX_PARAMETER_FEEDBACK));
#endif  // PLAITS_BLOBS
  } else {
    plaits_synth_->InitVoices(bank_);
  }
  bank_owner_ = owner;
}

// Swap bank owner, or configure Rings, only after the output has faded out.
void EngineTransition::SwapPaused(SynthEngine owner, SynthEngine playing,
    Codec::FillBufferCallback callback) {
  if (codec_running_) {
    codec_->Pause();
  }
  if (owner == SYNTH_SILENT) {
    rings_synth_->Configure();
  } else {
    TakeBank(owner);
  }
  playing_synth_ = playing;
  ui_->set_playing_synth(playing);
  if (codec_running_) {
    codec_->Resume(callback);
  }
  codec_fade_ = false;
}

}  // namespace beads
