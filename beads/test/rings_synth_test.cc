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
// The Rings synth core and Beads' controls for it: sizes, zeroed RAM,
// configuration, control mapping, strum precedence and the pitch bound.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "stmlib/utils/random.h"

#include "beads/blob/plaits_blob.h"
#include "beads/blob/synth_choice.h"
#include "beads/dsp/rings_synth.h"
#include "beads/test/rings_core_host.h"
#include "beads/test/test_checker.h"

using namespace beads;
using namespace rings;
using namespace std;

const size_t kBlocks = 400;
const uint8_t kPoison = 0xa5;

// The core in its own RAM, as the blob's init places it.
struct Ram {
  Ram(uint8_t fill) {
    bytes = static_cast<uint8_t*>(malloc(CoreSize()));
    memset(bytes, fill, CoreSize());
  }
  ~Ram() { free(bytes); }
  void* Build() {
    return HostInit(bytes, CoreSize(), NULL);
  }
  uint8_t* bytes;
};

void Controls(size_t block, RingsSynthControls* c) {
  c->patch.structure = 0.4f;
  c->patch.brightness = 0.7f;
  c->patch.damping = 0.8f;
  c->patch.position = 0.3f;
  PerformanceState* s = &c->performance_state;
  s->internal_exciter = true;
  s->internal_note = true;
  s->internal_strum = false;
  s->strum = block % 100 == 0;
  s->tonic = kRingsRootNote;
  const float notes[4] = { 0.0f, 7.0f, 12.0f, 5.0f };
  s->note = notes[(block / 100) % 4];
  s->fm = 0.0f;
  s->chord = 3;
}

// Renders a target; returns a checksum of every output word.
uint64_t Render(void* core, const RingsSynthTarget& target) {
  stmlib::Random::Seed(1);
  RingsSynthControls c;
  Controls(0, &c);
  HostConfigure(core, &target, &c, true);
  uint64_t sum = 0;
  for (size_t b = 0; b < kBlocks; ++b) {
    float out[kRingsBlockSize];
    float aux[kRingsBlockSize];
    Controls(b, &c);
    HostRender(core, &c, out, aux, kRingsBlockSize);
    for (size_t i = 0; i < kRingsBlockSize; ++i) {
      uint32_t o, a;
      memcpy(&o, &out[i], 4);
      memcpy(&a, &aux[i], 4);
      sum = sum * 1000003u + o;
      sum = sum * 1000003u + a;
    }
  }
  return sum;
}

RingsSynthTarget Target(int core, int model, int polyphony, int fx) {
  RingsSynthTarget t;
  t.core = core;
  t.model = model;
  t.polyphony = polyphony;
  t.fx = fx;
  return t;
}

void TestSizes() {
  test_checker.Start("TestSizes");
  CHECK(CoreBlockSize() == kRingsBlockSize);
  CHECK(CoreSize() <= 256 * 1024);
  CHECK(CoreSizesAddUp());
}

void TestZeroedRam() {
  test_checker.Start("TestZeroedRam");
  const RingsSynthTarget targets[] = {
    Target(RINGS_CORE_PART, 0, 1, kRingsDefaultFx),
    Target(RINGS_CORE_PART, 2, 4, kRingsDefaultFx),
    Target(RINGS_CORE_PART, 3, 2, kRingsDefaultFx),
    Target(RINGS_CORE_STRING_SYNTH, 0, 2, 2),
  };
  for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); ++i) {
    Ram fresh(0);
    // A bank another synth used, zeroed as the loader does before init.
    Ram used(kPoison);
    memset(used.bytes, 0, CoreSize());
    const uint64_t expected = Render(fresh.Build(), targets[i]);
    // Without it, members Rings never sets (a string synth envelope's shape)
    // would keep the old bytes: the sanitizers stop on the first read.
    CHECK(Render(used.Build(), targets[i]) == expected);
  }
}

