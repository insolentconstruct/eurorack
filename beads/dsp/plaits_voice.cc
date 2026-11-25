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
// One Plaits voice for the synth mode, adapted from plaits/dsp/voice.cc.

#include "beads/dsp/plaits_voice.h"

#include <algorithm>
#include <new>

#include "stmlib/utils/buffer_allocator.h"

#ifdef PLAITS_BLOBS
#include "beads/blob/plaits_blob_engine.h"
#else
#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/bass_drum_engine.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/hi_hat_engine.h"
#include "plaits/dsp/engine/modal_engine.h"
#include "plaits/dsp/engine/noise_engine.h"
#include "plaits/dsp/engine/particle_engine.h"
#include "plaits/dsp/engine/snare_drum_engine.h"
#include "plaits/dsp/engine/speech_engine.h"
#include "plaits/dsp/engine/string_engine.h"
#include "plaits/dsp/engine/swarm_engine.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine/wavetable_engine.h"
#include "plaits/dsp/engine2/chiptune_engine.h"
#include "plaits/dsp/engine2/phase_distortion_engine.h"
#include "plaits/dsp/engine2/six_op_engine.h"
#include "plaits/dsp/engine2/string_machine_engine.h"
#include "plaits/dsp/engine2/virtual_analog_vcf_engine.h"
#include "plaits/dsp/engine2/wave_terrain_engine.h"
#include "plaits/dsp/speech/lpc_speech_synth_words.h"
#include "plaits/resources.h"
#endif  // PLAITS_BLOBS

namespace beads {

using namespace plaits;
using namespace std;
using namespace stmlib;

#ifdef PLAITS_BLOBS
// Every model is a blob, built in the slot when a voice selects it.
struct PlaitsEngines {
  PlaitsBlobEngine blob_engine;

  float aux[kMaxBlockSize];
};
#else
struct PlaitsEngines {
  VirtualAnalogEngine virtual_analog_engine;
  WaveshapingEngine waveshaping_engine;
  FMEngine fm_engine;
  GrainEngine grain_engine;
  AdditiveEngine additive_engine;
  WavetableEngine wavetable_engine;
  ChordEngine chord_engine;
  SpeechEngine speech_engine;

  SwarmEngine swarm_engine;
  NoiseEngine noise_engine;
  ParticleEngine particle_engine;
  StringEngine string_engine;
  ModalEngine modal_engine;
  BassDrumEngine bass_drum_engine;
  SnareDrumEngine snare_drum_engine;
  HiHatEngine hi_hat_engine;

  VirtualAnalogVCFEngine virtual_analog_vcf_engine;
  PhaseDistortionEngine phase_distortion_engine;
  SixOpEngine six_op_engine;
  WaveTerrainEngine wave_terrain_engine;
  StringMachineEngine string_machine_engine;
  ChiptuneEngine chiptune_engine;

  EngineRegistry<kNumPlaitsModels> registry;

