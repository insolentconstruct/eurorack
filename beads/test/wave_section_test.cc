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
// -----------------------------------------------------------------------------
//
// Beads' synth from a WAVE section, as the region reader hands it over: bit
// for bit the compiled-in table's output, and a changed row is heard.

#include <cmath>
#include <cstring>

#include "beads/blob/plaits_region.h"
#include "beads/dsp/synth.h"
#include "beads/profile.h"
#include "beads/resources.h"
#include "beads/test/test_checker.h"

using namespace beads;

const uint32_t kRelease = 20261123;
const uint32_t kPayload = kNumWaveRows * kWaveRowSize * 2;
const uint32_t kSectionSize = 32 + kPayload + 4;
// A region offset off the table's own alignment, as Hygrostat may place it.
const uint32_t kOffset = 0x1fe0;

uint8_t storage[kOffset + kSectionSize + 64] __attribute__((aligned(32)));

void Write32(uint8_t* p, uint32_t value) {
  for (int i = 0; i < 4; ++i) {
    p[i] = value >> (8 * i);
  }
}

// WAVE as blob_sections.py writes it: header, the table, the CRC.
PlaitsDirectoryEntry WriteSection(const int16_t* table) {
  memset(storage, 0xff, sizeof(storage));
  uint8_t* s = storage + kOffset;
  const uint32_t words[8] = {
    kPlaitsSectionMagic, kPlaitsSectionWave, kPlaitsSectionVersion, kRelease,
    kSectionSize, kNumWaveRows, 0, 0
  };
  for (int i = 0; i < 8; ++i) {
    Write32(s + 4 * i, words[i]);
  }
  for (uint32_t i = 0; i < kPayload / 2; ++i) {
    s[32 + 2 * i] = uint16_t(table[i]);
    s[32 + 2 * i + 1] = uint16_t(table[i]) >> 8;
  }
  const uint32_t crc = Profile::Crc32(s, kSectionSize - 4);
  Write32(s + kSectionSize - 4, crc);
  PlaitsDirectoryEntry e = { kPlaitsSectionWave, kOffset, kSectionSize, crc };
  return e;
}

const int16_t* ReadWaves(const PlaitsDirectoryEntry& e) {
  static PlaitsRegion region;
  region.Read(
      storage, storage + sizeof(storage), kRelease, &e, 1, NULL, 0,
      &plaits_blob_services, NULL, 0);
  return region.waves();
}

// Hash output bits across banks, wave positions, pitch, size and shape,
// with seeded grains and drone: dry_wet 0 is drone only, 1 is grains only.
uint32_t Render(const int16_t* waves, float dry_wet, size_t* nonzero) {
  integrated_waves = waves;
  static uint16_t reverb_buffer[32768];
  static Synth synth;
  memset(reverb_buffer, 0, sizeof(reverb_buffer));
  synth.Init(reverb_buffer);
  stmlib::Random::Seed(0x21);
  Parameters p;
  memset(&p, 0, sizeof(p));
  Processor::MixParameters mix;
  memset(&mix, 0, sizeof(mix));
  mix.dry_wet = dry_wet;
  ModulatedParameter density;
  density.base = 0.8f;
  density.cv = 0.0f;
  const size_t kSize = 12;
  stmlib::GateFlags seed[kSize];
  bool freeze[kSize] = { };
  FloatFrame out[kSize];
  uint32_t hash = 2166136261u;
  *nonzero = 0;
  const int kBlocks = 4000;
  for (int block = 0; block < kBlocks; ++block) {
    const float t = float(block) / kBlocks;
    mix.feedback = t;
    p.time.base = 0.5f + 0.5f * sinf(t * 97.0f);
    p.pitch.base = 24.0f * sinf(t * 31.0f);
    p.size.base = 0.5f + 0.5f * sinf(t * 13.0f);
    p.shape.base = 0.5f + 0.5f * sinf(t * 7.0f);
    for (size_t i = 0; i < kSize; ++i) {
      seed[i] = (block % 50) == 0 && i == 0 ? stmlib::GATE_FLAG_HIGH
          : stmlib::GATE_FLAG_LOW;
    }
    synth.Render(SEED_BUTTON_STATE_OFF, seed, freeze,
        Synth::Resolution(block / 1000), density, p, mix, out, kSize);
    for (size_t i = 0; i < kSize; ++i) {
      uint32_t bits[2];
      memcpy(bits, &out[i], sizeof(bits));
      for (int c = 0; c < 2; ++c) {
        hash = (hash ^ bits[c]) * 16777619u;
      }
      *nonzero += out[i].l != 0.0f;
    }
  }
  return hash;
}

void TestSectionTable() {
  test_checker.Start("TestSectionTable");
  const PlaitsDirectoryEntry e = WriteSection(wav_integrated_waves);
  const int16_t* waves = ReadWaves(e);
  CHECK(waves == reinterpret_cast<const int16_t*>(storage + kOffset + 32));
  CHECK(waves && waves != wav_integrated_waves &&
      !memcmp(waves, wav_integrated_waves, kPayload));
  CHECK(WAV_INTEGRATED_WAVES_SIZE * 2 == kPayload);
}

void TestSectionRender() {
  test_checker.Start("TestSectionRender");
  size_t compiled_nonzero, section_nonzero;
  const uint32_t compiled = Render(
      wav_integrated_waves, 0.5f, &compiled_nonzero);
  const int16_t* waves = ReadWaves(WriteSection(wav_integrated_waves));
  CHECK(waves);
  const uint32_t section = Render(waves, 0.5f, &section_nonzero);
  printf("compiled 0x%08x (%zu nonzero), section 0x%08x\n",
      compiled, compiled_nonzero, section);
  CHECK(compiled_nonzero > 15000);
  CHECK(section == compiled && section_nonzero == compiled_nonzero);

  // Control: every wave halved (guards kept) changes the drone and the
  // grains, each on its own.
  static int16_t changed[WAV_INTEGRATED_WAVES_SIZE];
  for (uint32_t i = 0; i < WAV_INTEGRATED_WAVES_SIZE; ++i) {
    changed[i] = wav_integrated_waves[i] / 2;
  }
  waves = ReadWaves(WriteSection(changed));
  CHECK(waves);
  size_t n;
  for (int i = 0; i < 2; ++i) {
    const float dry_wet = float(i);
    const uint32_t reference = Render(wav_integrated_waves, dry_wet, &n);
    const size_t reference_nonzero = n;
    const uint32_t changed_hash = Render(waves, dry_wet, &n);
    printf("dry_wet %.0f: compiled 0x%08x (%zu nonzero), changed 0x%08x "
        "(%zu)\n", dry_wet, reference, reference_nonzero, changed_hash, n);
    CHECK(reference_nonzero > 5000);
    CHECK(changed_hash != reference);
  }
}

int main() {
  TestSectionTable();
  TestSectionRender();
  integrated_waves = NULL;
  return test_checker.failures() ? 1 : 0;
}
