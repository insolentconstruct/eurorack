**Beta 2 “Fog”** — alternative Beads firmware. Earlier builds were tested on one module;
final qualification of the expanded candidate is pending. Standard options default to stock behaviour.

TODO: release date; rename this file and replace `{TAG}` with `drizzlet-YYYY.MM.DD` before publishing.

## Controls

[A] is the quality button; [M] is CV assign. Hold [M] for three seconds,
then release to open the options page. Tap [M] to change pages. Turn a knob
and wait one second to save. Hold [M] for three seconds or press another
button to close. [A] previews the stored values.

| Page / LED | FEEDBACK | DRY/WET | REVERB |
|---|---|---|---|
| 1 / FEEDBACK | Pitch scale | Random intervals | Attacks |
| 2 / DRY/WET | Delay mode | Reverb character | Frozen feedback |
| 3 / REVERB | Processor | Synth | Synth start |
| 4 / all three | Unused | Unused | Limiter |

- Hold [A] and turn REVERB to set grain diffusion with either reverb.
  It can also receive assignable CV through [M].
- Select Plaits with DRY/WET on page 3. Six voices play one of 24 models
  selected by FEEDBACK. TIME controls HARMONICS, SIZE controls TIMBRE and
  SHAPE controls MORPH. DRY/WET sets decay; FREEZE sustains.
- Select Spectral with FEEDBACK on page 3. TIME selects the spectrum, SIZE
  warps it, SHAPE quantizes it and PITCH shifts it. DENSITY controls renewal
  and phase scrambling. Hold SEED to glitch; FREEZE holds the spectrum.
  Switching processor clears the recording.
- Select Evolving frozen feedback with REVERB on page 2. FEEDBACK controls
  how far the frozen sound drifts and softens.
- Select random fifths and octaves with DRY/WET on page 1. They apply to
  counter-clockwise PITCH randomness and work with any scale.
- Select Attacks with REVERB on page 1. Off keeps normal playback; Snap
  starts grains on recorded hits; Snap and trigger also triggers per hit.
- Stereo width now uses [A]+DRY/WET. Reverb moves to page 2, DRY/WET;
  delay moves to page 2, FEEDBACK. Never is renamed Disabled, and Wide
  detent becomes Wide/smooth filter, with unchanged behaviour.
- Synth start moves to page 3, REVERB; the limiter moves to page 4, REVERB.
- Earlier controls remain: Chromatic and the eight factory scales, Clouds-style reverb,
  manual synth start and the limiter. Hold [A], then press SEED for
  monophonic grains or FREEZE for buffer trim.
- Changes between mono and stereo, and synth switching at 48 kHz, are
  pop-free. Quality changes involving 32 or 24 kHz still briefly silence
  the output, empty the recording and stop synth reverb.

## Hygrostat