  float aux[kMaxBlockSize];
};
#endif  // PLAITS_BLOBS

// As Plaits' shared buffer; the particle engine's diffuser is the largest
// user.
const size_t kEngineRamSize = 16384;

STATIC_ASSERT(
    sizeof(PlaitsEngines) + kEngineRamSize <= kPlaitsVoiceRamSize,
    plaits_voice_ram);
// The engines' shared RAM follows them, aligned as their own members.
STATIC_ASSERT(
    __alignof__(PlaitsEngines) <= kPlaitsRamAlignment,
    plaits_engines_alignment);
STATIC_ASSERT(
    kPlaitsVoiceRamSize % kPlaitsRamAlignment == 0 &&
    kPlaitsSpeechRamSize % kPlaitsRamAlignment == 0,
    plaits_regions_alignment);
#ifdef PLAITS_BLOBS
// The libraries follow the voices: records, then each library's RAM on 32
// bytes, then the 6-op banks, all on the same boundary.
STATIC_ASSERT(
    kPlaitsLibraryRamAlignment % kPlaitsRamAlignment == 0 &&
    kPlaitsLibraryRamSize % kPlaitsLibraryRamAlignment == 0 &&
    __alignof__(PlaitsBlobLibrary) <= kPlaitsRamAlignment,
    plaits_library_alignment);
#else
STATIC_ASSERT(
    __alignof__(LPCSpeechSynthWordBank) <= kPlaitsRamAlignment &&
    __alignof__(fm::Patch) <= kPlaitsRamAlignment,
    plaits_banks_alignment);
#endif  // PLAITS_BLOBS

// Plaits' default LPG colour.
const float kLpgColour = 0.5f;

#ifdef PLAITS_BLOBS
void PlaitsVoice::Init(uint8_t* ram) {
  // A blob's state starts zeroed in its slot, as Plaits' .bss.
  fill(&ram[0], &ram[kPlaitsVoiceRamSize], 0);
  engines_ = new(ram) PlaitsEngines;
  model_ = -1;
  sustain_ = false;
  unpatched_ = false;
  sustain_level_ = 0.0f;
  level_ = 0.0f;
  lpg_envelope_.Init();
  lpg_.Init();
  limiter_.Init();
}
#else
/* static */
const fm::Patch* PlaitsVoice::UnpackFmBanks(uint8_t* ram) {
  fm::Patch* patches = reinterpret_cast<fm::Patch*>(ram);
  for (int i = 0; i < kNumPlaitsFmBanks; ++i) {
    for (int j = 0; j < kPlaitsFmBankSize; ++j) {
      patches[i * kPlaitsFmBankSize + j].Unpack(
          fm_patches_table[i] + j * fm::Patch::SYX_SIZE);
    }
  }
  return patches;
}

void PlaitsVoice::Init(uint8_t* ram, const fm::Patch* fm_banks) {
  // Plaits' engines leave some state to its zeroed .bss.
  fill(&ram[0], &ram[kPlaitsVoiceRamSize], 0);
  engines_ = new(ram) PlaitsEngines;
  EngineRegistry<kNumPlaitsModels>& r = engines_->registry;
  PlaitsEngines& e = *engines_;
  r.Init();

  r.RegisterInstance(&e.virtual_analog_vcf_engine, false, 1.0f, 1.0f);
  r.RegisterInstance(&e.phase_distortion_engine, false, 0.7f, 0.7f);
  r.RegisterInstance(&e.six_op_engine, true, 1.0f, 1.0f);
  r.RegisterInstance(&e.six_op_engine, true, 1.0f, 1.0f);
  r.RegisterInstance(&e.six_op_engine, true, 1.0f, 1.0f);
  r.RegisterInstance(&e.wave_terrain_engine, false, 0.7f, 0.7f);
  r.RegisterInstance(&e.string_machine_engine, false, 0.8f, 0.8f);
  r.RegisterInstance(&e.chiptune_engine, false, 0.5f, 0.5f);

  r.RegisterInstance(&e.virtual_analog_engine, false, 0.8f, 0.8f);
  r.RegisterInstance(&e.waveshaping_engine, false, 0.7f, 0.6f);
  r.RegisterInstance(&e.fm_engine, false, 0.6f, 0.6f);
  r.RegisterInstance(&e.grain_engine, false, 0.7f, 0.6f);
  r.RegisterInstance(&e.additive_engine, false, 0.8f, 0.8f);
  r.RegisterInstance(&e.wavetable_engine, false, 0.6f, 0.6f);
  r.RegisterInstance(&e.chord_engine, false, 0.8f, 0.8f);
  r.RegisterInstance(&e.speech_engine, false, -0.7f, 0.8f);

  r.RegisterInstance(&e.swarm_engine, false, -3.0f, 1.0f);
  r.RegisterInstance(&e.noise_engine, false, -1.0f, -1.0f);
  r.RegisterInstance(&e.particle_engine, false, -2.0f, 1.0f);
  r.RegisterInstance(&e.string_engine, true, -1.0f, 0.8f);
  r.RegisterInstance(&e.modal_engine, true, -1.0f, 0.8f);
  r.RegisterInstance(&e.bass_drum_engine, true, 0.8f, 0.8f);
  r.RegisterInstance(&e.snare_drum_engine, true, 0.8f, 0.8f);
  r.RegisterInstance(&e.hi_hat_engine, true, 0.8f, 0.8f);

  BufferAllocator allocator(ram + sizeof(PlaitsEngines), kEngineRamSize);
  for (int i = 0; i < r.size(); ++i) {
    // All engines will share the same RAM space.
    allocator.Free();
    r.get(i)->Init(&allocator);
  }

  fm_banks_ = fm_banks;
  model_ = -1;
  sustain_ = false;
  unpatched_ = false;
  chiptune_tail_ = false;
  sustain_level_ = 0.0f;
  level_ = 0.0f;
  lpg_envelope_.Init();
  lpg_.Init();
  limiter_.Init();
}

#endif  // PLAITS_BLOBS

void PlaitsVoice::Render(
    const PlaitsPatch& patch,
    int trigger,
    bool sustain,
    float decay,
    int bit_shift,
    float* out,
    size_t size) {
#ifdef PLAITS_BLOBS
  if (!region_ || !region_->has_blob(patch.model)) {
    // Not reached: the synth is locked when a bank's model is missing.
    fill(&out[0], &out[size], 0.0f);
    return;
  }
  PlaitsBlobEngine* e = &engines_->blob_engine;
  if (patch.model != model_) {
    // A blob is built in its zeroed slot on every switch; reset loads the
    // 6-op bank.
    e->Bind(
        region_->blob(patch.model), &plaits_blob_services, patch.model,
        libraries_);
    BufferAllocator allocator(
        reinterpret_cast<uint8_t*>(engines_) + sizeof(PlaitsEngines),
        kEngineRamSize);
    e->Init(&allocator);
    e->Reset();
    limiter_.Init();
    model_ = patch.model;
  }
#else
  Engine* e = engines_->registry.get(patch.model);
  if (patch.model != model_) {
    if (patch.model >= 2 && patch.model <= 4) {
      engines_->six_op_engine.LoadUnpackedBank(
          &fm_banks_[(patch.model - 2) * kPlaitsFmBankSize]);
    } else {
      e->LoadUserData(NULL);
    }
    e->Reset();
    limiter_.Init();
    model_ = patch.model;
  }
#endif  // PLAITS_BLOBS

  // FREEZE holds the gate at its level; a note started meanwhile opens it.
  const bool unpatched = trigger & TRIGGER_UNPATCHED;
  if (trigger & TRIGGER_RISING_EDGE) {
    if (sustain) {
      sustain_level_ = 1.0f;
    } else {
      lpg_envelope_.Trigger();
    }
  } else if (sustain && !sustain_) {
    sustain_level_ = level_;
  }
  sustain_ = sustain;

#ifndef PLAITS_BLOBS
  // Keep Chiptune's drone voicing until its next note takes over.
  if (patch.model == 7) {
    chiptune_tail_ = !unpatched && !(trigger & TRIGGER_RISING_EDGE) &&
        (unpatched_ || chiptune_tail_);
    // Plaits' chiptune envelope, with the TIMBRE attenuverter at noon.
    engines_->chiptune_engine.set_envelope_shape(
        unpatched || chiptune_tail_ ? ChiptuneEngine::NO_ENVELOPE : 0.0f);
  } else {
    chiptune_tail_ = false;
  }
#endif  // PLAITS_BLOBS

  EngineParameters p;
  p.trigger = trigger;
#ifndef PLAITS_BLOBS
  if (chiptune_tail_) {
    p.trigger = TRIGGER_UNPATCHED;
  }
#endif  // PLAITS_BLOBS
  p.note = patch.note;
  p.harmonics = patch.harmonics;
  p.timbre = patch.timbre;
  p.morph = patch.morph;
  p.accent = 0.8f;

  const PostProcessingSettings& pp_s = e->post_processing_settings;
  bool already_enveloped = pp_s.already_enveloped;
  e->Render(p, out, engines_->aux, size, &already_enveloped);

  // Resolution, as for the wavetable synth grains; engine output stays far
  // below 2^(31 - bit_shift), as Plaits' own int32_t conversion assumes.
  const float scale_up = float(1 << bit_shift);
  const float scale_down = 1.0f / scale_up;
  for (size_t i = 0; i < size; ++i) {
    out[i] = float(int32_t(out[i] * scale_up)) * scale_down;
  }

  const bool lpg_bypass = already_enveloped || unpatched;
  // A drone ends as a note at its peak, decaying through the gate.
  const bool drone_end = unpatched_ && !lpg_bypass;
  unpatched_ = unpatched;

  // Compute LPG parameters.
  if (!lpg_bypass) {
    const float hf = kLpgColour;
    const float short_decay = (200.0f * kBlockSize) / kSampleRate *
        SemitonesToRatio(-96.0f * decay);
    const float decay_tail = (20.0f * kBlockSize) / kSampleRate *
        SemitonesToRatio(-72.0f * decay + 12.0f * hf) - short_decay;
    if (drone_end) {
      // Fully open, with no decay yet.
      lpg_envelope_.Trigger();
      lpg_envelope_.ProcessPing(1.0f, 0.0f, 0.0f, hf);
    }

    if (sustain) {
      lpg_envelope_.ProcessLP(sustain_level_, short_decay, decay_tail, hf);
    } else {
      const float attack = NoteToFrequency(p.note) * float(kBlockSize) * 2.0f;
      lpg_envelope_.ProcessPing(attack, short_decay, decay_tail, hf);
    }
  } else {
    lpg_envelope_.Init();
  }
  // The gate level last heard: a voice that never played holds silence.
  level_ = lpg_bypass ? (unpatched ? 1.0f : 0.0f) : lpg_envelope_.gain();

  float gain = pp_s.out_gain;
  if (gain < 0.0f) {
    limiter_.Process(-gain, out, size);
    gain = 1.0f;
  }
  if (drone_end) {
    // The open gate settles on the drone's level before it takes over.
    float settle[kMaxBlockSize];
    for (int pass = 0; pass < 2; ++pass) {
      fill(&settle[0], &settle[size], out[0]);
      lpg_.Process(
          gain,
          lpg_envelope_.frequency(),
          lpg_envelope_.hf_bleed(),
          settle,
          size);
    }
  }
  if (!lpg_bypass) {
    lpg_.Process(
        gain * lpg_envelope_.gain(),
        lpg_envelope_.frequency(),
        lpg_envelope_.hf_bleed(),
        out,
        size);
  } else {
    for (size_t i = 0; i < size; ++i) {
      out[i] *= gain;
    }
  }
}

#ifndef PLAITS_BLOBS
/* static */
void PlaitsVoice::DecodeSpeechBanks(uint8_t* ram, uint8_t* scratch) {
  BufferAllocator allocator(ram, kPlaitsSpeechRamSize);
  LPCSpeechSynthWordBank* banks = allocator.Allocate<LPCSpeechSynthWordBank>(
      LPC_SPEECH_SYNTH_NUM_WORD_BANKS);
  LPCSpeechSynthWordBank::set_decoded_banks(NULL);
  for (int i = 0; i < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++i) {
    BufferAllocator scratch_allocator(scratch, kPlaitsVoiceRamSize);
    banks[i].Init(
        word_banks_,
        LPC_SPEECH_SYNTH_NUM_WORD_BANKS,
        &scratch_allocator);
    banks[i].Decode(i, &allocator);
  }
  LPCSpeechSynthWordBank::set_decoded_banks(banks);
}
#endif  // PLAITS_BLOBS

}  // namespace beads
