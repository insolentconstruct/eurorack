/* Copyright 2026 Insolent Construct.
 * Author: Insolent Construct (insolentconstruct@gmail.com)
 * MIT; see beads/build/LICENSE.
 */
SECTIONS {
  .deadline_trace 0x30041460 (NOLOAD) : {
    KEEP(*(.deadline_trace))
  }
}
INSERT AFTER .dwt_calibration;
ASSERT(SIZEOF(.deadline_trace) == 24832, "Trace ABI changed")
ASSERT(ADDR(.deadline_trace) >= ADDR(.dwt_calibration) + SIZEOF(.dwt_calibration),
    "Trace overlaps the DWT stats")
ASSERT(ADDR(.deadline_trace) + SIZEOF(.deadline_trace) <= 0x30048000,
    "Trace leaves the uncached MPU region")
ASSERT(_eramd2dma <= ADDR(.dwt_calibration), "DWT overlaps DMA")
