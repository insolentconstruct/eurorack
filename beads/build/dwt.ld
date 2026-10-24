/* Copyright 2026 Insolent Construct.
 * Author: Insolent Construct (insolentconstruct@gmail.com)
 */
SECTIONS {
  .dwt_calibration 0x30041000 (NOLOAD) : {
    KEEP(*(.dwt_calibration))
  }
}
INSERT AFTER .dma;
ASSERT(SIZEOF(.dwt_calibration) == 1232, "DWT ABI changed")
ASSERT(_eramd2dma <= 0x30041000, "DWT overlaps DMA")
