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
// Wave terrain's TERR grids: Plaits' own user terrain where its reads stay in
// the grid, the nearest edge past it, the stock terrains without grids.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "beads/test/test_checker.h"
#include "beads/test/wave_terrain_grids.h"

using plaits::EngineParameters;

const size_t kGridSize = 64 * 64;
const size_t kMaxGrids = 16;
const size_t kNumBlocks = 64;
const size_t kNumSamples = kNumBlocks * kGridBlockSize;

uint32_t seed = 0x2545f491;

int8_t NextByte() {
  seed = seed * 1664525 + 1013904223;
  return int8_t(seed >> 24);
}

// Exactly sized on the heap, so the sanitizer sees any read past the grids.
int8_t* NewGrids(size_t num_grids) {
  int8_t* grids = new int8_t[num_grids * kGridSize];
  for (size_t i = 0; i < num_grids * kGridSize; ++i) {
    grids[i] = NextByte();
  }
  grids[0] = -128;
  grids[kGridSize - 1] = 127;
  return grids;
}

// Bilinear in double on clamped coordinates: what the blob must compute.
double Reference(const int8_t* grid, double x, double y) {
  x = std::min(std::max((x + 1.0) * 31.0, 0.0), 62.9999);
  y = std::min(std::max((y + 1.0) * 31.0, 0.0), 62.9999);
  const int xi = int(x);
  const int yi = int(y);
  const double xf = x - xi;
  const double yf = y - yi;
  const int8_t* row = &grid[yi * 64];
  const double a = row[xi] + (row[xi + 1] - row[xi]) * xf;
  row += 64;
  const double b = row[xi] + (row[xi + 1] - row[xi]) * xf;
  return (a + (b - a) * yf) / 128.0;
}

void TestLookupInside() {
  test_checker.Start("TestLookupInside");
  int8_t* grid = NewGrids(1);
  int compared = 0;
  // Up to x = 1.0322 Plaits' reads stay in the grid too.
  for (int i = 0; i <= 640; ++i) {
    for (int j = 0; j <= 640; ++j) {
      const float x = -1.0f + 2.0322f * float(i) / 640.0f;
      const float y = -1.0f + 2.0322f * float(j) / 640.0f;
      if ((x + 1.0f) * 31.0f > 62.9999f || (y + 1.0f) * 31.0f > 62.9999f) {
        continue;
      }
      const float a = BlobTerrainLookupGrid(grid, x, y);
      const float b = PlaitsTerrainLookup(grid, x, y);
      CHECK(memcmp(&a, &b, sizeof(a)) == 0);
      ++compared;
    }
  }
  printf("%d points equal Plaits' TerrainLookup\n", compared);
  delete[] grid;
}

void TestLookupEdges() {
  test_checker.Start("TestLookupEdges");
  // A grid between two poisoned ones: a read outside it moves the value.
  int8_t* grids = NewGrids(3);
  for (size_t i = 0; i < kGridSize; ++i) {
    grids[i] = grids[2 * kGridSize + i] = i & 1 ? 127 : -128;
  }
  const int8_t* grid = &grids[kGridSize];
  const float specials[] = {
    -INFINITY, -1e30f, -2.0f, -1.0001f, -1.0f, -0.99999f, 0.0f,
    0.99999f, 1.0f, 1.0001f, 1.0322f, 1.0323f, 2.0f, 1e30f, INFINITY
  };
  const size_t num_specials = sizeof(specials) / sizeof(specials[0]);
  std::vector<float> coordinates(specials, specials + num_specials);
  for (int i = 0; i <= 400; ++i) {
    coordinates.push_back(-2.0f + 4.0f * float(i) / 400.0f);
  }
  int checked = 0;
  double worst = 0.0;
  for (size_t i = 0; i < coordinates.size(); ++i) {
    for (size_t j = 0; j < coordinates.size(); ++j) {
      const float x = coordinates[i];
      const float y = coordinates[j];
      const float a = BlobTerrainLookupGrid(grid, x, y);
      const double b = Reference(grid, x, y);
      CHECK(std::isfinite(a));
      worst = std::max(worst, fabs(a - b));
      ++checked;
    }
  }
  CHECK(worst < 2e-5);
  // NaN reads the first row or column instead of propagating.
  CHECK(BlobTerrainLookupGrid(grid, NAN, NAN) == grid[0] / 128.0f);
  CHECK(BlobTerrainLookupGrid(grid, NAN, -1.0f) == grid[0] / 128.0f);
  CHECK(BlobTerrainLookupGrid(grid, -1.0f, NAN) == grid[0] / 128.0f);
  printf("%d points within %g of the clamped reference\n", checked, worst);
  delete[] grids;
}

