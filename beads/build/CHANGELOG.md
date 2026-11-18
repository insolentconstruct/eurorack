# Changelog

Put new changes under an `## Unreleased` heading. For a release, replace it with a dated heading.

## Unreleased

- Custom profiles can use options pages 5-7; page LEDs: 4 FEEDBACK+DRY/WET, 5 FEEDBACK+REVERB, 6 DRY/WET+REVERB, 7 all three.
- Optional options-page idle timeout in Hygrostat (Off, 5, 10, 20 or 30 s); Off by default.
- Optional delay-mode SEED latch fade in Hygrostat (Off: stock switch; Fade: 10 ms dip, no pop); Off by default (#183).
- Optional Hygrostat choices, all stock or unchanged by default: manual gain at unity when no gain is saved (#19); Attacks sensitivity Low or High (#58, #47); SEED unlatched at power-on (#41); a smooth PITCH knob without notches (#43).
- Optional Hygrostat choices, stock by default: a gentler DRY/WET law whose wet side rises linearly (#21); reverb on the grains only, before the dry/wet mix as on Clouds (#20). Granular, delay and spectral; both synths unchanged.
- Random pitch steps gains Recent notes (#220): with PITCH patched, each new counter-clockwise grain plays one of the last one to eight notes sent to PITCH. Page 1 DRY/WET is now in thirds: Free, Fifths and octaves (middle), Recent notes. Beta 2 reads a saved Recent notes as Free.
- Recent notes takes a glided note where the glide stops, not on the way, for glides faster than half a semitone per second (#243); stepped notes still count after 10 ms.
- Plaits synth banks (#190): FEEDBACK selects one of eight models in the green, red or yellow bank, as on Plaits, instead of all 24; [A]+FREEZE steps the bank during Plaits synth play and keeps FREEZE. The bank is saved; Hygrostat chooses the starting bank (green by default).
- With Version at boot on, the light-blue cue flashes once, then three times for Beta 3.
- Plaits synth Speech: changing word bank no longer overruns the audio block (#225). The bank decodes over several blocks; a word started meanwhile plays when it is loaded, 1.75 to 7 ms later, then exactly as before.
- Plaits synth: the 6-op FM banks are unpacked once for all six voices instead of at each voice's model switch (#270). Same sound; a voice switching to a 6-op model costs 13 to 18 CPU points less in that block on a development board.
- At 66% and 33% LED brightness, the quality LED no longer halves white (#240): its red dims faster (level^1.5) and lit green and blue keep a small offset (+4 at 66%, +8 at 33%), a rule chosen by eye on a module; to be measured with a light sensor in Beta 4. 100% is unchanged.
- Plaits synth Speech: the word decoder reverses bits through a 256-byte table (#270). Same sound; Speech's worst six-voice block is 6.1 CPU points lighter on a development board.
- Plaits synth Speech: the five word banks are decoded once when the synth starts and shared by all six voices (#270). Same sound; Speech's worst block (a bank change or a switch to Speech) is 14.4 CPU points lighter on a development board. The decoded banks use 36 KB more of the delay memory the synth already borrows.
- Plaits synth 6-op FM: two of Plaits' six-operator chain renderers are on (#270). Same sound; on a development board most 6-op FM blocks are 1.4 to 2.0 CPU points lighter, but switch-in blocks and FM B's worst steady block are unchanged.
- Spectral mode sets up its 4096-point FFT directly, so only that size's CMSIS tables are linked: the application is 38,912 B smaller, sound and audio code unchanged (#275).
- Plaits synth: Virtual analog, Phase distortion, Granular formant, Harmonic and Waveshaping skip Plaits' AUX output, which Beads never plays (#270). Same sound; their worst six-voice block is 8.7 to 23.5 CPU points lighter on a development board.

## 2026.11.09 (tag `drizzlet-2026.11.09`, beta 2 "Fog")

- Hold the converter in reset while its clocks change on quality changes; the click is about 25 dB quieter, not gone.
- Mark profiles with format number 1 instead of a release date; firmware accepts profiles of its own format.

- Hygrostat builds custom firmware, controls, defaults and guides.
- Replace and reorder the scale bank; edit notes, repeat intervals and LEDs.
- Revise factory scale colours; pentatonics blink and Shri uses two pink pulses.
- Keep synth reverb tails by default; optionally Clear on exit in Hygrostat.
- Invalid profiles use standard controls without saving; storage repair remains available.
- Older Drizzlet options, width and diffusion reset to profile defaults.
- Add optional boot-version, delay and pitch-notch lights.
- Add optional manual input attenuation with unity at noon.
- Add optional live CV-assignment lights and fully lit FREEZE during saves.
- Add LED brightness (100%, 66%, 33%) and SEED latch (Off, Ignore, Sync, Add) at page 4, initially 100% and Off.
- Page 2: FEEDBACK selects Frozen feedback, DRY/WET Delay mode, REVERB Reverb character.
- Add optional TIME scanning with the attenurandomizer.
- Add random fifths/octaves at page 1, DRY/WET.
- Expand options to four pages with stored-value previews.
- Move stereo width to [A]+DRY/WET.
- Make mono/stereo and synth switching pop-free at 48 kHz.
- Add Spectral processing at page 3, FEEDBACK.
- Add attack snapping/triggers at page 1, REVERB.
- Add evolving frozen feedback at page 2, FEEDBACK.
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
