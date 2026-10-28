/* Copyright 2026 Insolent Construct.
 * Author: Insolent Construct (insolentconstruct@gmail.com)
 *
 * The application must end in bank 1: bank 2 (0x08100000) holds the frozen
 * buffer, the calibration mirror and the settings. stmlib's script allows
 * 2 MB less the bootloader. ITCM code is the last section loaded from flash.
 */
ASSERT(LOADADDR(.itcm) + SIZEOF(.itcm) <= 0x08100000,
    "The application reaches bank 2")
