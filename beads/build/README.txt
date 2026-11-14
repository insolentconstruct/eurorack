Drizzlet
========

Drizzlet is alternative firmware for Mutable Instruments Beads, adding
Plaits, spectral processing and grain controls. Beta 2 “Fog” was
tested on one module; see docs/TESTING.md. Install at your own risk.
This is not a Mutable Instruments product. All settings save; standard
defaults retain stock behaviour.

Downloads and installation
--------------------------

Download audio updates and guides in <tag>-wav.zip from
https://github.com/insolentconstruct/eurorack/releases. For ST-Link, the
combo ZIP includes bootloader and application images. HEX carries its
address; BIN uses 0x08000000.

Updating is not a remedy for unexplained module behaviour. Disconnect
speakers: update audio can harm hearing. Unplug all CV
connections, connect your audio output to IN L, set TIME near noon and
power on holding FREEZE. Silence other computer sounds and disable
effects. Play the update WAV, adjusting TIME or the source volume until
[A] shows yellow. It restarts when finished; if it doesn't, start again
from the beginning. If all LEDs blink red, press FREEZE and retry
louder. Otherwise, try 44.1 kHz or another computer or interface. Audio
updates leave the bootloader and calibration untouched.

Stereo width
------------

Hold [A] and turn DRY/WET to set grain width; release to save. At the
bottom of the knob it is Stock; turn upwards to enable width, reaching
full random spread clockwise. Minimum width preserves stereo position
and centres mono. Each grain, including synth, retains its pan, even at
maximum DENSITY. [A] meters green through yellow to red; tapping [A]
alone changes quality. Enabled WIDTH follows the three stock [M]
destinations, pulsing all mix LEDs. CV input 7 adds width; custom
assignments work. Stock retains routing and pan rounding.

Grain diffusion
---------------

Hold [A] and turn REVERB; [A] meters the amount. Fully counter-clockwise
removes diffusion. Until set, Stock has none and Clouds-style uses its
own amount. Setting it adds DIFFUSION after REVERB and WIDTH in the [M]
cycle, with REVERB pulsing alone. The amount is saved. Diffusion affects
grains only.

Monophonic grains
-----------------

Hold [A], then press SEED to toggle this saved setting, initially off.
[A] dims when enabled; new grains fade earlier grains. Square and decay
shapes suit it; slower shapes can become nearly silent. Recover by
changing shape, reducing triggers, shortening SIZE or disabling it.
Disable before downgrading.

Buffer trim
-----------

Hold [A], then press FREEZE to restart the recording window at zero
length and clear the FREEZE latch. Release both, record, then freeze. A
FREEZE gate holds the window empty until low. Existing grains finish on
old audio; new grains use the new window. Saved buffers keep trimmed
lengths; stock loads them full-length.

Recording and switching
-----------------------

Releasing FREEZE resumes recording with a crossfade. Quality changes,
input changes between mono and stereo, leaving wavetable synth, and
power-up without a saved buffer start an empty recording. Switching
without changing sample rate is pop-free, including synth switching at
48 kHz. Quality changes involving 32 or 24 kHz still briefly silence the
output and stop synth reverb.

