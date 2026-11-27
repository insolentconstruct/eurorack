# Transition instruction counts

`transition_emu.py` boots a release `app.elf` under Unicorn's Cortex-M7 and
counts the instructions of every main loop pass, audio block (the codec's DMA
interrupt) and SysTick across scripted transitions: steady granular, a quality
change, the Clouds reverb toggle, granular to spectral and back, FREEZE during
a fade, synth entry after 10 s without input and its exit (with MEDIA+FREEZE
while Plaits, selected without its models, falls back to Beads' synth),
FREEZE, SEED and ASSIGN gestures, and the MEDIA and ASSIGN combos, each
checked for its effect. `Process`, `MonitorStateChange` and `Ui::Poll` are also
counted inclusively. Counts are instructions, not cycles. A refactor must keep
boot, every call's count and every output hash identical. ELFs before
the #353 fix step the bank there and fail that check.

Needs Python 3, `unicorn==2.1.4` and `pyelftools`:

    transition_emu.py BASE.elf base.json
    transition_emu.py CANDIDATE.elf candidate.json
    transition_emu.py --compare base.json candidate.json  # fails on any change
    transition_emu.py --compare-audio base.json candidate.json
    transition_emu.py --check BASE.elf  # block counts vs instruction hooks
    transition_emu.py --summary BASE.elf summary.json

Boot holds SEED so blank storage skips the factory test; clock, regulator and
ADC flags read ready. Each millisecond runs one main loop pass, four audio
blocks and one tick, as `firmware_test.cc` does. The synth timer's 10 s runs
the main loop and tick only. Phase labels (steady, fading, silent, blanking,
synth) come from the firmware's globals and are informative; `--compare`
checks the per-call counts, silence and output hashes.

Use `--compare-audio` for UI changes. It requires full traces and checks each
audio interrupt's instruction count, silence flag and inclusive counts,
then compares the audio SHA-256.
`baseline-fd5a17a.json` is the `--summary` of that release ELF.

`region_boots.py ELF REGION.bin OUT.json` checks the section-fault table in
`beads/blob/ABI.md`. It inserts a terrain grid and a version 3 profile with
Wave Terrain in every bank slot, then initializes valid saved records. Each
fault case boots normally with SEED released, a nondefault saved State, and
each saved Synth selection. WAVE and PLTS independently take five forms:
valid, corrupt CRC, bad magic, interrupted payload, and erased section.
TERR takes the same faults separately; a final case cuts the installation
inside PLTS. `--shard N/TOTAL` divides the boots between bounded workers.
The harness observes the synth timer starting, advances both clocks over the
idle ten seconds, then runs audio to settle and asserts synth mode before
recording. The idle interval does not test periodic UI or autosave timing.
Instruction counting stays enabled in the normal transition emulator; fault
boots use a native ten-second timeout for each call instead.

Checks cover the playable mask, selected and effective synth, output silence,
assignment LED timer registers across a full blink period, unchanged profile
and saved storage bytes, no writes to either flash area, unchanged runtime
State and Synth choice, and equal plugged-input granular output hashes.
Failed TERR renders are compared with erased TERR using the same terrain
model. Host `wave_terrain_grid.mk` separately compares stock terrains with
Plaits. These are software checks; they do not qualify physical flash or LEDs.

The cue follows Mutable Instruments' bootloader `UI_STATE_ERROR`, which
blinks every LED (`beads/bootloader/bootloader.cc`, upstream commit
`d8f473915232af74347762b6365202bfd26f73f6`, `UpdateLeds`). Its assignment-LED
pattern and 250 ms period match the existing `Ui::MODE_REPAIR_FAILED` cue;
the exact period is Drizzlet's recovery convention.

CI runs the release ELF and its region in four bounded shards after the build.
Each shard checks its fault boots and transition scenarios. The
explicit `release_synth` scenario requires all three installed synths, enters
synth mode, switches Plaits/Rings/Plaits/Beads, checks shared bank ownership,
steps the Plaits bank and model control, then checks exit on plugged inputs.
It is excluded from the default erased-region scenarios. Release publication
waits for these checks; JSON evidence is retained per shard.

CI also checks `saved_clouds`, which boots with a saved Clouds-style reverb
record and verifies that audio starts at full fade gain. With the release
region installed, `chiptune_drone` checks the gate and output on Chiptune drone
exit, with and without a preceding note.
