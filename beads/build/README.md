# Drizzlet

Drizzlet is alternative firmware for Mutable Instruments Beads, adding Plaits, spectral processing and grain controls. Earlier builds were tested on one module. Final qualification of Beta 2 “Fog” is pending. Install at your own risk. This is not a Mutable Instruments product. All settings save; standard defaults retain stock behaviour.

## Downloads and installation

Download audio updates and guides in `<tag>-wav.zip` from https://github.com/insolentconstruct/eurorack/releases. For ST-Link, the combo ZIP includes bootloader and application images. HEX carries its address; BIN uses 0x08000000.

Updating is not a remedy for unexplained module behaviour. Disconnect speakers: update audio can harm hearing. Unplug all CV connections, connect your audio output to IN L, set TIME near noon and power on holding FREEZE. Silence other computer sounds and disable effects. Play the update WAV, adjusting TIME or the source volume until [A] shows yellow. It restarts when finished; if it doesn't, start again from the beginning. If all LEDs blink red, press FREEZE and retry louder. Otherwise, try 44.1 kHz or another computer or interface. Audio updates leave the bootloader and calibration untouched.

## Options page

[A] is the quality button; [M] is CV assign. Hold [M] alone for three seconds with the mix knobs still, then release. Tap [M] to change pages. The indicated mix LED pulses. Reopening remembers the page until power-off; startup selects page 1.

[A] previews each stored value in turn for about two seconds, with its knob LED steady. Turn a knob or tap SEED to end preview. Move a knob slightly to take over; [A] shows its setting. Unused knobs do nothing.

Page / pulsing LED  FEEDBACK      DRY/WET           REVERB
1 / FEEDBACK        Pitch scale   Random intervals  Attacks
2 / DRY/WET         Delay mode    Reverb character  Frozen feedback
3 / REVERB          Processor     Synth             Synth start
4 / all three       Not used yet  Not used yet      Limiter

Changes save about a second after turning stops. Wait before power-off. Hold [M] for three seconds or press another button to close; mix controls catch up smoothly. On page 1 with Scale selected, SEED taps step the scale instead of closing. The page LED flashes on entry, close and save. First values are steady green; later values use yellow, red and blinking colours. See [OPTIONS.md](docs/OPTIONS.md) for every value.

Output limiter: REVERB on page 4 selects Stock, Fast release or Off in thirds, indicated by steady green, yellow or red respectively. Off can hard-clip loud peaks.

Pitch scale. Three zones: Off, Chromatic and Scale. The factory bank cycles through Fifths (steady coral), Major (steady amber), Minor (steady blue), Major pentatonic (slow amber), Minor pentatonic (slow blue), Pelog (steady teal), Raag Bhairav (steady violet) and Raag Shri (two pink pulses). Tap SEED in Scale to step; holds do nothing, and taps neither trigger nor latch. Pelog and the raags are microtonal. Grains combine knob, CV and randomness at birth and retain that pitch. Synth grains follow scales; the continuous oscillator and delay do not. Hygrostat can replace and reorder this bank, including each scale definition, colour and flash pattern. Use the guide generated for your profile.

Random intervals selects Free or Fifths and octaves. The latter quantizes counter-clockwise PITCH randomness, including synth grains; clockwise stays continuous. The pitch scale still applies.

Attacks selects Off, Snap or Snap and trigger in thirds. Snap moves grain starts onto recorded hits; Snap and trigger also triggers a grain per hit. Patched SEED still works; only granular playback uses marks.

Reverb character selects Stock or Clouds-style, which adds diffusion until manually set. In delay and wavetable synth, only reverb changes. Switching fades sound and clears the tail.

Frozen feedback selects Stock or Evolving. Evolving fades in to soften and drift frozen grains, controlled by FEEDBACK; it stops with FREEZE off.

Processor: Lower half selects Granular (Stock); upper half selects Spectral, using Mutable Instruments Clouds. TIME selects a spectrum, SIZE warps it, SHAPE quantizes it, PITCH shifts it and DENSITY controls renewal and phase scrambling. SEED glitches; FREEZE holds. Random controls update per spectrum. Mixing, quality, assignable CV and R gate work normally; diffusion, delay and wavetable synth are unavailable. Closing fades the change and clears recording. The saved frozen buffer returns with Granular.

