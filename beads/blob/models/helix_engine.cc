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
// Helix: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/helix_engine.h"


#include <algorithm>
#include <cmath>
#include <stdint.h>   // not <cstdint>: the firmware is C++98, and <cstdint> needs C++11

#include "stmlib/dsp/units.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

namespace {

// The firmware is bare-metal: libm's sinf/cosf/expf/logf/powf can't link (they
inline float FastLog2(float x) {
  union { float f; uint32_t i; } u = { x };
  const float e = static_cast<float>((u.i >> 23) & 0xFFu) - 127.0f;
  u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;   // mantissa -> [1, 2)
  const float m = u.f;
  // Minimax polynomial for log2(m) on [1,2); ~1e-4, and crucially no divide.
  return e + (-1.7417939f + (2.8212026f + (-1.4699568f
             + (0.44717955f - 0.056570851f * m) * m) * m) * m);
}

// 2^x, branch-free. Valid for the range this engine uses (x > -100): the window
inline float FastExp2(float x) {
  // Round to nearest without a compare: adding 1.5*2^23 forces the fraction out
  const float kRoundMagic = 12582912.0f;
  const float rounded = (x + kRoundMagic) - kRoundMagic;
  const float f = x - rounded;                       // f in [-0.5, 0.5]
  const float y = 1.0f + f * (0.6931472f + f * (0.2402265f
                  + f * (0.0555041f + f * 0.0096181f)));
  union { float f; int32_t i; } p2;
  p2.i = (static_cast<int32_t>(rounded) + 127) << 23;  // 2^rounded
  return y * p2.f;
}

inline float Exp2(float x) { return FastExp2(x); }                  // 2^x
inline float Power(float x, float y) { return FastExp2(y * FastLog2(x)); }

// A voice whose frequency passes half the sample rate folds back as an
const float kNyquistLimit = 0.49f;
const float kNyquistFadeStart = 0.40f;
const float kInvNyquistFade = 1.0f / (kNyquistLimit - kNyquistFadeStart);

}  // namespace

void Helix::Init(stmlib::BufferAllocator* allocator) {
  chords_.Init(allocator);
  Reset();
}

void Helix::Reset() {
  chords_.Reset();
  shift_ = 0.0f;
  for (int i = 0; i < kHelixOctaves * kPaletteChordNumNotes; ++i) {
    osc_x_[i] = 1.0f;  // unit vector at phase 0, so the sine component starts at 0
    osc_y_[i] = 0.0f;
    gain_[i] = 0.0f;
    cos_w_[i] = 1.0f;
    sin_w_[i] = 0.0f;
    pan_l_[i] = 0.0f;
    pan_r_[i] = 0.0f;
  }
  inv_w_ = 1.0f;
  setup_phase_ = 0;
  weight_half_[0] = weight_half_[1] = 0.0f;
}

