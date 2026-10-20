// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct
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
// Host render: every oscillator, engine and voice test, then every engine
// through the voice, with and without a trigger patched.

#include <new>

#define main desktop_test_main
#include "plaits/test/plaits_test.cc"
#undef main

float Triangle(size_t i, size_t period) {
  const float phase = float(i % period) / float(period);
  return phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
}

const size_t kRate = 48000;

void TestVoiceEngine(int engine, bool trigger_patched) {
  char name[64];
  sprintf(name, "plaits_voice_%02d_%s.wav", engine,
      trigger_patched ? "triggered" : "free");
  WavWriter wav_writer(2, kRate, 4);
  wav_writer.Open(name);

  // Constructed in zeroed storage, like the firmware's global voice.
  static Voice* storage = static_cast<Voice*>(malloc(sizeof(Voice)));
  memset(static_cast<void*>(storage), 0, sizeof(Voice));
  memset(ram_block, 0, sizeof(ram_block));
  Voice& v = *new(storage) Voice;
  BufferAllocator allocator(ram_block, 16384);
  v.Init(&allocator);

  Patch patch;
  patch.engine = engine;
  patch.frequency_modulation_amount = 0.0f;
  patch.timbre_modulation_amount = trigger_patched ? 0.4f : 0.0f;
  patch.morph_modulation_amount = trigger_patched ? -0.3f : 0.0f;
  patch.decay = 0.4f;
  patch.lpg_colour = 0.5f;

  Modulations modulations;
  modulations.engine = 0.0f;
  modulations.note = 0.0f;
  modulations.frequency = 0.0f;
  modulations.harmonics = 0.0f;
  modulations.level = 1.0f;
  modulations.frequency_patched = false;
  modulations.timbre_patched = trigger_patched;
  modulations.morph_patched = trigger_patched;
  modulations.trigger_patched = trigger_patched;
  modulations.level_patched = false;

  for (size_t i = 0; i < kRate * 4; i += kBlockSize) {
    patch.note = 36.0f + 36.0f * Triangle(i, kRate * 3);
    patch.harmonics = Triangle(i, kRate * 4 / 3);
    patch.timbre = Triangle(i, kRate * 2 / 3);
    patch.morph = Triangle(i, kRate / 2);
    modulations.timbre = Triangle(i, kRate / 5);
    modulations.morph = Triangle(i, kRate / 7);
    modulations.trigger = i % (kRate / 4) < kBlockSize * 4
        ? 1.0f : 0.0f;
    Voice::Frame frames[kBlockSize];
    v.Render(patch, modulations, frames, kBlockSize);
    wav_writer.WriteFrames(&frames[0].out, kBlockSize);
  }
}

int main(void) {
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  TestOscillator();
  TestVariableShapeOscillator();
  TestVariableSawOscillator();
  TestStringSynthOscillator();
  TestHarmonicOscillator();
  TestWavetableOscillator();
  TestNESTriangleOscillator();
  TestSuperSquareOscillator();
  TestFormantOscillator();
  TestVosimOscillator();
  TestZOscillator();
  TestGrainletOscillator();
  TestAdditiveEngine();
  TestChiptuneEngine();
  TestChordEngine();
  TestFMEngine();
  TestGrainEngine();
  TestModalEngine();
  TestStringEngine();
  TestNoiseEngine();
  TestParticleEngine();
  TestPhaseDistortionEngine();
  TestSpeechEngine();
  TestStringMachineEngine();
  TestSwarmEngine();
  TestVirtualAnalogEngine();
  TestVirtualAnalogVCFEngine();
  TestWaveshapingEngine();
  TestWavetableEngine();
  TestWaveTerrainEngine();
  TestBassDrumEngine();
  TestSnareDrumEngine();
  TestHiHatEngine();
  TestSampleRateReducer();
  TestVoice();
  TestFMGlitch();
  TestLPGAttackDecay();
  TestLimiterGlitch();
  TestSixOpEngine();
  for (int engine = 0; engine < kMaxEngines; ++engine) {
    TestVoiceEngine(engine, false);
    TestVoiceEngine(engine, true);
  }
}
