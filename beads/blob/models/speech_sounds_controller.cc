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

#include "beads/blob/models/speech_sounds_controller.h"

#include <algorithm>

#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"
#include "plaits/dsp/oscillator/oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void SpeechSoundsController::Init() {

  clock_phase_ = 0.0f;
  playback_frame_ = -1;
  last_playback_frame_ = -1;
  remaining_frame_samples_ = 0;

  fill(&sample_[0], &sample_[2], 0.0f);
  fill(&next_sample_[0], &next_sample_[2], 0.0f);

  gain_ = 0.0f;

  synth_.Init();
}

void SpeechSoundsController::Render(
    bool free_running,
    bool trigger,
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
      (rate_ratio * kSpeechSoundsLPCDefaultF0 / kCorrectedSampleRate);
  const float time_stretch = SemitonesToRatio(-speed * 24.0f +
        (formant_shift < 0.4f ? (formant_shift - 0.4f) * -45.0f
            : (formant_shift > 0.6f ? (formant_shift - 0.6f) * -45.0f : 0.0f)));

  const int num_frames = kSpeechSoundsNumVowels;
  const SpeechSoundsLPC::Frame* frames = phonemes_;

  if (trigger) {
    // Pick a consonant and play it for one frame.
    int r = (address + 3.0f * formant_shift + 7.0f * frequency) * 8.0f;
    playback_frame_ = (r % kSpeechSoundsNumConsonants);
    playback_frame_ += kSpeechSoundsNumVowels;
    last_playback_frame_ = playback_frame_ + 1;
    remaining_frame_samples_ = 0;
  }

  if (playback_frame_ == -1 && remaining_frame_samples_ == 0) {
    synth_.PlayFrame(
        frames,
        address * (static_cast<float>(num_frames) - 1.0001f),
        true);
  } else {
    if (remaining_frame_samples_ == 0) {
      synth_.PlayFrame(frames, float(playback_frame_), false);
      remaining_frame_samples_ = kSampleRate / kSpeechSoundsFPS * \
          time_stretch;
      ++playback_frame_;
      if (playback_frame_ >= last_playback_frame_) {
        playback_frame_ = -1;
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