Synth selects Beads (Stock) or six Plaits voices. Triggers advance voices, retaining pitch and random settings. FEEDBACK selects 24 models; TIME controls HARMONICS, SIZE controls TIMBRE and SHAPE controls MORPH. DRY/WET sets decay; FREEZE sustains. Other grain controls remain. At DENSITY noon with SEED unpatched, the last voice drones.

Synth start selects Automatic (Stock), starting after ten seconds with both inputs unpatched, or Manual. With inputs unpatched and FREEZE off, hold [A] about a second and a half to start; repeat to stop. Patching an input stops it once FREEZE is off. With an input patched, holding [A] restarts automatic gain.

## Delay mode

Delay mode: Auto (Stock) enters delay at SIZE fully clockwise. Disabled keeps granular playback there. Wide/smooth filter retains delay with a neutral middle and no resonant boost at either end. SIZE CV and its attenurandomizer control the filter. FEEDBACK on page 2 selects these values in thirds; [A] shows steady green / yellow / red respectively. The saved default is Auto (Stock). This does not affect wavetable synth mode.

## Stereo width

Hold [A] and turn DRY/WET to set grain width; release to save. At the bottom of the knob it is Stock; turn upwards to enable width, reaching full random spread clockwise. Minimum width preserves stereo position and centres mono. Each grain, including synth, retains its pan, even at maximum DENSITY. [A] meters green through yellow to red; tapping [A] alone changes quality. Enabled WIDTH follows the three stock [M] destinations, pulsing all mix LEDs. CV input 7 adds width; custom assignments work. Stock retains routing and pan rounding.

## Grain diffusion

Hold [A] and turn REVERB; [A] meters the amount. Fully counter-clockwise removes diffusion. Until set, Stock has none and Clouds-style uses its own amount. Setting it adds DIFFUSION after REVERB and WIDTH in the [M] cycle, with REVERB pulsing alone. The amount is saved. Diffusion affects grains only.

## Monophonic grains

Hold [A], then press SEED to toggle this saved setting, initially off. [A] dims when enabled; new grains fade earlier grains. Square and decay shapes suit it; slower shapes can become nearly silent. Recover by changing shape, reducing triggers, shortening SIZE or disabling it. Disable before downgrading.

## Buffer trim

Hold [A], then press FREEZE to restart the recording window at zero length and clear the FREEZE latch. Release both, record, then freeze. A FREEZE gate holds the window empty until low. Existing grains finish on old audio; new grains use the new window. Saved buffers keep trimmed lengths; stock loads them full-length.

## Recording and switching

Releasing FREEZE resumes recording with a crossfade. Quality changes, input changes between mono and stereo, leaving wavetable synth, and power-up without a saved buffer start an empty recording. Switching without changing sample rate is pop-free, including synth switching at 48 kHz. Quality changes involving 32 or 24 kHz still briefly silence the output and stop synth reverb.

