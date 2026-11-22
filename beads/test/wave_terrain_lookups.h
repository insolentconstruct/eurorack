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
// Plaits' Wave terrain lookups, MI's and the blob's, for wave_terrain_blob_test.

#ifndef BEADS_TEST_WAVE_TERRAIN_LOOKUPS_H_
#define BEADS_TEST_WAVE_TERRAIN_LOOKUPS_H_

#include "stmlib/stmlib.h"

// TerrainLookupWT of plaits/dsp/engine2/wave_terrain_engine.cc, on Plaits'
// table.
float PlaitsTerrainLookupWT(float x, float y, int bank);

// The blob's (beads/blob/models/wave_terrain_engine.cc), on waves.
float BlobTerrainLookupWT(const int16_t* waves, float x, float y, int bank);

#endif  // BEADS_TEST_WAVE_TERRAIN_LOOKUPS_H_
