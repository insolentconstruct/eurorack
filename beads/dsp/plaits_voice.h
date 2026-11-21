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
// One Plaits voice for the synth mode, adapted from plaits/dsp/voice.h: the
// model and note are set per note, there is no AUX output, no user data and
// no trigger delay, and FREEZE holds the low-pass gate open.

#ifndef BEADS_DSP_PLAITS_VOICE_H_
#define BEADS_DSP_PLAITS_VOICE_H_

#include "stmlib/stmlib.h"

#include "stmlib/dsp/limiter.h"

#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/envelope.h"
#include "plaits/dsp/fm/patch.h"
#include "plaits/dsp/fx/low_pass_gate.h"

#ifdef PLAITS_BLOBS
#include "beads/blob/plaits_region.h"
#endif  // PLAITS_BLOBS

namespace beads {

const int kNumPlaitsModels = 24;

// A voice's engines and the RAM they take turns in. The engines stay out of
// this header: Plaits' and Beads' resources.h define clashing macros.
#ifdef PLAITS_BLOBS
// Plus one blob slot, the largest state a version 2 blob may declare.
const size_t kPlaitsVoiceRamSize = 29 * 1024;
// Stock models served by blobs in development builds: Particle noise.
const int kPlaitsParticleNoise = 18;
#else
const size_t kPlaitsVoiceRamSize = 28 * 1024;
#endif  // PLAITS_BLOBS
// Speech's five word banks, decoded once for all voices.
const size_t kPlaitsSpeechRamSize = 36 * 1024;

// Plaits' three 6-op banks, unpacked once and shared by all voices.
const int kNumPlaitsFmBanks = 3;
const int kPlaitsFmBankSize = 32;
const size_t kPlaitsFmBanksRamSize =
    kNumPlaitsFmBanks * kPlaitsFmBankSize * sizeof(plaits::fm::Patch);

struct PlaitsEngines;

struct PlaitsPatch {
  int model;
  float note;
  float harmonics;
  float timbre;
  float morph;
};

class PlaitsVoice {
 public:
  PlaitsVoice() { }
  ~PlaitsVoice() { }

  // Decodes Speech's word banks into kPlaitsSpeechRamSize bytes of ram for
  // all voices, with kPlaitsVoiceRamSize bytes of scratch.
  static void DecodeSpeechBanks(uint8_t* ram, uint8_t* scratch);

  // Unpacks the 6-op banks into kPlaitsFmBanksRamSize bytes of ram.
  static const plaits::fm::Patch* UnpackFmBanks(uint8_t* ram);

  // Builds the engines in kPlaitsVoiceRamSize bytes of ram; the 6-op models
  // play the shared fm_banks.
  void Init(uint8_t* ram, const plaits::fm::Patch* fm_banks);

  // trigger takes plaits::TriggerState flags; size is at most 24 samples.
  void Render(
      const PlaitsPatch& patch,
      int trigger,
      bool sustain,
      float decay,
      int bit_shift,
      float* out,
      size_t size);

  inline int model() const { return model_; }

#ifdef PLAITS_BLOBS
  inline void set_region(const PlaitsRegion* region) { region_ = region; }

  // A model this build plays: compiled in, or a checked blob in region.
  static inline bool Available(int model, const PlaitsRegion& region) {
    return (model < kNumPlaitsModels && model != kPlaitsParticleNoise) ||
        region.has_blob(model);
  }

  // Plaits may play when every model of every bank is available.
  static inline bool Playable(
      const uint8_t* models,
      size_t count,
      const PlaitsRegion& region) {
    bool playable = true;
    for (size_t i = 0; i < count; ++i) {
      playable = playable && Available(models[i], region);
    }
    return playable;
  }
#endif  // PLAITS_BLOBS

 private:
#ifdef PLAITS_BLOBS
  plaits::Engine* engine(int model);
  const PlaitsRegion* region_;
#endif  // PLAITS_BLOBS
  PlaitsEngines* engines_;
  const plaits::fm::Patch* fm_banks_;
  int model_;

  bool sustain_;
  float sustain_level_;
  float level_;
  plaits::LPGEnvelope lpg_envelope_;
  plaits::LowPassGate lpg_;
  stmlib::Limiter limiter_;

  DISALLOW_COPY_AND_ASSIGN(PlaitsVoice);
};

}  // namespace beads

#endif  // BEADS_DSP_PLAITS_VOICE_H_