void Sweep(
    EngineParameters* parameters,
    float harmonics_low,
    float harmonics_high,
    float timbre_high) {
  for (size_t i = 0; i < kNumBlocks; ++i) {
    const float t = float(i) / float(kNumBlocks - 1);
    parameters[i].trigger = 0;
    parameters[i].note = 36.0f + 60.0f * float((i * 7) % 11) / 10.0f;
    parameters[i].timbre = timbre_high * float((i * 5) % 9) / 8.0f;
    parameters[i].morph = float((i * 3) % 13) / 12.0f;
    parameters[i].harmonics = harmonics_low +
        (harmonics_high - harmonics_low) * t;
    parameters[i].accent = 0.8f;
  }
}

bool Equal(const float* a, const float* b) {
  return memcmp(a, b, kNumSamples * sizeof(float)) == 0;
}

void TestStock() {
  test_checker.Start("TestStock");
  EngineParameters parameters[kNumBlocks];
  float out[2][kNumSamples];
  float aux[2][kNumSamples];
  // Terrains 0-4 at any radius; 5-7 where the path stays in their bank.
  Sweep(parameters, 0.0f, 0.54f, 1.0f);
  BlobRender(NULL, 0, parameters, kNumBlocks, out[0], aux[0]);
  PlaitsRender(NULL, parameters, kNumBlocks, out[1], aux[1]);
  CHECK(Equal(out[0], out[1]));
  CHECK(Equal(aux[0], aux[1]));
  Sweep(parameters, 0.0f, 1.0f, 0.3f);
  BlobRender(NULL, 0, parameters, kNumBlocks, out[0], aux[0]);
  PlaitsRender(NULL, parameters, kNumBlocks, out[1], aux[1]);
  CHECK(Equal(out[0], out[1]));
  CHECK(Equal(aux[0], aux[1]));
  // With a count of 0 the grids are never played, even below the range.
  int8_t* around = NewGrids(32);
  const int8_t* grid = &around[16 * kGridSize];
  BlobRender(grid, 0, parameters, kNumBlocks, out[0], aux[0]);
  CHECK(Equal(out[0], out[1]));
  CHECK(Equal(aux[0], aux[1]));
  Sweep(parameters, -1.0f, -0.15f, 1.0f);
  BlobRender(grid, 0, parameters, kNumBlocks, out[0], aux[0]);
  PlaitsRender(NULL, parameters, kNumBlocks, out[1], aux[1]);
  CHECK(Equal(out[0], out[1]));
  CHECK(Equal(aux[0], aux[1]));
  // Nor read: the sanitizer stops a read of freed grids (after any report).
  fflush(stdout);
  delete[] around;
  Sweep(parameters, 0.0f, 1.0f, 1.0f);
  BlobRender(grid, 0, parameters, kNumBlocks, out[0], aux[0]);
  CHECK(std::isfinite(out[0][kNumSamples - 1]));
}

