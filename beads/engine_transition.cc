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
#include "stmlib/system/system_clock.h"

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
  reverb_character_ = REVERB_STOCK;
  reverb_style_gain_ = 1.0f;
  reverb_style_previous_gain_ = 1.0f;
  codec_fade_ = false;
  codec_running_ = false;
  codec_sample_rate_ = 0;
  codec_retry_rate_ = 0;
}

void EngineTransition::Start(
    int sample_rate, Codec::FillBufferCallback callback, bool blank_output) {
  codec_transition_frames_ = blank_output ? kBlockRate / 4 : 0;
  codec_fade_ = false;
  if (codec_running_ && sample_rate == codec_sample_rate_) {
    // Same clocks: the converter played silence while paused.
    codec_->Resume(callback);
    return;
  }
  StartCodec(sample_rate, callback);
}

void EngineTransition::StartCodec(
    int sample_rate, Codec::FillBufferCallback callback) {
  const size_t block_size = sample_rate / kBlockRate;
  codec_running_ = codec_sample_rate_ || codec_retry_rate_
      ? codec_->Restart(codec_sample_rate_, sample_rate, block_size, callback)
      : codec_->Start(sample_rate, block_size, callback);
  codec_sample_rate_ = codec_running_ ? sample_rate : 0;
  codec_retry_rate_ = codec_running_ ? 0 : sample_rate;
  if (!codec_running_) {
    codec_retry_callback_ = callback;
    codec_retry_time_ = stmlib::system_clock.milliseconds();
  }
}

bool EngineTransition::RetryCodec() {
  if (!codec_retry_rate_) {
    return false;
  }
  if (stmlib::system_clock.milliseconds() - codec_retry_time_ >= 100) {
    StartCodec(codec_retry_rate_, codec_retry_callback_);
  }
  return true;
}

void EngineTransition::Pause() {
  codec_->Pause();
}

void EngineTransition::SetReverbCharacter(
    uint8_t character, Processor* processor, Synth* synth) {
  reverb_character_ = character;
  clouds_style_ = character == REVERB_CLOUDS;
  processor->SetReverbCharacter(character);
  synth->SetReverbCharacter(character, false);
  plaits_synth_->SetReverbCharacter(character, false);
  rings_synth_->SetReverbCharacter(character, false);
}

void EngineTransition::Resume(Codec::FillBufferCallback callback) {
  codec_fade_ = false;
  if (codec_running_) {
    codec_->Resume(callback);
  }
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
  codec_fade_ = false;
  if (codec_running_) {
    codec_->Resume(callback);
  }
}

}  // namespace beads
