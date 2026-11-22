// Copyright 2016 Emilie Gillet.
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

#include "plaits/dsp/speech/lpc_speech_synth_controller.h"

#include <algorithm>

#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "plaits/dsp/oscillator/oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

// Bytes with their bits in reverse order; faster than three swaps on the M7.
/* static */
const uint8_t BitStream::reverse_lut_[256] = {
  0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
  0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
  0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
  0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
  0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
  0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
  0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
  0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
  0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
  0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
  0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
  0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
  0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
  0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
  0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
  0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
  0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
  0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
  0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
  0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
  0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
  0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
  0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
  0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
  0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
  0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
  0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
  0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
  0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
  0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
  0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
  0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#ifndef PLAITS_BLOB
/* static */
const LPCSpeechSynthWordBank* LPCSpeechSynthWordBank::decoded_banks_ = NULL;
#endif  // PLAITS_BLOB

/* static */
const uint8_t LPCSpeechSynthWordBank::energy_lut_[16] = {
  0x00, 0x02, 0x03, 0x04, 0x05, 0x07, 0x0a, 0x0f,
  0x14, 0x20, 0x29, 0x39, 0x51, 0x72, 0xa1, 0xff
};

/* static */
const uint8_t LPCSpeechSynthWordBank::period_lut_[64] = {
  0, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45, 47, 49, 51, 53,
 54, 57, 59, 61, 63, 66, 69, 71, 73, 77, 79, 81, 85, 87, 92, 95, 99,
 102, 106, 110, 115, 119, 123, 128, 133, 138, 143, 149, 154, 160
};

/* static */
const int16_t LPCSpeechSynthWordBank::k0_lut_[32] = {
  -32064, -31872, -31808, -31680, -31552, -31424, -31232, -30848,
  -30592, -30336, -30016, -29696, -29376, -28928, -28480, -27968,
  -26368, -24256, -21632, -18368, -14528, -10048,  -5184,      0,
    5184,  10048,  14528,  18368,  21632,  24256,  26368,  27968
};

/* static */
const int16_t LPCSpeechSynthWordBank::k1_lut_[32] = {
  -20992, -19328, -17536, -15552, -13440, -11200,  -8768,  -6272,
  -3712,   -1088,   1536,   4160,   6720,   9216,  11584,  13824,
  15936,   17856,  19648,  21248,  22656,  24000,  25152,  26176,
  27072,   27840,  28544,  29120,  29632,  30080,  30464,  32384
};

/* static */
const int8_t LPCSpeechSynthWordBank::k2_lut_[16] = {
-110, -97, -83, -70, -56, -43, -29, -16, -2, 11, 25, 38, 52, 65, 79, 92
};

/* static */
const int8_t LPCSpeechSynthWordBank::k3_lut_[16] = {
-82, -68, -54, -40, -26, -12, 1, 15, 29, 43, 57, 71, 85, 99, 113, 126
};

/* static */
const int8_t LPCSpeechSynthWordBank::k4_lut_[16] = {
 -82, -70, -59, -47, -35, -24, -12, -1, 11, 23, 34, 46, 57, 69, 81, 92
};

/* static */
const int8_t LPCSpeechSynthWordBank::k5_lut_[16] = {
  -64, -53, -42, -31, -20, -9, 3, 14, 25, 36, 47, 58, 69, 80, 91, 102
};

/* static */
const int8_t LPCSpeechSynthWordBank::k6_lut_[16] = {
  -77, -65, -53, -41, -29, -17, -5, 7, 19, 31, 43, 55, 67, 79, 90, 102
};

/* static */
const int8_t LPCSpeechSynthWordBank::k7_lut_[8] = {
-64, -40, -16, 7, 31, 55, 79, 102
};

/* static */
const int8_t LPCSpeechSynthWordBank::k8_lut_[8] = {
  -64, -44, -24, -4, 16, 37, 57, 77
};

/* static */
const int8_t LPCSpeechSynthWordBank::k9_lut_[8] = {
  -51, -33, -15, 4, 22, 32, 59, 77
};

void LPCSpeechSynthWordBank::Init(
    const LPCSpeechSynthWordBankData* word_banks,
    int num_banks,
    BufferAllocator* allocator) {
  word_banks_ = word_banks;
  num_banks_ = num_banks;
  frames_ = allocator->Allocate<LPCSpeechSynth::Frame>(
      kLPCSpeechSynthMaxFrames);
  word_boundaries_ = allocator->Allocate<int>(kLPCSpeechSynthMaxWords);
  Reset();
}

void LPCSpeechSynthWordBank::Reset() {
  loaded_bank_ = -1;
  loading_bank_ = -1;
  num_frames_ = 0;
  num_words_ = 0;
  fill(
      &word_boundaries_[0],
      &word_boundaries_[kLPCSpeechSynthMaxWords], 0);
}

void LPCSpeechSynthWordBank::StartWord() {
  frame_.energy = 0;
  frame_.period = 0;
  frame_.k0 = 0;
  frame_.k1 = 0;
  frame_.k2 = 0;
  frame_.k3 = 0;
  frame_.k4 = 0;
  frame_.k5 = 0;
  frame_.k6 = 0;
  frame_.k7 = 0;
  frame_.k8 = 0;
  frame_.k9 = 0;
}

bool LPCSpeechSynthWordBank::LoadNextFrame() {
  int energy = bitstream_.GetBits(4);
  if (energy == 0) {
    frame_.energy = 0;
  } else if (energy == 0xf) {
    bitstream_.Flush();
    return false;
  } else {
    frame_.energy = energy_lut_[energy];
    bool repeat = bitstream_.GetBits(1);
    frame_.period = period_lut_[bitstream_.GetBits(6)];
    if (!repeat) {
      frame_.k0 = k0_lut_[bitstream_.GetBits(5)];
      frame_.k1 = k1_lut_[bitstream_.GetBits(5)];
      frame_.k2 = k2_lut_[bitstream_.GetBits(4)];
      frame_.k3 = k3_lut_[bitstream_.GetBits(4)];
      if (frame_.period) {
        frame_.k4 = k4_lut_[bitstream_.GetBits(4)];
        frame_.k5 = k5_lut_[bitstream_.GetBits(4)];
        frame_.k6 = k6_lut_[bitstream_.GetBits(4)];
        frame_.k7 = k7_lut_[bitstream_.GetBits(3)];
        frame_.k8 = k8_lut_[bitstream_.GetBits(3)];
        frame_.k9 = k9_lut_[bitstream_.GetBits(3)];
      }
    }
  }
  frames_[num_frames_++] = frame_;
  return true;
}

bool LPCSpeechSynthWordBank::Load(int bank) {
  if (bank == loaded_bank_ || bank >= num_banks_) {
    return false;
  }

  if (bank != loading_bank_) {
    loaded_bank_ = -1;
    loading_bank_ = bank;
    num_frames_ = 0;
    num_words_ = 0;
    word_boundaries_[0] = 0;
    bitstream_.Init(word_banks_[bank].data);
    end_ = word_banks_[bank].data + word_banks_[bank].size;
    StartWord();
  }
  
  if (decoded_banks_) {
    // As many calls as decoding takes: words ending within the next frames.
    const LPCSpeechSynthWordBank& decoded = decoded_banks_[bank];
    const int end = num_frames_ + kLPCSpeechSynthFramesPerLoad;
    while (num_words_ < decoded.num_words_ &&
           decoded.word_boundaries_[num_words_ + 1] < end) {
      ++num_words_;
      word_boundaries_[num_words_] = decoded.word_boundaries_[num_words_];
    }
    const int num_frames = min(end, decoded.num_frames_);
    copy(
        &decoded.frames_[num_frames_],
        &decoded.frames_[num_frames],
        &frames_[num_frames_]);
    num_frames_ = num_frames;
    if (num_words_ == decoded.num_words_) {
      loaded_bank_ = bank;
      return true;
    }
    return false;
  }

  int budget = kLPCSpeechSynthFramesPerLoad;
  while (budget) {
    if (LoadNextFrame()) {
      --budget;
    } else {
      word_boundaries_[++num_words_] = num_frames_;
      if (bitstream_.ptr() >= end_) {
        loaded_bank_ = bank;
        return true;
      }
      StartWord();
    }
  }
  return false;
}

void LPCSpeechSynthWordBank::Decode(int bank, BufferAllocator* allocator) {
  Reset();
  while (!Load(bank)) { }
  // An even number of 14-byte frames keeps the word boundaries aligned.
  LPCSpeechSynth::Frame* frames = \
      allocator->Allocate<LPCSpeechSynth::Frame>((num_frames_ + 1) & ~1);
  int* word_boundaries = allocator->Allocate<int>(num_words_ + 1);
  copy(&frames_[0], &frames_[num_frames_], &frames[0]);
  copy(
      &word_boundaries_[0],
      &word_boundaries_[num_words_ + 1],
      &word_boundaries[0]);
  frames_ = frames;
  word_boundaries_ = word_boundaries;
}

void LPCSpeechSynthController::Init(LPCSpeechSynthWordBank* word_bank) {
  word_bank_ = word_bank;
  
  clock_phase_ = 0.0f;
  playback_frame_ = -1;
  last_playback_frame_ = -1;
  remaining_frame_samples_ = 0;
  pending_trigger_ = false;
  playback_bank_ = -1;

  fill(&sample_[0], &sample_[2], 0.0f);
  fill(&next_sample_[0], &next_sample_[2], 0.0f);

  gain_ = 0.0f;
  
  synth_.Init();
}

void LPCSpeechSynthController::Render(
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
    size_t size) {
  const float rate_ratio = SemitonesToRatio((formant_shift - 0.5f) * 36.0f);
  const float rate = rate_ratio / 6.0f;
  
  // All utterances have been normalized for an average f0 of 100 Hz.
  const float pitch_shift = frequency / \
      (rate_ratio * kLPCSpeechSynthDefaultF0 / kCorrectedSampleRate);
  const float time_stretch = SemitonesToRatio(-speed * 24.0f +
        (formant_shift < 0.4f ? (formant_shift - 0.4f) * -45.0f
            : (formant_shift > 0.6f ? (formant_shift - 0.6f) * -45.0f : 0.0f)));
  
  // A trigger waits only for the bank it came with.
  if (bank == -1 || bank != word_bank_->loading_bank()) {
    pending_trigger_ = false;
  }

  if (bank != -1) {
    bool reset_everything = word_bank_->Load(bank);
    if (reset_everything) {
      playback_frame_ = -1;
      last_playback_frame_ = -1;
    }
  }
  
  // While a word bank loads, hold the current frame and delay any trigger.
  const bool loading = bank != -1 && word_bank_->loading();
  if (loading) {
    pending_trigger_ = pending_trigger_ || trigger;
    trigger = false;
  } else {
    trigger = trigger || pending_trigger_;
    pending_trigger_ = false;
  }

  const int num_frames = bank == -1
      ? kLPCSpeechSynthNumVowels
      : word_bank_->num_frames();

  const LPCSpeechSynth::Frame* frames = bank == -1
      ? phonemes_
      : word_bank_->frames();
  
  if (trigger) {
    if (bank == -1) {
      // Pick a pseudo-random consonant, and play it for the duration of a
      // frame.
      int r = (address + 3.0f * formant_shift + 7.0f * frequency) * 8.0f;
      playback_frame_ = (r % kLPCSpeechSynthNumConsonants);
      playback_frame_ += kLPCSpeechSynthNumVowels;
      last_playback_frame_ = playback_frame_ + 1;
    } else {
      word_bank_->GetWordBoundaries(
          address,
          &playback_frame_,
          &last_playback_frame_);
    }
    playback_bank_ = bank;
    remaining_frame_samples_ = 0;
  }
  
  // A word's frame numbers do not index the phonemes: scan them instead.
  if (bank == -1 && playback_bank_ != -1 && remaining_frame_samples_ == 0) {
    playback_frame_ = -1;
  }
  
  if (loading) {
    // Keep playing the current frame.
  } else if (playback_frame_ == -1 && remaining_frame_samples_ == 0) {
    synth_.PlayFrame(
        frames,
        address * (static_cast<float>(num_frames) - 1.0001f),
        true);
  } else {
    if (remaining_frame_samples_ == 0) {
      synth_.PlayFrame(frames, float(playback_frame_), false);
      remaining_frame_samples_ = kSampleRate / kLPCSpeechSynthFPS * \
          time_stretch;
      ++playback_frame_;
      if (playback_frame_ >= last_playback_frame_) {
        bool back_to_scan_mode = bank == -1 || free_running;
        playback_frame_ = back_to_scan_mode ? -1 : last_playback_frame_;
      }
    }
    remaining_frame_samples_ -= min(size, remaining_frame_samples_);
  }
  
  ParameterInterpolator gain_modulation(&gain_, gain, size);
  
  while (size--) {
    float this_sample[2];
    copy(&next_sample_[0], &next_sample_[2], &this_sample[0]);
    fill(&next_sample_[0], &next_sample_[2], 0.0f);
    
    clock_phase_ += rate;
    if (clock_phase_ >= 1.0f) {
      clock_phase_ -= 1.0f;
      float reset_time = clock_phase_ / rate;
      float new_sample[2];
      
      synth_.Render(
          prosody_amount,
          pitch_shift,
          &new_sample[0],
          &new_sample[1], 1);
      
      float discontinuity[2] = {
        new_sample[0] - sample_[0],
        new_sample[1] - sample_[1]
      };
      this_sample[0] += discontinuity[0] * ThisBlepSample(reset_time);
      next_sample_[0] += discontinuity[0] * NextBlepSample(reset_time);
      this_sample[1] += discontinuity[1] * ThisBlepSample(reset_time);
      next_sample_[1] += discontinuity[1] * NextBlepSample(reset_time);
      copy(&new_sample[0], &new_sample[2], &sample_[0]);
    }
    next_sample_[0] += sample_[0];
    next_sample_[1] += sample_[1];
    const float gain = gain_modulation.Next();
    *excitation++ = this_sample[0] * gain;
    *output++ = this_sample[1] * gain;
  }
}

}  // namespace plaits
