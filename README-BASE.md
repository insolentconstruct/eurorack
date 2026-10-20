# Peaks tuneup

This branch starts at `python3` (`a4e86e9`): Mutable Instruments' vanilla
source with the build scripts ported to Python 3, repaired desktop test
harnesses, host render tests and a linker-script fix in stmlib. With the
pinned GCC 4.8 it builds the same firmware as vanilla. The plan is in
[#68](https://github.com/insolentconstruct/eurorack/issues/68).

Changes from vanilla, one line per change:

- None yet.

## Build and verify

The build runs in the Docker image from
[mutable-dev-environment](https://github.com/insolentconstruct/mutable-dev-environment). With a checkout
of it beside this one, from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
touch peaks/resources.cc
M=../mutable-dev-environment/mutable
$M image
$M -C . render peaks
$M -C . make -f peaks/makefile
$M -C . make -f peaks/bootloader/makefile
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
