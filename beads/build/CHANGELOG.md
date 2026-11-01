# Changelog

Put new changes under Unreleased. For a release, replace that heading with a dated heading.

## Unreleased

<!-- TODO: release date; replace YYYY.MM.DD in the heading and tag. -->
## YYYY.MM.DD (beta 2 "Fog")

- Hygrostat builds custom firmware, controls, defaults and guides.
- Replace and reorder the scale bank; edit notes, repeat intervals and LEDs.
- Revise factory scale colours; pentatonics blink and Shri uses two pink pulses.
- Keep synth reverb tails by default; optionally Clear on exit in Hygrostat.
- Invalid profiles use standard controls without saving; storage repair remains available.
- Older Drizzlet options, width and diffusion reset to profile defaults.
- Add optional boot-version, delay and pitch-notch lights.
- Add optional manual input attenuation with unity at noon.
- Add optional 100%, 66% and 33% LED brightness.
- Add optional live CV-assignment lights and fully lit FREEZE during saves.
- Add optional SEED latch Ignore, Sync and Add modes.
- Add optional TIME scanning with the attenurandomizer.
- Add random fifths/octaves at page 1, DRY/WET.
- Expand options to four pages with stored-value previews.
- Move stereo width to [A]+DRY/WET.
- Make mono/stereo and synth switching pop-free at 48 kHz.
- Add Spectral processing at page 3, FEEDBACK.
- Add attack snapping/triggers at page 1, REVERB.
- Add evolving frozen feedback at page 2, REVERB.
- Add six-voice, 24-model Plaits synth at page 3, DRY/WET.
- Set grain diffusion with [A]+REVERB; assign CV with [M].
- Add addressed combo HEX for ST-Link recovery. Erase only the sectors being written. Never use a full-chip or mass erase: it also wipes calibration and settings.
- Pause saves during supply sags; resume when supply recovers.
- Recover calibration automatically and start despite damaged settings.
- Hold SEED at boot to repair lost calibration/settings, then recalibrate.
- Skip damaged frozen buffers.
- Prevent settings-save failures that left modules unable to start.

## 2026.10.18 (beta 1 "Mist")

- Add three options pages with automatic saving.
- Include print, phone and text options guides.
- Add Chromatic and eight scales at page 1, FEEDBACK; SEED steps scales.
- Add Clouds-style reverb/diffusion at page 1, REVERB.
- Add stereo width and CV assignment at page 1, DRY/WET.
- Add Auto, Never and Wide detent delay at page 2, DRY/WET.
- Hold [A], then FREEZE to trim recording; saves retain trim.

## 2026.10.16 (alpha 2)

- Add manual synth start/stop: hold [A] 1.5 seconds, inputs unpatched, FREEZE off.
- Interrupted buffer saves double the wait, up to 320 seconds.
- Hold [A], then SEED for monophonic grains; disable before downgrading.
- Fix delay tempo after prolonged absence of clocks.
- Prevent empty frozen delay slices from very fast SEED edges.

## 2026.10.14 (alpha 1)

- Add Stock, Fast release and Off limiter: hold [M] three seconds, DRY/WET.
- Reject damaged, interrupted or failed frozen-buffer saves.
- Prevent settings saves from breaking buffer backups.
- Fix frozen delay playback with negative FM.

## 2026.10.11

- First build of unmodified Mutable Instruments Beads source.

## Earlier Mutable Instruments firmware

### 2024 recovery firmware → vanilla source

- Improve grain stealing; our module stopped clicking at high DENSITY/PITCH.

### Factory firmware → 2021 forum release

- Fix left-channel overload crackles in 48 kHz stereo.
- Smooth recording resumption after FREEZE.
- Smooth delay TIME/clock changes and slice transitions.
- Reduce delay-tap modulation and smooth granular/delay transitions.
- Improve grain stealing.
- Reduce supply whine with some power supplies.
- Smooth dry wavetable pitch and improve high notes.
- Narrow attenurandomizer noon notches and SIZE deadband.
- Keep PITCH transposition at unpatched attenurandomizer extremes.
- Let reverb tails fade at minimum REVERB.
- Lower wavetable root one octave.
- Remove PITCH CV low-pass filtering.
- Change clocked DENSITY probability and quantized-CV response.
- Increase FEEDBACK range 30%.
- Add four FM wavetables.
- Toggle autosave with [M]+FREEZE; dim FREEZE during saves.
- Filter delay with SIZE attenurandomizer.
- Show overwrite point/manual gain on [A]; gain clipping follows quality.

### 2021 forum release → 2024 recovery firmware

- Hold SEED at boot to reset settings while keeping valid calibration.
- Smooth knob/CV changes.
- Improve delay DENSITY/FM response and frozen-slice/TIME stability.
- Let both FM inputs affect the clock divider.
