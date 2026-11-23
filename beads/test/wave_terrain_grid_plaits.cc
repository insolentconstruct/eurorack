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
// Plaits' own user terrain lookup and engine, the grid tests' reference.

#include "plaits/dsp/engine2/wave_terrain_engine.cc"

#include "beads/test/wave_terrain_grids.h"

float PlaitsTerrainLookup(const int8_t* grid, float x, float y) {
  return plaits::TerrainLookup(x, y, grid);
}

void PlaitsRender(
    const int8_t* grid,
    const plaits::EngineParameters* parameters,
    size_t num_blocks,
    float* out,
    float* aux) {
  static uint8_t arena[plaits::kMaxBlockSize * 4 * sizeof(float)];
  stmlib::BufferAllocator allocator(arena, sizeof(arena));
  plaits::WaveTerrainEngine engine;
  engine.Init(&allocator);
  engine.LoadUserData(reinterpret_cast<const uint8_t*>(grid));
  engine.Reset();
  for (size_t i = 0; i < num_blocks; ++i) {
    bool enveloped = false;
    engine.Render(parameters[i], &out[i * kGridBlockSize],
        &aux[i * kGridBlockSize], kGridBlockSize, &enveloped);
  }
}
