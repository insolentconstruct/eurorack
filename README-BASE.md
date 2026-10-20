# Plaits tuneup

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
M=../mutable-dev-environment/mutable
$M image
$M -C . render plaits
$M -C . make -f plaits/makefile
$M -C . make -f plaits/bootloader/makefile
$M -C . make -f plaits/makefile wav
sha256sum -c plaits/tuneup/expected-hashes.sha256
```

CI runs the same steps. When a change alters the firmware on purpose, update
the hashes and say why in the commit:

```sh
sha256sum build/plaits/plaits.bin build/plaits_bootloader/plaits_bootloader.hex build/plaits/plaits.wav > plaits/tuneup/expected-hashes.sha256
```

Run the render tests on a clean checkout: the build can regenerate
`resources.cc`. When a change alters a render on purpose, run
`$M -C . render --update plaits` and give the reason in the commit.
`$M -C . render --sanitize plaits` adds AddressSanitizer, UBSan and a check
for output that depends on uninitialised locals.

See [CHANGELOG.md](plaits/tuneup/CHANGELOG.md), [OPTIMIZATIONS.md](plaits/tuneup/OPTIMIZATIONS.md)
and the [style guide](https://github.com/insolentconstruct/eurorack/blob/beads-tuneup/beads/build/docs/STYLE.md).
