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
// Clouds' phase vocoder as a Beads engine: Beads' controls, feedback and
// recording quality around Clouds' spectral processing.

#ifndef BEADS_DSP_SPECTRAL_PROCESSOR_H_
#define BEADS_DSP_SPECTRAL_PROCESSOR_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/sample_rate_converter.h"
#include "stmlib/utils/gate_flags.h"

#include "beads/dsp/delay_line.h"
#include "beads/dsp/frame.h"
#include "beads/dsp/granular_player.h"
#include "beads/dsp/limiter.h"
#include "beads/dsp/modulated_parameter.h"
#include "beads/dsp/pitch_quantizer.h"
#include "beads/dsp/pvoc/phase_vocoder.h"
#include "beads/dsp/pvoc/spectral_parameters.h"
#include "beads/dsp/sample_rate_conversion_filters.h"
#include "beads/dsp/trigger_generator.h"

namespace beads {

// Two of the three recording banks hold the spectral data.
const size_t kSpectralBufferSize = 256 * 1024;
// Codec blocks are at most 24 samples.
const size_t kMaxSpectralBlockSize = 32;

class SpectralProcessor {
 public:
  SpectralProcessor() { }
  ~SpectralProcessor() { }

  void Init(uint8_t* buffers[]);

  // Clears the spectral data and restarts the analysis.
  void Reset(float sample_rate, int ratio, Resolution resolution, bool mono);

  void Process(
      SeedButtonState seed_button_state,
      const stmlib::GateFlags* seed_input,
      const bool* freeze,
      const ModulatedParameter& density,
      const Parameters& parameters,
      float feedback,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size);

  // Main loop: computes the frames queued by Process.
  inline void Buffer() {
    phase_vocoder_.Buffer();
  }

  inline void set_pitch_scale(PitchScale scale) {
    pitch_scale_ = scale;
  }

 private:
  void DrawParameters(
      const ModulatedParameter& density,
      const Parameters& parameters);

  void ProcessInternal(
      float feedback,
      const FloatFrame* in,
      FloatFrame* out,
      size_t size);

  PhaseVocoder phase_vocoder_;
  SpectralParameters parameters_;
  uint8_t* buffers_[2];

  float fft_buffer_[kMaxFftSize];
  float ifft_buffer_[kMaxFftSize];

  int ratio_;
  bool mono_;
  int draw_counter_;
  PitchScale pitch_scale_;

  float feedback_;
  float lp_coefficient_;
  float soft_limit_gain_;
  FloatFrame previous_sample_;
  FloatFrame feedback_buffer_[kMaxSpectralBlockSize];

  stmlib::Svf dc_blocker_[2];
  Limiter input_limiter_;

  stmlib::SampleRateConverter<stmlib::SRC_DOWN, 2, 48> src_down_[2];
  stmlib::SampleRateConverter<stmlib::SRC_UP, 2, 48> src_up_[2];

  DISALLOW_COPY_AND_ASSIGN(SpectralProcessor);
};

}  // namespace beads

#endif  // BEADS_DSP_SPECTRAL_PROCESSOR_H_
