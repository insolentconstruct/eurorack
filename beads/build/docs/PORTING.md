# Porting the DSP engine

Drizzlet adds no assembly, section attributes, C++11 features, 64-bit assumptions
or hardware constants to `beads/dsp`. Host tests build every changed DSP file
with g++, AddressSanitizer and UndefinedBehaviorSanitizer. Clang, MSVC and VCV
builds are untried. Replace drivers, UI, options, settings, buffer saves,
bootloader and DWT build for your host.

## What a port should know

1. **Rounding:** `randomized_parameter.h` uses standard `fmaf()` once per grain
   start to pin pan rounding. Without hardware FMA this calls a slower library
   function. Match GCC's default contraction and Cortex-M7 flush-to-zero for
   matching audio; disabling contraction moves further from module sound.
   Grain-render pan, shape and speed targets round explicitly; GCC 9–16 agree.
   Compare with host tests.
2. **Delay reads:** `DelayLine::Read()` uses unsigned modulo. Every reader must
   stay behind the write head, as grains and delay taps do.
3. **Memory/rates:** Clouds-style reverb keeps 64 KiB; `Processor` adds 2,048
   diffuser samples (4 KiB). Internal rates are 48, 32 and 24 kHz by quality;
   resample around the engine for other host rates.
4. **Timing:** DSP hooks compile away unless `DWT_CALIBRATION` is defined.

## Already in Mutable Instruments' code

- Packed bitfield samples in `dsp/delay_line.h` use `__attribute__((packed))`.
- stmlib ARM saturation/square-root assembly and `IN_RAM` have plain C++
  alternatives selected with `TEST`, as used by host tests.

## Sound

[Optimization checks](OPTIMIZATIONS.md) show bit-identical module audio.
Other compilers or floating-point settings may differ by a few rounding steps;
compare with host tests.
