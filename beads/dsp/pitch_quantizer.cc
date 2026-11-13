// Copyright 2015 Emilie Gillet.
// Copyright 2026 Insolent Construct.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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
// Fixed-root grain pitch quantization, and the notes recently sent to PITCH.

#include "beads/dsp/pitch_quantizer.h"

#include <algorithm>
#include <cmath>

#include "stmlib/utils/random.h"

namespace beads {

// Marbles preset notes with weight >= 64, in semitones.
const float kPitchScaleNotes[] = {
  0.0000f, 1.0000f, 2.0000f, 3.0000f, 4.0000f, 5.0000f,
  6.0000f, 7.0000f, 8.0000f, 9.0000f, 10.0000f, 11.0000f,
  0.0000f, 7.0000f,
  0.0000f, 2.0004f, 3.9996f, 5.0004f, 6.9996f, 9.0000f, 11.0004f,
  0.0000f, 2.0004f, 3.0000f, 5.0004f, 6.9996f, 8.0004f, 9.9996f,
  // Pentatonics are subsets of Marbles' major/minor scales.
  0.0000f, 2.0004f, 3.9996f, 6.9996f, 9.0000f,
  0.0000f, 3.0000f, 5.0004f, 6.9996f, 9.9996f,
  0.0000f, 1.5300f, 7.0596f, 8.4804f,
  0.0000f, 0.9024f, 3.8628f, 4.9800f, 7.0200f, 7.9212f, 10.8828f,
  0.0000f, 2.0388f, 3.1560f, 4.9800f, 7.0200f, 9.0588f, 10.1748f,
};

const PitchScaleNotes kPitchScales[] = {
  { 0, 12 },
  { 12, 2 },
  { 14, 7 },
  { 21, 7 },
  { 28, 5 },
  { 33, 5 },
  { 38, 4 },
  { 42, 7 },
  { 49, 7 },
};

struct CustomPitchScale {
  uint8_t size;
  float period;
  float notes[kMaxScaleDegrees];
};

static bool custom_bank;
static CustomPitchScale custom_scales[kMaxPitchScaleId + 1];

void ConfigurePitchScales(const PitchScaleDefinition* scales, size_t count,
    bool custom) {
  custom_bank = custom;
  if (!custom) {
    return;
  }
  for (size_t i = 0; i <= kMaxPitchScaleId; ++i) {
    custom_scales[i].size = 0;
  }
  for (size_t i = 0; i < count && i < kMaxCustomScales; ++i) {
    const PitchScaleDefinition& from = scales[i];
    if (from.id < PITCH_SCALE_FIFTHS || from.id > kMaxPitchScaleId ||
        !from.size || from.size > kMaxScaleDegrees) {
      continue;
    }
    CustomPitchScale& to = custom_scales[from.id];
    to.size = from.size;
    to.period = from.period;
    for (size_t j = 0; j < from.size; ++j) {
      to.notes[j] = from.notes[j];
    }
  }
}

static float QuantizeDegrees(float pitch, const float* degrees,
    size_t size, float period) {
  const float root = floorf(pitch / period) * period;
  float lower = root;
  for (size_t i = 1; i <= size; ++i) {
    const float upper = root + (i == size ? period : degrees[i]);
    if (pitch <= 0.5f * (lower + upper)) {
      return lower;
    }
    lower = upper;
  }
  return lower;
}

float QuantizeFifths(float pitch) {
  return QuantizeDegrees(pitch, &kPitchScaleNotes[12], 2, 12.0f);
}

float QuantizePitch(float pitch, PitchScale scale) {
  if (scale == PITCH_SCALE_OFF) {
    return pitch;
  }
  if (scale == PITCH_SCALE_CHROMATIC) {
    return ceilf(pitch - 0.5f);
  }
  if (custom_bank) {
    if (scale < PITCH_SCALE_FIFTHS || scale > kMaxPitchScaleId ||
        !custom_scales[scale].size) {
      return pitch;
    }
    const CustomPitchScale& notes = custom_scales[scale];
    return QuantizeDegrees(pitch, notes.notes, notes.size, notes.period);
  }
  if (scale < PITCH_SCALE_FIFTHS || scale >= PITCH_SCALE_LAST) {
    return pitch;
  }
  const PitchScaleNotes& notes = kPitchScales[scale - 1];
  const float octave = floorf(pitch / 12.0f) * 12.0f;
  const float* degrees = &kPitchScaleNotes[notes.offset];
  float lower = octave;
  for (int i = 1; i <= notes.size; ++i) {
    const float upper = octave + (i == notes.size ? 12.0f : degrees[i]);
    if (pitch <= 0.5f * (lower + upper)) {
      return lower;
    }
    lower = upper;
  }
  return lower;
}

const size_t kNumRecentNotes = 8;
// A note is new once the CV holds within 0.1 semitone for 10 ms, at least
// half a semitone away from the last note.
const float kNoteTolerance = 0.1f;
const float kNoteMinimumStep = 0.5f;
const uint32_t kNoteSettleTime = 10;

static float recent_notes[kNumRecentNotes];
static volatile size_t num_recent_notes;
static volatile size_t newest_note;
static float note_candidate;
static uint32_t note_candidate_time;

void RememberNote(float cv, bool patched, uint32_t now) {
  if (!patched) {
    num_recent_notes = 0;
    note_candidate = cv;
    note_candidate_time = now;
    return;
  }
  if (fabsf(cv - note_candidate) > kNoteTolerance) {
    note_candidate = cv;
    note_candidate_time = now;
    return;
  }
  if (now - note_candidate_time < kNoteSettleTime || (num_recent_notes &&
      fabsf(cv - recent_notes[newest_note]) < kNoteMinimumStep)) {
    return;
  }
  // The slot is written before it is published to the audio interrupt.
  const size_t slot = (newest_note + 1) % kNumRecentNotes;
  recent_notes[slot] = cv;
  newest_note = slot;
  if (num_recent_notes < kNumRecentNotes) {
    ++num_recent_notes;
  }
}

float RecentNote(float amount, float cv) {
  size_t count = 1 + size_t(amount * float(kNumRecentNotes - 1) + 0.5f);
  const size_t stored = num_recent_notes;
  if (!stored) {
    return cv;
  }
  count = std::min(count, stored);
  const size_t age = size_t(stmlib::Random::GetFloat() * float(count));
  return recent_notes[(newest_note + kNumRecentNotes - age) % kNumRecentNotes];
}

}  // namespace beads
