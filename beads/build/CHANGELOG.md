# Changelog

Changes to Drizzlet, newest first. New changes go under an "Unreleased"
heading; a release turns it into a dated heading.

## Unreleased
- Add random intervals (page 1, DRY/WET): with the PITCH attenurandomizer counter-clockwise, each grain's random offset lands on fifths and octaves around the knob's pitch; clockwise it stays continuous. It works with any pitch scale (#8).
- The options page now has four pages, lighting the mix LEDs left to right, then all three: 1 pitch scale, random intervals and attacks; 2 delay mode, reverb character and frozen feedback; 3 processor, synth and synth start; 4 limiter. Opening a page shows each stored value on [A] in turn. The wavetable synth's start (Automatic or Manual) is now its own setting, Synth start.
- Stereo width moves off the options page: hold [A] and turn DRY/WET, like manual gain and grain diffusion. Saved widths are kept.
- Quality changes, mono/stereo switches and the wavetable synth starting or stopping now fade the output out briefly before the change takes effect, and the synth's reverb tail no longer plays on after it stops. Changes that keep the sample rate no longer stop the converter, which removes the pop it made in every earlier firmware; changes to or from vintage digital (32 kHz) and dirty tape (24 kHz) still restart it. The brief silence during a change, and the empty buffer after a quality change, remain (#53).
- Add a spectral engine: Clouds' spectral processing in place of the grains, chosen with Processor (page 3, FEEDBACK): Granular (Stock) or Spectral. TIME picks the stored spectrum, SIZE warps, SHAPE quantizes, PITCH shifts (with the pitch scale), DENSITY sets the refresh rate and phase randomization, holding SEED glitches and FREEZE holds. Changing processor clears the recording; a frozen buffer saved to flash is kept (#3).
- Add attacks (page 1, REVERB): Snap starts grains on the hits in the recording instead of partway through their decay; Snap and trigger also starts a grain on each hit (#58, #47).
- Add frozen feedback (page 2, REVERB): Evolving runs the grains through a slowly moving all-pass loop while FREEZE is on, so a frozen sound drifts and softens instead of repeating exactly; FEEDBACK sets how far (#4).
- Add Plaits as a second synth (page 3, DRY/WET): six voices of Mutable Instruments' Plaits play in place of the wavetable synth, with FEEDBACK picking one of 24 models, TIME, SIZE and SHAPE as HARMONICS, TIMBRE and MORPH, DRY/WET as decay and FREEZE as sustain (#50).
- Add adjustable grain diffusion: hold [A] and turn REVERB, with either reverb. Once set, it is also an [M] destination for CV. Clouds-style reverb keeps its Beta 1 diffusion until you set an amount (#2).
- Clouds-style reverb uses less CPU; the sound is unchanged.
- Grains use less CPU (about 6% of the audio budget in the busiest case); the sound is unchanged.

## 2026.10.18 (tag `drizzlet-2026.10.18`, beta 1 "Mist")
- New options page layout on three pages. Page 1 (REVERB LED): pitch scale, stereo width, reverb character. Page 2 (FEEDBACK LED): delay mode. Page 3 (DRY/WET LED): wavetable synth (formerly self-play) and limiter. Changes save by themselves about a second after you stop turning; close by holding [M] for three seconds or pressing another button. Values show on the quality LED [A] in green, yellow and red, steady or blinking; stock values are steady green.
- Printable, phone and text guides to the options page, also in the release zip.
- Add pitch scale (page 1, FEEDBACK): Off, Chromatic, or one of eight scales from Marbles, chosen by tapping SEED. Each grain's pitch, including randomness, snaps to the scale when the grain starts (#6, #8).
- Add Clouds-style reverb (page 1, REVERB): the Clouds reverb's character, plus diffusion on the grains (#2, #22).
- Add stereo width (page 1, DRY/WET): spreads grains randomly across the stereo field, and can be a fourth [M] destination for CV (#5).
- Add delay mode (page 2, DRY/WET): Auto (Stock), Never (SIZE fully clockwise stays granular) or Wide detent (a neutral zone in the middle of the delay filter, without the resonant boost) (#9, #26).
- Add buffer trim: hold [A], then press FREEZE to start a fresh recording window; the next freeze keeps only what was recorded since. Saved buffers keep the trimmed length (#31).
- More CPU headroom: about 6,900 fewer cycles per audio block in the heaviest stock case, with the same sound.

## 2026.10.16 (tag `drizzlet-2026.10.16`, alpha 2)
- Self-play option (options page, FEEDBACK): Automatic (Stock) or On request, started and stopped by holding MEDIA 1.5 s with no inputs and FREEZE off (#17).
- Less flash wear from a modulated FREEZE: each interrupted buffer save doubles the wait before the next, up to 320 s, reset by a completed save or a minute unfrozen (#56).
- Monophonic grains (Mutable Instruments' original code): hold MEDIA and press SEED to toggle; as Mutable Instruments noted, this works well only with the first two SHAPE envelopes (square and decay), and should be turned off before downgrading (#31).
- App built with GCC 9 instead of 4.9.3; 20 of 20 emulated scenarios give bit-identical audio.
- Delay mode: no tempo glitch on the first clock after about 12 hours without one (#57).
- Clocked frozen delay: very fast SEED edges no longer create a zero-length slice (#33).

## 2026.10.14 (tag `drizzlet-2026.10.14`, alpha 1)
- Output limiter option (Stock, Fast release, Off): hold ASSIGN for three seconds to open the options page and set it with DRY/WET.
- Frozen-buffer backups carry a CRC and are range-checked; stock and Drizzlet still load each other's backups.
- An interrupted or failed backup is never restored.
- A setting saved during a backup no longer breaks the backup.
- Frozen delay with negative FM no longer reads outside the buffer (#33).
- Fixed undefined reads in the dummy CPU-load grains and past the last wavetable (output unchanged).
- Named Drizzlet; release tags are `drizzlet-YYYY.MM.DD`.

## 2026.10.11
- First build (tag `beads-2026.10.11`): the vanilla Mutable Instruments Beads source, unmodified.

## Earlier Mutable Instruments firmware

- **Factory firmware:** what modules shipped with at launch. We have no copy; Mutable Instruments' release notes for the 2021 update list what changed (below).
- **2021 forum release:** `beads.wav`, posted on the Mutable Instruments forum in April 2021.
- **2024 recovery firmware:** `beads_patch_firmware.wav` / `beads_golden_bootloader_combo.bin`, shared in the ModWiggler Beads thread in early 2024 to fix modules stuck on boot.

These are the source changes needed to reproduce the older images, not the original history. The vanilla bootloader matches the 2024 combo bootloader byte-for-byte. Effects below come from code and emulation, not listening tests.

### 2024 recovery firmware → vanilla source

One change: grain stealing in polyphonic mode (`beads/dsp/granular_player.h`). New grains fade out safety grains early to keep a voice free. The older firmware steals the oldest grain at intervals <5 ms and otherwise sorts by remaining time; this build steals the oldest at intervals <25 ms. Between 5 and 25 ms it also swaps two safety-grain fade-out caps, so the stolen grain fades out within 5 ms instead of one grain interval.

In emulation with identical input and state, outputs are bit-identical at intervals of 25 ms or more and differ from 5 to <25 ms. Below 5 ms both run the same code, though differences can persist briefly if earlier intervals in the 5–25 ms range changed grain history. The "freeze gate" and "gated density" bench tests reached the changed range.

On our module the 2021 and 2024 firmware clicked at DENSITY 0.98 and PITCH +24 semitones; the vanilla source and Drizzlet did not (145 and 164 clicks against 0; recipe and counts in TESTING.md). A desktop run of both schedulers produced no clicks, so that test found no click in the DSP itself. CPU timing is the working hypothesis: the 2024 scheduler costs more in that range, but it has not been timed on the module.

Monophonic mode is compiled out in these older builds (`ENABLE_ADVANCED_FEATURES` is commented out).

### Factory firmware → 2021 forum release

From Mutable Instruments' [release notes](https://forum.mutable-instruments.net/t/beads-firmware-update-release-candidate/18572) (April 2021), condensed; this list is theirs, not ours.

- **Audio:** a CPU overload causing crackles on the left channel in 48 kHz stereo, with some SIZE and DENSITY settings, fixed "through several code optimizations having no negative impact on the module’s functionality". A longer crossfade when recording resumes after FREEZE. Smooth transitions between TIME factors in delay mode, crossfades when an external clock changes the delay time, and slices that finish before TIME picks the next one. Tamer modulation of the secondary delay tap. A fade between granular and delay mode. More cautious grain stealing. Less bursty power draw, removing a whine with some power supplies. Interpolated pitch for the dry wavetable signal, and better band-limiting for high notes.
- **Controls:** a smaller notch at noon on the attenurandomizers, and a smaller SIZE deadband at 10:30 for shorter grains. With no CV patched, the PITCH knob still transposes with its attenurandomizer at either extreme. REVERB at minimum stops the send, so the tail fades. The wavetable root is an octave lower. No software low-pass on PITCH CV. A quadratic probability curve for clocked DENSITY. Less hysteresis on quantized CVs. FEEDBACK range up 30%.
- **New:** four FM wavetables; [M] + FREEZE toggles buffer autosave; FREEZE dims while the buffer saves; the SIZE attenurandomizer filters the delay; the quality LED shows the overwrite point and the manual gain; manual-gain clipping depends on the recording quality.

### 2021 forum release → 2024 recovery firmware

Five changes:

1. **Settings recovery:** SEED held at power-on rewrites settings while keeping valid calibration, fixing modules stuck on boot. The 2021 release lacks this path.
2. **Knob/CV smoothing:** stmlib's `HysteresisFilter` loses its brief tracking mode after a change. State per channel shrinks from 28 to 20 bytes.
3. **Delay density/FM smoothing:** the dead band changes from 0.005 to 0.0025. The FM follower no longer tracks the input directly for the 7 calls after a change.
4. **Frozen delay slices:** during frozen playback, manual and clocked delay add hysteresis with a 0.0025 dead band to slice/TIME selection.
5. **Clocked delay FM:** both FM inputs affect the clock divider. Exponential FM previously bent tap ratios separately. Manual delay is unchanged.

None of these touch grain playback, and the 2024 firmware clicks like the 2021 release. Upgrading from 2021 to this build includes all six changes. Patches and comparison details for the older builds are available on request.