void TestOneGrid() {
  test_checker.Start("TestOneGrid");
  int8_t* grid = NewGrids(1);
  EngineParameters parameters[kNumBlocks];
  float out[2][kNumSamples];
  float aux[2][kNumSamples];
  // Plaits' own ninth terrain, where its reads stay in the grid.
  Sweep(parameters, 0.0f, 1.0f, 0.3f);
  BlobRender(grid, 1, parameters, kNumBlocks, out[0], aux[0]);
  PlaitsRender(grid, parameters, kNumBlocks, out[1], aux[1]);
  CHECK(Equal(out[0], out[1]));
  CHECK(Equal(aux[0], aux[1]));
  delete[] grid;
}

// Grid k holds one value throughout, so OUT tells which grids are played.
float GridValue(size_t k) {
  return float(int(k) * 13 - 100) / 128.0f;
}

float Settled(const int8_t* grids, size_t num_grids, float harmonics) {
  EngineParameters parameters[2];
  float out[2 * kGridBlockSize];
  float aux[2 * kGridBlockSize];
  for (size_t i = 0; i < 2; ++i) {
    parameters[i].trigger = 0;
    parameters[i].note = 48.0f;
    parameters[i].timbre = 1.0f;
    parameters[i].morph = 0.3f;
    parameters[i].harmonics = harmonics;
    parameters[i].accent = 0.8f;
  }
  BlobRender(grids, num_grids, parameters, 2, out, aux);
  return out[2 * kGridBlockSize - 1];
}

void TestGridSelection() {
  test_checker.Start("TestGridSelection");
  for (size_t n = 1; n <= kMaxGrids; ++n) {
    // One more grid than declared: playing it is an error, not a crash.
    int8_t* grids = new int8_t[(n + 1) * kGridSize];
    for (size_t k = 0; k <= n; ++k) {
      memset(&grids[k * kGridSize], int8_t(k * 13 - 100), kGridSize);
    }
    const float span = 1.05f * (float(n + 8) - 1.0001f);
    // Halfway between grids k and k + 1.
    for (size_t k = 0; k + 1 < n; ++k) {
      const float z = 8.5f + float(k);
      const float expected = 0.5f * (GridValue(k) + GridValue(k + 1));
      CHECK(fabsf(Settled(grids, n, z / span) - expected) < 1e-4f);
    }
    // HARMONICS fully up plays the last grid.
    CHECK(fabsf(Settled(grids, n, 1.0f) - GridValue(n - 1)) < 2e-3f);
    // The stock terrains keep the range below the first grid.
    CHECK(fabsf(Settled(grids, n, 6.5f / span) - GridValue(0)) > 1e-2f);
    delete[] grids;
  }
}

void TestFullRange() {
  test_checker.Start("TestFullRange");
  EngineParameters parameters[kNumBlocks];
  float out[kNumSamples];
  float aux[kNumSamples];
  const size_t counts[] = { 0, 1, 2, 15, 16 };
  for (size_t c = 0; c < sizeof(counts) / sizeof(counts[0]); ++c) {
    int8_t* grids = counts[c] ? NewGrids(counts[c]) : NULL;
    for (int pass = 0; pass < 4; ++pass) {
      Sweep(parameters, pass & 1 ? 1.0f : 0.0f, pass & 2 ? 0.0f : 1.0f, 1.0f);
      for (size_t i = 0; i < kNumBlocks; ++i) {
        parameters[i].morph = i & 1 ? 1.0f : 0.0f;
      }
      BlobRender(grids, counts[c], parameters, kNumBlocks, out, aux);
      for (size_t i = 0; i < kNumSamples; ++i) {
        CHECK(std::isfinite(out[i]) && std::isfinite(aux[i]));
      }
    }
    delete[] grids;
  }
}

int main() {
  TestLookupInside();
  TestLookupEdges();
  TestStock();
  TestOneGrid();
  TestGridSelection();
  TestFullRange();
  puts(test_checker.failures() ? "Wave terrain grid checks: FAIL"
                               : "Wave terrain grid checks: PASS");
  return test_checker.failures() ? 1 : 0;
}
