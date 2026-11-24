// Copyright 2026 Lyle Mills.
// Copyright 2026 Insolent Construct.
// SPDX-License-Identifier: MIT
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
//
// NaturalSpeech: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/natural_speech_engine.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "beads/blob/models/natural_speech_data.h"

namespace plaits {

using namespace stmlib;

namespace data = ::natural_speech_data;

namespace bank = ::natural_speech_data;

namespace {

const float kInternalRate = 16000.0f;
const float kRegisterHz = 100.0f;

const int kSmoothDecimation = 4;
const float kSmoothK = 0.03075f;
const float kSmoothV = 0.04088f;
const float kSmoothGain = 0.0206f;
const float kSmoothF0 = 0.00623f;

const int kWhisperOrder = 6;

const float kArticulationCeiling = 4.6f;

const float kPoleDamping = 0.020f;

const float kJitter = 0.006f;
const float kShimmerDb = 0.4f;
const float kFlutterSt = 0.12f;
const float kOutputGain = 0.4f;

inline float DbToAmp(float db) {
  if (db < -90.0f) {
    return 0.0f;
  }
  CONSTRAIN(db, -60.0f, 12.0f);
  return SemitonesToRatio(db * 1.9931569f);
}

inline float TanhLut(float x) {
  float sign = x < 0.0f ? -1.0f : 1.0f;
  float ax = x * sign * data::kTanhLutScale;
  int i = static_cast<int>(ax);
  if (i >= data::kTanhLutSize - 1) {
    return sign * 0.9999f;
  }
  float frac = ax - static_cast<float>(i);
  return sign * (data::kTanhLut[i] +
                 (data::kTanhLut[i + 1] - data::kTanhLut[i]) * frac);
}

inline float Gauss() {
  return (Random::GetFloat() + Random::GetFloat() + Random::GetFloat()
          - 1.5f) * 2.0f;
}

}  // namespace

void NaturalSpeech::Init(stmlib::BufferAllocator* allocator) {
  bank_quantizer_.Init(bank::kNumBanks, 0.1f, false);
  words_in_bank_ = bank::kBankFirstWord[1] - bank::kBankFirstWord[0];
  word_quantizer_.Init(words_in_bank_, 0.1f, false);
  Reset();
}

void NaturalSpeech::Reset() {
  for (int i = 0; i < kOrder; ++i) {
    k_target_[i] = k_[i] = 0.0f;
  }
  for (int b = 0; b < kBands; ++b) {
    v_target_[b] = v_[b] = 0.0f;
  }
  for (int i = 0; i <= kOrder; ++i) {
    lattice_[i] = whisper_lattice_[i] = 0.0f;
  }
  gain_target_ = gain_ = 0.0f;
  f0_st_target_ = f0_st_ = 0.0f;
  voiced_ = false;
  DesignBands();
  smooth_countdown_ = 0;
  for (int b = 0; b < kBands; ++b) {
    UpdateBandWeights(b);
  }
  period_phase_ = 0.0f;
  period_samples_ = kInternalRate / kRegisterHz;
  wavelet_pos_ = 64;
  period_amp_ = 0.0f;
  denormal_guard_ = 1e-15f;
  pole_damp_ = 1.0f;
  jitter_mul_ = 1.0f;
  flutter_phase_[0] = 0.37f;
  flutter_phase_[1] = 0.11f;
  flutter_phase_[2] = 0.79f;
  flutter_countdown_ = 0;
  flutter_value_ = 0.0f;

  prosody_amount_ = 1.0f;
  prosody_now_ = 1.0f;
  speed_bipolar_ = 0.0f;
  bank_ = 0;
  word_ = 0;
  playback_frame_ = -1;
  frame_phase_ = 0.0f;
  word_done_ = true;
  last_decoded_frame_ = -1;
  last_decoded_gamma_ = 1.0f;
  clock_phase_ = 0.0f;
  sample_[0] = sample_[1] = 0.0f;
  next_sample_[0] = next_sample_[1] = 0.0f;
}

void NaturalSpeech::DesignBands() {
  const float cutoffs[kBands - 1] = { 500.0f, 1000.0f, 2000.0f, 4000.0f };

  const float noise_cal[kBands] = { 1.7724f, 1.9106f, 1.3540f, 0.9144f,
                                    0.4861f };
  for (int b = 0; b < kBands; ++b) {
    noise_cal_[b] = noise_cal[b];
  }
  for (int b = 0; b < kBands - 1; ++b) {
    float w = cutoffs[b] / kInternalRate;
    float s = Sine(w);
    float c = Sine(w + 0.25f);
    float alpha = s / 1.414f;
    float a0 = 1.0f + alpha;
    float b0 = (1.0f - c) * 0.5f / a0;
    pulse_lp_[b].Set(b0, (1.0f - c) / a0, b0, -2.0f * c / a0,
                     (1.0f - alpha) / a0);
    noise_lp_[b].Set(b0, (1.0f - c) / a0, b0, -2.0f * c / a0,
                     (1.0f - alpha) / a0);
  }
}

void NaturalSpeech::UpdateBandWeights(int band) {
  float v = v_[band];
  CONSTRAIN(v, 0.0f, 1.0f);
  wp_comp_[band] = 1.0f - Sqrt(v);
  wn_cal_[band] = Sqrt(1.0f - v) * noise_cal_[band];
}

void NaturalSpeech::DecodeFrame(int frame_index, float gamma) {
  uint8_t frame[data::kBytesPerFrame];
  const uint8_t* stored =
      &bank::kBankFrames[frame_index * data::kBytesPerFrame];
  for (int i = 0; i < data::kBytesPerFrame; ++i) {
    frame[i] = stored[i] ^ data::kFrameStorageXor;
  }
  const uint8_t* f = frame;
  gain_target_ = f[0] == 0 ? 0.0f : DbToAmp(0.5f * f[0] - 96.0f);
  f0_st_target_ = 0.25f * static_cast<int8_t>(f[1]);
  v_target_[0] = (f[2] & 15) * 0.066667f;
  v_target_[1] = (f[2] >> 4) * 0.066667f;
  v_target_[2] = (f[3] & 15) * 0.066667f;
  v_target_[3] = (f[3] >> 4) * 0.066667f;
  v_target_[4] = (f[4] & 15) * 0.066667f;
  voiced_ = (f[4] >> 4) & 1;

  const int8_t* lars = reinterpret_cast<const int8_t*>(&f[5]);
  const int8_t* mean = &bank::kBankMeanLar[bank_ * kOrder];
  const float lar_unit = data::kLarMax / 127.0f;
  const float lar_neutral = lar_unit;

  float gain_num = 1.0f;
  float gain_den = 1.0f;
  for (int i = 0; i < kOrder; ++i) {
    float raw = static_cast<float>(lars[i]);
    float recorded = raw * lar_unit;
    float mean_lar = static_cast<float>(mean[i]) * lar_unit;
    float lar = mean_lar + gamma * (recorded - mean_lar);

    float reach = recorded < 0.0f ? -recorded : recorded;
    if (reach < kArticulationCeiling) {
      reach = kArticulationCeiling;
    }
    CONSTRAIN(lar, -reach, reach);
    k_target_[i] = TanhLut(lar * 0.5f);
    float kn = TanhLut(raw * lar_neutral * 0.5f);
    gain_den *= 1.0f - kn * kn;
  }

  for (int i = 0; i < kOrder; ++i) {
    gain_num *= 1.0f - k_target_[i] * k_target_[i];
  }
  float comp = Sqrt(gain_num / (gain_den + 1e-12f));
  CONSTRAIN(comp, 0.06f, 16.0f);
  gain_target_ *= comp;
}

float NaturalSpeech::InternalTick(float f0_phase_inc, float* whisper) {

  // Smooth the tract and voicing weights at one quarter of the internal rate.
  if (--smooth_countdown_ <= 0) {
    smooth_countdown_ = kSmoothDecimation;
    for (int i = 0; i < kOrder; ++i) {
      k_[i] += (k_target_[i] - k_[i]) * kSmoothK;
    }
    for (int b = 0; b < kBands; ++b) {
      v_[b] += (v_target_[b] - v_[b]) * kSmoothV;
      UpdateBandWeights(b);
    }
  }
  gain_ += (gain_target_ - gain_) * kSmoothGain;
  f0_st_ += (f0_st_target_ - f0_st_) * kSmoothF0;

  // Hold the three-sine flutter over sixteen internal ticks.
  if (--flutter_countdown_ <= 0) {
    flutter_countdown_ = 16;
    flutter_phase_[0] += 16.0f * 12.7f / kInternalRate;
    flutter_phase_[1] += 16.0f * 7.1f / kInternalRate;
    flutter_phase_[2] += 16.0f * 4.7f / kInternalRate;
    for (int i = 0; i < 3; ++i) {
      if (flutter_phase_[i] >= 1.0f) {
        flutter_phase_[i] -= 1.0f;
      }
    }
    flutter_value_ = (Sine(flutter_phase_[0]) + Sine(flutter_phase_[1]) +
                      Sine(flutter_phase_[2])) * (kFlutterSt / 3.0f);
  }
  float flutter = flutter_value_;

  float pulse = 0.0f;
  if (voiced_ && gain_target_ > 0.0f) {
    period_phase_ += 1.0f;
    if (period_phase_ >= period_samples_) {
      period_phase_ -= period_samples_;
      jitter_mul_ = 1.0f + kJitter * Gauss();
      float inc = f0_phase_inc *
          SemitonesToRatio(f0_st_ * prosody_now_ + flutter) *
          jitter_mul_;
      CONSTRAIN(inc, 30.0f / kInternalRate, 0.3f);
      period_samples_ = 1.0f / inc;
      wavelet_pos_ = 0;
      float shimmer = SemitonesToRatio(kShimmerDb * Gauss() * 1.9931569f);
      period_amp_ = Sqrt(period_samples_) * shimmer;
    }
  } else {
    period_phase_ = period_samples_;
  }
  if (wavelet_pos_ < 64) {
    pulse = data::kWavelet[wavelet_pos_++] * period_amp_;
  }

  float noise = Gauss();
  float excitation = pulse;
  float p_prev = 0.0f;
  float n_prev = 0.0f;
  for (int b = 0; b < kBands - 1; ++b) {
    const float p_lp = pulse_lp_[b].Process(pulse);
    const float n_lp = noise_lp_[b].Process(noise);
    excitation -= wp_comp_[b] * (p_lp - p_prev);
    excitation += wn_cal_[b] * (n_lp - n_prev);
    p_prev = p_lp;
    n_prev = n_lp;
  }
  excitation -= wp_comp_[kBands - 1] * (pulse - p_prev);
  excitation += wn_cal_[kBands - 1] * (noise - n_prev);

  excitation *= gain_;

  // Add the denormal guard after gain so it survives between words.
  denormal_guard_ = -denormal_guard_;
  excitation += denormal_guard_;

  float f = excitation;
  for (int i = kOrder - 1; i >= 0; --i) {
    f -= k_[i] * lattice_[i];
    lattice_[i + 1] = (lattice_[i] + k_[i] * f) * pole_damp_;
  }

  CONSTRAIN(f, -4.0f, 4.0f);
  lattice_[0] = f;

  float w = (Random::GetFloat() - 0.5f) * 2.0f * gain_ * 0.7f;
  for (int i = kWhisperOrder - 1; i >= 0; --i) {
    w -= k_[i] * whisper_lattice_[i];
    whisper_lattice_[i + 1] = (whisper_lattice_[i] + k_[i] * w) * pole_damp_;
  }
  CONSTRAIN(w, -4.0f, 4.0f);
  whisper_lattice_[0] = w;

  *whisper = w;
  return f;
}

void NaturalSpeech::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const bool free_running = parameters.trigger & TRIGGER_UNPATCHED;
  *already_enveloped = !free_running;

