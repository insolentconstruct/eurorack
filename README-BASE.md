# Rings tuneup

This branch starts at `python3` (`a4e86e9`): Mutable Instruments' vanilla
source with the build scripts ported to Python 3, repaired desktop test
harnesses, host render tests and a linker-script fix in stmlib. With the
pinned GCC 4.8 it builds the same firmware as vanilla. The plan is in
[#68](https://github.com/insolentconstruct/eurorack/issues/68).

Changes from vanilla, one line per change:

- Define `__errno` as the function libm calls: GCC 14 and later link Rings; no change in sound ([#75](https://github.com/insolentconstruct/eurorack/pull/75)).

## Build and verify

The build runs in the Docker image from
[mutable-dev-environment](https://github.com/insolentconstruct/mutable-dev-environment). With a checkout
of it beside this one, from the repository root:

```sh
git submodule update --init stmlib stm_audio_bootloader
touch rings/resources.cc
M=../mutable-dev-environment/mutable
$M image
$M -C . render rings
$M -C . make -f rings/makefile
$M -C . make -f rings/bootloader/makefile
$M -C . make -f rings/makefile wav
sha256sum -c rings/tuneup/expected-hashes.sha256
```

The `touch` keeps the committed `resources.cc`: without it, make
regenerates the file in a fresh checkout with today's stmlib, which
builds different firmware. CI runs the same steps. When a change alters
the firmware on purpose, update the hashes and say why in the commit:

```sh
sha256sum build/rings/rings.bin build/rings_bootloader/rings_bootloader.hex build/rings/rings.wav > rings/tuneup/expected-hashes.sha256
```

When a change alters a render on purpose, run
`$M -C . render --update rings` and give the reason in the commit.
`$M -C . render --sanitize rings` adds AddressSanitizer, UBSan and a check
for output that depends on uninitialised locals.

See [CHANGELOG.md](rings/tuneup/CHANGELOG.md), [OPTIMIZATIONS.md](rings/tuneup/OPTIMIZATIONS.md)
and the [style guide](https://github.com/insolentconstruct/eurorack/blob/beads-tuneup/beads/build/docs/STYLE.md).