void TestConfigure() {
  test_checker.Start("TestConfigure");
  Ram ram(0);
  void* core = ram.Build();
  RingsSynthControls c;
  Controls(0, &c);
  const RingsSynthTarget good = Target(RINGS_CORE_STRING_SYNTH, 3, 2, 5);
  CHECK(HostConfigure(core, &good, &c, true));
  // Those Rings could not render come last: a fault there stops the test.
  const RingsSynthTarget bad[] = {
    Target(RINGS_CORE_LAST, 0, 1, 0),
    Target(RINGS_CORE_PART, 0, 3, 0),
    Target(RINGS_CORE_PART, 0, 8, 0),
    Target(RINGS_CORE_PART, 0, 1, kNumRingsFx),
    Target(RINGS_CORE_PART, kNumRingsModels, 1, 0),
    Target(RINGS_CORE_PART, 0, 0, 0),
  };
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
    CHECK(!CoreValid(bad[i]));
    CHECK(!HostConfigure(core, &bad[i], &c, false));
    CHECK(!memcmp(&CoreTarget(core), &good, sizeof(good)));
  }
  for (int p = 1; p <= 4; p *= 2) {
    CHECK(CoreValid(Target(RINGS_CORE_PART, 5, p, 5)));
  }
}

void TestControlMapping() {
  test_checker.Start("TestControlMapping");
  RingsSynth synth;
  synth.Init();
  RingsSynthControls c;
  synth.ComputeControls(
      0.0f, 1.0f, 1.2f, -0.1f, 0.5f, false, false, false, false, &c);
  CHECK(c.patch.structure == kRingsMaxStructure);
  CHECK(c.patch.brightness == 1.0f);
  CHECK(c.patch.position == 0.0f);
  CHECK(c.patch.damping == 0.5f);
  CHECK(c.performance_state.tonic == kRingsRootNote);
  CHECK(c.performance_state.fm == 0.0f);
  CHECK(c.performance_state.internal_exciter);
  CHECK(c.performance_state.chord == kNumChords - 1);
  // FREEZE holds the longest decay.
  synth.ComputeControls(
      0.0f, 0.5f, 0.5f, 0.5f, 0.2f, true, false, false, false, &c);
  CHECK(c.patch.damping == 1.0f);
  // The pitch stays inside Rings' own reach.
  synth.ComputeControls(
      200.0f, 0.5f, 0.5f, 0.5f, 0.2f, false, false, false, false, &c);
  CHECK(c.performance_state.tonic + c.performance_state.note ==
      kRingsHighestNote);
  synth.ComputeControls(
      -200.0f, 0.5f, 0.5f, 0.5f, 0.2f, false, false, false, false, &c);
  CHECK(c.performance_state.tonic + c.performance_state.note ==
      kRingsLowestNote);
  synth.ComputeControls(
      7.0f, 0.5f, 0.5f, 0.5f, 0.2f, false, false, false, false, &c);
  CHECK(c.performance_state.note == 7.0f);

  // Chords: Rings' 0.1 hysteresis between the eleven steps.
  synth.ComputeControls(
      0.0f, 0.0f, 0.5f, 0.5f, 0.5f, false, false, false, false, &c);
  CHECK(c.performance_state.chord == 0);
  synth.ComputeControls(
      0.0f, 0.059f, 0.5f, 0.5f, 0.5f, false, false, false, false, &c);
  CHECK(c.performance_state.chord == 0);
  synth.ComputeControls(
      0.0f, 0.061f, 0.5f, 0.5f, 0.5f, false, false, false, false, &c);
  CHECK(c.performance_state.chord == 1);
  synth.ComputeControls(
      0.0f, 0.041f, 0.5f, 0.5f, 0.5f, false, false, false, false, &c);
  CHECK(c.performance_state.chord == 1);
  synth.ComputeControls(
      0.0f, 0.039f, 0.5f, 0.5f, 0.5f, false, false, false, false, &c);
  CHECK(c.performance_state.chord == 0);

  // FEEDBACK: twelve zones, models then FX; the idle core keeps its own.
  RingsSynthTarget t;
  synth.ComputeTarget(0.0f, 1, &t);
  CHECK(t.core == RINGS_CORE_PART && t.model == 0 && t.fx == kRingsDefaultFx);
  synth.ComputeTarget(2.5f / 12.0f, 2, &t);
  CHECK(t.core == RINGS_CORE_PART && t.model == 2 && t.polyphony == 2);
  synth.ComputeTarget(1.0f, 4, &t);
  CHECK(t.core == RINGS_CORE_STRING_SYNTH && t.model == 2 && t.fx == 5);
  CHECK(t.polyphony == 4);
  synth.ComputeTarget(6.5f / 12.0f, 3, &t);
  CHECK(t.core == RINGS_CORE_STRING_SYNTH && t.fx == 0 && t.polyphony == 1);
  synth.ComputeTarget(5.5f / 12.0f, 1, &t);
  CHECK(t.core == RINGS_CORE_PART && t.model == 5 && t.fx == 0);
  CHECK(CoreValid(t));
  // Hysteresis at a zone edge.
  synth.ComputeTarget(6.0f / 12.0f + 0.003f, 1, &t);
  CHECK(t.core == RINGS_CORE_PART && t.model == 5);
  synth.ComputeTarget(6.0f / 12.0f + 0.005f, 1, &t);
  CHECK(t.core == RINGS_CORE_STRING_SYNTH);
  int previous = -1;
  for (int i = 0; i <= 1000; ++i) {
    synth.ComputeTarget(float(i) / 1000.0f, 1, &t);
    const int zone = t.core == RINGS_CORE_PART ? t.model : 6 + t.fx;
    CHECK(zone >= previous);
    CHECK(CoreValid(t));
    previous = zone;
  }
  CHECK(previous == kNumRingsZones - 1);
}

