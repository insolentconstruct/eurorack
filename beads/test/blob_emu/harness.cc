// Copyright 2026 Insolent Construct.
//
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
//
// -----------------------------------------------------------------------------
//
// Emulator harness: the region reader, the libraries built as at synth entry,
// a blob voice (one slot and arena, as PlaitsVoice) and the same engines
// compiled in, built in PlaitsVoice's order and sharing their own arena as
// PlaitsVoice's do; called by render_compare.py under Unicorn's Cortex-M7.

#include <algorithm>
#include <new>

#include "stmlib/utils/buffer_allocator.h"
#include "stmlib/utils/random.h"

#include "beads/blob/models/glisson_engine.h"
#include "beads/blob/models/scanned_engine.h"
#include "beads/blob/models/pulsar_engine.h"
#include "beads/blob/models/loopback_engine.h"
#include "beads/blob/models/lockstep_engine.h"
#include "beads/blob/models/tapfield_engine.h"
#include "beads/blob/models/phase_weave_engine.h"
#include "beads/blob/models/sideband_engine.h"
#include "beads/blob/models/attractor_engine.h"
#include "beads/blob/models/undertow_engine.h"
#include "beads/blob/models/reed_pipe_engine.h"
#include "beads/blob/models/phase_flock_engine.h"
#include "beads/blob/models/rulefield_engine.h"
#include "beads/blob/models/spectral_spiral_engine.h"
#include "beads/blob/models/z_filter_engine.h"
#include "beads/blob/models/toy_engine.h"
#include "beads/blob/models/csaw_engine.h"
#include "beads/blob/models/ring_mod_engine.h"
#include "beads/blob/models/buzz_engine.h"
#include "beads/blob/models/saw_square_engine.h"
#include "beads/blob/models/harmonics_engine.h"
#include "beads/blob/models/struck_bell_engine.h"
#include "beads/blob/models/question_mark_engine.h"
#include "beads/blob/models/bowed_engine.h"
#include "beads/blob/models/sub_oscillator_engine.h"
#include "beads/blob/models/digital_modulation_engine.h"
#include "beads/blob/models/raw_fm_engine.h"
#include "beads/blob/models/triple_engine.h"
#include "beads/blob/models/bytebeat_engine.h"
#include "beads/blob/models/acid_engine.h"
#include "beads/blob/models/gendy_engine.h"
#include "beads/blob/models/chords_supersaw_engine.h"
#include "beads/blob/models/bubbletime_engine.h"
#include "beads/blob/models/saw_swarm_engine.h"
#include "beads/blob/models/dual_sync_engine.h"
#include "beads/blob/models/morph_engine.h"
#include "beads/blob/models/particle_burst_engine.h"
#include "beads/blob/models/vowel_engine.h"
#include "beads/blob/models/plucked_engine.h"
#include "beads/blob/models/blown_engine.h"
#include "beads/blob/models/struck_drum_engine.h"
#include "beads/blob/models/fluted_engine.h"
#include "beads/blob/models/shakers_engine.h"
#include "beads/blob/models/brass_engine.h"
#include "beads/blob/models/helix_engine.h"
#include "beads/blob/models/skins_engine.h"
#include "beads/blob/models/circuit_zaps_engine.h"
#include "beads/blob/models/metalwork_engine.h"
#include "beads/blob/models/clap_engine.h"
#include "beads/blob/models/analog_percussion_engine.h"
#include "beads/blob/models/kick_engine.h"
#include "beads/blob/models/snare_engine.h"
#include "beads/blob/models/cymbal_engine.h"
#include "beads/blob/models/saw_comb_engine.h"
#include "beads/blob/models/wave_paraphonic_engine.h"
#include "beads/blob/models/wavetable_chord_engine.h"
#include "beads/test/scale_voices_stubs.h"
#include "beads/blob/models/fold_engine.h"
#include "beads/blob/models/wavetable_scale_stack_engine.h"
#include "beads/blob/models/diatonic_chord_engine.h"
#include "beads/blob/models/scale_stack_engine.h"
#include "beads/blob/models/noise_bank_engine.h"
#include "beads/blob/models/freshets_formant_engine.h"
#include "beads/blob/models/speech_sounds_engine.h"
#include "beads/test/speech_sounds_stubs.h"
#include "beads/blob/models/lpc_words_engine.h"
#include "beads/blob/plaits_blob.h"
#include "beads/blob/plaits_blob_engine.h"
#include "beads/blob/plaits_libraries.h"
#include "beads/blob/plaits_region.h"
#include "beads/test/blob_gate/test_sine.h"
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

