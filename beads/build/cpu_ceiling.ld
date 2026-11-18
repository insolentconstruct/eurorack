/* Copyright 2026 Insolent Construct.
 * Author: Insolent Construct (insolentconstruct@gmail.com)
 * MIT; see beads/build/LICENSE.
 */
SECTIONS {
  .cpu_ceiling 0x30041500 (NOLOAD) : {
    KEEP(*(.cpu_ceiling))
  }
}
INSERT AFTER .dma;
ASSERT(SIZEOF(.cpu_ceiling) == 64, "CPU ceiling ABI changed")
ASSERT(0x30041000 + 1112 <= 0x30041500, "CPU ceiling overlaps DWT")