// One block through the core's strummer; returns whether it strummed.
bool Strum(RingsSynth* synth, void* core, float pitch, bool trigger,
           bool seed_patched, bool pitch_patched) {
  RingsSynthControls c;
  synth->ComputeControls(pitch, 0.4f, 0.5f, 0.5f, 0.5f, false, trigger,
                         seed_patched, pitch_patched, &c);
  float out[kRingsBlockSize];
  float aux[kRingsBlockSize];
  HostRender(core, &c, out, aux, kRingsBlockSize);
  return c.performance_state.strum;
}

void TestStrumPrecedence() {
  test_checker.Start("TestStrumPrecedence");
  // Rings' strummer inhibits strums for 10 ms after one: 40 blocks of 12.
  const int inhibit = 40;
  for (int core_type = 0; core_type < RINGS_CORE_LAST; ++core_type) {
    for (int seed_patched = 0; seed_patched < 2; ++seed_patched) {
      for (int pitch_patched = 0; pitch_patched < 2; ++pitch_patched) {
        Ram ram(0);
        void* core = ram.Build();
        RingsSynth synth;
        synth.Init();
        RingsSynthControls c;
        synth.ComputeControls(0.0f, 0.4f, 0.5f, 0.5f, 0.5f, false, false,
                              seed_patched, pitch_patched, &c);
        const RingsSynthTarget first = Target(core_type, 0, 1, 0);
        HostConfigure(core, &first, &c, true);
        // Settle the strummer's previous note.
        for (int i = 0; i < 2 * inhibit; ++i) {
          Strum(&synth, core, 0.0f, false, seed_patched, pitch_patched);
        }
        // A PITCH step strums only with PITCH patched and SEED unpatched.
        const bool step = Strum(&synth, core, 7.0f, false, seed_patched,
                                pitch_patched);
        CHECK(step == (pitch_patched && !seed_patched));
        for (int i = 0; i < 2 * inhibit; ++i) {
          CHECK(!Strum(&synth, core, 7.0f, false, seed_patched,
                       pitch_patched));
        }
        // A trigger strums whatever is patched; with a step, once.
        CHECK(Strum(&synth, core, 7.0f, true, seed_patched, pitch_patched));
        for (int i = 0; i < inhibit; ++i) {
          CHECK(!Strum(&synth, core, i == 5 ? 0.0f : 7.0f, i == 10,
                       seed_patched, pitch_patched));
        }
        CHECK(Strum(&synth, core, 12.0f, true, seed_patched, pitch_patched));
        for (int i = 0; i < inhibit - 1; ++i) {
          CHECK(!Strum(&synth, core, 12.0f, false, seed_patched,
                       pitch_patched));
        }
        CHECK(!Strum(&synth, core, 12.0f, true, seed_patched, pitch_patched));
        // Past the inhibit, the next trigger plays.
        CHECK(Strum(&synth, core, 12.0f, true, seed_patched, pitch_patched));
      }
    }
  }
}

// The active chord tables' range of offsets, read from Rings' source.
void ChordOffsets(const char* path, float* lowest, float* highest) {
  ifstream file(path);
  stringstream text;
  text << file.rdbuf();
  const string s = text.str();
  const size_t begin = s.find("#ifdef BRYAN_CHORDS");
  const size_t end = s.find("#else", begin);
  CHECK(begin != string::npos && end != string::npos);
  string table = s.substr(begin, end - begin);
  table = table.substr(table.find('{'));
  *lowest = 1000.0f;
  *highest = -1000.0f;
  int count = 0;
  for (size_t i = 0; i < table.size(); ++i) {
    if (table.compare(i, 2, "//") == 0) {
      i = table.find('\n', i);
      continue;
    }
    if (isdigit(table[i]) || table[i] == '-') {
      char* stop;
      const float value = strtof(&table[i], &stop);
      *lowest = min(*lowest, value);
      *highest = max(*highest, value);
      i = stop - table.c_str() - 1;
      ++count;
    }
  }
  CHECK(count > 100);
}