using namespace beads;
using namespace plaits;

class TestSineEngine : public Engine {
 public:
  TestSineEngine() { }
  ~TestSineEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    sine_.Init();
  }

  virtual void Reset() {
    sine_.Init();
  }

  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    sine_.Render(parameters, out, aux, size);
  }

 private:
  TestSine sine_;

  DISALLOW_COPY_AND_ASSIGN(TestSineEngine);
};

// The proof's Palette models and test sine, then the stock engines in
// PlaitsVoice's registry order (the 6-op engine serves models 2-4).
const size_t kFirstStock = 65;
const size_t kNumStock = 22;
const size_t kNumCompiled = kFirstStock + kNumStock;
const size_t kSixOpSlot = kFirstStock + 2;
const size_t kChiptuneSlot = kFirstStock + 5;
const int kNumFmBanks = 3;
const int kFmBankSize = 32;
const size_t kArenaSize = 16384;
const size_t kCanaryWords = 16;
const size_t kMaxDigests = 64;
// As the development build: kPlaitsLibraryRamSize, and the first voice's RAM
// as scratch (enough for a word bank).
const size_t kLibraryRamSize = 64 * 1024;
const size_t kScratchSize = 16 * 1024;
const size_t kSpeechRamSize = 36 * 1024;

// Engines are placed by harness_setup: no static constructors run here.
static uint8_t glisson_storage[sizeof(GlissonEngine)]
    __attribute__((aligned(8)));
static uint8_t scanned_storage[sizeof(ScannedEngine)]
    __attribute__((aligned(8)));
static uint8_t pulsar_storage[sizeof(PulsarEngine)]
    __attribute__((aligned(8)));

static uint8_t loopback_storage[sizeof(LoopbackEngine)]
    __attribute__((aligned(8)));

static uint8_t lockstep_storage[sizeof(LockstepEngine)]
    __attribute__((aligned(8)));

static uint8_t tapfield_storage[sizeof(TapfieldEngine)]
    __attribute__((aligned(8)));

static uint8_t phase_weave_storage[sizeof(PhaseWeaveEngine)]
    __attribute__((aligned(8)));

static uint8_t sideband_storage[sizeof(SidebandEngine)]
    __attribute__((aligned(8)));

static uint8_t attractor_storage[sizeof(AttractorEngine)]
    __attribute__((aligned(8)));

static uint8_t undertow_storage[sizeof(UndertowEngine)]
    __attribute__((aligned(8)));

static uint8_t reed_pipe_storage[sizeof(ReedPipeEngine)]
    __attribute__((aligned(8)));

static uint8_t phase_flock_storage[sizeof(PhaseFlockEngine)]
    __attribute__((aligned(8)));

static uint8_t rulefield_storage[sizeof(RulefieldEngine)]
    __attribute__((aligned(8)));

static uint8_t spectral_spiral_storage[sizeof(SpectralSpiralEngine)]
    __attribute__((aligned(8)));

static uint8_t z_filter_storage[sizeof(ZFilterEngine)]
    __attribute__((aligned(8)));

static uint8_t toy_storage[sizeof(ToyEngine)]
    __attribute__((aligned(8)));

static uint8_t csaw_storage[sizeof(CSawEngine)]
    __attribute__((aligned(8)));

static uint8_t ring_mod_storage[sizeof(RingModEngine)]
    __attribute__((aligned(8)));

static uint8_t buzz_storage[sizeof(BuzzEngine)]
    __attribute__((aligned(8)));

static uint8_t saw_square_storage[sizeof(SawSquareEngine)]
    __attribute__((aligned(8)));

static uint8_t harmonics_storage[sizeof(HarmonicsEngine)]
    __attribute__((aligned(8)));

static uint8_t struck_bell_storage[sizeof(StruckBellEngine)]
    __attribute__((aligned(8)));

static uint8_t question_mark_storage[sizeof(QuestionMarkEngine)]
    __attribute__((aligned(8)));

static uint8_t bowed_storage[sizeof(BowedEngine)]
    __attribute__((aligned(8)));

static uint8_t sub_oscillator_storage[sizeof(SubOscillatorEngine)]
    __attribute__((aligned(8)));

static uint8_t digital_modulation_storage[sizeof(DigitalModulationEngine)]
    __attribute__((aligned(8)));

