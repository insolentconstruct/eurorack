# Peaks tuneup

This branch starts at `python3` (`a4e86e9`): Mutable Instruments' vanilla
source with the build scripts ported to Python 3, repaired desktop test
harnesses, host render tests and a linker-script fix in stmlib. With the
pinned GCC 4.8 it builds the same firmware as vanilla; the firmware here is
built with GCC 15.3. The plan is in
[#68](https://github.com/insolentconstruct/eurorack/issues/68).

Changes from vanilla, one line per change:

- Define the overflowing shifts and products in the bass drum, FM drum, high hat, bouncing ball, LFO and number station: the firmware is byte-identical.
- Define the snare drum's noise product, which overflows on fast retriggers: the output is unchanged; with GCC 4.8 the snare executes one more instruction per sample but issues one fewer, counting the conditional instructions it skips.
- Step the pulse randomizer's slots with a pointer and skip the pulse shaper's idle slots first: the output is unchanged, and both run fewer instructions per block with GCC 4.8 and 15.3 (see [OPTIMIZATIONS.md](peaks/tuneup/OPTIMIZATIONS.md)).
- Compute the drum filters' coefficients when a setting changes, and check the punch only in the bass drum: the output is unchanged, and the high hat, snare, bass drum and number station run fewer instructions with GCC 4.8 and 15.3.
- Choose the LFO shape once per block, and compute the sine and steps settings when the parameter changes: the output is unchanged, and every shape runs fewer instructions with GCC 4.8 and 15.3.
- Build the application with GCC 15.3 instead of GCC 4.8, with the makefile's flags and `-Werror` unchanged: the output is identical, and the processors run 14% fewer instructions in total than vanilla (see [OPTIMIZATIONS.md](peaks/tuneup/OPTIMIZATIONS.md)). The bootloader stays on GCC 4.8.
- Fix the LFO ramp (triangle at maximum parameter) staying at full output until another triangle had played ([#90](https://github.com/insolentconstruct/eurorack/issues/90)): the slopes are computed when the parameter changes. Every other output is unchanged, and the triangle runs fewer instructions.

## Build and verify

The build runs in the Docker images from
[mutable-dev-environment](https://github.com/insolentconstruct/mutable-dev-environment): the base image
for the render tests, and its GCC 15.3 image for the application and update file
(the bootloader is built there with the GCC 4.8 it carries). With a checkout
of it beside this one, from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
touch peaks/resources.cc
M=../mutable-dev-environment/mutable
$M image
$M compilers --compiler 15.3.Rel2
$M -C . render peaks
export MUTABLE_IMAGE=mutable-dev:gcc-15.3.Rel2
$M -C . make -f peaks/makefile
$M -C . make TOOLCHAIN_PATH=/usr/local/arm-4.8.3/ -f peaks/bootloader/makefile
$M -C . make -f peaks/makefile wav
sha256sum -c peaks/tuneup/expected-hashes.sha256
```

The `touch` keeps the committed `resources.cc`: without it, make
regenerates the file in a fresh checkout with today's stmlib, which
builds different firmware. CI runs the same steps. When a change alters
the firmware on purpose, update the hashes and say why in the commit:

```sh
sha256sum build/peaks/peaks.bin build/peaks_bootloader/peaks_bootloader.hex build/peaks/peaks.wav > peaks/tuneup/expected-hashes.sha256
```

When a change alters a render on purpose, run
`$M -C . render --update peaks` and give the reason in the commit.
`$M -C . render --sanitize peaks` adds AddressSanitizer, UBSan and a check
for output that depends on uninitialised locals.

See [CHANGELOG.md](peaks/tuneup/CHANGELOG.md), [OPTIMIZATIONS.md](peaks/tuneup/OPTIMIZATIONS.md)
and the [style guide](https://github.com/insolentconstruct/eurorack/blob/beads-tuneup/beads/build/docs/STYLE.md).