// SemitonesToRatio's index stays in its 257-entry table.
bool InTable(float semitones) {
  const float pitch = semitones + 128.0f;
  return pitch >= 0.0f && pitch < 257.0f;
}

void TestPitchBound() {
  test_checker.Start("TestPitchBound");
  ifstream dsp("rings/dsp/dsp.h");
  stringstream text;
  text << dsp.rdbuf();
  CHECK(text.str().find("\n#define BRYAN_CHORDS\n") != string::npos);
  float part_low, part_high, synth_low, synth_high;
  ChordOffsets("rings/dsp/part.cc", &part_low, &part_high);
  ChordOffsets("rings/dsp/string_synth_part.cc", &synth_low, &synth_high);
  printf("chord offsets: Part %+.2f to %+.2f, Disastrous peace %+.2f to "
         "%+.2f\n", part_low, part_high, synth_low, synth_high);
  // Sympathetic strings: notes - 12 to + 24, detuned by up to 0.017.
  const float lowest = min(min(part_low, synth_low), -12.0f);
  const float highest = max(max(part_high, synth_high), 24.0f + 0.017f);
  // Every note Rings computes is tonic + note + offset - 69 (fm is 0); the
  // note filter only interpolates between notes it was given.
  CHECK(InTable(kRingsLowestNote + lowest - 69.0f));
  CHECK(InTable(kRingsHighestNote + highest - 69.0f));
  CHECK(InTable(kRingsRootNote - 69.0f));
  // A negative control: Beads' pitch without the clamp could leave it.
  CHECK(!InTable(kRingsRootNote + 160.0f + highest - 69.0f));

  // Every model and FX at both extremes, under the sanitizers.
  for (int extreme = 0; extreme < 2; ++extreme) {
    for (int zone = 0; zone < kNumRingsZones; ++zone) {
      for (int p = 1; p <= 4; p *= 2) {
        Ram ram(0);
        void* core = ram.Build();
        RingsSynth synth;
        synth.Init();
        RingsSynthTarget t;
        synth.ComputeTarget((float(zone) + 0.5f) / kNumRingsZones, p, &t);
        RingsSynthControls c;
        const float pitch = extreme ? 500.0f : -500.0f;
        synth.ComputeControls(pitch, 0.9f, 1.0f, 0.5f, 0.5f, false, true,
                              false, true, &c);
        HostConfigure(core, &t, &c, true);
        for (int b = 0; b < 50; ++b) {
          float out[kRingsBlockSize];
          float aux[kRingsBlockSize];
          synth.ComputeControls(pitch, float(b % 11) / 10.0f, 1.0f, 0.5f,
                                0.5f, false, b % 10 == 0, false, true, &c);
          HostRender(core, &c, out, aux, kRingsBlockSize);
          for (size_t i = 0; i < kRingsBlockSize; ++i) {
            CHECK(std::isfinite(out[i]) && std::isfinite(aux[i]));
          }
        }
      }
    }
  }
}

// A synth header as the region gives it.
PlaitsBlobSynthHeader SynthHeader() {
  PlaitsBlobSynthHeader h;
  memset(&h, 0, sizeof(h));
  h.kind = kPlaitsBlobKindSynth;
  h.id = kRingsSynthId;
  h.ram_size = CoreSize();
  h.controls_size = sizeof(RingsSynthControls);
  h.block_size = kRingsBlockSize;
  return h;
}

inline const uint8_t* Bytes(const PlaitsBlobSynthHeader& h) {
  return reinterpret_cast<const uint8_t*>(&h);
}