static uint8_t raw_fm_storage[sizeof(RawFmEngine)]
    __attribute__((aligned(8)));

static uint8_t triple_storage[sizeof(TripleEngine)]
    __attribute__((aligned(8)));

static uint8_t bytebeat_storage[sizeof(BytebeatEngine)]
    __attribute__((aligned(8)));

static uint8_t acid_storage[sizeof(AcidEngine)]
    __attribute__((aligned(8)));

static uint8_t sine_storage[sizeof(TestSineEngine)]
    __attribute__((aligned(8)));

static uint8_t gendy_storage[sizeof(GendyEngine)]
    __attribute__((aligned(8)));

static uint8_t particle_burst_storage[sizeof(ParticleBurstEngine)]
    __attribute__((aligned(8)));

static uint8_t struck_drum_storage[sizeof(StruckDrumEngine)]
    __attribute__((aligned(8)));

static uint8_t plucked_storage[sizeof(PluckedEngine)]
    __attribute__((aligned(8)));

static uint8_t blown_storage[sizeof(BlownEngine)]
    __attribute__((aligned(8)));

static uint8_t fluted_storage[sizeof(FlutedEngine)]
    __attribute__((aligned(8)));

static uint8_t brass_storage[sizeof(BrassEngine)]
    __attribute__((aligned(8)));

static uint8_t skins_storage[sizeof(SkinsEngine)]
    __attribute__((aligned(8)));

static uint8_t circuit_zaps_storage[sizeof(CircuitZapsEngine)]
    __attribute__((aligned(8)));

static uint8_t metalwork_storage[sizeof(MetalworkEngine)]
    __attribute__((aligned(8)));

static uint8_t clap_storage[sizeof(ClapEngine)]
    __attribute__((aligned(8)));

static uint8_t analog_percussion_storage[sizeof(AnalogPercussionEngine)]
    __attribute__((aligned(8)));

static uint8_t dual_sync_storage[sizeof(DualSyncEngine)]
    __attribute__((aligned(8)));

static uint8_t morph_storage[sizeof(MorphEngine)]
    __attribute__((aligned(8)));

static uint8_t vowel_storage[sizeof(VowelEngine)]
    __attribute__((aligned(8)));

static uint8_t helix_storage[sizeof(HelixEngine)]
    __attribute__((aligned(8)));

static uint8_t shakers_storage[sizeof(ShakersEngine)]
    __attribute__((aligned(8)));

static uint8_t saw_swarm_storage[sizeof(SawSwarmEngine)]
    __attribute__((aligned(8)));

static uint8_t bubbletime_storage[sizeof(BubbleTimeEngine)]
    __attribute__((aligned(8)));

static uint8_t chords_supersaw_storage[sizeof(ChordsSupersawEngine)]
    __attribute__((aligned(8)));

static uint8_t kick_storage[sizeof(KickEngine)]
    __attribute__((aligned(8)));

static uint8_t snare_storage[sizeof(SnareEngine)]
    __attribute__((aligned(8)));

static uint8_t cymbal_storage[sizeof(CymbalEngine)]
    __attribute__((aligned(8)));

static uint8_t saw_comb_storage[sizeof(SawCombEngine)]
    __attribute__((aligned(8)));

static uint8_t wave_paraphonic_storage[sizeof(WaveParaphonicEngine)]
    __attribute__((aligned(8)));

static uint8_t wavetable_chord_storage[sizeof(WavetableChordEngine)]
    __attribute__((aligned(8)));

static uint8_t fold_storage[sizeof(FoldEngine)]
    __attribute__((aligned(8)));

static uint8_t wavetable_scale_stack_storage[sizeof(WavetableScaleStackEngine)]
    __attribute__((aligned(8)));

static uint8_t diatonic_chord_storage[sizeof(DiatonicChordEngine)]
    __attribute__((aligned(8)));

static uint8_t scale_stack_storage[sizeof(ScaleStackEngine)]
    __attribute__((aligned(8)));

static uint8_t noise_bank_storage[sizeof(NoiseBankEngine)]
    __attribute__((aligned(8)));

static uint8_t freshets_formant_storage[sizeof(FreshetsFormantEngine)]
    __attribute__((aligned(8)));

static uint8_t speech_sounds_storage[sizeof(SpeechSoundsEngine)]
    __attribute__((aligned(8)));

static uint8_t lpc_words_storage[sizeof(LPCWordsEngine)]
    __attribute__((aligned(8)));