SEED starts latched. Latching it in stock delay can pop (#183); a
Hygrostat profile can fade it instead (below). The unresponsive-module
report (#112) is unresolved; this release is not a demonstrated cure.

Frozen-buffer autosave
----------------------

FREEZE saves after ten seconds. Interrupting a save doubles the next
wait up to a cap, until a save completes or FREEZE stays off for a
minute. In wavetable synth that minute begins on exit. Power-up resets
the wait. Supply sags pause saves; interrupted saves lose only that
save.

Hygrostat
---------

https://hygrostat.insolentconstruct.net/ builds updates and guides
locally. Choose features, controls and defaults, using templates or
custom Off, Fixed and Adjustable settings. It warns about CPU load and
refuses excessive combinations. Per-feature estimates await module
measurements.

Profiles, custom scales, LED indications and tail choices are checked
by host tests, not yet on a module. Page 4 provides LED brightness and
SEED latch with stock starting values. Seven additional options are off
in standard firmware:

- Version at boot flashes light-blue, then twice for Beta 2, without
  delaying startup.
- Delay indication flashes [A] red entering or leaving delay.
- Pitch notch indication lights [A] green at unison and octave knob
  plateaus, including both ends, independently of CV and randomness.
  These cues yield to warnings and meters.
- Live assignment LEDs follow CV magnitude, retaining assignment
  patterns and stock weighting.
- FREEZE during saves keeps its LED fully lit.
- Input attenuation extends [A]+FEEDBACK to silence, with unity near
  noon, marked green, and gain above. Saved gain immediately uses this
  range. Automatic gain never attenuates; hold [A] to restart.
- TIME modulation selects Random below noon or Scan above. With TIME CV
  unpatched, its attenurandomizer scans backwards or forwards, stopping
  at noon, taking four seconds per buffer at either end. TIME offsets
  the wrapping position. Patching CV restores normal control and pauses
  scanning, as do delay, spectral and synth. Position is not saved.

Page 4 controls:

- LED brightness selects 100%, 66% or 33%, fixed or adjustable in thirds
  with preview; warnings dim too. The default is 100%; use FEEDBACK on
  page 4. There is no power-on gesture.
- SEED latch selects Off (Stock), Ignore, Sync or Add in quarters with
  DRY/WET on page 4. With a cable patched and SEED latched, Off retains
  the divider; Ignore free-runs
  without cable triggers or flashes; Sync restarts grain-clock phase per
  edge; Add adds a grain per edge. Synth clocks follow these modes;
  delay free-runs outside Off. Unpatched attack triggers still work.

The factory layout is a starting point. Customize it with Hygrostat
and share feedback to help shape Beta 3.

Hygrostat can replace the complete scale bank with up to 14 scales, each
with 1–24 notes including its root and a repeat interval from 0.01 to
128 semitones. Edit notes in semitones or cents, import Scala tuning
files within these limits, include or remove scales, reorder them and
choose RGB colours with steady, slow, fast or two-pulse indications.
Factory reset restores the starting bank. Off and Chromatic remain
direct choices. Reordering preserves saved identity; removing the
selected scale falls back to Off. Shared links and generated guides
include the chosen bank. Custom-bank CPU costs still need final module
measurements.

Synth reverb tail is a separate profile choice: Keep is the stock
default; Fixed Clear clears the tail when leaving either synth by manual
stop, input patching or processor changes. It adds no panel gesture or
options-page slot. Sample-rate changes still restart the converter and
briefly interrupt audio.

Delay SEED latch fade is a separate profile choice: switching at once is
the stock default; Fixed Fade briefly dips the delay around a delay-mode
SEED latch, unlatch or cable change, instead of popping. It adds no
panel gesture or options-page slot.

Four more profile choices keep stock or Beta 2 behaviour by default and
add no panel gesture or options-page slot. Input gain at power-on:
Manual starts at unity manual gain when no gain is saved; a saved gain
wins. Attack sensitivity: Normal, Low or High. SEED at power-on:
Unlatched waits for SEED presses or gates. PITCH knob: Smooth glides
between the intervals without notches; PITCH CV is unchanged.

Two mix choices keep stock behaviour by default and add no panel gesture
or options-page slot. Mix law: Gentle brings the grains in more slowly
on DRY/WET (7.6 dB under the dry at 9 o'clock instead of 2.5 dB); the
dry and both ends of the knob stay stock. Reverb input: Grains sends
only the grains to the reverb, before the dry/wet mix as on Clouds, so
fully dry has no reverb. Both synths are unchanged.

Plaits synth banks: FEEDBACK selects one of the eight models of the
current bank, as on Plaits: green (its first bank), red (noise and
drums) or yellow (the Plaits 1.2 models). While the Plaits synth plays,
hold [A] and press FREEZE to step to the next bank; [A] flashes the bank
colour and FREEZE stays as it was. Everywhere else [A]+FREEZE is still
buffer trim. A profile can choose the starting bank (green by default).

Options page
------------

[A] is the quality button; [M] is CV assign. Hold [M] alone for three
seconds with the mix knobs still, then release. Tap [M] to change pages.
All three mix LEDs pulse while the entry hold continues; after release,
the indicated page LEDs pulse. Reopening remembers the page until
power-off; startup selects page 1. Custom layouts can add pages 5-7
(FEEDBACK and REVERB, DRY/WET and REVERB, then all three); taps skip
empty pages.

[A] previews each stored value in turn for about two seconds, with its
knob LED steady. Turn a knob or tap SEED to end preview. Move a knob more
than a tenth of its travel to take over; [A] shows its setting. Unused
knobs do nothing.

Page / pulsing LED  FEEDBACK      DRY/WET           REVERB
1 / FEEDBACK        Pitch scale   Random pitch steps  Attacks
2 / DRY/WET         Frozen feedback  Delay mode      Reverb character
3 / REVERB          Processor     Synth             Synth start
4 / FEEDBACK and DRY/WET  LED brightness  SEED latch    Limiter

Changes save about a second after turning stops. Wait before power-off.
Hold [M] for three seconds or press another button to close; mix
controls catch up smoothly. On page 1 with Scale selected, SEED taps
step the scale instead of closing. The exit press does nothing else until
the button is released.
The page LED flashes on entry, close and save. First values are steady
green; later values use yellow, red and blinking colours.

See below for every value.

Delay mode: Auto (Stock) enters delay at SIZE fully clockwise. Disabled
keeps granular playback there. Wide/smooth filter retains delay with a
neutral middle and no resonant boost at either end. SIZE CV and its
attenurandomizer control the filter. DRY/WET on page 2 selects these
values in thirds; [A] shows steady green / yellow / red respectively.
The saved default is Auto (Stock). This does not affect wavetable synth
mode.

Output limiter: REVERB on page 4 selects Stock, Fast release or Off in
thirds, indicated by steady green, yellow or red respectively. Off can
hard-clip loud peaks.

Pitch scale. Three zones: Off, Chromatic and Scale. The factory bank
cycles through Fifths (steady coral), Major (steady amber), Minor
(steady blue), Major pentatonic (slow amber), Minor pentatonic (slow
blue), Pelog (steady teal), Raag Bhairav (steady violet) and Raag
Shri (two pink pulses). Tap SEED in Scale to step; holds do nothing,
and taps neither trigger nor latch. Pelog and the raags are
microtonal. Pitch scale snaps each new grain's final pitch after
combining PITCH, CV and randomness, including Beads wavetable
grains. Plaits and Spectral also follow Pitch scale. Grains already
playing, the continuous wavetable oscillator and delay pitch keep
their pitch. Hygrostat can replace and reorder this bank, including
each scale definition, colour and flash pattern. Use the guide
generated for your profile.

Random pitch steps selects Free, Fifths and octaves or Recent notes
in thirds (steady green / yellow / red). Fifths and octaves snaps
counter-clockwise PITCH randomness relative to the PITCH setting. For
a C note, this gives C/G across octaves; G below C is a downward
fourth. Recent notes, with PITCH patched, gives each new grain one of
the last notes sent to PITCH, above the PITCH setting; turning the
attenurandomizer further counter-clockwise chooses from more of them
(up to eight). Unpatched it plays as Free. Free and clockwise stay
continuous before Pitch scale, which applies afterward and can change
the interval. This affects granular playback and Beads wavetable
grains, not the continuous oscillator, Plaits or Spectral.

Attacks selects Off, Snap or Snap and trigger in thirds. Snap moves
grain starts onto recorded hits; Snap and trigger also triggers a grain
per hit. Patched SEED still works; only granular playback uses marks.

Reverb character selects Stock or Clouds-style, which adds diffusion
until manually set. In delay and wavetable synth, only reverb changes.
Switching fades sound and clears the tail.

Frozen feedback selects Stock or Evolving. Evolving fades in to soften
and drift frozen grains, controlled by FEEDBACK; it stops with FREEZE
off.

Processor: Lower half selects Granular (Stock); upper half selects
Spectral, using Mutable Instruments Clouds. TIME selects a spectrum,
SIZE warps it, SHAPE quantizes it, PITCH shifts it and DENSITY controls
renewal and phase scrambling. SEED glitches; FREEZE holds. Random
controls update per spectrum. Mixing, quality, assignable CV and R gate
work normally; diffusion, delay and wavetable synth are unavailable.
Closing fades the change and clears recording. The saved frozen buffer
returns with Granular.

Synth selects Beads (Stock) or six Plaits voices. Triggers advance
voices, retaining pitch and random settings. FEEDBACK selects 24 models;
TIME controls HARMONICS, SIZE controls TIMBRE and SHAPE controls MORPH.
DRY/WET sets decay; FREEZE sustains. Other grain controls remain. At
DENSITY noon with SEED unpatched, the last voice drones.

Synth start selects Automatic (Stock), starting after ten seconds with
both inputs unpatched, or Manual. With inputs unpatched and FREEZE off,
hold [A] about a second and a half to start; repeat to stop. Patching an
input stops it once FREEZE is off. With an input patched, holding [A]
restarts automatic gain.

Options reference (TSV)
page	LED	knob	setting	value	colour	pulse
1	FEEDBACK	FEEDBACK	Pitch scale	Off	green	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Chromatic	yellow	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Fifths	coral	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Major	amber	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Minor	blue	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Pentatonic major	amber	slow (0.98 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Pentatonic minor	blue	slow (0.98 Hz)
1	FEEDBACK	FEEDBACK	Pitch scale	Pelog	teal	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Raag Bhairav	violet	steady
1	FEEDBACK	FEEDBACK	Pitch scale	Raag Shri	pink	2 pulses / 2.048 s
1	FEEDBACK	DRY/WET	Random pitch steps	Free (Stock)	green	steady
1	FEEDBACK	DRY/WET	Random pitch steps	Fifths and octaves	yellow	steady
1	FEEDBACK	DRY/WET	Random pitch steps	Recent notes	red	steady
1	FEEDBACK	REVERB	Attacks	Off	green	steady
1	FEEDBACK	REVERB	Attacks	Snap	yellow	steady
1	FEEDBACK	REVERB	Attacks	Snap and trigger	red	steady
2	DRY/WET	FEEDBACK	Frozen feedback	Stock	green	steady
2	DRY/WET	FEEDBACK	Frozen feedback	Evolving	yellow	steady
2	DRY/WET	DRY/WET	Delay mode	Auto (Stock)	green	steady
2	DRY/WET	DRY/WET	Delay mode	Disabled	yellow	steady
2	DRY/WET	DRY/WET	Delay mode	Wide/smooth filter	red	steady
2	DRY/WET	REVERB	Reverb character	Stock	green	steady
2	DRY/WET	REVERB	Reverb character	Clouds-style	yellow	steady
3	REVERB	FEEDBACK	Processor	Granular (Stock)	green	steady
3	REVERB	FEEDBACK	Processor	Spectral	yellow	steady
3	REVERB	DRY/WET	Synth	Beads (Stock)	green	steady
3	REVERB	DRY/WET	Synth	Plaits	yellow	steady
3	REVERB	REVERB	Synth start	Automatic (Stock)	green	steady
3	REVERB	REVERB	Synth start	Manual	yellow	steady
4	FEEDBACK and DRY/WET	FEEDBACK	LED brightness	100%	green	steady
4	FEEDBACK and DRY/WET	FEEDBACK	LED brightness	66%	yellow	steady
4	FEEDBACK and DRY/WET	FEEDBACK	LED brightness	33%	red	steady
4	FEEDBACK and DRY/WET	DRY/WET	SEED latch	Off	green	steady
4	FEEDBACK and DRY/WET	DRY/WET	SEED latch	Ignore	yellow	steady
4	FEEDBACK and DRY/WET	DRY/WET	SEED latch	Sync	red	steady
4	FEEDBACK and DRY/WET	DRY/WET	SEED latch	Add	green	slow (0.98 Hz)
4	FEEDBACK and DRY/WET	REVERB	Limiter	Stock	green	steady
4	FEEDBACK and DRY/WET	REVERB	Limiter	Fast release	yellow	steady
4	FEEDBACK and DRY/WET	REVERB	Limiter	Off	red	steady
End options reference

Saved data
----------

Stock settings and calibration carry over; earlier Drizzlet options,
width and diffusion reset to profile defaults. Frozen buffers remain
stock-compatible. Disable monophonic grains before downgrading. Stock
clears Drizzlet options when saving and loads trimmed buffers
full-length; earlier Drizzlet reads new settings as stock.

Reset and recovery
------------------

Hold SEED at power-on only to reset settings; valid calibration is kept.

Damaged calibration restores automatically from its copy; damaged frozen
buffers are skipped. If both calibration copies are lost, all LEDs blink
fast for three seconds. It then uses factory calibration and defaults,
saves nothing and restores no buffer.

Power on holding SEED to repair. Blinking green [A] with mix LEDs lit
means success; recalibrate. Fast red and mix blinking means failure;
retry. If reinstalling fails to restore startup, consult
https://github.com/insolentconstruct/eurorack/releases/tag/
storage-recovery-2026.10.27
and its warnings.

Back up calibration before ST-Link recovery. Erase only written sectors;
a full-chip or mass erase loses calibration and settings. For backup,
restore and ST-Link commands, see:

  https://github.com/insolentconstruct/eurorack/blob/drizzlet/beads/build/docs/RECOVERY.md

Use backups only on their original module.

Building it yourself
--------------------

From the repository root on an amd64 host with Docker, using the
published build image:

git submodule update --init stmlib stm_audio_bootloader
docker pull ghcr.io/insolentconstruct/mi-toolchain@sha256:06d243bb94bb7d714cd3a5a79f8de48745f0f99c0ce95011cbafdd89041156fd
docker tag ghcr.io/insolentconstruct/mi-toolchain@sha256:06d243bb94bb7d714cd3a5a79f8de48745f0f99c0ce95011cbafdd89041156fd \
  drizzlet
bash beads/build/build.sh "$PWD/artifacts" --wav

This repository's CI builds that image from beads/build/Dockerfile,
with the same pinned toolchain hashes. To build the image from source
instead, replace the pull and tag with:

docker build --platform linux/amd64 \
  --target compiler \
  -t drizzlet beads/build

The first image build downloads two pinned compilers, GCC 9 for the
firmware and GCC 4.9 to reproduce Mutable Instruments' bootloader
exactly, and pinned Debian packages, so it takes a while; later builds
reuse the image. The build runs entirely in Docker, with Python 3.
Running the Python 2 makefiles on the host is not supported; use the
commands above. Use build.sh. Plain make may try to regenerate the
resource tables with python; if you use it, add
-o beads/resources.cc -o beads/resources.h.

Without Docker: these steps were checked on x86-64 Debian 13 (trixie);
on other hosts use Docker. They replace build.sh and skip the
resource-table regeneration, so the Docker-only notes above do not
apply. The two pinned Arm compilers are GCC 9-2020-q2 for the firmware
and GCC 4.9-2015-q2 for Mutable Instruments' bootloader; the GCC 4.9
build is 32-bit, hence the i386 libraries. Outside the repository:

sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install --no-install-recommends ca-certificates curl bzip2 git make \
  python3 python3-numpy libc6:i386 libstdc++6:i386 zlib1g:i386
curl -fLO https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q2-update/+download/gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2
curl -fLO https://developer.arm.com/-/media/Files/downloads/gnu-rm/9-2020q2/gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
printf '%s\n' \
  '3a574c5becfca496bafe07bbd76e9deae1fd0cc99c5a9de0f8c610750f2f89ac  gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2' \
  '5adc2ee03904571c2de79d5cfc0f7fe2a5c5f54f44da5b645c17ee57b217f11f  gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2' \
  | sha256sum -c -
sudo tar -xjf gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2 -C /opt
sudo tar -xjf gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2 -C /opt

Then, from the repository root, build the bootloader, the application
and the combo image, and check them:

git submodule update --init stmlib stm_audio_bootloader
export LC_ALL=C
mkdir -p build/beads_bootloader build/beads
make -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/bootloader/makefile KIT_GCC9=0 \
  TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/ -j"$(nproc)" bin hex
make -o beads/resources.cc -o beads/resources.h -f beads/build/link-order.mk \
  KIT_MAKEFILE=beads/makefile KIT_GCC9=1 \
  TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-9-2020-q2-update/ -j"$(nproc)" \
  bin hex build/beads/beads_bootloader_combo.bin
mkdir -p artifacts
cp build/beads_bootloader/beads_bootloader.bin artifacts/bootloader.bin
cp build/beads/beads.bin artifacts/app.bin
cp build/beads/beads_bootloader_combo.bin artifacts/combo.bin
(cd artifacts && grep -E ' (bootloader|app|combo)\.bin$' ../beads/build/expected-hashes.sha256 | sha256sum -c -)

Encode the update WAV:

PYTHONPATH=. python3 stm_audio_bootloader/qpsk/encoder.py \
  -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 artifacts/app.bin

This WAV decodes to the same firmware, but numpy's floating-point
results depend on the CPU, so its hash may differ from the release.
build.sh runs the encoder under QEMU with a fixed CPU model; to match
the published WAV (checked with trixie's numpy 2.2.4), encode the same
way:

sudo apt-get install --no-install-recommends qemu-user-static
PYTHONPATH=. qemu-x86_64-static -cpu SandyBridge,-avx,-x2apic,-tsc-deadline \
  /usr/bin/python3 stm_audio_bootloader/qpsk/encoder.py \
  -t stm32h7 -k -s 48000 -b 12000 -c 6000 -p 256 artifacts/app.bin
(cd artifacts && sha256sum -c ../beads/build/expected-wav.sha256)

Matching the published hashes needs exactly these compilers and, for the
WAV, this encoder setup; other compiler versions may build, but produce
different binaries. app.elf also records build paths: it matches only
when built from /src with BUILD_ROOT=/out/objects/.

Licence and credits
-------------------

Code: MIT, from the Mutable Instruments source, with credits to Frédéric
Meslin / Fred's Lab and Oliver Rockstedt; see LICENSE. The manual
excerpt and hardware designs are licensed CC-BY-SA 3.0. Copyright and
permission notices are in NOTICE.txt; preserve them when redistributing.

Report problems at https://github.com/insolentconstruct/eurorack/issues
and questions at
https://github.com/insolentconstruct/eurorack/discussions. Do not
contact Mutable Instruments. See CHANGELOG.md for changes,
docs/TESTING.md for testing, and the Beads manual at
https://pichenettes.github.io/mutable-instruments-documentation/
modules/beads/manual/ for
stock controls. See docs/CALIBRATION.md in the repository to
recalibrate.