void TestRingsSynthApp() {
  test_checker.Start("TestRingsSynthApp");
  const PlaitsBlobSynthHeader good = SynthHeader();
  CHECK(RingsSynth::Fits(Bytes(good), CoreSize()));
  CHECK(!RingsSynth::Fits(Bytes(good), CoreSize() - 1));
  CHECK(!RingsSynth::Fits(NULL, 1 << 20));
  PlaitsBlobSynthHeader bad = good;
  bad.block_size = 24;
  CHECK(!RingsSynth::Fits(Bytes(bad), 1 << 20));
  bad = good;
  bad.controls_size = 40;
  CHECK(!RingsSynth::Fits(Bytes(bad), 1 << 20));
  bad = good;
  bad.id = 2;
  CHECK(!RingsSynth::Fits(Bytes(bad), 1 << 20));
  bad = good;
  bad.kind = kPlaitsBlobKindModel;
  CHECK(!RingsSynth::Fits(Bytes(bad), 1 << 20));

  static uint16_t reverb_buffer[32768];
  Ram ram(kPoison);
  RingsSynth synth;
  synth.Init(reverb_buffer);
  synth.set_entries(&HostInit, &HostConfigure, &HostRender);
  synth.set_polyphony(2);
  configure_calls = render_calls = 0;
  synth.InitVoices(ram.bytes, CoreSize(), NULL, 0.0f);
  CHECK(configure_calls == 1);
  CHECK(!synth.configure_pending());

  Parameters p;
  memset(&p, 0, sizeof(p));
  p.time.base = p.size.base = p.shape.base = 0.5f;
  ModulatedParameter density;
  memset(&density, 0, sizeof(density));
  density.base = 0.5f;
  Processor::MixParameters mix;
  memset(&mix, 0, sizeof(mix));
  mix.dry_wet = 0.7f;
  mix.reverb_amount = 0.2f;
  bool freeze[kRingsBlockSize];
  std::fill(&freeze[0], &freeze[kRingsBlockSize], false);
  FloatFrame out[kRingsBlockSize];
  float peak = 0.0f;
  // SEED presses strum; nothing reconfigures while the zone holds.
  for (int b = 0; b < 2000; ++b) {
    const SeedButtonState seed = b % 400 == 0
        ? SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
    synth.Render(seed, NULL, freeze, Synth::RESOLUTION_16_BIT, density, p,
                 mix, out, kRingsBlockSize);
    for (size_t i = 0; i < kRingsBlockSize; ++i) {
      peak = max(peak, fabsf(out[i].l) + fabsf(out[i].r));
    }
  }
  CHECK(peak > 0.01f);
  CHECK(render_calls == 2000);
  CHECK(configure_calls == 1);
  CHECK(!synth.configure_pending());

  // A new zone is requested only once held 30 ms; render never applies it.
  mix.feedback = 1.0f;
  for (int b = 0; b < kRingsSettleBlocks; ++b) {
    synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze,
                 Synth::RESOLUTION_16_BIT, density, p, mix, out,
                 kRingsBlockSize);
    CHECK(!synth.configure_pending());
  }
  synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze, Synth::RESOLUTION_16_BIT,
               density, p, mix, out, kRingsBlockSize);
  CHECK(synth.configure_pending());
  CHECK(configure_calls == 1);
  // A move during the fade withdraws the request until it settles again.
  mix.feedback = 0.0f;
  synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze, Synth::RESOLUTION_16_BIT,
               density, p, mix, out, kRingsBlockSize);
  CHECK(!synth.configure_pending());
  synth.Configure();
  CHECK(configure_calls == 1);
  mix.feedback = 1.0f;
  for (int b = 0; b <= kRingsSettleBlocks; ++b) {
    synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze,
                 Synth::RESOLUTION_16_BIT, density, p, mix, out,
                 kRingsBlockSize);
  }
  CHECK(synth.configure_pending());
  synth.Configure();
  CHECK(configure_calls == 2);
  CHECK(!synth.configure_pending());
  void* core = ram.bytes;
  CHECK(CoreTarget(core).core == RINGS_CORE_STRING_SYNTH);
  CHECK(CoreTarget(core).fx == 5 && CoreTarget(core).polyphony == 2);

  // A sweep through several zones configures once, after it settles.
  for (int b = 0; b < 600; ++b) {
    mix.feedback = b < 300 ? float(b) / 300.0f : 0.1f;
    synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze,
                 Synth::RESOLUTION_16_BIT, density, p, mix, out,
                 kRingsBlockSize);
    if (synth.configure_pending()) {
      synth.Configure();
    }
  }
  CHECK(configure_calls == 3);
  CHECK(CoreTarget(core).core == RINGS_CORE_PART);
  CHECK(CoreTarget(core).model == 1);

  // Polyphony 4 through feature 42's value.
  synth.set_polyphony(4);
  for (int b = 0; b <= kRingsSettleBlocks; ++b) {
    synth.Render(SEED_BUTTON_STATE_OFF, NULL, freeze,
                 Synth::RESOLUTION_16_BIT, density, p, mix, out,
                 kRingsBlockSize);
  }
  CHECK(synth.configure_pending());
  synth.Configure();
  CHECK(CoreTarget(core).polyphony == 4);

  // Rings' own output, quantized, times one gain: no other change while
  // the limiters idle (a fresh synth, no reverb, width off: Rings' pair).
  Ram exact_ram(0);
  RingsSynth exact;
  static uint16_t exact_reverb[32768];
  exact.Init(exact_reverb);
  exact.set_entries(&HostInit, &HostConfigure, &HostRender);
  exact.InitVoices(exact_ram.bytes, CoreSize(), NULL, 2.5f / 12.0f);
  mix.feedback = 2.5f / 12.0f;
  mix.reverb_amount = 0.0f;
  float worst = 0.0f;
  float loudest = 0.0f;
  float loudest_aux = 0.0f;
  for (int b = 0; b < 400; ++b) {
    exact.Render(b % 100 ? SEED_BUTTON_STATE_OFF : SEED_BUTTON_STATE_ON,
                 NULL, freeze, Synth::RESOLUTION_16_BIT, density, p, mix,
                 out, kRingsBlockSize);
    for (size_t i = 0; i < kRingsBlockSize && b >= 10; ++i) {
      const float a = float(int32_t(last_out[i] * 65536.0f)) / 65536.0f;
      const float c = float(int32_t(last_aux[i] * 65536.0f)) / 65536.0f;
      worst = max(worst, fabsf(out[i].l - a * kRingsGain));
      worst = max(worst, fabsf(out[i].r - c * kRingsGain));
      loudest = max(loudest, fabsf(out[i].l));
      loudest_aux = max(loudest_aux, fabsf(c));
    }
  }
  CHECK(loudest > 0.05f && loudest < 0.9f);
  CHECK(loudest_aux > 0.01f);
  CHECK(worst < 1e-6f);

  // Unbound: silence, never a call.
  RingsSynth unbound;
  unbound.Init(reverb_buffer);
  const uint32_t calls = render_calls;
  unbound.Render(SEED_BUTTON_STATE_ON, NULL, freeze, Synth::RESOLUTION_16_BIT,
                 density, p, mix, out, kRingsBlockSize);
  CHECK(render_calls == calls && out[0].l == 0.0f && out[0].r == 0.0f);
}

