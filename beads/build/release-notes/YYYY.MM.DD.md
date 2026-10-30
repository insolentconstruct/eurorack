**Beta 2 "Fog".** Plaits as a second synth, a spectral processor, adjustable grain diffusion, attacks and more, on a four-page options page. Modules with damaged settings flash now start instead of crashing. [Drizzlet Hygrostat](https://insolentconstruct.github.io/eurorack/) builds your own update file in the browser: choose which features the firmware has, where they sit and their defaults, including nine new options that are off in the standard firmware. Tested on one module so far; please try it on yours and [report what you find](https://github.com/insolentconstruct/eurorack/issues).

TODO before publishing: rename this file to the release date, fill in the test totals below, and remove this line.

## Upgrading

- **From the Mutable Instruments firmware:** your settings and calibration carry over.
- **From an earlier Drizzlet release** (alpha or Beta 1): the calibration and the stock settings (recording quality, [M] routing and amounts, input gain, trigger output, autosave, monophonic grains) are kept. Drizzlet's own options, stereo width and grain diffusion start from their defaults; set them again on the new options page.

The options page now has four pages, and its page LEDs light left to right: FEEDBACK, DRY/WET, REVERB, then all three. Where the Beta 1 options went:

| Option | Beta 1 | Beta 2 |
|---|---|---|
| Pitch scale | Page 1, FEEDBACK | Page 1, FEEDBACK |
| Stereo width | Page 1, DRY/WET | Off the pages: hold [A], turn DRY/WET |
| Reverb character | Page 1, REVERB | Page 2, DRY/WET |
| Delay mode | Page 2, DRY/WET | Page 2, FEEDBACK |
| Wavetable synth | Page 3, FEEDBACK | Page 3, REVERB, now called Synth start |
| Limiter | Page 3, DRY/WET | Page 4, REVERB |

Delay mode's values are renamed, with the same behaviour: Never is now Disabled, Wide detent is now Wide/smooth filter. Guides for the standard firmware: [printable](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/options-guide.svg), [phone](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/options-guide-mobile.svg), [text](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/OPTIONS.md). Hygrostat makes the guides for your own layout.

## New in Beta 2

Listed by the number of requests ([feature requests](https://github.com/insolentconstruct/eurorack/issues/130), [roadmap](https://github.com/insolentconstruct/eurorack/issues/32)).

- **Grain diffusion** (7 requests, #2): hold [A] and turn REVERB, with either reverb; also an [M] destination for CV.
- **Plaits synth** (page 3; 6 requests, #50): six Plaits voices in place of the wavetable synth. FEEDBACK picks one of 24 models; TIME, SIZE and SHAPE are HARMONICS, TIMBRE and MORPH.
- **Spectral processor** (page 3; 4 requests, #3): Clouds' spectral processing in place of the grains. Switching processor clears the recording.
- **Frozen feedback** (page 2; 3 requests, #4): Evolving runs a frozen sound through a slowly moving all-pass loop, so it drifts and softens instead of repeating exactly.
- **Random intervals** (page 1; 2 requests, #8): with the PITCH attenurandomizer counter-clockwise, random pitch lands on fifths and octaves. Works with any pitch scale.
- **Attacks** (page 1; #58, #47): Snap starts grains on the hits in the recording; Snap and trigger also starts a grain on each hit.
- **Quieter quality changes** (#53): the output fades out first, and changes that keep the sample rate no longer pop. The pop came from the converter restarting, in every earlier firmware. The synth's reverb tail now stops with it.
- **Less CPU, same sound:** Clouds-style reverb, the grains, and the wavetable synth, which now computes two grains side by side (about 3 points of the budget on its heaviest settings, #168).

### Drizzlet Hygrostat

[Hygrostat](https://insolentconstruct.github.io/eurorack/) is a web page that builds the update file from this release in your browser; nothing is uploaded. Start from a template or by hand. Each feature can be Off or Fixed at a value, and most can also be Adjustable on the panel, at the options page knob or [A] gesture you choose, with your defaults. The page warns when a combination may be too heavy for the processor, and refuses one that is. It also makes printable and phone guides for your layout. Without changes it builds exactly the update file published here.

Nine new options, off in the standard firmware, are available through Hygrostat:

- **Version at boot** (#54): a short light-blue flash at power-up, then two pulses for Beta 2.
- **Input attenuation** (#44): manual gain ([A]+FEEDBACK) goes down to silence, with unity at noon.
- **Delay indication** (#42): the quality LED flashes red briefly when SIZE enters or leaves delay mode.
- **LED brightness** (#40): 100%, 66% or 33%.
- **Live assignment LEDs** (#27): the mix LEDs follow the assignable CV.
- **FREEZE during saves** (#25): the FREEZE LED stays fully lit while a frozen buffer is saved.
- **SEED latch** (#13): with SEED latched and a cable patched, Ignore free-runs and ignores the cable, Sync restarts the grain clock on each edge, Add plays one extra grain per edge.
- **Pitch notch indication** (#7): the quality LED lights green while PITCH sits on unison or an octave.
- **TIME scan** (#141): with TIME CV unpatched, the TIME attenurandomizer moves the granular playback position forwards or backwards; try it on a frozen recording.

## Fixes

- **Settings saves** (#149): a save could, rarely, skip erasing the flash and write over the old settings, leaving a module that crashes at every start. Mutable Instruments' firmware has the same code. Each flash operation is now checked, and settings are only written onto erased flash.
- **Damaged settings** (#148): a module whose settings flash was damaged crashed at every start, on any firmware. Drizzlet now keeps a copy of the calibration in a second flash sector. A calibration that cannot be read is restored from the copy, with no sign. A save cut short, by a power cut for example, loses only that save. Only if the calibration and its copy are both lost do all LEDs blink fast for 3 s at each power-up; the module then plays on the factory calibration and default settings and saves nothing. Power on holding SEED to repair it with the factory calibration, then [recalibrate](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/CALIBRATION.md). A damaged frozen buffer is skipped instead of crashing. See the [README](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/README.md#damaged-settings).
- **Firmware size** (#155): the build refuses firmware large enough to overwrite the frozen buffer and settings. Today's firmware is far from that.

## Everything Drizzlet adds

Compared with the Mutable Instruments firmware. In the standard firmware every option defaults to stock behaviour; a Hygrostat profile can change the defaults and the layout.

| Feature | Where (standard firmware) | Since |
|---|---|---|
| Pitch scale: Chromatic or eight Marbles scales | Page 1, FEEDBACK | Beta 1 |
| Random intervals | Page 1, DRY/WET | Beta 2 |
| Attacks | Page 1, REVERB | Beta 2 |
| Delay mode: Auto, Disabled or Wide/smooth filter | Page 2, FEEDBACK | Beta 1 |
| Clouds-style reverb | Page 2, DRY/WET | Beta 1 |
| Evolving frozen feedback | Page 2, REVERB | Beta 2 |
| Spectral processor | Page 3, FEEDBACK | Beta 2 |
| Plaits synth | Page 3, DRY/WET | Beta 2 |
| Wavetable synth started by hand | Page 3, REVERB | Alpha 2 |
| Output limiter: Stock, Fast release or Off | Page 4, REVERB | Alpha 1 |
| Stereo width, with CV | Hold [A], turn DRY/WET | Beta 1 |
| Grain diffusion, with CV | Hold [A], turn REVERB | Beta 2 |
| Monophonic grains (Mutable Instruments' code) | Hold [A], press SEED | Alpha 2 |
| Buffer trim | Hold [A], press FREEZE | Beta 1 |
| The nine Hygrostat options above | Off; Hygrostat only | Beta 2 |
| Safer frozen-buffer saves | Automatic | Alpha 1 |
| Calibration copy; starts with damaged settings, SEED repairs them | Automatic; hold SEED at power-up | Beta 2 |
| Less flash wear from a modulated FREEZE | Automatic | Alpha 2 |
| More CPU headroom, same sound | Automatic | Alpha 2 |

How each works: [README](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/README.md). Every change, including bug fixes: [CHANGELOG.md](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/CHANGELOG.md).

## Coming from the 2021 or 2024 firmware

Drizzlet includes everything Mutable Instruments changed after those releases: settings recovery for modules stuck on boot, smoother knobs and delay controls, and the grain-scheduling change after which our module no longer crackled at high DENSITY.

## How it was tested

Host tests on every push, reproducible builds, and TODO hours of automated runs on a module, with TODO firmware and settings installs, none failed to boot. The damaged-settings handling was tested with real flash errors on a development board with the same processor, and on the module with a deliberately damaged settings sector. CV calibration was checked twice against an Intellijel Scales reference. TODO: the Hygrostat profiles module session. Details: [TESTING.md](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/TESTING.md).

## Downgrading

- To the Mutable Instruments firmware: safe. Turn monophonic grains off first. It ignores Drizzlet's options, clears them when it saves, and loads trimmed buffers at full length.
- To an earlier Drizzlet release: its options start from stock.

## Known limits

- Tested on one module only.
- With SEED latch on Ignore, Sync or Add, a patched cable no longer divides the grain clock; the divider works only with SEED latch Off.
- The brown-out reset level is unchanged from the Mutable Instruments firmware; a higher level is planned for a later release.
- Hygrostat's CPU figures for single features are estimates until they are measured on a module; the warning and refusal are based on them.
- A module that no longer starts after an update, even after reinstalling: see the [storage recovery tool](https://github.com/insolentconstruct/eurorack/releases/tag/storage-recovery-2026.10.27) and its warnings first.
- Settings are stored as in Mutable Instruments' firmware. About once every 4,000 saves, the settings sector is erased and rewritten. A power cut then loses only the settings being saved; the calibration copy keeps the calibration. The README shows how to back up calibration.
- ST-Link recovery: the release adds `-combo.hex`, which carries its flash address. Erase only the sectors written: a full-chip or mass erase also wipes calibration and settings ([README](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/README.md#st-link-recovery)).
- Where the Beads manual and the firmware disagree: [MANUAL_ERRATA.md](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/docs/MANUAL_ERRATA.md).

## Reporting problems

Open an [issue](https://github.com/insolentconstruct/eurorack/issues) with the release tag, your options page settings (or your Hygrostat profile link), what you did, what you heard, and a recording if you can. Questions, feature ideas and what works: [Discussions](https://github.com/insolentconstruct/eurorack/discussions). Please do not contact Mutable Instruments about this firmware.

Built on the Beads source Mutable Instruments released under the MIT licence.
