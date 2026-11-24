<!--
Copyright 2026 Insolent Construct.

Author: Insolent Construct (insolentconstruct@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

<!--
Before publication, recheck the pending automatic-gain ceiling, fallback gesture safeguards, calibration-save and resonator fixes, and additional Palette models.
Publish Hygrostat copy separately after its model picker, starter banks and matching assets are available.
-->

**Beta 4 — draft** · date `{DATE}` · tag `{TAG}`

**TL;DR:** Beta 4 keeps the familiar Beads controls and stock musical option
settings. Optional additions include a resonator for the grains, Rings
synthesis and more Plaits models selected in Hygrostat. Start with the
standard WAV; use the guide generated for your profile when customizing.
Earlier Drizzlet options reset to profile defaults on upgrade; stock
settings and calibration are retained.

This is release copy for review. Final release-image qualification is
pending; the additions below are not yet claimed qualified on a module.

## Fixes and everyday use

- Changing Processor while frozen keeps playing until FREEZE is released.
- Snap and trigger adds grains from recorded hits, including while frozen,
  without taking over delay timing or stopping synths. SEED latch Sync starts
  a grain on each edge, even with DENSITY at noon.
- Long [A] holds act on release; using a knob or button during the hold
  cancels that action. Slow width or bank gestures no longer reset gain first.
- Plaits drone release and Speech word-to-phoneme switching avoid unwanted
  clicks or noise bursts.
- Calibration storage protects a usable backup. Cancelled or out-of-range
  calibration keeps the previous values. Drizzlet options now have their own
  saved record, separate from stock settings.
- The default grain stealing follows Mutable Instruments' 2021/2024 firmware.
  This can make dense textures a little louder than Betas 1–3.
- Audio gains one frame of output delay: about 21, 31 or 42 microseconds at
  48, 32 or 24 kHz, giving processing more time to finish.

## Optional sounds

The standard image contains Plaits' three banks of eight models, Beads'
original wavetables and Rings synthesis. Rings and the wavetables are
removable in Hygrostat to make room for other models. Palette models are
available only through Hygrostat; none are installed in the standard WAV.

- **Resonator:** page 5 adds placement, Material and Ring time. Choose After
  the grains, Before the buffer or In the feedback; Off is the default.
  PITCH tunes it; SEED and detected hits strike it. All 32 grains remain.
- **Rings synth:** page 3 Synth gains Rings when installed. While Rings plays,
  [A]+FREEZE steps one, two or four voices. This is separate from the
  resonator alongside grains.
- **Plaits and Palette reselection:** models start fresh when a voice switches
  to them, with a short fade-in. Returning to a model can restart filters,
  grains, random sequences or modulation instead of resuming earlier state.
  VOSIM also prepares for about 4 ms in silence after selection. See
  [selection details](../docs/FEATURES.md#model-selection-and-reselection).

## Customizing in Hygrostat

Profiles keep the module focused on the controls and sounds you use.
The standard layout remains available; customized layouts get matching guides.

- **Send mixing:** keeps dry audio present; DRY/WET sets new grains' levels.
  At zero, existing grains and reverb finish and recording continues.
  Granular only.
- **Synth output level:** Stock, Half, Quarter or Eighth; applies to Beads,
  Plaits and Rings audio without changing input gain or OUT R triggers.
- **Divided grain triggers:** Stock, /2, /4, /8 or /16 on OUT R; keeps the first
  gate and pulse width, without changing grain timing or audio.

These choices are set in a profile. See the
[options guide](../docs/OPTIONS.md) and [feature catalogue](../docs/FEATURES.md).
Banks estimated to exceed the processing limit should show a prominent
warning in Hygrostat; confirm this in the released editor.
CPU use never blocks that bank's selection;
a warned bank may crackle. Flash capacity and missing dependencies still
need to be resolved before export.

## Updating and remaining checks

Beta 2/3 Drizzlet options reset to the profile's defaults. Calibration and
stock settings are retained. Save your profile and check its guide after
updating. Follow the [installation and recovery instructions](../README.md);
keep speakers disconnected during audio updates.

Before publication: replace date/tag placeholders, confirm final model and
editor assets, resolve Gain ceiling's pending status and add exact-image
module results and remaining known issues. Do not copy older release CPU
peaks or test counts as Beta 4 results. No final Beta 4 module measurements
were made for this draft.