// RINGS-SYNTH-PLAN.md section 7: every availability against each selection.
void TestSynthChoice() {
  test_checker.Start("TestSynthChoice");
  for (int wave = 0; wave < 2; ++wave) {
    for (int plaits = 0; plaits < 2; ++plaits) {
      for (int rings = 0; rings < 2; ++rings) {
        const uint8_t set = SynthMask(wave, plaits, rings);
        const SynthEngine fallback = wave ? SYNTH_BEADS
            : plaits ? SYNTH_PLAITS : SYNTH_SILENT;
        CHECK(EffectiveSynth(SYNTH_RINGS, set) ==
              (rings ? SYNTH_RINGS : fallback));
        CHECK(EffectiveSynth(SYNTH_BEADS, set) ==
              (wave ? SYNTH_BEADS : fallback));
        CHECK(EffectiveSynth(SYNTH_PLAITS, set) ==
              (plaits ? SYNTH_PLAITS : fallback));
        // A value an older or newer manifest gave: as not playable.
        CHECK(EffectiveSynth(7, set) == fallback);
        // Actions step through the playable synths only.
        for (int v = 0; v < SYNTH_LAST; ++v) {
          const uint8_t next = NextSynth(v, set);
          CHECK(!set || (set & (1 << next)));
        }
      }
    }
  }
  const uint8_t all = SynthMask(true, true, true);
  CHECK(NextSynth(SYNTH_BEADS, all) == SYNTH_PLAITS);
  CHECK(NextSynth(SYNTH_RINGS, all) == SYNTH_BEADS);
  CHECK(NextSynth(SYNTH_BEADS, SynthMask(true, false, true)) == SYNTH_RINGS);
}

int main() {
  // Errors reach the log even if a sanitizer stops the program after them.
  setvbuf(stdout, NULL, _IONBF, 0);
  TestSizes();
  TestZeroedRam();
  TestConfigure();
  TestControlMapping();
  TestStrumPrecedence();
  TestPitchBound();
  TestRingsSynthApp();
  TestSynthChoice();
  return test_checker.failures() ? 1 : 0;
}
