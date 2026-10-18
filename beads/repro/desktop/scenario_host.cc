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
// Repro kit desktop driver: feeds one stimulus file through the processor.
//
// Usage: scenario_host STIMULUS OUT_PREFIX [key=value ...]
// Writes OUT_PREFIX.f32 (interleaved stereo float32) and OUT_PREFIX.delay
// (one byte per block, 1 while the processor is in delay mode). Keys are the
// panel settings listed in kKeys; unknown keys are an error. Runs on x86
// floats, so results are compared as observables, never as hashes.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "beads/dsp/processor.h"
#undef private

#include "stmlib/utils/gate_flags.h"
#include "stmlib/utils/random.h"

using namespace beads;
using namespace std;
using namespace stmlib;

const uint32_t kMagic = 0x4f525052;  // "RPRO", little endian.
const size_t kBlockSize = 12;
const int kNumCvTracks = 6;

struct Stimulus {
  uint32_t frames;
  vector<float> audio;                  // Interleaved L R.
  vector<float> cv[kNumCvTracks];       // pitch density size shape assign time
  vector<uint8_t> seed;
  vector<uint8_t> freeze;
};

enum CvTrack { CV_PITCH, CV_DENSITY, CV_SIZE, CV_SHAPE, CV_ASSIGN, CV_TIME };

struct Setting {
  const char* name;
  float value;
};

// Panel defaults: every knob at noon, attenuverters fully clockwise.
static Setting settings[] = {
  { "media", 3 }, { "mono", 0 }, { "density", 0.5f }, { "size", 0.5f },
  { "shape", 0.5f }, { "time", 0.5f }, { "pitch", 0.0f },
  { "size_att", 1.0f }, { "shape_att", 1.0f }, { "time_att", 1.0f },
  { "pitch_att", 1.0f }, { "dry_wet", 1.0f }, { "feedback", 0.0f },
  { "reverb", 0.0f }, { "seed_button", 0 }, { "seed", 1 },
};

static float Get(const char* name) {
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
    if (!strcmp(settings[i].name, name)) {
      return settings[i].value;
    }
  }
  fprintf(stderr, "internal: no setting %s\n", name);
  exit(2);
}

static bool Set(const char* arg) {
  const char* eq = strchr(arg, '=');
  if (!eq) {
    return false;
  }
  const string key(arg, eq - arg);
  for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); ++i) {
    if (key == settings[i].name) {
      settings[i].value = strtof(eq + 1, NULL);
      return true;
    }
  }
  return false;
}

template<typename T>
static bool ReadArray(FILE* fp, vector<T>* v, size_t count) {
  v->resize(count);
  return fread(&(*v)[0], sizeof(T), count, fp) == count;
}

static bool ReadStimulus(const char* path, Stimulus* s) {
  FILE* fp = fopen(path, "rb");
  if (!fp) {
    return false;
  }
  uint32_t header[2];
  bool ok = fread(header, sizeof(header), 1, fp) == 1 && header[0] == kMagic;
  if (ok) {
    s->frames = header[1];
    ok = ReadArray(fp, &s->audio, size_t(s->frames) * 2);
    for (int i = 0; ok && i < kNumCvTracks; ++i) {
      ok = ReadArray(fp, &s->cv[i], s->frames);
    }
    ok = ok && ReadArray(fp, &s->seed, s->frames);
    ok = ok && ReadArray(fp, &s->freeze, s->frames);
  }
  fclose(fp);
  return ok;
}

static uint8_t banks[3][262144];
static uint16_t reverb_buffer[32768];
static Processor processor;

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s STIMULUS OUT_PREFIX [key=value ...]\n", argv[0]);
    return 2;
  }
  for (int i = 3; i < argc; ++i) {
    if (!Set(argv[i])) {
      fprintf(stderr, "bad setting: %s\n", argv[i]);
      return 2;
    }
  }
  Stimulus s;
  if (!ReadStimulus(argv[1], &s)) {
    fprintf(stderr, "cannot read stimulus %s\n", argv[1]);
    return 2;
  }
  const string prefix(argv[2]);
  FILE* out_fp = fopen((prefix + ".f32").c_str(), "wb");
  FILE* delay_fp = fopen((prefix + ".delay").c_str(), "wb");
  if (!out_fp || !delay_fp) {
    fprintf(stderr, "cannot write %s\n", argv[2]);
    return 2;
  }

  uint8_t* buffers[3] = { banks[0], banks[1], banks[2] };
  memset(static_cast<void*>(&processor), 0, sizeof(processor));
  processor.Init(buffers, reverb_buffer);
  processor.Configure(
      Processor::Media(int(Get("media"))), Get("mono") != 0.0f);
  Random::Seed(uint32_t(Get("seed")));

  Parameters parameters;
  memset(&parameters, 0, sizeof(parameters));
  parameters.time.base = Get("time");
  parameters.time.attenuverter = Get("time_att");
  parameters.size.base = Get("size");
  parameters.size.attenuverter = Get("size_att");
  parameters.shape.base = Get("shape");
  parameters.shape.attenuverter = Get("shape_att");
  parameters.pitch.base = Get("pitch");
  parameters.pitch.attenuverter = Get("pitch_att");
  // The bench patches a CV into every jack, so the attenuverters scale CV.
  parameters.time.has_cv = true;
  parameters.size.has_cv = true;
  parameters.shape.has_cv = true;
  parameters.pitch.has_cv = true;

  ModulatedParameter density;
  memset(&density, 0, sizeof(density));
  density.base = Get("density");

  Processor::MixParameters mix;
  memset(&mix, 0, sizeof(mix));
  mix.dry_wet = Get("dry_wet");
  mix.feedback = Get("feedback");
  mix.reverb_amount = Get("reverb");

  const SeedButtonState seed_state = SeedButtonState(int(Get("seed_button")));
  GateFlags previous = GATE_FLAG_LOW;
  const size_t blocks = s.frames / kBlockSize;
  for (size_t b = 0; b < blocks; ++b) {
    const size_t pos = b * kBlockSize;
    FloatFrame in[kBlockSize];
    FloatFrame out[kBlockSize];
    GateFlags seed_flags[kBlockSize];
    bool freeze[kBlockSize];
    for (size_t i = 0; i < kBlockSize; ++i) {
      in[i].l = s.audio[2 * (pos + i)];
      in[i].r = s.audio[2 * (pos + i) + 1];
      seed_flags[i] = previous = ExtractGateFlags(previous, s.seed[pos + i]);
      freeze[i] = s.freeze[pos + i] != 0;
    }
    // CV targets are settled values, sampled once per block.
    parameters.pitch.cv = s.cv[CV_PITCH][pos];
    density.cv = s.cv[CV_DENSITY][pos];
    parameters.size.cv = s.cv[CV_SIZE][pos];
    parameters.shape.cv = s.cv[CV_SHAPE][pos];
    parameters.time.cv = s.cv[CV_TIME][pos];
    // Process() picks its mode from the state at block entry.
    const uint8_t delay = processor.delay_mode_transition_ > 0.0f ? 1 : 0;
    processor.Process(seed_state, seed_flags, freeze, density, parameters,
        mix, in, out, kBlockSize);
    fwrite(out, sizeof(FloatFrame), kBlockSize, out_fp);
    fwrite(&delay, 1, 1, delay_fp);
  }
  fclose(out_fp);
  fclose(delay_fp);
  return 0;
}
