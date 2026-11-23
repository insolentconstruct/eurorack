// Copyright 2026 Dirk Hoppmann.
// Copyright 2016 Emilie Gillet.
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
// ChordsSupersaw: Palette 0390557b, mono, no FM, fourth macro 0.5.

#include "beads/blob/models/chords_supersaw_engine.h"

#include <algorithm>

#include "plaits/dsp/dsp.h"
#include "plaits/resources.h"

namespace plaits {

using namespace std;
using namespace stmlib;

// MORPH at full travel spreads neighbouring unison ranks this far apart, in
const float kSupersawMaxDetune = 0.35f;

// Outer ranks are pulled down slightly so the stack keeps a centre of pitch
const float kSupersawRankTilt = 0.12f;

// The waveform is fixed rather than on a control. The sine and square paths
const SupersawShape kSupersawFixedShape = SUPERSAW_SHAPE_SAW;
const float kSupersawFixedShapeAmount = 0.0f;

// Which detune positions a stack of 1, 2 or 3 uses. One oscillator plays the
const int kStackPositions[kSupersawUnison + 1][kSupersawUnison] = {
  { 1, 1, 1 },
  { 1, 0, 0 },
  { 0, 2, 0 },
  { 0, 1, 2 },
};

// TIMBRE drives the mix into stmlib's soft limiter.
const float kSupersawMaxDrive = 5.0f;
const int kSupersawMakeupPoints = 9;
const float kSupersawMakeup[kSupersawMakeupPoints] = {
  1.0000f, 0.7026f, 0.5539f, 0.4746f, 0.4264f,
  0.3944f, 0.3718f, 0.3552f, 0.3425f
};



const float kSupersawLevel = 0.2f;

// Where each oscillator of a chord tone sits in the spread, as a fraction of
const float kDetuneSpread[kPaletteChordNumNotes][4] = {
  { -0.62f, 0.0f, 0.45f, 0.88f },
  { -0.85f, 0.0f, 1.00f, -0.46f },
  { -1.00f, 0.0f, 0.62f, 0.31f },
  { -0.72f, 0.0f, 0.94f, -0.35f },
};


// 1 / sqrt(n) for n = 0..7, spelled out so the engine needs no math header.
const float kInverseRoot[8] = {
  0.0f, 1.0f, 0.70710678f, 0.57735027f,
  0.5f, 0.4472136f, 0.40824829f, 0.37796447f
};

void ChordsSupersaw::Init(BufferAllocator* allocator) {
  for (int note = 0; note < kPaletteChordNumNotes; ++note) {
    for (int rank = 0; rank < kSupersawUnison; ++rank) {
      voice_[note][rank].Init();
    }
  }
  chords_.Init(allocator);

  morph_lp_ = 0.0f;
  timbre_lp_ = 0.0f;
  active_unison_ = kSupersawUnison;

  ScatterPhases();
}

void ChordsSupersaw::Reset() {
  chords_.Reset();
}

// Detuned saws that all start at phase zero sum into one loud impulse and only
const float kScatter[kPaletteChordNumNotes][kSupersawUnison] = {
  { 0.00f, 0.41f, 0.77f }, { 0.23f, 0.62f, 0.09f },
  { 0.53f, 0.88f, 0.31f }, { 0.71f, 0.17f, 0.46f },
};

void ChordsSupersaw::ScatterPhases() {
  for (int note = 0; note < kPaletteChordNumNotes; ++note) {
    for (int rank = 0; rank < kSupersawUnison; ++rank) {
      voice_[note][rank].set_phase(kScatter[note][rank]);
    }
  }
}

void ChordsSupersaw::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  if (parameters.trigger & TRIGGER_RISING_EDGE) {
    ScatterPhases();
  }

  ONE_POLE(morph_lp_, parameters.morph, 0.1f);
  ONE_POLE(timbre_lp_, parameters.timbre, 0.1f);

  chords_.set_chord(parameters.harmonics, 0);

  fill(&out[0], &out[size], 0.0f);
  fill(&aux[0], &aux[size], 0.0f);

  const bool stereo = false;
  float center_samples[kMaxBlockSize];
  if (stereo) {
    fill(&center_samples[0], &center_samples[size], 0.0f);
  }

  const float f0 = NoteToFrequency(parameters.note) * 0.998f;

  // MORPH: the detune spread, squared so the first part of the travel covers
  const float detune_semitones = morph_lp_ * morph_lp_ * kSupersawMaxDetune;

  // MACRO: the stack size, one to three oscillators per chord tone. It is set
  int unison =
      1 + static_cast<int>(0.5f * (kSupersawUnison - 0.001f));
  CONSTRAIN(unison, 1, kSupersawUnison);
  // A rank that has just been dialled away is rendered once more at zero gain,
  const int rendered_ranks = unison > active_unison_ ? unison : active_unison_;
  active_unison_ = unison;

  // Detuned saws drift out of phase with one another, so the stack sums
  const float unison_gain = kSupersawLevel * kInverseRoot[unison];

  // No inversion: the chord is voiced in root position and the fifth
  for (int note = 0; note < kPaletteChordNumNotes; ++note) {
    const float note_f0 = f0 * chords_.ratio(note);
    // The root goes to AUX and is added back into OUT below, so OUT carries
    float* mono_destination = note == 0 ? aux : out;

    for (int rank = 0; rank < rendered_ranks; ++rank) {
      const float offset =
          kDetuneSpread[note][kStackPositions[unison][rank]];
      const float frequency =
          note_f0 * SemitonesToRatio(offset * detune_semitones);
      const float distance = offset < 0.0f ? -offset : offset;
      const float rank_gain = 1.0f - kSupersawRankTilt * distance;

      float* destination;
      if (stereo) {
        // The detune spread becomes the image: ranks below the centre pitch
        destination = offset < 0.0f
            ? out
            : (offset > 0.0f ? aux : center_samples);
      } else {
        destination = mono_destination;
      }

      voice_[note][rank].Render(
          kSupersawFixedShape,
          kSupersawFixedShapeAmount,
          frequency,
          rank < unison ? unison_gain * rank_gain : 0.0f,
          destination,
          size);
    }
  }

  if (stereo) {
    // The same near/far crossfeed the stock stereo path uses, so a hard-panned
    for (size_t i = 0; i < size; ++i) {
      const float left_group = out[i];
      const float right_group = aux[i];
      const float center = center_samples[i] * 0.707106769f;
      out[i] = left_group * 0.935f + right_group * 0.335f + center;
      aux[i] = right_group * 0.935f + left_group * 0.335f + center;
    }
  } else {
    for (size_t i = 0; i < size; ++i) {
      const float selected = aux[i];
      out[i] += selected;
      aux[i] = selected * 3.0f;
    }
  }

  // TIMBRE: drive into the soft limiter. It compresses rather than folding
  if (timbre_lp_ > 0.0f) {
    const float drive = 1.0f + timbre_lp_ * kSupersawMaxDrive;
    float position = timbre_lp_ * (kSupersawMakeupPoints - 1.001f);
    MAKE_INTEGRAL_FRACTIONAL(position);
    const float a = kSupersawMakeup[position_integral];
    const float b = kSupersawMakeup[position_integral + 1];
    const float makeup = a + (b - a) * position_fractional;
    for (size_t i = 0; i < size; ++i) {
      out[i] = SoftLimit(out[i] * drive) * makeup;
      aux[i] = SoftLimit(aux[i] * drive) * makeup;
    }
  }

}

}  // namespace plaits
