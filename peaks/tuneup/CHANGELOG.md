# Changelog

Changes from the vanilla Mutable Instruments Peaks source. Every change, one
line each, is in [README-BASE.md](../../README-BASE.md).

## Unreleased
- Built with GCC 15.3 instead of GCC 4.8: the output is identical (the bootloader stays on GCC 4.8) and the application is 12,072 bytes smaller.
- LFO: the ramp no longer stays at full output when it is selected before any other triangle has played ([#90](https://github.com/insolentconstruct/eurorack/issues/90)).
