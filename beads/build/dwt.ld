/* Copyright 2026 Insolent Construct.
 * Author: Insolent Construct (insolentconstruct@gmail.com)
 * MIT; see beads/build/LICENSE.
 */
SECTIONS {
  .dwt_calibration 0x30041000 (NOLOAD) : {
    KEEP(*(.dwt_calibration))
  }
}
INSERT AFTER .dma;
ASSERT(SIZEOF(.dwt_calibration) == 1112, "DWT ABI changed")
ASSERT(_eramd2dma <= 0x30041000, "DWT overlaps DMA")