SEED starts latched. Latching it in stock delay can pop (#183); this is explicitly deferred beyond Beta 2. The unresponsive-module report #112 remains unresolved; neither a successful updater completion nor recovery has been confirmed.

## Frozen-buffer autosave

FREEZE saves after ten seconds. Interrupting a save doubles the next wait up to a cap, until a save completes or FREEZE stays off for a minute. In wavetable synth that minute begins on exit. Power-up resets the wait. Supply sags pause saves; interrupted saves lose only that save.

## Hygrostat

https://hygrostat.insolentconstruct.net/ builds updates and guides locally. Choose features, controls and defaults, using templates or custom Off, Fixed and Adjustable settings. It warns about CPU load and refuses excessive combinations. Per-feature estimates await module measurements.

Final-candidate profiles, custom scales, LED indications and tail choices still need module qualification. These nine additional options are off in standard firmware:

- Version at boot flashes light-blue, then twice for Beta 2, without delaying startup.
- Delay indication flashes [A] red entering or leaving delay.
- Pitch notch indication lights [A] green at unison and octave knob plateaus, including both ends, independently of CV and randomness. These cues yield to warnings and meters.
- Live assignment LEDs follow CV magnitude, retaining assignment patterns and stock weighting.
- FREEZE during saves keeps its LED fully lit.
- Input attenuation extends [A]+FEEDBACK to silence, with unity near noon, marked green, and gain above. Saved gain immediately uses this range. Automatic gain never attenuates; hold [A] to restart.
- LED brightness selects 100%, 66% or 33%, fixed or adjustable in thirds with preview; warnings dim too. The default is 100%; there is no power-on gesture.
- SEED latch selects Off, Ignore, Sync or Add in quarters. With a cable patched and SEED latched, Off retains the divider; Ignore free-runs without cable triggers or flashes; Sync restarts grain-clock phase per edge; Add adds a grain per edge. Synth clocks follow these modes; delay free-runs outside Off. Unpatched attack triggers still work.
- TIME modulation selects Random below noon or Scan above. With TIME CV unpatched, its attenurandomizer scans backwards or forwards, stopping at noon, taking four seconds per buffer at either end. TIME offsets the wrapping position. Patching CV restores normal control and pauses scanning, as do delay, spectral and synth. Position is not saved.


Hygrostat can replace the complete scale bank with up to 14 scales, each with 1–24 notes including its root and a repeat interval from 0.01 to 128 semitones. Edit notes in semitones or cents, import Scala tuning files within these limits, include or remove scales, reorder them and choose RGB colours with steady, slow, fast or two-pulse indications. Factory reset restores the starting bank. Off and Chromatic remain direct choices. Reordering preserves saved identity; removing the selected scale falls back to Off. Shared links and generated guides include the chosen bank. Custom-bank CPU costs still need final module measurements.

Synth reverb tail is a separate profile choice: Keep is the stock default; Fixed Clear clears the tail when leaving either synth by manual stop, input patching or processor changes. It adds no panel gesture or options-page slot. Sample-rate changes still restart the converter and briefly interrupt audio.

## Saved data

Stock settings and calibration carry over; earlier Drizzlet options, width and diffusion reset to profile defaults. Frozen buffers remain stock-compatible. Disable monophonic grains before downgrading. Stock clears Drizzlet options when saving and loads trimmed buffers full-length; earlier Drizzlet reads new settings as stock.

## Reset and recovery

Hold SEED at power-on only to reset settings; valid calibration is kept.

Damaged calibration restores automatically from its copy; damaged frozen buffers are skipped. If both calibration copies are lost, all LEDs blink fast for three seconds. It then uses factory calibration and defaults, saves nothing and restores no buffer.

Power on holding SEED to repair. Blinking green [A] with mix LEDs lit means success; recalibrate. Fast red and mix blinking means failure; retry. If reinstalling fails to restore startup, consult https://github.com/insolentconstruct/eurorack/releases/tag/storage-recovery-2026.10.27 and its warnings.

Back up calibration before ST-Link recovery. Erase only written sectors; a full-chip or mass erase loses calibration and settings. See [RECOVERY.md](docs/RECOVERY.md) for backup, restore and ST-Link commands. Use backups only on their original module.

## Building it yourself

From the repository root on an amd64 host with Docker:

```sh
git submodule update --init stmlib stm_audio_bootloader
docker build --platform linux/amd64 --target compiler -t beads-build-kit beads/build
bash beads/build/build.sh "$PWD/artifacts" --wav
```

## Licence and credits

Code: MIT, from the Mutable Instruments source, with credits to Frédéric Meslin / Fred's Lab and Oliver Rockstedt; see [LICENSE](LICENSE).
The manual excerpt and hardware designs are licensed CC-BY-SA 3.0.
Copyright and permission notices are in `NOTICE.txt`; preserve them when redistributing.

Report problems at https://github.com/insolentconstruct/eurorack/issues and questions at https://github.com/insolentconstruct/eurorack/discussions. Do not contact Mutable Instruments. See CHANGELOG.md for changes, docs/TESTING.md for testing, and the [Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) for stock controls. See [CALIBRATION.md](docs/CALIBRATION.md) to recalibrate.
