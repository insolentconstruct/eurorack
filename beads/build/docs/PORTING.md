# Porting the DSP engine

What to know when building this branch's DSP (`beads/dsp`) somewhere other than the module: a desktop build, VCV Rack, a plugin or another MCU.

## TL;DR

- This branch's changes make the DSP no less portable than Mutable Instruments' vanilla source. They add no assembly, section attributes, C++11 features, 64-bit assumptions or hardware constants.
- Every changed DSP file is built and run on a desktop compiler (g++, most suites with AddressSanitizer and UndefinedBehaviorSanitizer) by the host tests in `beads/test`, which run the real processor, synth and firmware callback on small hardware stubs. No clang, MSVC or VCV build has been tried, and no plugin format is provided.
- A port replaces the firmware side anyway: drivers, UI, settings and frozen-buffer saves, and the bootloader.

## What a port should know

1. **Pinned rounding.** The random pan spread in `dsp/randomized_parameter.h` calls standard C `fmaf()` (fused multiply-add). It makes one pan calculation round the same way everywhere. On a CPU without hardware FMA it is a slower library call, made once per grain start. The firmware keeps GCC's default contraction, which fuses some other multiply-adds on the Cortex-M7; turning contraction off moves the sound further from the module, not closer. For matching sound, use the same contraction setting on a target with hardware FMA, match its flush-to-zero behaviour, and compare with the host tests.
   The grain pan mix also uses `fmaf()`, and the shape and speed targets are rounded separately; sound is the same on GCC 9 through 16. The integer Hermite read keeps each rounding step in a fixed order.
2. **Delay-line reads stay behind the write head.** `DelayLine::Read()` in `dsp/delay_line.h` uses an unsigned modulo for its index, which assumes no caller reads ahead of the write head. Grains (which stop at the heads) and delay taps satisfy this. A new kind of reader must too.
3. **Grains add to the output.** The grain player sums grains locally and adds the sum to the caller's output frame once, so the caller must clear that frame first, as the processor does.
4. **Rates.** Like Mutable Instruments' own code, the engine runs at fixed internal rates (48, 32 and 24 kHz by recording quality); a host at another rate resamples around it.

## Already in Mutable Instruments' code

These were non-portable before this branch, and any port of Beads already deals with them:

- Packed bitfield sample structs in `dsp/delay_line.h` (`__attribute__((packed))`), and embedded memory placement.
- ARM saturation and square-root assembly, and the `IN_RAM` code section, in stmlib. Each has a plain C++ version selected by defining `TEST`, which the host tests use; a port can do the same.

## Sound

The audio is bit-identical across the changes listed in [OPTIMIZATIONS.md](OPTIMIZATIONS.md), shown by development-board hashes, emulator comparisons and the host tests. A port compiled with a different compiler or floating-point settings can differ by a few float rounding steps; use the host tests to compare.
