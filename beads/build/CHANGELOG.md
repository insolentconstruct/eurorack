# Changelog

Put new changes under an `## Unreleased` heading. For a release, replace it with a dated heading.

## Unreleased

- Plaits synth: all 24 models are installed in flash after the app, written by the same WAV (#194). They play bit for bit as before, except that a model fades in over its first 48 samples (1 ms) each time a voice switches to it, from a fresh start where it used to keep its old filters; Wave terrain's terrains 5-7 keep their path inside the wavetable bank where Plaits read past it, and 6-op FM no longer plays the previous model's leftovers in its first blocks. The WAV plays for about 9 minutes.
- The pitch notch cue lights [A] pure blue instead of green (#313), apart from every quality colour.
- Optional Hygrostat Resonator (#88): Rings' modal resonator, 24 modes in stereo, after the grains or before the buffer; Off by default. PITCH tunes it with Pitch scale and Recent notes, Attacks hits strike it, and all 32 grains still play. Its controls are fixed for now.
- Custom profiles can put a two-value setting on [A]+SEED or [A]+FREEZE (#301): Reverb character, Processor, Frozen feedback, Synth (on [A]+SEED only) or TIME modulation. The gesture flips and saves it, and [A] shows the new value's options colour. Processor clears the recording, as from its page. [A]+FREEZE still steps the Plaits bank in Plaits synth play.
- Custom profiles can put Manual gain, Stereo width or Diffusion on any of [A]+FEEDBACK, [A]+DRY/WET and [A]+REVERB, or leave manual gain off the knobs (#301). Without a choice, manual gain stays on FEEDBACK. Older firmware refuses these profiles.
- Snap and trigger with SEED unpatched no longer stops the synths or replaces the delay's knob timing (#290). Each hit adds a grain on top of DENSITY's own grains, at noon too, in granular playback only; synths, delay and spectral ignore hits. With SEED patched, hits add grains instead of joining the SEED clock, so latch Off no longer counts them in its divider and Ignore no longer drops them.
- Snap and trigger starts grains from hits while FREEZE is on, too (#294); marks are still taken only while recording.
- SEED latch Sync is a hard sync (#293): each edge starts a grain as well as restarting the grain clock, so a clock faster than DENSITY's rate, or DENSITY at noon, plays a grain per edge instead of none. Both synths follow.
- Changing Processor while FREEZE is on no longer silences the module (#288): the current engine plays on, and the change happens when FREEZE is released.
- The [A] hold acts when [A] is released, and not at all if a knob was turned or a button pressed meanwhile (#289), so slow width, diffusion or Plaits bank gestures no longer clear a saved manual gain or toggle the synth first. Stock acts at 1.5 s while held; a long [A] press during calibration also steps on release; [A]+FEEDBACK ends as in stock.
- Custom profiles: Monophonic grains or Buffer trim on the [A] hold, and Synth start on Manual or Adjustable without a gesture, are refused by the firmware and by Hygrostat (#291, #292). A refused layout plays with the standard controls and saves nothing until it is re-exported from Hygrostat; no saved data is erased.
- Custom profiles can use options pages 5-7; page LEDs: 4 FEEDBACK+DRY/WET, 5 FEEDBACK+REVERB, 6 DRY/WET+REVERB, 7 all three.
- Optional options-page idle timeout in Hygrostat (Off, 5, 10, 20 or 30 s); Off by default.
- Optional Hygrostat options-page choices, today's behaviour by default (#285): skip the walk through the stored values when a page opens; always open on the first page instead of the last one used.
- Optional Hygrostat Start in quality (#287): a profile can choose the quality Beads starts in after the profile is installed and after a settings reset, until something is saved; Keep current by default. Saved settings now record which profile saved them (fields 16 and 18).
- Optional delay-mode SEED latch fade in Hygrostat (Off: stock switch; Fade: 10 ms dip, no pop); Off by default (#183).
- Optional Hygrostat choices, all stock or unchanged by default: manual gain at unity when no gain is saved (#19); Attacks sensitivity Low or High (#58, #47); SEED unlatched at power-on (#41); a smooth PITCH knob without notches (#43).
- Optional Hygrostat choices, stock by default: a gentler DRY/WET law whose wet side rises linearly (#21); reverb on the grains only, before the dry/wet mix as on Clouds (#20). Granular, delay and spectral; both synths unchanged.
- Random pitch steps gains Recent notes (#220): with PITCH patched, each new counter-clockwise grain plays one of the last one to eight notes sent to PITCH. Page 1 DRY/WET is now in thirds: Free, Fifths and octaves (middle), Recent notes. Beta 2 reads a saved Recent notes as Free.
- Recent notes takes a glided note where the glide stops, not on the way, for glides faster than half a semitone per second (#243); stepped notes still count after 10 ms.
- Recent notes now waits up to 1 s after a glide, so glides down to a tenth of a semitone per second count only where they stop (#243); stepped notes still count after 10 ms.
- Optional Hygrostat delay choices, stock by default: a unity notch on FEEDBACK for sound on sound, with a 10 Hz bass filter in the loop (#10); DENSITY crossfades to a new delay time, once it stops moving, instead of bending the pitch (#30).
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
- Grain stealing returns to the 2021 and 2024 firmware's by default (#237): grains arriving 5 to 25 ms apart re-sort again, a little louder in dense textures and 3.4 to 3.6 points more peak load (Cold digital not measured). The 2026 source's stealing, used in Betas 1 to 3, is a Hygrostat choice. Granular and wavetable synth.
- Audio output is one frame later (21 µs at 48 kHz, 31 µs at 32 kHz, 42 µs at 24 kHz), so the audio callback can run to about the full block instead of stopping a frame short (#1). In uninterrupted playback it is otherwise identical; a quality, mono/stereo or synth change drops the one pending frame, inside the fade. Feedback loops through OUT and IN gain one frame of delay, and audio moves one frame against gates or triggers. Module-qualified through the beads-tuneup build, which has the same codec driver: late samples start at 96.9%, 97.3% and 97.7% of the block at 48, 32 and 24 kHz (from 90.8%, 86.6% and 82.7%). On the module a Beta 4 test build is clean up to 97.8% at 48 kHz and 98.1% at 32 kHz, so the working limits there rise from 90% to 96.5% and 97.0%; 24 kHz is not yet measured and keeps 90%.
- Plaits synth Speech: turning TIME (HARMONICS) down from a word bank to the phonemes during or after a word no longer plays a burst of noise (#280). The word's frame numbers were read from the much shorter phoneme table; the current frame now finishes and Speech returns to scanning the phonemes. Stock Mutable Instruments Plaits code; every other path is unchanged.
- The Stock reverb loads its delay-line reads before the sample's writes, as in beads-tuneup (#258). Same sound; on a development board about 1,200 fewer cycles per block (1.0 CPU point) in the busiest 48 kHz granular case and in the wavetable synth, 0.5 to 0.6 points at 32 and 24 kHz. The Clouds-style reverb is unchanged.

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
