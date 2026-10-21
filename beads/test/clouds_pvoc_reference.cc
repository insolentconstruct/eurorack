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
// Clouds' phase vocoder as Clouds ships it (ShyFFT), for comparison with the
// Beads port. The Clouds sources are compiled here unchanged.

#undef USE_ARM_FFT

#include "beads/test/clouds_pvoc_reference.h"

#include <cstring>

#include "clouds/dsp/parameters.h"
#include "clouds/dsp/pvoc/frame_transformation.cc"
#include "clouds/dsp/pvoc/phase_vocoder.cc"
#include "clouds/dsp/pvoc/stft.cc"
#include "clouds/resources.cc"

namespace clouds_reference {

static uint8_t buffers[2][256 * 1024];
static clouds::PhaseVocoder phase_vocoder;
static clouds::Parameters parameters;

void Init(int num_channels) {
  memset(buffers, 0, sizeof(buffers));
  memset(&parameters, 0, sizeof(parameters));
  void* buffer[2] = { buffers[0], buffers[1] };
  size_t buffer_size[2] = { sizeof(buffers[0]), sizeof(buffers[1]) };
  phase_vocoder.Init(
      buffer, buffer_size,
      clouds::lut_sine_window_4096, 4096,
      num_channels, 16, 48000.0f);
}

void Process(const Settings& settings, const float* in, float* out,
             size_t size) {
  parameters.position = settings.position;
  parameters.pitch = settings.pitch;
  parameters.freeze = settings.freeze;
  parameters.gate = settings.gate;
  parameters.spectral.quantization = settings.quantization;
  parameters.spectral.refresh_rate = settings.refresh_rate;
  parameters.spectral.phase_randomization = settings.phase_randomization;
  parameters.spectral.warp = settings.warp;
  phase_vocoder.Process(
      parameters,
      reinterpret_cast<const clouds::FloatFrame*>(in),
      reinterpret_cast<clouds::FloatFrame*>(out),
      size);
}

void Buffer() {
  phase_vocoder.Buffer();
}

}  // namespace clouds_reference
