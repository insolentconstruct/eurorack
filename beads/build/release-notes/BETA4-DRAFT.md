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

**Beta 4 — draft** · date `{DATE}` · tag `{TAG}`

**TL;DR:** Beta 4 keeps the familiar Beads controls and stock musical option
settings. Optional additions include a resonator for the grains, Rings
synthesis and more Plaits models through the matching Hygrostat release. Start with the
standard WAV and use the guide generated for your profile when customizing.
Earlier Drizzlet options reset to profile defaults on upgrade. Stock
settings and calibration are retained.

Final release image pending module qualification.

## Fixes and everyday use

- Changing Processor while frozen keeps playing until FREEZE is released.
- Snap and trigger adds grains from recorded hits, including while frozen,
  without taking over delay timing or stopping synths. SEED latch Sync starts
  a grain on each edge, even with DENSITY at noon.
- Long [A] holds act on release. Turning a knob or pressing a button during
  the hold cancels the hold action. A delayed options save keeps newer panel
  gestures and leaves the current page preview alone. Slow width or bank
  gestures no longer reset gain first.
- Chiptune drones end with a smooth decay. Speech switches from words to
  phonemes without a burst of noise. Saved Clouds-style reverb starts without
  the brief power-on dip, and a synth swap starts fading in as audio resumes.
- Failed frozen-buffer saves retry up to three times while FREEZE stays on.
  Retries wait 10, 20 and 40 seconds. Once the audio is saved, the frozen
  status gets up to three retries of its own. Entering synth mode cancels
  an unfinished save.
- Audio startup retries after a converter error, and automatic synth entry
  finishes its fade even if a late input peak resets the idle timer.
- Calibration recovery keeps a usable backup. If both copies are unusable,
  the module uses factory calibration and default settings with saves locked.
  Buttons held through the storage warning must be released before reuse.
  Cancelling calibration before the first measurement restores jack detection.
  Cancelled or out-of-range calibration keeps the previous values. All LEDs
  blink if calibration is rejected or fails to save. After a failed save,
  the module uses the calibration it recovers from flash. Drizzlet options
  save separately from stock settings. After an interrupted write, a later
  save keeps stale options from resurfacing.
  See the [storage recovery instructions](../README.md#reset-and-recovery) if saves are locked.
- The default grain stealing follows Mutable Instruments' 2021/2024 firmware.
  This can make dense textures a little louder than Betas 1–3.
- Audio gains one frame of output delay: about 21, 31 or 42 microseconds at
  48, 32 or 24 kHz, giving processing more time to finish.

## Optional sounds

The standard image contains Plaits' three banks of eight models, Beads'
original wavetables and Rings synthesis. Rings and the wavetables are
removable in custom profiles. Palette customization becomes available when
Hygrostat ships its matching Beta 4 release with the model picker, starter
banks and assets. Models include Vowel FOF, Granular Cloud and Wave Scan.
The standard WAV contains no Palette models.

- **Resonator:** page 5 adds placement, Material and Ring time. Choose After
  the grains, Before the buffer or In the feedback. Off is the default.
  PITCH tunes it, and SEED and detected hits strike it. Changing placement
  starts a fresh resonance, and the first strikes sound at normal level.
  ASSIGN returns its CV to FEEDBACK when the
  resonator is unavailable, including in Spectral. All 32 grains remain.
- **Synth shortcuts:** page 3 Synth gains Rings when installed. [A]+FREEZE
  steps Rings through one, two or four voices, or steps the bank when Plaits
  plays, including as a fallback for missing Beads wavetables. With the Beads
  synth or silence, the same chord runs the profile action (normally Buffer
  trim). Put manual Synth start on another gesture if Plaits or Rings is
  selectable.
- **Plaits and Palette reselection:** models start fresh when a voice switches
  to them, with a short fade-in. Returning to a model can restart filters,
  grains, random sequences or modulation instead of resuming earlier state.
  VOSIM also prepares for about 4 ms in silence after selection. See
  [selection details](../docs/FEATURES.md#model-selection-and-reselection).
  Wave Terrain fixes playback for terrains 5–7, and 6-op FM starts without
  audio left over from the previous model.

## Customizing in Hygrostat

Profiles keep the module focused on the controls and sounds you use.
The standard layout remains available, and customized layouts get matching
guides. Width and diffusion follow the installed profile's defaults until
edited. A new profile's Start in quality setting survives power cycles once
saved. An automatically restored frozen recording keeps its original quality.

- **Gain ceiling:** custom options pages can cap automatic input gain at
  +24, +18 or +12 dB. The standard profile uses Stock (+32 dB).
- **Send mixing:** keeps dry audio present while DRY/WET sets new grains' levels.
  At zero, existing grains and reverb finish and recording continues.
  Granular only.
- **Synth output level:** Stock, Half, Quarter or Eighth; applies to Beads,
  Plaits and Rings audio without changing input gain or OUT R triggers.
- **Divided grain triggers:** choose Stock, /2, /4, /8 or /16 on OUT R. Division
  keeps the first gate and its pulse width without changing grain timing or audio.
- **LED brightness:** a new 15% setting joins 66% and 33%. At these levels,
  measured light curves help preserve the quality LED's colour as it dims. The
  curves were measured on one module, and colour and brightness can vary from
  unit to unit and with temperature. Drizzlet's factory pitch-scale colours and
  light-blue boot cue were retuned. At full brightness, stock LED colour values
  are unchanged.

These choices need a matching Beta 4 profile in Hygrostat. See the
[options guide](../docs/OPTIONS.md) and [feature catalogue](../docs/FEATURES.md).
Heavy Palette banks may crackle. Hygrostat currently offers no CPU warning
or CPU-based export limit. Flash capacity and model dependencies still apply.

## Updating and remaining checks

Beta 2/3 Drizzlet options reset to the profile's defaults. Calibration and
stock settings are retained. Save your profile and check its guide after
updating. Follow the [installation and recovery instructions](../README.md);
keep speakers disconnected during audio updates.

<!-- Before publication, verify the final model and editor assets, replace the date and tag, and add exact-image module results and known issues. -->
