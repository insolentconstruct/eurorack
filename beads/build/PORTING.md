# Porting the DSP engine

What to know when building Drizzlet's DSP (`beads/dsp`) somewhere other than the module: a desktop build, VCV Rack, a plugin or another MCU.

## TL;DR

- Drizzlet's changes make the DSP no less portable than Mutable Instruments' released source. They add no assembly, section attributes, C++11 features, 64-bit assumptions or hardware constants.
- Every changed DSP file is built and run on a desktop compiler (g++ with AddressSanitizer and UndefinedBehaviorSanitizer) by the host tests in `beads/test`. No clang, MSVC or VCV build has been tried.
- A port replaces the firmware side anyway: drivers, UI, the options page, settings and frozen-buffer saves, the bootloader and the DWT calibration build.

## What a port should know

1. **Pinned rounding.** `MulAdd()` in `dsp/randomized_parameter.h` is a standard C `fmaf()` (fused multiply-add). It makes one pan calculation round the same way everywhere. On a CPU without hardware FMA it is a slower library call, made once per grain start. Build the rest with `-ffp-contract=off` (some host tests already do) so the compiler does not fuse other expressions differently from the module.
2. **Delay-line reads stay behind the write head.** `DelayLine::Read()` in `dsp/delay_line.h` uses an unsigned modulo for its index, which assumes no caller reads ahead of the write head. Grains (which stop at the heads) and delay taps satisfy this. A new kind of reader must too.
3. **Memory and rates.** The Clouds-style reverb uses the same 64 KiB reverb memory; `Processor` adds a 2,048-sample (4 KiB) diffuser buffer. Like Mutable Instruments' own code, the engine runs at fixed internal rates (48, 32 and 24 kHz by recording quality); a host at another rate resamples around it.
4. **Calibration hooks.** The DWT timing hooks in the DSP headers compile to nothing unless `DWT_CALIBRATION` is defined.

## Already in Mutable Instruments' code

These were non-portable before Drizzlet, and any port of Beads already deals with them:

- Packed bitfield sample structs in `dsp/delay_line.h` (`__attribute__((packed))`).
- ARM saturation and square-root assembly, and the `IN_RAM` code section, in stmlib. Each has a plain C++ version selected by defining `TEST`, which the host tests use; a port can do the same.

## Sound

On the module the audio is bit-identical across the changes listed in [OPTIMIZATIONS.md](OPTIMIZATIONS.md). A port compiled with a different compiler or floating-point settings can differ by a few float rounding steps; use the host tests to compare.
