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
    transition_emu.py --check BASE.elf  # block counts vs instruction hooks
    transition_emu.py --summary BASE.elf summary.json

Boot holds SEED so blank storage skips the factory test; clock, regulator and
ADC flags read ready. Each millisecond runs one main loop pass, four audio
blocks and one tick, as `firmware_test.cc` does. The synth timer's 10 s runs
the main loop and tick only. Phase labels (steady, fading, silent, blanking,
synth) come from the firmware's globals and are informative; `--compare`
checks the per-call counts, silence and output hashes.
`baseline-fd5a17a.json` is the `--summary` of that release ELF.