  prosody_now_ = free_running ? 0.0f : prosody_amount_;

  const float rate_ratio = SemitonesToRatio((parameters.timbre - 0.5f) *
                                            10.0f);
  const float rate = rate_ratio * (kInternalRate / kCorrectedSampleRate);

  const float frequency = NoteToFrequency(parameters.note);
  const float pitch_shift =
      frequency / (rate_ratio * kRegisterHz / kCorrectedSampleRate);
  float f0_phase_inc = (kRegisterHz / kInternalRate) * pitch_shift;
  CONSTRAIN(f0_phase_inc, 0.0f, 0.35f);

  const float gamma = 0.5f * 2.0f;
  pole_damp_ = gamma > 1.0f ? 1.0f - (gamma - 1.0f) * kPoleDamping : 1.0f;

  const float speed = SemitonesToRatio(speed_bipolar_ * 24.0f);

  const int bank = bank_quantizer_.Process(parameters.harmonics);
  if (bank != bank_) {
    bank_ = bank;
    words_in_bank_ = bank::kBankFirstWord[bank + 1] -
        bank::kBankFirstWord[bank];
    if (words_in_bank_ < 1) {
      words_in_bank_ = 1;
    }
    word_quantizer_.Init(words_in_bank_, 0.1f, false);
  }
  const int word = bank::kBankFirstWord[bank] +
      word_quantizer_.Process(parameters.morph);
  const int word_start = bank::kWordBoundaries[word];

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    word_ = word;
    playback_frame_ = word_start;
    frame_phase_ = 0.0f;
    word_done_ = false;
    DecodeFrame(playback_frame_, gamma);
    last_decoded_frame_ = playback_frame_;
    last_decoded_gamma_ = gamma;
  }

  if (free_running) {

    const int bank_first = bank::kWordBoundaries[bank::kBankFirstWord[bank]];
    const int bank_last =
        bank::kWordBoundaries[bank::kBankFirstWord[bank + 1]];
    int span = bank_last - bank_first - 1;
    if (span < 0) {
      span = 0;
    }
    const int frame = bank_first +
        static_cast<int>(parameters.morph * static_cast<float>(span));
    const float dg = gamma - last_decoded_gamma_;
    if (frame != last_decoded_frame_ || dg > 0.005f || dg < -0.005f) {
      DecodeFrame(frame, gamma);
      last_decoded_frame_ = frame;
      last_decoded_gamma_ = gamma;
    }
    word_ = word;
    word_done_ = true;
  }

  const float frame_inc =
      data::kFrameRateHz * speed / (kInternalRate * rate_ratio);

  for (size_t n = 0; n < size; ++n) {
    clock_phase_ += rate;
    if (clock_phase_ >= 1.0f) {
      clock_phase_ -= 1.0f;
      sample_[0] = next_sample_[0];
      sample_[1] = next_sample_[1];

      if (!word_done_) {
        frame_phase_ += frame_inc;
        while (frame_phase_ >= 1.0f) {
          frame_phase_ -= 1.0f;
          ++playback_frame_;
          if (playback_frame_ >= bank::kWordBoundaries[word_ + 1]) {
            if (free_running) {

              playback_frame_ = bank::kWordBoundaries[word_];
            } else {
              word_done_ = true;
              gain_target_ = 0.0f;
              break;
            }
          }
          DecodeFrame(playback_frame_, gamma);
          last_decoded_frame_ = playback_frame_;
          last_decoded_gamma_ = gamma;
        }
      }

      float whisper = 0.0f;
      next_sample_[0] = InternalTick(f0_phase_inc, &whisper
          ) * kOutputGain;
      next_sample_[1] = whisper * kOutputGain;
    }
    out[n] = sample_[0] + (next_sample_[0] - sample_[0]) * clock_phase_;
    aux[n] = sample_[1] + (next_sample_[1] - sample_[1]) * clock_phase_;
  }

}

}  // namespace plaits