void Helix::Render(const EngineParameters& parameters, float* out,
    float* aux, size_t size, bool* already_enveloped) {
  // HARMONICS selects the chord — its shape is what rises forever.
  chords_.set_chord(parameters.harmonics, 0);
  chords_.Sort();  // fold each chord note into one octave -> sorted_ratio()

  float pitch_class[kPaletteChordNumNotes];
  for (int n = 0; n < kPaletteChordNumNotes; ++n) {
    pitch_class[n] = FastLog2(chords_.sorted_ratio(n));  // octaves, in [0, 1)
  }
  const float f0 = NoteToFrequency(parameters.note);  // normalized cycles/sample

  // TIMBRE = bipolar glide (0.5 still). Eased; ~1.4 octaves/sec at the extremes.
  const float g = (parameters.timbre - 0.5f) * 2.0f;
  const float shift_inc = g * std::fabs(g) * 3.0e-5f;   // octaves per sample

  // MACRO = window width. Raised cosine (silent at both edges -> seamless wrap),
  const float peak = 1.0f + (1.0f - 0.5f) * 4.0f;
  const float inv_octaves = 1.0f / static_cast<float>(kHelixOctaves);

  const bool stereo = false;


  // PER-BLOCK voice setup. A voice's window gain and frequency depend only on
  const int kVoices = kHelixOctaves * kPaletteChordNumNotes;
  const float shift_mid = shift_ + shift_inc * (static_cast<float>(size) * 0.5f);
  // Refresh HALF the palette's coefficients each block (staggered): every
  const int kHalfVoices = (kHelixOctaves * kPaletteChordNumNotes) / 2;
  {
    const int start = setup_phase_ * kHalfVoices;
    float weight = 0.0f;
    for (int j = start; j < start + kHalfVoices; ++j) {
      const int oct = j / kPaletteChordNumNotes;
      const int n = j - oct * kPaletteChordNumNotes;
      const int idx = j;
      {
        // Per-voice position wrap: the classic Shepard construction. Positions
        float p = static_cast<float>(oct) + shift_mid + pitch_class[n];
        if (p >= static_cast<float>(kHelixOctaves)) {
          p -= static_cast<float>(kHelixOctaves);
        }
        const float inc = f0 * Exp2(p);        // 2^p — this voice's frequency
        float w = 0.0f;
        if (p > 0.0f && p < kHelixOctaves) {
          const float rc = 0.5f - 0.5f * Sine(p * inv_octaves + 0.25f);  // raised cosine
          if (rc > 1.0e-4f) {
      w = Power(rc, peak);
    }
          // Six stacked octaves reach past Nyquist well inside the module's
          w *= std::min(1.0f,
              std::max(0.0f, (kNyquistLimit - inc) * kInvNyquistFade));
        }
        gain_[idx] = w;
        weight += w;
        // Sine(t) is sin(2*pi*t), so the rotation by one sample of a voice at
        sin_w_[idx] = Sine(inc);
        cos_w_[idx] = Sine(inc + 0.25f);
        // Rounding makes the rotated vector drift off the unit circle over
        const float x = osc_x_[idx], y = osc_y_[idx];
        const float renorm = 1.5f - 0.5f * (x * x + y * y);
        osc_x_[idx] = x * renorm;
        osc_y_[idx] = y * renorm;
        if (stereo) {
          // Wrap 0..1 so the cloud rotates. Truncation plus one conditional add
          float pos = p * inv_octaves;
          pos -= static_cast<float>(static_cast<int32_t>(pos));
          if (pos < 0.0f) {
            pos += 1.0f;
          }
          pan_l_[idx] = Sine(pos * 0.25f + 0.25f);  // cos(pos * pi/2)
          pan_r_[idx] = Sine(pos * 0.25f);          // sin(pos * pi/2)
        }
      }
    }
    weight_half_[setup_phase_] = weight;
    inv_w_ = 1.0f
        / std::max(weight_half_[0] + weight_half_[1], 1.0e-3f);
    setup_phase_ ^= 1;
  }
  const float inv_w = inv_w_;

  // MORPH = brightness: a wavefolder that folds harder as MORPH opens. sin(k*s)
  const float fold_depth = 0.25f + parameters.morph * 1.7507f;  // (pi/2 + morph*11) / 2pi

  // Two separate loops rather than one with `if (stereo)` inside: that test ran
  for (size_t i = 0; i < size; ++i) {
    float s0 = 0.0f, s1 = 0.0f, s2 = 0.0f, s3 = 0.0f;
    float l0 = 0.0f, l1 = 0.0f, r0 = 0.0f, r1 = 0.0f;
    if (!stereo) {
      for (int idx = 0; idx < kVoices; idx += 4) {
        const float xa = osc_x_[idx],     ya = osc_y_[idx];
        const float xb = osc_x_[idx + 1], yb = osc_y_[idx + 1];
        const float xc = osc_x_[idx + 2], yc = osc_y_[idx + 2];
        const float xd = osc_x_[idx + 3], yd = osc_y_[idx + 3];
        const float ca = cos_w_[idx],     sa = sin_w_[idx];
        const float cb = cos_w_[idx + 1], sb = sin_w_[idx + 1];
        const float cc = cos_w_[idx + 2], sc = sin_w_[idx + 2];
        const float cd = cos_w_[idx + 3], sd = sin_w_[idx + 3];
        const float na = xa * sa + ya * ca;
        const float nb = xb * sb + yb * cb;
        const float nc = xc * sc + yc * cc;
        const float nd = xd * sd + yd * cd;
        osc_x_[idx]     = xa * ca - ya * sa;  osc_y_[idx]     = na;
        osc_x_[idx + 1] = xb * cb - yb * sb;  osc_y_[idx + 1] = nb;
        osc_x_[idx + 2] = xc * cc - yc * sc;  osc_y_[idx + 2] = nc;
        osc_x_[idx + 3] = xd * cd - yd * sd;  osc_y_[idx + 3] = nd;
        s0 += gain_[idx] * na;
        s1 += gain_[idx + 1] * nb;
        s2 += gain_[idx + 2] * nc;
        s3 += gain_[idx + 3] * nd;
      }
    } else {
      for (int idx = 0; idx < kVoices; idx += 2) {
        const float xa = osc_x_[idx],     ya = osc_y_[idx];
        const float xb = osc_x_[idx + 1], yb = osc_y_[idx + 1];
        const float ca = cos_w_[idx],     sa = sin_w_[idx];
        const float cb = cos_w_[idx + 1], sb = sin_w_[idx + 1];
        const float na = xa * sa + ya * ca;
        const float nb = xb * sb + yb * cb;
        osc_x_[idx]     = xa * ca - ya * sa;  osc_y_[idx]     = na;
        osc_x_[idx + 1] = xb * cb - yb * sb;  osc_y_[idx + 1] = nb;
        const float va = gain_[idx] * na, vb = gain_[idx + 1] * nb;
        s0 += va;             s1 += vb;
        l0 += va * pan_l_[idx]; l1 += vb * pan_l_[idx + 1];
        r0 += va * pan_r_[idx]; r1 += vb * pan_r_[idx + 1];
      }
    }
    const float sum = (s0 + s1) + (s2 + s3);
    const float sum_l = l0 + l1;
    const float sum_r = r0 + r1;

    if (stereo) {
      const float l = sum_l * inv_w, r = sum_r * inv_w;
      const float fold_l = Sine(l * fold_depth + 4.0f);
      const float fold_r = Sine(r * fold_depth + 4.0f);
      out[i] = 0.6f * (l + parameters.morph * (fold_l - l));   // left
      aux[i] = 0.6f * (r + parameters.morph * (fold_r - r));   // right
    } else {
      const float s = sum * inv_w;
      const float folded = Sine(s * fold_depth + 4.0f);
      out[i] = 0.6f * (s + parameters.morph * (folded - s));
      aux[i] = 0.6f * s;
    }
  }

  // Advance the glide. The shift's period is the FULL window span N, not one
  shift_ += shift_inc * static_cast<float>(size);
  if (shift_ >= static_cast<float>(kHelixOctaves)) {
    shift_ -= static_cast<float>(kHelixOctaves);
  } else if (shift_ < 0.0f) {
    shift_ += static_cast<float>(kHelixOctaves);
  }
  *already_enveloped = false;
}

}  // namespace plaits