// The stock engines, as PlaitsEngines in beads/dsp/plaits_voice.cc.
struct StockEngines {
  VirtualAnalogVCFEngine virtual_analog_vcf_engine;
  PhaseDistortionEngine phase_distortion_engine;
  SixOpEngine six_op_engine;
  WaveTerrainEngine wave_terrain_engine;
  StringMachineEngine string_machine_engine;
  ChiptuneEngine chiptune_engine;
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
};
static uint8_t stock_storage[sizeof(StockEngines)] __attribute__((aligned(8)));
static uint8_t stock_arena[kArenaSize] __attribute__((aligned(8)));
// PlaitsVoice::UnpackFmBanks' banks.
static fm::Patch fm_banks[kNumFmBanks * kFmBankSize];
static Engine* compiled[kNumCompiled];
static uint8_t compiled_arena[kArenaSize] __attribute__((aligned(8)));
// The compiled-in engines' decoded word banks (PlaitsVoice::DecodeSpeechBanks).
static uint8_t compiled_speech_ram[kSpeechRamSize] __attribute__((aligned(8)));
static PlaitsRegion region;
static PlaitsLibraries libraries;

extern "C" {

// Guard bands around the blob voice's slot and arena.
struct BlobVoice {
  uint32_t canary_before[kCanaryWords];
  uint8_t engine[sizeof(PlaitsBlobEngine)] __attribute__((aligned(8)));
  uint32_t canary_between[kCanaryWords];
  uint8_t arena[kArenaSize] __attribute__((aligned(8)));
  uint32_t canary_after[kCanaryWords];
};

BlobVoice harness_blob_voice;
uint8_t harness_library_ram[kLibraryRamSize] __attribute__((aligned(32)));
uint8_t harness_scratch[kScratchSize] __attribute__((aligned(8)));
uint32_t harness_library_ram_size = kLibraryRamSize;
uint8_t harness_digests[kMaxDigests][kPlaitsBlobDigestSize];
uint32_t harness_num_digests;
PlaitsDirectoryEntry harness_directory[PLAITS_SECTION_LAST];
uint32_t harness_num_entries;
PlaitsDefaultSection harness_defaults[PLAITS_SECTION_LAST];
uint32_t harness_num_defaults;
PlaitsRegionIdentity harness_identity;
uint32_t harness_lock;
// The checked synth blob of RNGS, or 0.
uintptr_t harness_synth_blob;
uintptr_t harness_slot;
// Wave terrain lookups past the bank, counted by terrain_probe.h.
uint32_t harness_terrain_overshoot;
uint32_t harness_num_terrains;
const uint8_t* harness_user_data = NULL;
float harness_out[kMaxBlockSize];
float harness_aux[kMaxBlockSize];
EngineParameters harness_parameters;

static PlaitsBlobEngine* blob_engine() {
  return reinterpret_cast<PlaitsBlobEngine*>(harness_blob_voice.engine);
}

static void DecodeSpeechBanks() {
  stmlib::BufferAllocator allocator(compiled_speech_ram, kSpeechRamSize);
  LPCSpeechSynthWordBank* banks = allocator.Allocate<LPCSpeechSynthWordBank>(
      LPC_SPEECH_SYNTH_NUM_WORD_BANKS);
  LPCSpeechSynthWordBank::set_decoded_banks(NULL);
  for (int i = 0; i < LPC_SPEECH_SYNTH_NUM_WORD_BANKS; ++i) {
    stmlib::BufferAllocator scratch(harness_scratch, kScratchSize);
    banks[i].Init(word_banks_, LPC_SPEECH_SYNTH_NUM_WORD_BANKS, &scratch);
    banks[i].Decode(i, &allocator);
  }
  LPCSpeechSynthWordBank::set_decoded_banks(banks);
}

// Engines are built in zeroed storage and RAM, as at power-on.
static void BuildProof() {
  std::fill(&compiled_arena[0], &compiled_arena[kArenaSize], 0);
  std::fill(&phase_weave_storage[0], &phase_weave_storage[sizeof(
      phase_weave_storage)], 0);
  std::fill(&sine_storage[0], &sine_storage[sizeof(sine_storage)], 0);
  std::fill(&spectral_spiral_storage[0], &spectral_spiral_storage[sizeof(
      spectral_spiral_storage)], 0);
  std::fill(&pulsar_storage[0], &pulsar_storage[sizeof(pulsar_storage)], 0);
  std::fill(&glisson_storage[0], &glisson_storage[sizeof(glisson_storage)], 0);
  std::fill(&scanned_storage[0],
      &scanned_storage[sizeof(scanned_storage)], 0);
  std::fill(&loopback_storage[0],
      &loopback_storage[sizeof(loopback_storage)], 0);
  std::fill(&lockstep_storage[0],
      &lockstep_storage[sizeof(lockstep_storage)], 0);
  std::fill(&tapfield_storage[0],
      &tapfield_storage[sizeof(tapfield_storage)], 0);
  std::fill(&sideband_storage[0],
      &sideband_storage[sizeof(sideband_storage)], 0);
  std::fill(&attractor_storage[0],
      &attractor_storage[sizeof(attractor_storage)], 0);
  std::fill(&undertow_storage[0],
      &undertow_storage[sizeof(undertow_storage)], 0);
  std::fill(&reed_pipe_storage[0],
      &reed_pipe_storage[sizeof(reed_pipe_storage)], 0);
  std::fill(&phase_flock_storage[0],
      &phase_flock_storage[sizeof(phase_flock_storage)], 0);
  std::fill(&rulefield_storage[0],
      &rulefield_storage[sizeof(rulefield_storage)], 0);
  std::fill(&z_filter_storage[0],
      &z_filter_storage[sizeof(z_filter_storage)], 0);
  std::fill(&toy_storage[0],
      &toy_storage[sizeof(toy_storage)], 0);
  std::fill(&csaw_storage[0],
      &csaw_storage[sizeof(csaw_storage)], 0);
  std::fill(&ring_mod_storage[0],
      &ring_mod_storage[sizeof(ring_mod_storage)], 0);

  std::fill(&buzz_storage[0],
      &buzz_storage[sizeof(buzz_storage)], 0);
  std::fill(&saw_square_storage[0],
      &saw_square_storage[sizeof(saw_square_storage)], 0);
  std::fill(&harmonics_storage[0],
      &harmonics_storage[sizeof(harmonics_storage)], 0);
  std::fill(&struck_bell_storage[0],
      &struck_bell_storage[sizeof(struck_bell_storage)], 0);
  std::fill(&question_mark_storage[0],
      &question_mark_storage[sizeof(question_mark_storage)], 0);
  std::fill(&bowed_storage[0],
      &bowed_storage[sizeof(bowed_storage)], 0);
  std::fill(&sub_oscillator_storage[0],
      &sub_oscillator_storage[sizeof(sub_oscillator_storage)], 0);
  std::fill(&digital_modulation_storage[0],
      &digital_modulation_storage[sizeof(digital_modulation_storage)], 0);
  std::fill(&raw_fm_storage[0],
      &raw_fm_storage[sizeof(raw_fm_storage)], 0);
  std::fill(&triple_storage[0],
      &triple_storage[sizeof(triple_storage)], 0);
  std::fill(&bytebeat_storage[0],
      &bytebeat_storage[sizeof(bytebeat_storage)], 0);
  std::fill(&acid_storage[0],
      &acid_storage[sizeof(acid_storage)], 0);
  std::fill(&gendy_storage[0], &gendy_storage[sizeof(gendy_storage)], 0);
  std::fill(&particle_burst_storage[0],
      &particle_burst_storage[sizeof(particle_burst_storage)], 0);
  std::fill(&struck_drum_storage[0],
      &struck_drum_storage[sizeof(struck_drum_storage)], 0);
  std::fill(&plucked_storage[0],
      &plucked_storage[sizeof(plucked_storage)], 0);
  std::fill(&blown_storage[0],
      &blown_storage[sizeof(blown_storage)], 0);
  std::fill(&fluted_storage[0],
      &fluted_storage[sizeof(fluted_storage)], 0);
  std::fill(&brass_storage[0],
      &brass_storage[sizeof(brass_storage)], 0);
  std::fill(&skins_storage[0],
      &skins_storage[sizeof(skins_storage)], 0);
  std::fill(&circuit_zaps_storage[0],
      &circuit_zaps_storage[sizeof(circuit_zaps_storage)], 0);
  std::fill(&metalwork_storage[0],
      &metalwork_storage[sizeof(metalwork_storage)], 0);
  std::fill(&clap_storage[0],
      &clap_storage[sizeof(clap_storage)], 0);
  std::fill(&analog_percussion_storage[0],
      &analog_percussion_storage[sizeof(analog_percussion_storage)], 0);
  std::fill(&dual_sync_storage[0],
      &dual_sync_storage[sizeof(dual_sync_storage)], 0);
  std::fill(&morph_storage[0],
      &morph_storage[sizeof(morph_storage)], 0);
  std::fill(&vowel_storage[0],
      &vowel_storage[sizeof(vowel_storage)], 0);
  std::fill(&helix_storage[0],
      &helix_storage[sizeof(helix_storage)], 0);
  std::fill(&shakers_storage[0],
      &shakers_storage[sizeof(shakers_storage)], 0);
  std::fill(&saw_swarm_storage[0],
      &saw_swarm_storage[sizeof(saw_swarm_storage)], 0);
  std::fill(&bubbletime_storage[0],
      &bubbletime_storage[sizeof(bubbletime_storage)], 0);
  std::fill(&chords_supersaw_storage[0],
      &chords_supersaw_storage[sizeof(chords_supersaw_storage)], 0);
  std::fill(&scanned_storage[0], &scanned_storage[sizeof(scanned_storage)], 0);
  std::fill(&kick_storage[0], &kick_storage[sizeof(kick_storage)], 0);
  std::fill(&snare_storage[0], &snare_storage[sizeof(snare_storage)], 0);
  std::fill(&cymbal_storage[0], &cymbal_storage[sizeof(cymbal_storage)], 0);
  std::fill(&saw_comb_storage[0],
      &saw_comb_storage[sizeof(saw_comb_storage)], 0);
  std::fill(&wave_paraphonic_storage[0],
      &wave_paraphonic_storage[sizeof(wave_paraphonic_storage)], 0);
  std::fill(&wavetable_chord_storage[0],
      &wavetable_chord_storage[sizeof(wavetable_chord_storage)], 0);
  std::fill(&fold_storage[0], &fold_storage[sizeof(fold_storage)], 0);
  std::fill(&wavetable_scale_stack_storage[0],
      &wavetable_scale_stack_storage[sizeof(wavetable_scale_stack_storage)], 0);
  std::fill(&diatonic_chord_storage[0],
      &diatonic_chord_storage[sizeof(diatonic_chord_storage)], 0);
  std::fill(&scale_stack_storage[0],
      &scale_stack_storage[sizeof(scale_stack_storage)], 0);
  std::fill(&noise_bank_storage[0], &noise_bank_storage[sizeof(
      noise_bank_storage)], 0);
  std::fill(&freshets_formant_storage[0], &freshets_formant_storage[sizeof(
      freshets_formant_storage)], 0);
  std::fill(&speech_sounds_storage[0], &speech_sounds_storage[sizeof(
      speech_sounds_storage)], 0);
  std::fill(&lpc_words_storage[0], &lpc_words_storage[sizeof(
      lpc_words_storage)], 0);
  stmlib::BufferAllocator allocator(compiled_arena, kArenaSize);
  compiled[0] = new(glisson_storage) GlissonEngine;
  compiled[1] = new(gendy_storage) GendyEngine;
  compiled[2] = new(scanned_storage) ScannedEngine;
  compiled[3] = new(pulsar_storage) PulsarEngine;
  compiled[4] = new(loopback_storage) LoopbackEngine;
  compiled[5] = new(lockstep_storage) LockstepEngine;
  compiled[6] = new(tapfield_storage) TapfieldEngine;
  compiled[7] = new(phase_weave_storage) PhaseWeaveEngine;
  compiled[8] = new(sideband_storage) SidebandEngine;
  compiled[9] = new(attractor_storage) AttractorEngine;
  compiled[10] = new(undertow_storage) UndertowEngine;
  compiled[11] = new(reed_pipe_storage) ReedPipeEngine;
  compiled[12] = new(phase_flock_storage) PhaseFlockEngine;
  compiled[13] = new(rulefield_storage) RulefieldEngine;
  compiled[14] = new(spectral_spiral_storage) SpectralSpiralEngine;
  compiled[15] = new(z_filter_storage) ZFilterEngine;
  compiled[16] = new(toy_storage) ToyEngine;
  compiled[17] = new(csaw_storage) CSawEngine;
  compiled[18] = new(ring_mod_storage) RingModEngine;
  compiled[19] = new(fold_storage) FoldEngine;
  compiled[20] = new(buzz_storage) BuzzEngine;
  compiled[21] = new(dual_sync_storage) DualSyncEngine;
  compiled[22] = new(morph_storage) MorphEngine;
  compiled[23] = new(noise_bank_storage) NoiseBankEngine;
  compiled[24] = new(particle_burst_storage) ParticleBurstEngine;
  compiled[25] = new(saw_square_storage) SawSquareEngine;
  compiled[26] = new(saw_swarm_storage) SawSwarmEngine;
  compiled[27] = new(vowel_storage) VowelEngine;
  compiled[28] = new(harmonics_storage) HarmonicsEngine;
  compiled[29] = new(plucked_storage) PluckedEngine;
  compiled[30] = new(blown_storage) BlownEngine;
  compiled[31] = new(struck_bell_storage) StruckBellEngine;
  compiled[32] = new(struck_drum_storage) StruckDrumEngine;
  compiled[33] = new(kick_storage) KickEngine;
  compiled[34] = new(snare_storage) SnareEngine;
  compiled[35] = new(cymbal_storage) CymbalEngine;
  compiled[36] = new(wave_paraphonic_storage) WaveParaphonicEngine;
  compiled[37] = new(fluted_storage) FlutedEngine;
  compiled[38] = new(question_mark_storage) QuestionMarkEngine;
  compiled[39] = new(bowed_storage) BowedEngine;
  compiled[40] = new(sub_oscillator_storage) SubOscillatorEngine;
  compiled[41] = new(digital_modulation_storage) DigitalModulationEngine;
  compiled[42] = new(saw_comb_storage) SawCombEngine;
  compiled[43] = new(raw_fm_storage) RawFmEngine;
  compiled[44] = new(triple_storage) TripleEngine;
  compiled[45] = new(bytebeat_storage) BytebeatEngine;
  compiled[46] = new(diatonic_chord_storage) DiatonicChordEngine;
  compiled[47] = new(scale_stack_storage) ScaleStackEngine;
  compiled[48] = new(wavetable_chord_storage) WavetableChordEngine;
  compiled[49] = new(wavetable_scale_stack_storage) WavetableScaleStackEngine;
  compiled[50] = new(shakers_storage) ShakersEngine;
  compiled[51] = new(brass_storage) BrassEngine;
  compiled[52] = new(helix_storage) HelixEngine;
  compiled[53] = new(skins_storage) SkinsEngine;
  compiled[54] = new(circuit_zaps_storage) CircuitZapsEngine;
  compiled[55] = new(metalwork_storage) MetalworkEngine;
  compiled[56] = new(clap_storage) ClapEngine;
  compiled[57] = new(analog_percussion_storage) AnalogPercussionEngine;
  compiled[58] = new(freshets_formant_storage) FreshetsFormantEngine;
  compiled[59] = new(bubbletime_storage) BubbleTimeEngine;
  compiled[60] = new(acid_storage) AcidEngine;
  compiled[61] = new(chords_supersaw_storage) ChordsSupersawEngine;
  compiled[62] = new(speech_sounds_storage) SpeechSoundsEngine;
  compiled[63] = new(lpc_words_storage) LPCWordsEngine;
  compiled[64] = new(sine_storage) TestSineEngine;
  for (size_t i = 0; i < kFirstStock; ++i) {
    // All engines share the same RAM space, as in PlaitsVoice::Init.
    allocator.Free();
    compiled[i]->Init(&allocator);
  }
}

static void BuildStock() {
  std::fill(&stock_arena[0], &stock_arena[kArenaSize], 0);
  std::fill(&stock_storage[0], &stock_storage[sizeof(stock_storage)], 0);
  StockEngines* e = new(stock_storage) StockEngines;
  Engine* stock[kNumStock] = {
    &e->virtual_analog_vcf_engine, &e->phase_distortion_engine,
    &e->six_op_engine, &e->wave_terrain_engine, &e->string_machine_engine,
    &e->chiptune_engine, &e->virtual_analog_engine, &e->waveshaping_engine,
    &e->fm_engine, &e->grain_engine, &e->additive_engine,
    &e->wavetable_engine, &e->chord_engine, &e->speech_engine,
    &e->swarm_engine, &e->noise_engine, &e->particle_engine,
    &e->string_engine, &e->modal_engine, &e->bass_drum_engine,
    &e->snare_drum_engine, &e->hi_hat_engine
  };
  stmlib::BufferAllocator stock_allocator(stock_arena, kArenaSize);
  for (size_t i = 0; i < kNumStock; ++i) {
    // PlaitsVoice::Init builds the 6-op engine once per model it serves.
    const int repeat = stock[i] == &e->six_op_engine ? 3 : 1;
    for (int j = 0; j < repeat; ++j) {
      stock_allocator.Free();
      stock[i]->Init(&stock_allocator);
    }
    compiled[kFirstStock + i] = stock[i];
  }
}

// Reads the region: from the directory, or the defaults when use_directory
// is 0, then builds its libraries. Returns the PLTS status.
uint32_t harness_setup(
    const uint8_t* begin,
    const uint8_t* end,
    uint32_t release,
    uint32_t use_directory) {
  DecodeSpeechBanks();
  for (int i = 0; i < kNumFmBanks; ++i) {
    for (int j = 0; j < kFmBankSize; ++j) {
      fm_banks[i * kFmBankSize + j].Unpack(
          fm_patches_table[i] + j * fm::Patch::SYX_SIZE);
    }
  }
  BuildProof();
  BuildStock();
  new(harness_blob_voice.engine) PlaitsBlobEngine;
  harness_slot = reinterpret_cast<uintptr_t>(blob_engine()->state());

  region.set_library_ram(harness_library_ram_size, kScratchSize);
  region.Read(
      begin, end, release,
      use_directory ? harness_directory : NULL, harness_num_entries,
      harness_defaults, harness_num_defaults,
      &plaits_blob_services, harness_digests, harness_num_digests);
  harness_identity = region.identity();
  harness_num_terrains = region.num_terrains();
  harness_synth_blob = reinterpret_cast<uintptr_t>(region.synth_blob());
  harness_lock = PlaitsRegion::Lock(
      region.ok(PLAITS_SECTION_PLAITS), region.ok(PLAITS_SECTION_WAVE));
  libraries.Init(
      region, &plaits_blob_services, harness_library_ram,
      harness_library_ram_size, harness_scratch, kScratchSize);
  return harness_identity.sections[PLAITS_SECTION_PLAITS].status;
}

// A voice switching to a compiled-in engine: Reset only, after the user data
// PlaitsVoice loads, or the 6-op bank of the model.
void harness_select_compiled(uint32_t engine, uint32_t model) {
  if (engine == kSixOpSlot) {
    static_cast<SixOpEngine*>(compiled[engine])->LoadUnpackedBank(
        &fm_banks[(model - 2) * kFmBankSize]);
  } else {
    compiled[engine]->LoadUserData(
        engine == kFirstStock + 3 ? harness_user_data : NULL);
  }
  compiled[engine]->Reset();
}

// The engines built afresh, as at power-on, then one selected: the reference
// for a rebuilt blob, which equals an engine's first selection.
void harness_rebuild_compiled(uint32_t engine, uint32_t model) {
  const uint32_t seed = stmlib::Random::state();
  if (engine < kFirstStock) {
    BuildProof();
  } else {
    BuildStock();
  }
  if (model == 56) {
    stmlib::Random::Seed(seed);
  }
  harness_select_compiled(engine, model);
}

// A voice switching to a blob: bound, constructed in the zeroed slot and
// arena, then reset. Returns 1 when no checked blob serves the model.
uint32_t harness_select_blob(uint32_t model) {
  const uint8_t* blob = region.blob(model);
  if (!blob) {
    return 1;
  }
  stmlib::BufferAllocator allocator(harness_blob_voice.arena, kArenaSize);
  blob_engine()->Bind(blob, &plaits_blob_services, model, &libraries);
  blob_engine()->Init(&allocator);
  blob_engine()->Reset();
  return 0;
}

// Each render starts from the given generator state and returns the one it
// leaves, so the blob and compiled-in renders draw the same numbers.
uint32_t harness_render_compiled(uint32_t engine, uint32_t size,
    uint32_t seed) {
  bool already_enveloped = false;
  if (engine == kChiptuneSlot) {
    // As PlaitsVoice::Render sets it.
    static_cast<ChiptuneEngine*>(compiled[engine])->set_envelope_shape(
        harness_parameters.trigger & TRIGGER_UNPATCHED
            ? ChiptuneEngine::NO_ENVELOPE
            : 0.0f);
  }
  stmlib::Random::Seed(seed);
  compiled[engine]->Render(
      harness_parameters, harness_out, harness_aux, size, &already_enveloped);
  return stmlib::Random::state();
}

uint32_t harness_render_blob(uint32_t size, uint32_t seed) {
  bool already_enveloped = false;
  stmlib::Random::Seed(seed);
  blob_engine()->Render(
      harness_parameters, harness_out, harness_aux, size, &already_enveloped);
  return stmlib::Random::state();
}

}  // extern "C"
