# Changelog

Changes from the vanilla Mutable Instruments Rings source. Every change, one
line each, is in [README-BASE.md](../../README-BASE.md).

## Unreleased
- Sympathetic strings and string synth: with V/OCT, FREQUENCY and FM all near the ends of their ranges, the highest and lowest strings no longer play wrong pitches; they stop at the ends of the pitch range.
- FM voice: at pitches above the sample rate the carrier stops just below it, instead of relying on an overflow.
