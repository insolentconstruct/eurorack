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
// Reads the region at boot and says which synths it lets play.

#include "beads/blob/region_loader.h"

#include <stm32h7xx_hal.h>

#include "beads/blob/plaits_catalog.h"
#include "beads/blob/synth_choice.h"
#include "beads/dsp/plaits_voice.h"
#include "beads/dsp/rings_synth.h"

extern "C" const uint8_t plaits_region_begin[];

namespace beads {

// Reports and clears a double ECC error and the bus fault it raised.
/* static */
bool RegionLoader::FlashEccOk() {
  const uint32_t bus = SCB->CFSR &
      (SCB_CFSR_PRECISERR_Msk | SCB_CFSR_IMPRECISERR_Msk |
          SCB_CFSR_BFARVALID_Msk);
  const bool ok = !(FLASH->SR1 & FLASH_SR_DBECCERR) && !bus;
  FLASH->CCR1 = FLASH_CCR_CLR_DBECCERR;
  SCB->CFSR = bus;
  return ok;
}

// Reads the region with data bus faults ignored: a double ECC error (an
// interrupted write) reads as garbage and faults that section only. The
// D-cache is off meanwhile, so no linefill raises an imprecise fault later.
uint8_t RegionLoader::Load(
    const Profile& profile,
    size_t synth_ram_size,
    PlaitsRegion* region) {
  region->set_flash_check(&FlashEccOk);
  region->set_library_ram(kPlaitsLibraryRamSize, kPlaitsVoiceRamSize);
  SCB_DisableDCache();
  SCB->CCR |= SCB_CCR_BFHFNMIGN_Msk;
  __DSB();
  __ISB();
  __set_FAULTMASK(1);
  region->Read(
      plaits_region_begin,
      reinterpret_cast<const uint8_t*>(kPlaitsRegionEnd),
      kPlaitsBlobRelease,
      profile.directory(), profile.num_sections(),
      kPlaitsDefaultLayout, kNumPlaitsDefaultSections,
      &plaits_blob_services, kPlaitsApprovedBlobs, kNumPlaitsApprovedBlobs);
  __set_FAULTMASK(0);
  SCB->CCR &= ~SCB_CCR_BFHFNMIGN_Msk;
  __DSB();
  __ISB();
  SCB_EnableDCache();
  const bool plaits = PlaitsVoice::Playable(
      profile.plaits_bank(0),
      profile.num_plaits_banks() * kProfileBankSize,
      *region);
  const bool rings = RingsSynth::Fits(region->synth_blob(), synth_ram_size);
  return SynthMask(region->waves() != NULL, plaits, rings);
}

}  // namespace beads
