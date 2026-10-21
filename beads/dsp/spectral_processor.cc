// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Clouds' phase vocoder as a Beads engine.

#include "beads/dsp/spectral_processor.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"

#include "beads/resources.h"

namespace beads {

using namespace std;
using namespace stmlib;

const int kSpectralHopSize = kMaxFftSize / 4;

void SpectralProcessor::Init(uint8_t* buffers[]) {
  buffers_[0] = buffers[0];
  buffers_[1] = buffers[1];
  pitch_scale_ = PITCH_SCALE_OFF;
  Reset(48000.0f, 1, RESOLUTION_16_BIT, false);
}

void SpectralProcessor::Reset(
    float sample_rate,
    int ratio,
    Resolution resolution,
    bool mono) {
  ratio_ = ratio;
  mono_ = mono;

  // As on Clouds, the spectral data replaces the recording.
  fill(&buffers_[0][0], &buffers_[0][kSpectralBufferSize], 0);
  fill(&buffers_[1][0], &buffers_[1][kSpectralBufferSize], 0);
  void* buffer[2] = { buffers_[0], buffers_[1] };
  size_t buffer_size[2] = { kSpectralBufferSize, kSpectralBufferSize };
  phase_vocoder_.Init(
      buffer, buffer_size,
      fft_buffer_, ifft_buffer_,
      lut_sine_window_4096, kMaxFftSize,
      mono ? 1 : 2, 16, sample_rate);
  draw_counter_ = 0;
  parameters_.position = 0.0f;
  parameters_.pitch = 0.0f;
  parameters_.freeze = false;
  parameters_.gate = false;
  parameters_.spectral.quantization = 0.5f;
  parameters_.spectral.refresh_rate = 0.5f;
  parameters_.spectral.phase_randomization = 0.0f;
  parameters_.spectral.warp = 0.5f;

  // Feedback as in the granular processor at this resolution.
  const float lp_coefficients[RESOLUTION_LAST] = { 0.75f, 0.9f, 0.999f };
  const float soft_limit_gains[RESOLUTION_LAST] = { 1.75f, 1.3f, 0.0f };
  lp_coefficient_ = lp_coefficients[resolution];
  soft_limit_gain_ = soft_limit_gains[resolution];
  feedback_ = 0.0f;
  previous_sample_.l = previous_sample_.r = 0.0f;
  for (size_t i = 0; i < kMaxSpectralBlockSize; ++i) {
    feedback_buffer_[i].l = feedback_buffer_[i].r = 0.0f;
  }
  for (int i = 0; i < 2; ++i) {
    dc_blocker_[i].Init();
    dc_blocker_[i].set_f_q<FREQUENCY_FAST>(40.0f / sample_rate, 0.5f);
    src_down_[i].Init();
    src_up_[i].Init();
  }
  input_limiter_.Init(1.0f / sample_rate, 0.95f);
}

void SpectralProcessor::Process(
    SeedButtonState seed_button_state,
    const GateFlags* seed_input,
    const bool* freeze,
    const ModulatedParameter& density,
    const Parameters& parameters,
    float feedback,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  bool freeze_block = false;
  bool gate = seed_button_state == SEED_BUTTON_STATE_ON;
  for (size_t i = 0; i < size; ++i) {
    freeze_block |= freeze[i];
    if (seed_input) {
      gate |= seed_input[i] & GATE_FLAG_HIGH;
    }
  }
  parameters_.freeze = freeze_block;
  parameters_.gate = gate;

  // The attenurandomizers draw once per frame.
  const size_t d_size = size / size_t(ratio_);
  draw_counter_ -= int(d_size);
  if (draw_counter_ <= 0) {
    draw_counter_ += kSpectralHopSize;
    DrawParameters(density, parameters);
  }

  if (ratio_ == 1) {
    ProcessInternal(feedback, in, out, size);
    return;
  }

  FloatFrame d_in[kMaxSpectralBlockSize];
  FloatFrame d_out[kMaxSpectralBlockSize];
  float downsampled[kMaxSpectralBlockSize];
  float upsampled[kMaxSpectralBlockSize * 2];
  for (size_t channel = 0; channel < 2; ++channel) {
    for (size_t i = 0; i < size; ++i) {
      upsampled[i] = *(&(in[i].l) + channel);
    }
    src_down_[channel].Process(upsampled, downsampled, size);
    for (size_t i = 0; i < d_size; ++i) {
      *(&(d_in[i].l) + channel) = downsampled[i];
    }
  }

  ProcessInternal(feedback, d_in, d_out, d_size);

  for (size_t channel = 0; channel < 2; ++channel) {
    for (size_t i = 0; i < d_size; ++i) {
      downsampled[i] = *(&(d_out[i].l) + channel);
    }
    src_up_[channel].Process(downsampled, upsampled, d_size);
    for (size_t i = 0; i < size; ++i) {
      *(&(out[i].l) + channel) = upsampled[i];
    }
  }
}

void SpectralProcessor::DrawParameters(
    const ModulatedParameter& density,
    const Parameters& parameters) {
  parameters_.position = parameters.time.value();
  float pitch = parameters.pitch.value(NULL, 0, 24.0f, false);
  if (pitch_scale_ != PITCH_SCALE_OFF) {
    pitch = QuantizePitch(pitch, pitch_scale_);
  }
  parameters_.pitch = pitch;

  // Clouds' spectral mapping: SHAPE stands for TEXTURE.
  parameters_.spectral.quantization = parameters.shape.value();
  const float d = density.value();
  parameters_.spectral.refresh_rate = 0.01f + 0.99f * d;
  float warp = parameters.size.value() - 0.5f;
  parameters_.spectral.warp = 4.0f * warp * warp * warp + 0.5f;

  float randomization = d - 0.5f;
  randomization *= randomization * 4.2f;
  randomization -= 0.05f;
  CONSTRAIN(randomization, 0.0f, 1.0f);
  parameters_.spectral.phase_randomization = randomization;
}

void SpectralProcessor::ProcessInternal(
    float feedback,
    const FloatFrame* in,
    FloatFrame* out,
    size_t size) {
  ParameterInterpolator feedback_modulation(&feedback_, feedback, size);
  const float max_feedback_gain = 3.0f;
  const float lp_coefficient = lp_coefficient_;
  const float soft_limit_gain = soft_limit_gain_;

  // The output comes a block late through the phase vocoder, so feedback
  // uses the previous block.
  FloatFrame s[kMaxSpectralBlockSize];
  for (size_t i = 0; i < size; ++i) {
    const float fb = feedback_modulation.Next();
    ONE_POLE(
        previous_sample_.l,
        dc_blocker_[0].Process<FILTER_MODE_HIGH_PASS>(feedback_buffer_[i].l),
        lp_coefficient);
    ONE_POLE(
        previous_sample_.r,
        dc_blocker_[1].Process<FILTER_MODE_HIGH_PASS>(feedback_buffer_[i].r),
        lp_coefficient);

    FloatFrame input = in[i];
    if (mono_) {
      input.l = input.r = 0.5f * (input.l + input.r);
    }
    FloatFrame x = input;
    if (soft_limit_gain) {
      x += previous_sample_ * fb * (max_feedback_gain / soft_limit_gain);
    } else {
      x += previous_sample_ * fb * fb * max_feedback_gain;
    }
    input_limiter_.Process(&x, 1);
    if (soft_limit_gain) {
      x *= soft_limit_gain;
      x.l = SoftClip(x.l);
      x.r = SoftClip(x.r);
      x = input + fb * (x - input);
    }
    s[i] = x;
  }

  phase_vocoder_.Process(parameters_, s, out, size);
  if (mono_) {
    for (size_t i = 0; i < size; ++i) {
      out[i].r = out[i].l;
    }
  }
  copy(&out[0], &out[size], &feedback_buffer_[0]);
}

}  // namespace beads