[Hygrostat](https://hygrostat.insolentconstruct.net/) builds updates locally
in your browser and produces guides. Choose a template or set features to
Off, Fixed or Adjustable, with your own controls and defaults. It warns when
a combination may be too heavy for the CPU and refuses combinations that
are too heavy. Per-feature CPU figures are estimates pending measurements
on a module; the warning and refusal use those estimates.

The complete scale bank is editable: include, define, remove or reorder up to
14 scales, each with up to 24 notes including its root. Repeat intervals span
0.01–128 semitones. Edit cents or semitones, or import Scala files within these
limits. Choose each scale colour and steady/slow/fast/two-pulse indication;
restore the factory bank at any time. Stable identities keep a saved choice
when reordering; removing it falls back to Off. Off and Chromatic remain
separate direct knob choices. Shared links and generated guides carry the bank.

Factory indications now pair amber major with blue minor scales, slow blinks
for pentatonics, coral Fifths, teal Pelog, violet Bhairav and two pink pulses
for Shri. Use the generated guide for a customized bank. Custom-bank CPU costs
still require final module measurements; factory costs do not bound every bank.

Synth reverb tail is another profile choice: Keep (stock) is the default.
Fixed Clear clears the tail when leaving either synth by manual stop, input
patching or processor changes. No new panel gesture or page slot is needed.
Sample-rate changes still restart the converter and interrupt audio briefly.

Nine profile-only options are off in standard firmware:

| Option | Action |
|---|---|
| Version | Flashes light-blue at boot, then twice for Beta 2 |
| Input attenuation | Extends [A]+FEEDBACK to silence, with unity at noon |
| Delay indication | Flashes [A] red when entering or leaving delay |
| Brightness | Selects 100%, 66% or 33% from the profile or a page knob |
| Live assignment LEDs | Follow the assignable CV |
| FREEZE during saves | Keeps FREEZE lit while saving |
| SEED latch | Retains the divider with Off, free-runs with Ignore, restarts phase with Sync or adds a grain per edge with Add |
| Pitch notches | Lights [A] green at unison and octave knob positions |
| TIME scan | Scans backwards or forwards with the attenurandomizer when TIME CV is unpatched |

## Update and recovery

Stock settings and calibration carry over. Upgrading from alpha or Beta 1
resets Drizzlet options, width and diffusion to profile defaults; set them again.

The [README](https://github.com/insolentconstruct/eurorack/blob/{TAG}/beads/build/README.md)
explains installation, options and recovery. Disconnect speakers during audio
updates: the update audio can harm hearing. Holding SEED at power-on resets
settings but keeps valid calibration.

Saves pause during supply sags. Interrupted saves lose only that save.
Damaged calibration restores from its copy; damaged frozen buffers are skipped.
If both calibration copies are lost, all LEDs blink fast for three seconds.
The module uses factory calibration and default settings, saves nothing and
restores no frozen buffer. Power on holding SEED to repair it. Blinking green
[A] with mix LEDs lit means success; recalibrate. Fast red and mix blinking
means failure; retry. If reinstalling fails to restore startup, consult the
[storage recovery tool](https://github.com/insolentconstruct/eurorack/releases/tag/storage-recovery-2026.10.27)
and its warnings.

Back up calibration before ST-Link recovery. Combo HEX includes its address.
Erase only the sectors being written. Never use a full-chip or mass erase:
it also wipes calibration and settings.

Disable monophonic grains before downgrading. Stock clears Drizzlet options
when saving and loads trimmed buffers full-length. Earlier Drizzlet reads
new settings as stock. SEED starts latched; latching in stock delay can pop.
A patched SEED cable divides the clock only with SEED latch set to Off.

## Known issues and release evidence

- SEED latch/unlatch can pop in delay mode (#183), matching stock behaviour.
  A fade is explicitly deferred beyond Beta 2. With the optional free-running
  latch, this can also occur with SEED patched.
- The unresponsive-module report #112 remains unresolved. Successful updater
  completion and recovery have not been confirmed; these changes are not a
  demonstrated cure for that report.
- Plaits bank selection (#190) remains a proposal for Beta 3, not a Beta 2 control.

The candidate passes all 19 host programs, with 265 named sections and 2,060
assertion sites in the source. CI detected all 392 deliberate faults. The app
(`09b43688…`) and WAV (`a84d6060…`) match their build pins; decoder checks pass
at both playback sample rates and reject corrupted/truncated controls. These
software results do not establish final module qualification.
TODO: final-image module CPU peaks, spectral frame budget and underruns.
TODO: attended profiles/options/scales/LEDs and Keep/Clear tail qualification.
TODO: automated run hours and firmware/settings install totals.

[Report problems](https://github.com/insolentconstruct/eurorack/issues) with the release,
your settings or profile, the steps to reproduce the problem and a recording.
Use [Discussions](https://github.com/insolentconstruct/eurorack/discussions) for questions and ideas.
Do not contact Mutable Instruments. This firmware uses Mutable Instruments source under the MIT licence.
