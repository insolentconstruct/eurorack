// Copyright 2012 Emilie Gillet.
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
// Digital Modulation: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/digital_modulation_engine.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

using namespace std;
using namespace stmlib;

namespace {

inline float ConstellationI(int dibit) {
  return (dibit == 0 || dibit == 1) ? 1.0f : -1.0f;
}

inline float ConstellationQ(int dibit) {
  return (dibit == 0 || dibit == 3) ? 1.0f : -1.0f;
}

inline float BraidsSine(float phase) {
  return (32639.0f / 32768.0f) * Sine(phase + 0.75f) + (127.0f / 32768.0f);
}

inline int32_t ToBraidsParameter(float normalized) {
  int32_t value = static_cast<int32_t>(normalized * 32767.0f);
  CONSTRAIN(value, 0, 32767);
  return value;
}

}  // namespace

void DigitalModulation::Init(BufferAllocator* allocator) {
  (void) allocator;
  Reset();
}

void DigitalModulation::Reset() {
  phase_ = 0.0f;
  symbol_phase_ = 0.0f;
  payload_filter_ = 0;
  symbol_count_ = 0;
  data_byte_ = 0;
  shaped_i_ = 0.0f;
  shaped_q_ = 0.0f;
  dc_aux_in_ = 0.0f;
  dc_aux_out_ = 0.0f;
}

void DigitalModulation::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  *already_enveloped = false;

  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    symbol_count_ = 0;
  }

  const float carrier_increment = NoteToFrequency(parameters.note);

  const float symbol_note = parameters.note +
      kDigitalModulationSymbolOffset -
      kDigitalModulationSymbolRange * (1.0f - parameters.timbre);
  const float symbol_increment = NoteToFrequency(symbol_note);

  const float frame = kDigitalModulationMinFrame *
      SemitonesToRatio(parameters.harmonics * kDigitalModulationFrameSpan);
  const float frame_scale = frame / kDigitalModulationStockFrame;
  const int preamble_end = static_cast<int>(
      kDigitalModulationPreamble * frame_scale) + 1;
  const int sync_a_end = static_cast<int>(
      kDigitalModulationSyncA * frame_scale) + 1;
  const int sync_b_end = static_cast<int>(
      kDigitalModulationSyncB * frame_scale) + 1;
  const int frame_end = static_cast<int>(frame) + 1;

  // Palette's fourth macro stays at its neutral value.
  const int32_t payload = ToBraidsParameter(0.5f);

  const float shaping = parameters.morph;
  const float shape_pole = shaping * shaping * 0.9995f;

  for (size_t i = 0; i < size; ++i) {
    float carrier_frequency = carrier_increment;
    phase_ += carrier_frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    } else if (phase_ < 0.0f) {
      phase_ += 1.0f;
    }
    symbol_phase_ += symbol_increment;
    if (symbol_phase_ >= 1.0f) {
      symbol_phase_ -= 1.0f;
      ++symbol_count_;
      if (!(symbol_count_ & 3)) {
        if (symbol_count_ >= frame_end) {
          symbol_count_ = 0;
        }
        if (symbol_count_ < preamble_end) {
          data_byte_ = 0x00;
        } else if (symbol_count_ < sync_a_end) {
          data_byte_ = 0x99;
        } else if (symbol_count_ < sync_b_end) {
          data_byte_ = 0xcc;
        } else {
          payload_filter_ = (payload_filter_ * 3 + payload) >> 2;
          data_byte_ = static_cast<uint8_t>(payload_filter_ >> 7);
        }
      } else {
        data_byte_ >>= 2;
      }
    }

    const int dibit = data_byte_ & 3;
    const float target_i = kDigitalModulationRadius * ConstellationI(dibit);
    const float target_q = kDigitalModulationRadius * ConstellationQ(dibit);
    shaped_i_ += (target_i - shaped_i_) * (1.0f - shape_pole);
    shaped_q_ += (target_q - shaped_q_) * (1.0f - shape_pole);

    const float in_phase = BraidsSine(phase_);
    const float quadrature = BraidsSine(phase_ + 0.25f);

    out[i] = shaped_i_ * in_phase + shaped_q_ * quadrature;

    const float staircase = (shaped_i_ + shaped_q_) *
        (1.0f / (2.0f * kDigitalModulationRadius));
    dc_aux_out_ = staircase - dc_aux_in_ +
        kDigitalModulationDcPole * dc_aux_out_;
    dc_aux_in_ = staircase;
    aux[i] = dc_aux_out_;

  }
}

}  // namespace plaits
