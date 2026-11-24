# Feature allocation

**TL;DR:** which buttons, gestures, settings bytes, flash and CPU Drizzlet already uses (TAKEN) and what is still free (FREE), with the rules for adding options.

The [options guide](OPTIONS.md) explains the controls, zones and LED patterns.

The `drizzlet` branch, based on the Mutable Instruments source `d8f4739`. TAKEN means existing behaviour; FREE means no dedicated action or allocation, with the limits below. Planned uses come from the tentative roadmap in [#32](https://github.com/insolentconstruct/eurorack/issues/32); they can change until built. <!-- source: git trees fc549d7, d8f4739 -->
Source comments use repository-relative paths; `stock:` means `d8f4739`; Drizzlet pins stmlib `d18def8` (stock `e3bd7c9`; the stmlib files cited here are identical in both); “linker script” means `stmlib/linker_scripts/stm32h7xx_flash_application_large.ld`, and `manual.md` is the Beads manual. <!-- source: git ls-tree fc549d7 stmlib; git ls-tree d8f4739 stmlib -->

## Known behaviour

Same-rate mono/stereo changes and synth start/stop at 48 kHz are pop-free. Rate changes still fade, restart the converter, clear the recording and cut reverb tails.

SEED starts latched, as in stock, unless a profile chooses [Unlatched](#seed-at-power-on). LED brightness is a profile option, with no power-on gesture. The reset level stays stock; brown-out reset #165 is deferred.

Delay-mode SEED latching can pop, as in stock. A Hygrostat profile can fade it instead ([Delay SEED latch fade](#delay-seed-latch-fade), #183).

## Fixes

If missing or damaged wavetable content forces Beads' synth to Plaits,
[A]+FREEZE steps the fallback bank while it plays, and the [A] hold starts
and stops Manual synth play. Turning a gain knob still consumes the hold;
Automatic synth start keeps the automatic-gain restart. Repairing the content
restores the profile's gestures without changing its profile or saved State.

Low supply voltage blocks settings and buffer saves. Interrupted saves follow the power-cut path; saving resumes after voltage recovers (#175).

## Manual delay toggle

A custom profile can assign Manual delay toggle (ID 53) to [A]+SEED or [A]+FREEZE
([#309](https://github.com/insolentconstruct/eurorack/issues/309)). The module
boots in grains and each press reverses the target of the existing wet fade.
SIZE still controls grain size and never switches modes in that profile.
The toggle works with any Delay mode setting. Wide/smooth filter still sets
the delay filter response. The toggle is kept in RAM until power-off. The
gesture acts during granular and delay playback. Plaits and Rings keep
[A]+FREEZE for their bank and polyphony. Factory profiles keep their
existing SIZE and gesture behaviour.

## Buttons and startup

| Gesture | Allocation / action | Saved? |
|---|---|---|
| the quality button [A] tap | TAKEN: cycle quality downward, 3 → 2 → 1 → 0 | Yes <!-- beads/ui.cc:472-495 --> |
| [A] hold, 1.5 s | TAKEN: with no input, FREEZE off and Synth start on Manual, toggle wavetable synth; otherwise restart automatic gain and clear manual gain. Drizzlet acts on release, and not after a knob turn or button press while [A] was down (#289); stock acts at 1.5 s | Gain only; manual says 1 s <!-- beads/ui.cc:48-50,478-484; manual.md:44-45 --> |
| [A] + feedback knob | TAKEN: manual input gain; release saves. A profile can start a blank gain in manual gain ([ID 27](#input-gain-at-power-on)), or put width, diffusion or nothing here instead ([ID 40](#a-gestures-in-profiles)) | Yes <!-- beads/ui.cc:174-177,491-495 --> |
| [A] + dry/wet knob | TAKEN by Drizzlet: stereo width; release saves. A profile can choose diffusion, manual gain or nothing | Yes |
| [A] + reverb knob | TAKEN by Drizzlet: grain diffusion; release saves. A profile can choose width, manual gain or nothing | Yes |
| FREEZE tap | TAKEN: toggle freeze | Buffer/state save starts after 10 s (with interruption backoff below) if autosave enabled, outside wavetable synth and spectral modes <!-- beads/ui.cc:402-404; beads/background_buffer_backup.cc:144-161; beads/beads.cc:406-408 --> |
| FREEZE hold | FREE dedicated long action; tap already toggles, 4 s event has no extra action | — <!-- beads/ui.cc:48-50,402-404,461-520 --> |
| SEED press / release | TAKEN: unlatch, then generate while held; release stops | No; starts latched at boot, or unlatched with [ID 29](#seed-at-power-on) <!-- beads/ui.cc:81-83,406-411,463-469 --> |
| SEED hold, 4 s | TAKEN: latch generation | No <!-- beads/ui.cc:48-50,463-469; manual.md:59 --> |
| the CV-assign button [M] tap | TAKEN: cycle feedback / dry/wet / reverb CV destination | Yes <!-- beads/ui.cc:499-515 --> |
| [M] + mix knobs | TAKEN: set three independent CV amounts; release saves | Yes <!-- beads/ui.cc:178-180,499-515 --> |
| [M] alone, 3 s | TAKEN by Drizzlet: options entry | Save gesture below <!-- beads/options.h:125-140; beads/build/README.md:97-128 --> |
| Hold SEED, press FREEZE | TAKEN: latch SEED; consumes both releases | No <!-- beads/ui.cc:320-324; manual.md:59 --> |
| Hold [M], press SEED | TAKEN: toggle R output between audio and grain gate | Yes <!-- beads/ui.cc:331-339; manual.md:138 --> |
| Hold [M], press FREEZE | TAKEN: toggle buffer autosave; briefly display status | Yes <!-- beads/ui.cc:346-355,560-562 --> |
| [M] + [A], either order | WARNING: calibration entry; do not allocate | Calibration can be overwritten <!-- beads/ui.cc:390-398,428-453 --> |
| Hold [A], press SEED | TAKEN: toggle monophonic grains; consumes both releases; [A] first. A profile can put buffer trim, Synth start or a [toggle](#a-gestures-in-profiles) here | Yes <!-- beads/ui.cc:358-369 --> |
| [A] + FREEZE | TAKEN: hold [A] first, then press FREEZE to trim the recording window to zero and clear the button freeze latch. In Plaits synth play it steps the [Plaits bank](#plaits-bank) instead (ID 31) and leaves FREEZE alone. A profile can put monophonic grains, Synth start or a [toggle](#a-gestures-in-profiles) here | Gesture from the source Mutable Instruments published in 2026, disabled there; saves keep the length; stock loads full length. The bank is saved |
| Power on holding FREEZE | TAKEN: audio updater; FREEZE release retries after an error; TIME sets update gain | Writes firmware <!-- beads/bootloader/bootloader.cc:76-78,160-162,315,377-381; manual.md:247-253 --> |
| Power on holding FREEZE + [M] | WARNING: erase app sectors 1–7, then receive update; enabled | Erase chord keeps boot/settings sectors; reception has no upper bound <!-- beads/bootloader/bootloader.cc:28,266-280,317-322 --> |
| Power on holding SEED | WARNING: reset State to defaults, including limiter; retain valid calibration. With both copies of the calibration lost: repair with the factory calibration; result shown 3 s | Rewrites settings sector <!-- beads/settings.cc; beads/settings_storage.h --> |
| Other power-on chords | No dedicated action; inherit FREEZE/SEED checks and may reach normal combos after startup | Not safe unused gestures <!-- beads/bootloader/bootloader.cc:315-322; beads/beads.cc:319-345; beads/ui.cc:315-425 --> |

All six pairs are covered above. Combo checks use live debounced state; event order is FREEZE, SEED, [A], [M], but simultaneous presses need not produce a combo. SEED+FREEZE takes priority over [M]+FREEZE; extra-button chords are not free slots. <!-- beads/drivers/switches.h:40-45,62-67; beads/ui.cc:140-168,315-399 -->
The manual agrees on the main gestures; autosave toggle and erase/reset chords are source-only here. Use source timing for [A] (1.5 s versus the manual's 1 s). <!-- manual.md:44-49,59,132,138,247-253; beads/ui.cc:48-50,346-355; beads/bootloader/bootloader.cc:317-322; beads/beads.cc:329 -->

## Drizzlet options

Enter with [M] alone for 3 s, mix knobs still, then release; entry needs normal mode and 20 ms with all buttons up. Tap [M] cycles the populated pages in order, 1 → 2 → 3 → 4 → 5 → 1 for the factory layout; custom profiles can use up to seven pages (slots 1–21) and empty pages are skipped; the last page is kept in RAM, with page 1 at power-up. A knob must move >10% to edit. Saves one second after the last qualified movement or value change; no idle close unless a profile sets an [idle timeout](#options-page-idle-timeout). Hold [M] 3 s or press another button to close, keeping changes. The exit press is consumed until the button is released. On page 1 with Scale selected, short SEED taps step the scale instead of closing. All three mix LEDs pulse while the entry hold continues; after release, the page marker pulses: pages 1–3 FEEDBACK, DRY/WET or REVERB; 4 FEEDBACK and DRY/WET; 5 FEEDBACK and REVERB; 6 DRY/WET and REVERB; 7 all three. Beta 2 lit all three for page 4.

| Page / pulsing LED | FEEDBACK | DRY/WET | REVERB |
|---|---|---|---|
| 1 / FEEDBACK | Pitch scale | Random pitch steps | Attacks |
| 2 / DRY/WET | Frozen feedback | Delay mode | Reverb character |
| 3 / REVERB | Processor | Synth | Synth start |
| 4 / FEEDBACK and DRY/WET | LED brightness | SEED latch | Limiter |
| 5 / FEEDBACK and REVERB | Resonator | Resonator material | Resonator ring time |

Custom profiles can leave knobs unused; unused knobs have no effect on [A] or saving. On entry and on each page change, [A] shows each stored value for 2.048 s, left to right, skipping reserved knobs, with that knob's mix LED lit steady; a qualified knob or a SEED scale step ends it. [A] then shows the first available setting, then the last qualified knob moved. Blink patterns start when the shown setting changes. Every first value, including Pitch Off, is steady green. Other settings follow the green/yellow/red ladder. The factory scale bank uses coral Fifths, amber Major, blue Minor, amber/blue pentatonics with slow blinking, teal Pelog, violet Bhairav and two pink pulses for Shri. Slow is 0.98 Hz, fast is 3.91 Hz, and a two-pulse group repeats every 2.048 s. Hygrostat profiles can replace the scale definitions, order, colours and patterns; [Scale profiles](SCALE-PROFILES.md) specifies their format. The page LED flashes for 100 ms on entry, close and after a changed batch is written. README.txt's Options reference lists every colour and pulse; `make -f beads/build/tools/options_tables.mk test` checks it and `./build/options_tables` prints its TSV from the firmware tables. Pitch scale values and grain behaviour: [options guide](OPTIONS.md).

## Saved data

Settings live in two records. **State**, in the settings sector (15), is
exactly stock's: bytes 0–9 keep their stock meanings, including monophonic
mode (byte 9) and the manual gain (byte 5), and bytes 10–23 are written as
zero, as stock writes them. Stock, the alphas and Beta 1 read it as their own.

Every other saved option is in the **options record** (`DRZL`, 56 bytes,
stmlib's chunk format) in sector 14, after the mirrored calibration, in fixed
64-byte slots as stock's states are. The latest slot that passes its tag, size
and checksum loads. Stock, the alphas and Beta 1 never touch sector 14.

| Payload byte | Content |
|---|---|
| 0 | Layout version, 1 |
| 1 | CV target: 0 none, 1 stereo width, 2 diffusion |
| 2–3 | Reserved, 0xFF |
| 4–7 | The profile that last saved: its CRC-32, 0 for the built-in fallback |
| 8–23 | One byte per saved option (below) |
| 24, 25, 26 | Resonator, Resonator material, Resonator ring time |
| 27 | Rings polyphony |
| 28 | Gain ceiling |
| 29–55 | Reserved, 0xFF |

A latest record of another version gives the profile's defaults; no older slot
is read instead. A byte at or past its option's value count is not set, so the
profile's default applies. Off and Fixed features keep their saved byte.
Options are not promised across betas: Beta 2 and 3 options start from the
profile's defaults, and their packed bytes 10–23 are ignored and cleared at the
next state save. A new layout changes the version; within a version only new
enumerated options take reserved bytes. `resources/features-frozen.json`
freezes each version's offsets.

| Feature ID | Feature | Saved in |
|---|---|---|
| 1 | Pitch scale | Record byte 8 |
| 2 | Reverb character | 9 |
| 3 | Delay mode | 10 |
| 4 | Processor | 11 |
| 5 | Limiter | 12 |
| 6 | Frozen feedback | 13 |
| 7 | Synth | 14 |
| 8 | Attacks | 15 |
| 9 | Synth start | 16 |
| 10 | Random pitch steps | 17 |
| 11 | Stereo width | 18, full byte |
| 12 | Diffusion | 19, full byte |
| 13 | Monophonic grains | State byte 9 |
| 18 | LED brightness (#40) | 20 |
| 21 | SEED latch (#13) | 21 |
| 23 | TIME modulation (#141) | 22 |
| 31 | Plaits bank (#190) | 23 |
| 40 | Manual gain knob (#301) | The gain is State byte 5 |
| 41 | Resonator (#88) | 24 |
| 42 | Rings polyphony | 27 |
| 43 | Resonator material (#88) | 25 |
| 44 | Resonator ring time (#88) | 26 |
| 52 | Gain ceiling (#59) | 28 |

Other features are not saved: they always take the profile's value. A build without Rings reads Synth value 2 as not set. Bytes 24–28 sit in the record's second flash word, so a record now takes both words: a power cut between them leaves the previous record in use.

Options-only changes (an options page, an [A] gesture, the Plaits bank) append
a record and leave the settings sector alone; a change to both writes the
record first. Either sector is erased only while the other holds a usable
calibration (valid and within 25% of the factory values), read back from flash;
the one exception is a store with none (a new module, or a repair with SEED).
A full journal (2,046 slots) or a damaged slot compacts sector 14: erase, then
the settings sector's calibration, copied from flash and only if usable and
the one in use, then the latest record. A power cut there leaves the settings
sector untouched and may reset the options. A recalibration puts the record
back in the mirror before the settings sector is erased. If the settings
sector holds exactly the factory calibration while the mirror holds another
plausible one, as a stock-storage firmware can leave after an interrupted
rewrite, the mirror's comes back. SEED at power-up resets the options too.

The profile table in `beads/profile.cc` owns identities and allowed controls.
Missing or invalid profiles use the release layout and lock settings writes;
the boot guard's own recoveries still run from storage state. SEED at boot
with a valid profile resets to its defaults, preserving calibration.

### State layout

State is 24 bytes, all `uint8_t`, with no implicit gaps; enum tags occupy no payload bytes. Defaults below are before flash loading. <!-- beads/settings.h:51-62; beads/settings.cc:60-70 -->

| Byte offset | Field / owner | Default / allocation |
|---|---|---|
| 0 | `media` | 3; TAKEN, values 0–3 <!-- beads/settings.h:52; beads/settings.cc:60,80 --> |
| 1 | `assignable_cv_destination` | 0; TAKEN, 0–2 single destination, 3 custom amounts <!-- beads/settings.h:53; beads/settings.cc:61,81; beads/beads.cc:222-224 --> |
| 2, 3, 4 | `assignable_cv_amount[0,1,2]` | 255, 0, 128; TAKEN: feedback, dry/wet, reverb <!-- beads/settings.h:54; beads/settings.cc:62-64; beads/ui.h:69-73 --> |
| 5 | `gain` | 0 = auto; nonzero manual; TAKEN <!-- beads/settings.h:55; beads/settings.cc:65; beads/ui.cc:177 --> |
| 6 | `gate_output_mode` | 0 = audio; TAKEN <!-- beads/settings.h:56; beads/settings.cc:66; beads/ui.cc:333 --> |
| 7 | `frozen` | 0; TAKEN: completed backup flag, not immediate button latch <!-- beads/settings.h:57; beads/background_buffer_backup.cc:84-86,136-139 --> |
| 8 | `disable_autosave` | 0; TAKEN <!-- beads/settings.h:58; beads/settings.cc:68 --> |
| 9 | `monophonic_mode` | 0 = off (default), 1 = on; TAKEN: monophonic grains, using Mutable Instruments' original code <!-- beads/settings.h:59; beads/beads.cc:203; beads/ui.cc:358-369 --> |
| 10–23 | `padding[14]` | Zero, as stock writes it. Beta 1 to 3 kept options here; they are ignored and cleared at the next save |

Stock (2021 and 2024 releases) keeps byte 9 and shows the half-bright [A] cue, but plays grains polyphonically. <!-- stock:beads/ui.cc:191; stock:beads/settings.cc:94-96; stock:beads/beads.cc:203-207 -->

Stock loads and checksums the whole State, ignores padding semantically, then clears all 14 padding bytes on SaveState, which appends the full State (or rewrites the sector when full). Drizzlet writes State the same way; its options are in sector 14, which stock never reads or writes. <!-- stock:beads/settings.cc:75-83,94-96; stmlib/system/sector_storage_h7xx.h:133-141,168-209; beads/settings.cc:84,96-99 -->
Keep size, tags, offsets and checksum format compatible: records require matching tag and `sizeof(T)`, with an 8-byte header, 32-byte flash-word rounding, and latest valid sequential State loading. No general migration exists here. <!-- stmlib/system/sector_storage_h7xx.h:86-99,106-123,168-175,213-233 -->

Damaged storage (boot guard). Bank 2 is read with faults masked and the ECC error flag checked (`beads/drivers/flash.h`), so an uncorrectable word no longer crashes the module. At power-up the settings sector is read as stock reads it: the CALI record, then STAT records up to the first that fails its checks or reads with an uncorrectable error, as an interrupted save leaves it; nothing after that one is read, and the next save rewrites the sector. A CALI that is unreadable or fails its checks in a sector that isn't blank, or reads clean but is more than 25% from the factory values, is restored from the mirror if the mirror reads clean, passes its checks, and is within 25%, keeping the states that loaded; an out-of-range CALI with no such mirror loads as stock loads it. A blank sector takes the calibration from the mirror (a power cut after an erase); with a blank mirror it is a new module (factory test). A valid CALI with no valid STAT keeps the calibration, resets the state and never starts the factory test. Only a lost CALI with no usable mirror, or a blank sector with a mirror that isn't blank but holds no CALI, is damage: every bank-2 erase and program is locked and the module plays on the factory calibration and default settings. Only SEED at power-up writes then: the mirror first, then the sector, read back as the firmware loads it; damaged frozen-buffer sectors are erased after a successful repair. A repaired sector is in the stock format. Erases name the sector within bank 2 (settings 7, mirror 6, buffer 0-5) and are checked blank, masked, before anything is written. Each erase and program first checks the programmable voltage detector (level 6, 2.69-2.83 V falling) and is refused while it shows VDD low: a state save is skipped, and any other refused operation fails like a flash error, so a save cut short loads as a power cut at that point would. The detector is set at each start-up; no option byte is written. <!-- beads/settings_storage.h; beads/settings.cc; beads/background_buffer_backup.cc; beads/drivers/flash.h; beads/test/storage_test.cc -->

| Flash area | TAKEN | FREE / limit |
|---|---|---|
| Bootloader, 0x08000000–0x0801FFFF | Reserved 128 KiB sector; updater starts app at 0x08020000 | Not free app space <!-- beads/bootloader/bootloader.cc:53,266-280 --> |
| CALI + STAT, 0x081E0000–0x081FFFFF | One 128 KiB sector; CALI payload 120 B (6×8 B calibration + 72 B padding), record 128 B; STAT payload 24 B, record 32 B | Append space is owned by storage, not a new feature area; CALI padding[0–71] FREE format bytes, but cleared on save; change clearing before use <!-- beads/settings.h:40-61,101; beads/drivers/cv_adc.h:45-52; beads/settings.cc:88-99; stmlib/system/sector_storage_h7xx.h:58-59,213-224 --> |
| FRZZ, 0x08100000–0x081BFFFF | Intended reservation: six 128 KiB sectors; 32 B header word + 768,000 B audio | 18,400 B tail reserved with backup; header layout below <!-- beads/background_buffer_backup.cc; beads/background_buffer_backup.h; beads/dsp/processor.h:51; stmlib/system/sector_storage_h7xx.h:51-57 --> |
| Calibration mirror and options, 0x081C0000–0x081DFFFF | TAKEN by Drizzlet: a CALI record, as in the settings sector, written before each settings-sector erase when missing or different, then the options records in 64 B slots (2,046 per erase). Stock ignores it | Restores the calibration when the settings sector is blank <!-- beads/settings_storage.h --> |
| Application, 0x08020000–0x080FFFFF | Bank 1 after the bootloader | The build fails if the image reaches bank 2 <!-- beads/build/flash_bound.ld --> |

Frozen-buffer backup header: one 32-byte flash word at 0x08100000, little-endian, followed by the 768,000 audio bytes. Stock firmware reads bytes 0–9 only, so stock and Drizzlet load each other's backups.

| Bytes | Field | Rule |
|---|---|---|
| 0–3 | `tag` | `FRZZ` (0x5a5a5246), as stock |
| 4–7 | `write_ptr` | Signed frame index of the next write, as stock; must be below the buffer length: 384,000 / 256,000 / 256,000 / 192,000 stereo frames for media 0–3, twice that in mono |
| 8, 9 | `media`, `mono` | As stock; 0–3 and 0–1 |
| 10–11 | `padding` | Zero |
| 12–15 | `crc` | stmlib `crc32` (CRC-32/ISO-HDLC) of the audio, continued over this header with `crc` = 0 |
| 16–19 | `crc_tag` | `CRC1` (0x31435243): `crc` is valid. Stock backups hold leftover stack bytes in 10–31, so without this tag a backup loads as stock did, after the range checks |
| 20–23 | `trimmed_frames` | Frames removed from the full buffer; 0 = full length, CRC-covered; stock ignores it |
| 24–31 | `reserved` | Zero |

Autosave starts after FREEZE has been on for 10 s. A save cut short after erasing doubles the next wait, up to 320 s, until a save completes or FREEZE has been off for a minute; power-up starts at 10 s. The thaw reset is checked only outside wavetable synth mode (backup processing is skipped there), so the minute starts when wavetable synth mode ends. Backoff is RAM-only and runs in the main loop.

Saving erases the header's sector first and writes the header last, after the audio, so a header means a complete backup; a failed erase or program abandons the save. Loading checks the ranges, then the CRC when tagged, before touching the processor; any failure starts with an empty buffer. An uncorrectable ECC error in the header or audio is read masked and also starts with an empty buffer; the next save erases it. Stale error flags are cleared before each erase and program. <!-- beads/background_buffer_backup.h; beads/background_buffer_backup.cc; beads/test/backup_test.cc -->

Updater reception has no end-address guard; extending app/update files can overwrite FRZZ or settings. Reserve an update boundary as well as a linker boundary before assigning new flash storage. <!-- beads/bootloader/bootloader.cc:234-259,291,345-355 -->

## Panel I/O

| Resource | Granular / delay | Synth / other ownership |
|---|---|---|
| DENSITY knob + CV | Grain rate; noon stops latched grains; clock divider/probability; unlatched noon triggers single grains. Delay: base time or clock/tap ratio | Synth grain rate, with density CV also used for exponential FM at high rates; TAKEN <!-- manual.md:63-77,150-162; beads/dsp/synth.cc:70-81 --> |
| TIME, PITCH, SIZE, SHAPE knobs + CV | Position, transposition, duration/direction, envelope. SIZE fully CW enters delay; TIME then delay multiple/frozen slice, PITCH pitch-shift, SHAPE tap envelopes | Synth TIME wave position, PITCH root/grain pitch, SIZE duration, SHAPE envelope; all TAKEN <!-- manual.md:87-97,144-174; beads/dsp/wavetable_voice.h:84-94; beads/dsp/wavetable_granular_player.h:65-68; beads/dsp/delay_controller.cc:173-175,293-294,346 --> |
| Four attenurandomizers | Patched: CW CV amount, CCW CV-controlled randomness; unpatched: peaky/uniform randomness. Delay: slow random LFOs; SIZE amount + SIZE CV also set bandwidth | Synth PITCH CV always 1 V/oct root; PITCH attenurandomizer remains grain randomness; TAKEN <!-- manual.md:102-106,174,192-194; beads/dsp/granular_processor.h:277-282 --> |
| Feedback, dry/wet, reverb knobs; assignable CV | Feedback, audio blend, reverb; destinations 0/1/2 or custom three-way amounts | Synth feedback selects 12 banks (manual says 8), dry/wet oscillator/grains; options borrow all three mix knobs (all three in use); TAKEN <!-- beads/beads.cc:219-234; beads/dsp/synth.h:43; beads/dsp/synth.cc:52,83,98-108; manual.md:182-184; beads/options.h:94-113 --> |
| FREEZE + SEED gate inputs | Record stop; grain clock/gate, delay clock. Button freeze ORs with gate; SEED input delayed 2.5 ms | Synth freeze stops new grains/holds envelopes; SEED still drives grains; TAKEN <!-- beads/beads.cc:132-151; manual.md:158,186; beads/dsp/synth.cc:73-81 --> |
| L/R audio in, L/R audio out | Input normalization selects mono/stereo; output always stereo, unpatched R sums to L; optional R grain gate replaces audio | Automatic (Stock) wavetable synth: both inputs empty, FREEZE off and quiet output for 10 s selects synth; Manual: a [A] hold instead; patching an input exits once FREEZE is off; R gate costs stereo output; no spare dedicated gate output <!-- manual.md:36-40,134-138; beads/beads.cc:360-394; beads/dsp/synth.cc:138-142 --> |
| [A] RGB LED | 0 Scorched cassette red/pink, 1 Sunny tape amber, 2 Cold digital teal, 3 Bright digital white; dims with overwrite reserve; manual gain overrides with meter | Same palette marks synth resolutions 4/4, 16/6, 7/7, 16/16 bits (continuous/grains); options override colours; TAKEN <!-- beads/ui.cc:190-195,238-245; beads/dsp/processor.h:58-63; beads/dsp/synth.cc:84-85 --> |
| FREEZE / SEED LEDs | Freeze status (half-bright during backup); SEED gate/held or breathing latch. Autosave display FREEZE dim=disabled, brighter=enabled | Same ownership in synth; no spare status LED <!-- beads/ui.cc:247-262 --> |
| Three assignment LEDs / input-level LED | Destination or custom amounts / input VU, blinking during gain adjustment | Options page markers / VU retained; TAKEN <!-- beads/ui.cc:264-272,283-306 --> |
| Calibration, panic, factory test, storage, updater LEDs | Calibration: channel/step + pulsing assignment LEDs; panic blinks all; factory test fades all; lost calibration blinks all for 3 s at power-up; repair result for 3 s: green [A] blinking with lit assignment LEDs (repaired, factory calibration), red [A] and assignment LEDs blinking fast (failed) | Updater alternates FREEZE/SEED, [A] level meter; error blinks all, writing lights all. These are reserved overrides <!-- beads/ui.cc:207-231,276-280,301-307; beads/bootloader/bootloader.cc:99-148 --> |

No physical knob, CV jack, gate jack or LED is unallocated. New normal-mode controls need a deliberate shared gesture; only [A] is RGB, and R is already shared with gate output. <!-- beads/beads.cc:178-234; beads/drivers/leds.h:42-52; manual.md:138 -->

## Build options

Buffer trim and monophonic grains use the gestures Mutable Instruments included, disabled, in the source published in 2026 (`ENABLE_ADVANCED_FEATURES`, `beads/ui.h`); Drizzlet enables them. Neither was enabled in the 2021 or 2024 firmware. Automatic (Stock) input mono/stereo detection remains active.
`PROFILE_INTERRUPT` and bootloader `ENABLE_ERROR_DETECTION` are off; their debug-pin/error-check paths are excluded. `ENABLE_HARDWARE_24K`, `ENABLE_BANDWIDTH_CONTROL` and bootloader `ENABLE_ERASE_BUTTON_COMBO` are ON; their alternate branches are excluded. `TEST` hooks belong to host tests. <!-- beads/beads.cc:74,109-111,338-340; beads/bootloader/bootloader.cc:27-28,190-264; beads/dsp/processor.h:46,79-92; beads/dsp/multitap_delay.h:38,90; beads/ui.cc:546-549 -->
`WasteCPUCycles(16)` is always called in main, including wavetable synth and spectral modes: it renders a dummy vintage-digital grain workload with memory accesses, discarding output. In spectral mode the main loop also computes the frames (`Processor::Buffer()`), one per channel per hop. Main-loop occupancy is not audio CPU headroom. <!-- beads/beads.cc:403-410; beads/dsp/processor.h:150-153; beads/dsp/granular_processor.h:116-147 -->

## Budgets

Measured on the Beta 2 code before Hygrostat profiles and the synth grain pairs (the build pinned in `expected-hashes.sha256` when the startup guard merged): bytes from `app.elf` sections, not the debug ELF file size. These are historical sizes, not final-image headroom; profiles and the grain pairs add flash. The build checks the final image against the buffer boundary. <!-- measurement: app.bin SHA256 2e523f20dfc2f3a2550e8e04728b1ce1cada3406a1800fc69ce186681100b163, readelf -S app.elf; stmlib/linker_scripts/stm32h7xx_flash_application_large.ld; beads/build/flash_bound.ld -->

| Region | TAKEN | FREE / qualification |
|---|---|---|
| App flash before FRZZ (896 KiB) | 626,248: vectors 664 + text/rodata 624,388 + exidx 8 + data load 1,188 | 291,256; app ends 0x080B8E48. The build fails if the image reaches bank 2 |
| DTCM (128 KiB) | `.data` 1,188 + `.bss` 84,764; minimum heap/stack reservation 1,536 | 43,584 after reservation; the stack grows from 0x20020000 and must also fit; no Drizzlet high-water measurement |
| ITCM (64 KiB) | 0 | 65,536 for explicitly placed code; adds flash load bytes |
| D1 / D2 / D3 RAM | 524,288 delay / 262,144 delay / 65,536 reverb | 0 / 0 / 0 linker space; not interchangeable with DMA or TCM. Plaits borrows the first 168 KB of delay bank 0 in synth mode |
| D2 DMA RAM (32 KiB) | 1,216 | 31,552 static space; preserve uncached DMA placement |

Audio callback period is 250 µs (4,000 blocks/s): nominal budget 120,000 cycles at 480 MHz, before interrupt overhead. The makefile's 400 MHz `F_CPU` is not what the clock setup programs. On the Beta 2 image, one module peaked at 83.7% (wavetable synth, Clouds-style reverb, SIZE 97%) with no underruns; see TESTING.md.

The dummy workload keeps the main loop busy, so main-loop activity is not idle time. The wavetable synth sets the peak; time dense stereo grains, synth, delay transitions, backup and interrupt overlap before budgeting new DSP. <!-- beads/beads.cc:403-410 -->

## Design aims

Mutable Instruments built Beads as a real-time granular texture tool around one model: "a fixed-length tape loop. This is not a sampler." The panel should say what the module does ("I want to keep this module WYSIWYG"), and the stock firmware was to get "no further additional feature that will require a button combo" ([Beads diffuser, post 7](https://forum.mutable-instruments.net/t/beads-diffuser/18782/7)). Drizzlet adds some things Mutable Instruments declined for stock (diffusion, scales, trim, mono grains), so a new feature is judged by these tests instead:

- It shapes the sound live (texture), not edits or files it (studio).
- It is off by default and hard to reach by accident.
- One manual paragraph explains it; no hidden state the panel can't show.
- Patching can't already do it.
- CPU: the worst measured block stays at or under its codec rate's working limit: 96.5% of the budget at 48 kHz and 97.0% at 32 kHz, at least a point under the clean edges a Beta 4 test build measured on the module (97.8% and 98.1%, with the one-frame output delay; [One-frame output delay](OPTIMIZATIONS.md#one-frame-output-delay)). 24 kHz is not yet measured on Beta 4 and keeps the earlier 90% limit.
  Palette model banks a user picks in Hygrostat are not features under this rule: a bank over its rate's working limit gets a very obvious warning that it may crackle and is never refused for CPU use (owner 2026-11-21, 2026-11-23).

Prefer, in order: no control (a better default or an indicator), a new value in an existing option, a gesture, then a new option replacing another (the factory layout's twelve page slots are taken).

## Planning rules

- Allocate a reserved page knob before borrowing another normal gesture; record page, knob, colours and saved bytes together. <!-- derived: beads/options.h:90-113; beads/ui.cc:283-299; beads/settings.h:60 -->
- Give zero/unknown settings the stock default; preserve the 24-byte State layout and accept that stock saves erase extension bytes. <!-- derived: beads/settings.h:74-85; stock:beads/settings.cc:94-96; stmlib/system/sector_storage_h7xx.h:168-175 -->
- Change load/save clearing when assigning padding; do not claim byte 9 or flash journal/backup tails as free. <!-- derived: beads/settings.cc:84,96-99; beads/settings.h:59; beads/background_buffer_backup.cc:95-116,157 -->
- Check button order, long holds, startup and disabled-feature collisions; keep calibration/reset/update chords reserved. <!-- derived: beads/ui.cc:315-399; beads/bootloader/bootloader.cc:315-322; beads/beads.cc:329 -->
- Keep new options stock by default; account for both processor and synth, and restore borrowed controls/LEDs after editing. <!-- beads/build/docs/STYLE.md:80; derived: beads/beads.cc:244-261; beads/ui.cc:123-134,198-308 -->
- Reserve flash by erase sector; measure CPU and stack on the actual build before spending static size headroom. <!-- derived: stmlib/system/sector_storage_h7xx.h:43-59,149-164; linker script:29-42; measurement: CI 38491527444 app.elf -->

## Decisions

Agreed 2026-10-14; see the [roadmap](https://github.com/insolentconstruct/eurorack/issues/32).

- **Saved fields keep their meaning permanently.** Layout and defaults in
  [Saved data](#saved-data); the options record carries a layout version.
- **One firmware.** Plaits (#50) and spectral mode (#3) fit in the main firmware as options, so no separate variants are planned. Plaits plays in place of the wavetable synth and borrows the first 168 KB of delay bank 0 while synth mode runs; every exit from synth mode clears the banks.
- **The options page stays small.** An option has to earn its place: it fixes something users hear as a defect, or several users ask for it, and patching can't already do it. Factory cap: five pages with three settings each, including the limiter, wavetable synth and resonator; monophonic grains use [A]+SEED outside the page. Other requests stay open as issues.
- **Buffer trim:** grains stay within the trimmed window at their start; release can run about 10 ms past it (the envelope minimum release).
- **Gestures:** [A]+SEED is taken by monophonic grains; [A]+FREEZE = buffer trim, or the Plaits bank step in Plaits synth play; [A]+REVERB = grain diffusion amount (#2); the FREEZE long hold is unassigned; flash sector 14 holds the calibration mirror.

Clouds-style reverb retains the existing 64 KiB reverb buffer and Beads knob
mapping, with Clouds delay lengths and modulation. The grain wet path adds
four 16-bit allpasses per channel (4 KiB buffer), coefficient 0.625, amount 0.7.
Clouds' granular TEXTURE maps 0.75–1 to diffusion 0–1; amount 0.7 corresponds to
TEXTURE 0.925. Delay and wavetable synth use the reverb character without the grain
diffuser. A roughly 10 ms fade out, state clear, and 10 ms fade in separates
changes, including settled option changes. Module listening done; module CPU 105,764 cycles (88.1%), see TESTING.md.

Clouds-style modulation uses nominal 0.5/0.3 Hz rates in every recording quality; delay lengths remain in samples.

## Stereo width

| Allocation | Use |
|---|---|
| [A] + DRY/WET | Continuous grain width; release saves; [controls and indications](OPTIONS.md) |
| Byte 14 | 0 Stock; 1..255 enabled width |
| Byte 16 | 0 unassigned; 1 WIDTH assigned; all other values unassigned |
| [M] | Optional fourth destination when width is enabled |

Pitch scale quantizes the total pitch of each new grain (PITCH, CV and random variation), rooted at the unshifted recording; Plaits and Spectral also use the selected scale.

Random pitch steps selects Free, Fifths and octaves or Recent notes in thirds. Fifths and octaves quantizes counter-clockwise random offsets relative to PITCH to the fixed octave/fifth family: … −12, −5, 0, +7, +12 … semitones; for C, C/G across octaves includes a downward fourth.

Recent notes (#220), with PITCH patched, gives each counter-clockwise grain one of the last notes sent to PITCH, at 1 V/octave relative to the PITCH setting (a negative CV plays below it). A note counts once the CV holds within 0.1 semitone for 10 ms and is at least half a semitone from the previous note; after a glide it must hold twice as long as the glide took to move its last tenth of a semitone (at most 1 s), so a glide faster than a tenth of a semitone per second counts where it stops (#243); the attenurandomizer chooses from the newest 1 (near 12 o'clock) to 8 (fully counter-clockwise). Unplugging PITCH forgets the notes; unpatched, and in the wavetable synth (which adds its CV to the knob), it plays as Free. Beta 2 reads the saved value as Free.

Free and clockwise remain continuous before Pitch scale; subsequent scale quantization can change an interval. Random pitch steps affects granular playback and Beads wavetable grains, not the delay pitch, the continuous wavetable oscillator, Plaits or Spectral; custom scales do not redefine its fixed offset pattern.

## Exact option values

The user guides (OPTIONS.md, README) describe positions and timing in words. The numbers behind them are below; `tools/options_tables_test.py` checks each against the firmware export.

- **Open and close:** hold [M] 3 s. A knob must move more than 10% of its travel to take over. Pitch scale, Random pitch steps, Delay mode, Attacks and Limiter use thirds with a 3% dead band: thirds switch at 1/3 ± 3% and 2/3 ± 3%; ordinary two-value settings use halves, switching below 47% and above 53%. Synth uses those thirds with three playable synths and those halves with two. LED brightness uses thirds at 1/3 ± 2.5% and 2/3 ± 2.5%; SEED latch uses quarters at 1/4 ± 2.5%, 1/2 ± 2.5% and 3/4 ± 2.5%. A button press that closes options is consumed until release. On page 1 with Scale selected, SEED taps step the scale instead of closing. All three mix LEDs pulse while entry is held. Opening a page shows each stored value for 2.048 s each.
- **Stereo width ([A] + DRY/WET):** Stock below 7%; leaves Stock above 13%, returns below 7%. Enabled width runs from zero spread at 13% to full spread at 100%, saved as 1..255 (0 = Stock). Releasing [A] saves it. The assignable CV input (7) adds to the saved width, clamped to zero and full spread.
- **Saving and LEDs:** changes save after 1 second without movement that changes the setting or passes the movement threshold; pending changes also save on close. The page LED flashes for 100 ms on entry, close and saving. Slow blink is 0.98 Hz; fast blink is 3.91 Hz; two yellow pulses repeat as a 2.048 s group.
- **Delay mode, Wide/smooth filter:** unfiltered zone from 40% to 60% of the SIZE attenurandomizer plus SIZE CV range.
- **Synth start, Manual:** hold [A] 1.5 s and release to start or stop (the manual says 1 s).
- **Processor, Spectral:** takes effect once the page has closed and FREEZE is off (until then the current engine plays on): a 10 ms fade out, the codec restarts with 250 ms of silence, then a 10 ms fade in. Spectral frames are 4096 samples with a 1024-sample hop at the quality's rate (48, 32 or 24 kHz).
- **Monophonic grains:** the [A] LED is half as bright when on; the recording reserve can dim it further.
- **Frozen-buffer autosave:** a save cut short after erasing doubles the next wait, up to 320 s.

## Off-by-default features

A lightweight requested feature may ship Off by default. Its Off state must
leave the audio unchanged and add no measurable audio-path cost; branches
belong outside sample loops. Grain stealing (ID 34) is the exception: its
default is the 2021/2024 shipped behaviour and 2026 is the choice. Measure each feature and interacting controls.
Hygrostat shows no CPU estimate and blocks no combination for CPU; the
working limit is in [Design aims](#design-aims). New features still require flash, permanent storage fields, controls and
maintenance.

### Small Beta 2 profile options

IDs 15–23 default to Off. LED brightness Off means full brightness. Brightness,
SEED latch and TIME modulation allow Off, Fixed or Adjustable; an Adjustable
choice can occupy any page slot (1–21). They have no amount, action,
SEED-stepping or CV-assignment slot. All other new features allow Off or Fixed
only, with no control placement. Omitted entries are Off. The default profile
block is unchanged. The page-facing catalogue is
`beads/build/profile-features.json`; costs are unknown until measured. Its
format 2 adds the manifest's SHA-256, the record version, and each feature's
record byte and factory entry; each release publishes it.

Brightness uses 100% / 66% / 33% (0 / 1 / 2), selected in thirds of a page knob
when Adjustable or set via Hygrostat when Fixed. There is no power-on
brightness gesture. SEED latch uses Off / Ignore / Sync / Add (0 / 1 / 2 / 3). TIME modulation uses Random / Scan (0 / 1).
Fixed-only features use Off / On (0 / 1). Brightness scales the final LED
frame, including options and warning displays, and previews while turning. Off
and Fixed 100% preserve every stock LED value. At 66% and 33%, white is no
longer halved (#240): the quality LED's red scales by level^1.5 (truncated),
and its lit green and blue by the level plus 12 x (1 - level), i.e. +4 at 66%
and +8 at 33%; the other LEDs scale by the level. The rule was chosen by eye on
a module (white 102/130/141 at 66%, 36/71/76 at 33%); a light-sensor check is
planned for Beta 4. UI priority
is options and storage indications, manual meters, boot cue, delay cue, pitch notch, then
quality/overwrite. Delay cues last 150 ms on both actual transitions. The boot
cue is a 100 ms light-blue flash identifying Drizzlet, followed by 150 ms
light-blue pulses separated by 150 ms gaps. One release constant sets the pulse
count (3 for Beta 3); audio startup never waits for this cue. Notch indication
lights [A] pure blue and follows the detune law's unison and octave plateaus and both two-octave knob
ends before smoothing; it ignores CV, random pitch and quantization. A small
hysteresis holds the cue at plateau edges. Live assignment LEDs retain stock
width, diffusion and amount-weighted patterns, scaled by absolute assignable CV
(32/255 at zero to full stock brightness at full modulation). FREEZE remains
full intensity during a save. These features never change the audio or save
layout.

Manual attenuation reserves gain byte 0 for automatic gain and 1 for silence.
Bytes 120–136 snap to unity, with attenuation below and the usual maximum gain
above, never exceeding stock. The meter shows green at unity. Stock firmware
never goes below unity: Mutable Instruments deliberately kept unity as the
minimum because "it's a sensitivity setting". This option keeps unity easy to
find. Existing gain bytes are preserved; an enabled profile deliberately
interprets manual settings with this new law. Automatic gain is unchanged.
There are no new sample-loop tests or branches.

SEED latch defaults to Off: stock uses the patched cable as a clock divider
while latched. Ignore free-runs and ignores cable triggers, including their LED
flashes. Sync is a hard sync: each SEED edge starts a grain and restarts the
internal grain clock, so a clock faster than DENSITY's rate plays a grain per
edge and noon plays only the edges (#293). Add free-runs and fires an extra
grain per edge. The divider is unavailable in every non-Off value. These
choices act only while latched with a cable patched. Snap and trigger hits
never join the SEED stream: each adds a grain to the granular triggers in every
SEED state (#290). Synth grain clocks use the same choices;
delay has no grain clock and free-runs for non-Off values. In spectral mode,
Sync and Add behave like stock: cable gates pass through. Choose any options
page slot or a Fixed Hygrostat value. TIME Scan applies only to granular
playback with TIME CV unpatched. The attenurandomizer sets signed speed: noon
stops, clockwise moves forwards, counter-clockwise backwards; full travel scans
the whole buffer in four seconds. The position wraps and is added to TIME.
Patching CV restores Random/attenuation, and pauses the scan; delay and
spectral playback also pause it. The position resets at processor
initialization and is not saved. Feature selection occurs once per block; only
enabled Sync/Add inspect individual cable edges. Off adds no per-sample work.


### Synth reverb tail

Hygrostat feature ID 24 chooses Keep (stock) / Clear. Off or an omitted entry
keeps the shared reverb memory when leaving either synth, allowing its tail to
continue as in stock firmware. Fixed Clear clears it on manual stop, input
patching and processor changes. This is a firmware-profile choice, with no
panel slot or gesture. It has no saved byte and no per-audio-block work;
the check and optional clear run only during transitions. Fades and codec
handling are independent of this choice.

### Options page idle timeout

Hygrostat feature ID 25 chooses Off / 5 s / 10 s / 20 s / 30 s. Off or an
omitted entry keeps the pages open until you close them. A Fixed time closes
the pages after that long without a button press, page change or turn of an
option knob on the current page, counted from releasing the opening hold, and
never while a button is held. An option knob counts as turned once it is more than 2% of its travel from where it was
last counted, however slowly; jitter, CV and the stored-value preview don't
count. Closing by timeout is the same as holding [M]: changes are kept and
saved once if unsaved. Firmware-profile choice, with no panel slot or
saved byte.

### Delay SEED latch fade

Hygrostat feature ID 26 chooses Switch at once (stock) / Fade. In delay mode
(SIZE fully clockwise), stock latching or unlatching SEED switches between
tapped and knob timing in one sample, which can pop. Fade dips the delay taps
for 10 ms, switches timing at silence and fades back in over 10 ms, without
the short chirp. Patching SEED while latched fades the same way; a clocked
latch never fades. Firmware-profile choice, with no panel slot or saved
byte.

### Input gain at power-on

Hygrostat feature ID 27 chooses Automatic (stock) / Manual (#19). Off or an
omitted entry starts in automatic gain unless a manual gain is saved, as
stock. Fixed Manual starts a blank gain at unity manual gain instead: the
lowest setting on the stock law, or 128 (unity, green) with input
attenuation. A saved manual gain always wins. [A]+FEEDBACK still sets and
saves manual gain, and holding [A] still restarts automatic gain until the
next power-on. The unity value is saved with the next settings save, like any
manual gain, so it stays after switching back to Automatic until [A] is
held. This is a firmware-profile choice, with no panel slot or saved
byte; it runs only at power-on.

### Attack sensitivity

Hygrostat feature ID 28 chooses Normal (default) / Low / High for Attacks
(#58, #47). A block is an attack when its peak exceeds the threshold times
the larger of a fast reference (instant attack, 50 ms release) and half the
input's slow peak (0.5 s release). Normal keeps Beta 2's 1.5 (+3.5 dB). Low
is 2.0 (+6 dB) and ignores softer notes after a hit. High is 1.3 (+2.3 dB):
it catches quieter ghost notes, and more false attacks on noisy input. The
threshold is chosen at power-on; Snap and Snap and trigger share it. No panel
slot or saved byte. Host measurements are in
[Testing](TESTING.md#beta-3-host-measurements).

### SEED at power-on

Hygrostat feature ID 29 chooses Latched (stock) / Unlatched (#41). Fixed
Unlatched powers on as after a SEED tap: SEED presses, gates and, with Snap
and trigger, detected attacks start the grains, and in delay mode SEED taps set the time. Holding SEED for 4 s or
SEED+FREEZE latches as usual; the SEED latch options (#13) apply once
latched, and with the delay SEED latch fade the delay starts in tapped
timing without a fade. Power-on only, with no panel slot or saved
byte.

### PITCH knob

Hygrostat feature ID 30 chooses Notches (stock) / Smooth (#43). Smooth keeps
the knob's intervals at the same positions (unison, semitone, minor and
major third, fifth, one and two octaves) but moves linearly between them,
without the flat zone around each. Calibrated PITCH CV and the PITCH
attenurandomizer are unchanged. The pitch notch cue (ID 22) stays dark with
Smooth. Grains, delay pitch and both synths follow the knob law. The law is
chosen at power-on. No panel slot or saved byte.

### Synth and Rings polyphony

Synth (ID 7) value 2 is Rings, offered only when the Rings synth is
installed. Page 3's DRY/WET then picks Beads, Plaits or Rings in thirds
(green, yellow, red); with two playable synths it uses halves, and with one
it is shown but cannot change. A Synth action steps through the playable
synths. A saved synth that is not installed stays saved; another one plays
until it is back. While Rings plays, [A]+FREEZE steps its polyphony (ID 42,
record byte 27): one, two, then four voices, shown green, yellow and red
as on Rings, and saved. A profile's Synth start on [A]+FREEZE keeps it.
Polyphony and FEEDBACK changes also apply while FREEZE sustains Rings.

### Plaits bank

Palette VOSIM starts fresh on each selection, with 4 ms of silence while its initial DC estimate is prepared.

Plaits synth mode always plays one bank of eight models, in Plaits' own
groups (#190): green is its first bank (virtual analog to speech, models
9–16), red the second (swarm to hi-hat, 17–24), yellow the eight models
added in Plaits 1.2 (1–8). FEEDBACK selects one of the eight, about 34
degrees each; Beta 2's 24 models on one knob are gone. While the Plaits synth
plays, hold [A] and press FREEZE to step green, red, yellow, then green
again; [A] shows the new bank's colour for half a second, the bank is saved
(record byte 23) and FREEZE stays as it was. FREEZE alone still sustains.
Elsewhere [A]+FREEZE stays buffer trim; spectral and the wavetable synth
have no recording to trim there, so it only releases a latched FREEZE.

The step is fixed to [A]+FREEZE in Plaits synth play: it is the only way to
reach two thirds of the models, so it does not depend on where a profile
puts Buffer trim. Whatever a custom profile assigns to [A]+FREEZE keeps its
meaning everywhere else; only Synth start, which can stop the synth, keeps
[A]+FREEZE in Plaits play too, and then the bank stays at its start.

Two placements are refused, by the firmware's profile check and by
Hygrostat: Monophonic grains or Buffer trim on the [A] hold, which would
leave a saved manual gain no way back to automatic gain (#291), and Synth
start on Manual or Adjustable with no gesture, which would leave Manual
nothing to start the synth (#292). The firmware treats such a profile as
invalid: standard controls, no saving.

Speech decodes a new word bank over several audio blocks, 32 frames per
block, instead of all at once (#225): one bank took up to 1.7 blocks. While
it loads, the voice holds its current sound and a new word waits, 1.75 to
7 ms depending on the bank, each time a voice changes bank (each voice keeps
one bank). Once loaded, it plays exactly as Mutable Instruments' code does.

Hygrostat feature ID 31 chooses the starting bank (Green / Red / Yellow,
default Green) and is Adjustable only. Profiles without the entry start on
green. A blank or stock settings sector, a SEED power-on reset or a saved
value the firmware doesn't know starts on the profile's bank. More banks,
such as Plaits Palette models (#194), would add values to this ID.

### Synth output level

Hygrostat feature ID 50 chooses Stock / Half / Quarter / Eighth (#51),
reducing both synth audio outputs by 0, 6.02, 12.04 or 18.06 dB. This is a
final output trim, after the synth's own effects and limiting; it does not
change the saved input gain, grain levels, feedback or reverb tails. When
OUT R carries triggers, its level stays stock. It applies to Beads, Plaits
and Rings synths; granular, delay and spectral output stay stock. Chosen at
power-on, Fixed only, with no panel slot or saved byte. Off and omitted
entries keep the original audio callback.

### Mix law

Hygrostat feature ID 32 chooses Stock / Gentle / Send (#21, #48). Stock
DRY/WET brings the grains in quickly: at 9 o'clock they are only 2.5 dB under the dry.
Gentle makes the wet side rise in a straight line, so at 9 o'clock the
grains are 7.6 dB under the dry (Clouds: 8.8 dB) and at noon 3.5 dB quieter
than stock. The dry level and both ends of the knob stay stock. DRY/WET CV
follows the same law. Granular, delay and spectral; both synths keep their
own DRY/WET. Chosen at power-on, with no panel slot or saved byte.

Send (value 2) keeps dry at its fully dry level. DRY/WET sets each new
grain's level, linearly; that grain keeps its level until it finishes.
At zero no new grains start, but existing grains and reverb finish.
Recording and feedback continue, so raising DRY/WET finds recent material.
The reverb follows the grains before dry is added, regardless of Reverb
input. Granular only; delay, spectral and synths retain their own mix.
This adds a value to ID 32 without changing Stock or Gentle.

### Grain trigger output

Hygrostat feature ID 51 chooses Stock, /2, /4, /8 or /16 (#29). When OUT R
carries grain triggers, it passes the first grain's gate, then every chosen
number of grains; accepted gates keep their original width. It changes only
OUT R, not the grain timing, recording or audio mix. The divider keeps its place between
audio blocks while trigger output is on. Delay, spectral and synths keep
the stock output. Chosen at power-on, Fixed only, with no panel slot or
saved byte; Off and omitted entries select Stock.

### Reverb input

Hygrostat feature ID 33 chooses Mix (stock) / Grains (#20). Stock sends the
whole output, dry and grains, to the reverb, as an end-of-chain effect.
Grains sends only the grains, before the dry/wet mix, in Clouds' order: the
dry stays dry and REVERB no longer turns it down. DRY/WET then sets the
reverb with the grains, so fully dry has no reverb at all and turning
DRY/WET down cuts the tail too, as on Clouds. In Scorched cassette the wow
and flutter comes after the reverb, so the tail wobbles too. The reverb
returns with the grains, so the granular/delay switch fades its tail out and
back in with them. Granular, delay and spectral; both synths keep the stock
routing. Chosen at power-on, with
no panel slot or saved byte.

### Reverb decay

Hygrostat feature ID 54 offers Coupled (stock), Short, Medium and Long
(#38). A fixed decay lets you use a long tail at a low mix or a short tail
at a high mix. REVERB and its assigned CV keep their existing mix response.
The choice applies to both reverb characters, all qualities, granular, delay,
spectral and all three synths. It has no panel slot or saved byte. Off and
omitted entries use the stock coupled response. Firmware without ID 54
uses its fallback profile if this entry is present.

Short, Medium and Long set loop feedback to 0.30, 0.60 and 0.85. The
resulting tail length depends on sample rate and reverb character. The
existing mix limits are 0.90 for the processor and 0.80 for the synths.
An optional full-wet range would need a separate choice.

### Grain stealing

Hygrostat feature ID 34 chooses 2021/2024 (value 0, the default) or 2026
(value 1) (#237). When all 32 grains are playing, a new grain takes over an
old one. The 2021 and 2024 firmware Mutable Instruments shipped takes the next
one in turn when grains arrive less than 5 ms apart; from 5 ms on it re-sorts
the grains and takes the ones closest to their end. The published 2026 source
takes the next one in turn up to 25 ms. With grains 5 to 25 ms apart and
varying lengths, 2021/2024 gives a different take of the same texture, 0.3 to
1.1 dB louder in desktop renders of dense random patches.

Betas 1 to 3 used the 2026 stealing; Beta 4 returns to the shipped one. In our
crackle test the 2026 source clicks less often than the 2021 and 2024
firmware, but still clicks; Drizzlet's one-frame output delay (#1) addresses
the click's cause. This is a scheduler change, not an optimization: on one
module the 2021/2024 stealing added 3.4 to 3.6 points of peak load in Bright
digital, Sunny tape and Scorched cassette ([CPU ceiling](TESTING.md#beta-3));
Cold digital is not measured. Granular and the wavetable synth; delay,
spectral and the Plaits synth have no grain stealing. Chosen at power-on, with
no panel slot or saved byte. Value 0 always means 2021/2024, so
profiles without ID 34, including all Beta 3 profiles, get it.

### Delay feedback

Hygrostat feature ID 35 chooses Stock / Unity notch (#10), for sound on
sound in delay mode. Unity notch gives FEEDBACK a flat notch, 6% of its
travel wide, near 3 o'clock where stock feedback reaches unity: there each
repeat comes back at the level it went in, and new sound layers on top, as
on a looper. Below the notch the repeats fade; above it they grow into the
limiter, as in stock; both ends of the knob stay stock. FEEDBACK CV follows
the same law. The loop's bass filter drops from 40 Hz to 10 Hz, so at
100 Hz a repeat loses about 0.1 dB instead of 1.3 dB. Tape qualities still
darken and saturate a little on each pass. Past noon on DENSITY the second
tap joins: the notch then keeps the sum of both taps at unity, so the
pattern does not grow but slowly fades, faster on Bright digital; a jump
across noon can lift the loop above unity for part of one 0.25 ms block,
once. During a Delay time crossfade the notch allows for the old and new
delay times adding in phase, so the loop never rises above unity; other
material loses up to 3 dB on the passage that fades. Mutable
Instruments noted that grain levels vary too much for a fixed unity, so
granular, spectral and both synths keep the stock feedback. Chosen at
power-on, with no panel slot or saved byte.

### Delay time

Hygrostat feature ID 36 chooses Glide (stock) / Crossfade (#30). In delay
mode with knob timing (SEED latched, nothing patched), stock glides the
delay time when DENSITY moves, bending the pitch like tape. Crossfade
keeps the old delay time while DENSITY moves; 50 ms after it stops, an
equal-power crossfade (a clock's is linear) moves to the new time over
half of it, between 20 and 250 ms. A turn of the knob
gives one fade, after the turn. A fade runs to its end before the next one
starts, and changes under 2% are ignored, so a slow DENSITY CV moves in
steps rather than as a chorus or vibrato. A tone can still dip mid-fade
when its two copies are out of phase. Delays under 10 ms still glide, for
flanging. FREEZE slices keep stock's glide too: they replay at their own
speed, so they never bend the pitch. A fade already running when FREEZE
engages finishes first. Clocked and tapped timing and TIME are unchanged.
Chosen at power-on, with no panel slot or saved byte.

### Options page preview

Hygrostat feature ID 37 chooses Each knob in turn / First setting only
(#285). By default, whenever a page opens (releasing the opening hold, or
tapping [M] to the next page), [A] shows each used knob's stored value in
turn for 2048 ms, with that knob's mix LED steady, then the page's first
setting. First setting only goes straight to that end state: the page's mix
LEDs pulse at once and [A] shows the first used setting, its blink restarted.
The 100 ms open flash, knob pickup, SEED scale taps, saving and the idle
timeout are unchanged. Chosen at power-on, with no panel slot or saved
byte; it runs only while the pages are open.

### Options page on opening

Hygrostat feature ID 38 chooses Last used / First page (#285). By default the
pages reopen on the page last used, until power-off; the page is never saved.
First page opens on the lowest page in use every time, including after the
idle timeout closes the pages; on a custom layout whose page 1 is empty that
is the first page with a setting. While the opening hold continues, [A]
already shows that page's first setting and the open flash lights its LEDs.
Tapping [M] still steps through the pages. Chosen at power-on, with no panel
slot or saved byte; it runs only when the pages open.

### Start in quality

Hygrostat feature ID 39 chooses Keep current / Bright digital / Cold digital /
Sunny tape / Scorched cassette (#287). Keep current, the default, never
changes the saved quality. Any other value is the quality at start-up after
the profile is installed, over stock, an older release or another profile, and
after a settings reset (SEED held at power-up). Until something is saved, every
start-up applies it again. The saved settings record which profile last saved
them (the record's profile identity), so any save, even an unrelated one, accepts the
profile; from then on a quality chosen on [A] wins. When a frozen granular
recording is to be restored from flash, the module keeps that recording's
quality instead; the check is that a restore is intended, so with an unreadable
flash copy the old quality also stays. Chosen at power-on, with no panel slot
or saved choice; the audio path is unchanged.

### Resonator

Options page 5 (FEEDBACK and REVERB LEDs) holds Hygrostat feature ID 41,
Off (stock) / After the grains / Before the buffer / In the feedback
(#88), on FEEDBACK in quarters. It adds Rings' modal resonator,
24 modes in stereo (odd modes
left, even modes right), computed in Plaits' batched form with its tuning
and pickup position updated once per 0.25 ms block, so it is close to
Rings' but not identical. After the grains, the grains strike it and its
resonance joins them before DRY/WET; the recording and the dry signal are
unchanged. Before the buffer, the input strikes it and the input plus its
resonance is recorded, so the grains play the resonance; the dry stays dry
and the feedback loop is not resonated. PITCH and its CV tune it (C3 at
noon), not PITCH's randomness; Pitch scale snaps it, and with Recent notes
it plays the newest note sent to PITCH. Attacks hits also strike it. While FREEZE is on, Snap and trigger hits
still strike it but Snap hits do not, as they also leave no marks then.

SEED strikes it too, in every Attacks mode. A hit and SEED strike it the
same way, as Rings' strum does: a pulse through a low-pass that follows
pitch and brightness, at Rings' level. From silence String peaks near
-8 dBFS from C2 to C5, Wood and Glass at -7 to -13 dBFS, and Bell, the
darkest, near -21 dBFS. Unlatched, the button and the jack make one gate, as the grains
see them, and each rising edge strikes once; a press while the jack is high,
or a gate while the button is held, does not. Latched, each rising edge at
the jack strikes it (Sync and Add too; Ignore has no jack), and the press
that unlatches SEED does not. In delay mode SEED is the clock and strikes
nothing. Combos that take SEED ([A]+SEED, [M]+SEED, options pages) never
reach it. A hit and SEED in the same 0.25 ms block strike once, at SEED's
level. Turning the resonator on with SEED held does not strike it. Before
the buffer a strike while frozen is not heard: the resonance would only
be recorded.

In the feedback (value 3) is After the grains, and the grains' feedback
loop goes half through the resonator: the grains keep FEEDBACK x sqrt(1/2)
of their own loop (half its gain, as the gain goes with FEEDBACK squared),
and the resonance of the previous 0.25 ms block is added to the recording
at the other half of the grains' loop gain (3 x FEEDBACK squared, 1.5 in
delay mode), with the tape qualities' soft clip input gain taken out. While
FEEDBACK moves, the resonance takes the lower of the block's two ends and
the soft clip's gain the higher. On its way the resonance goes through the
grains' own feedback high-pass (40 Hz, 10 Hz in the unity delay loop) and
their low-pass, with the same time constant at the resonator's rate, so
these pass no more of it than of the grains' feedback at any frequency the
recording keeps (within 0.01 dB). It is divided by a bound on the bank's
peak gain at any frequency: the largest mode peak (|level| x Q, from this
block's damping and pitch, a Drum strike's glide included) plus the most
the other modes add near it (|level| x sqrt(r) / |r - 1| for frequency
ratio r), or the sum of all peaks where that is smaller. The parts that
depend on the object are worked out off the audio path when it changes, and
used only for the object they name. For fixed controls the loop's gain is
then never above the grains' own feedback's at any frequency; on 720
frequency responses (eight Materials, struck, four Ring times and both ends
of damping, C0 to C8, three rates) none was above the bound. This bounds
the gain, not the phase, and when damping, pitch or the object change the
bound falls no faster than its slowest sounding mode decays at the current
tuning (checked against every mode's poles, every semitone C0 to C8); how
the loop behaves is therefore measured rather than proven. The loop's level is never set from
its own output, so brightness and damping still shape it. Over 60 s of
silence (30 s frozen) after 10 s of input, in every quality, granular and
delay, every Material and Ring time, frozen or not and FEEDBACK from 0 to
full, it never
sustained where After the grains dies away; peaks were comparable
(4,096 runs each). With damping and PITCH's CV jumping, Limiter Off, unity
delay feedback and Evolving, it also never sustained where After dies away.
At FEEDBACK 0 it is After the grains exactly; at 0.3 and 0.5 its output
stays within 2 dB of After
the grains' on the same renders. Frozen, nothing is recorded and it is
After the grains. The recording's own input limiter is still the ceiling.

Material (ID 43, DRY/WET, eighths) sets structure and brightness: Wood
(0.5, 0.3), String (0.25, 0.5), Glass (0.4, 0.5), Bell (0.8, 0.15). They
were chosen from renders at C2 to C5: on grain-like noise each stays within
about 2 dB of String on average, Wood about 0.7 octave darker, Glass about
0.75 octave brighter with stretched partials, Bell strongly stretched.

Marimba, Drum, Bowl and Gong (values 4 to 7) are Drizzlet's own objects,
which Rings' stiffness curve can't make. Each has its own table of 12 mode
pairs, one mode of a pair on the left and one on the right, with each mode's
ring time relative to the fundamental's and its level; structure is unused
and brightness shapes only the strike. Marimba is a tuned bar (partials 1,
4 and 10, the upper ones dying 3 to 8 times faster; both sides alike, so
centred). Drum is a membrane (partials 1, 1.59, 2.14, 2.30...); a strike
raises its pitch 165 cents, falling back with a 0.125 s time constant.
Bowl is a bowl or wine glass (1, 2.83, 5.40, 8.74...) whose pairs sit 0.8%
apart, so each partial beats between left and right (0.5 Hz at C2, 4 Hz at
C5). Gong is a free plate (1, 1.73, 2.33, 3.91...) with beating pairs; a
strike lowers its pitch 107 cents, rising back with a 0.33 s time
constant. Only a strike (SEED or a hit) bends the pitch, never the input's
or the resonance's level, and each glide is held within its table's amount.
On grain-like noise each is within 1.1 dB of String on average and within
2.4 dB at any note from C2 to C5; strikes peak within 3.2 dB of String's and
under the limiter's ceiling. Each table's ring times are at most the
fundamental's, so no mode rings longer than Rings' own. An exact structure
from Hygrostat selects Rings' curve instead of the object's table.

Ring time (ID 44, REVERB,
quarters) sets damping: Short 0.35, Medium 0.5, Long 0.65, Very long 0.8,
each step ringing about four times longer (the fundamental about 0.6, 2.3,
9 and 37 s at 48 kHz; Cold digital rings 1.5 times longer, the tapes twice).
String and Medium are the fixed values of Beta 4's first resonator, so the
factory sound is unchanged; both light yellow, second on their pages. Page 5 saves
like the other pages, in options record bytes 24 (placement), 25
(Material) and 26 (Ring time).

Hygrostat can set exact values, IDs 45 to 49 (Fixed only, one byte):
structure, brightness, damping and pickup position as byte / 256, and amount
as byte / 128 (128 is unity). An exact value replaces the page's value for
that control only; Material still sets whichever of structure and
brightness is not exact, and the page still shows its own step. Hygrostat
takes Material off the page when structure and brightness are both exact,
and Ring time when damping is.

With the resonator on, tapping [M] after REVERB (and width and diffusion,
when they can be assigned) makes the CV input move its damping: FEEDBACK
and REVERB pulse, and the CV adds to Ring time's or the exact damping,
held between 0 and 1. The next tap returns to FEEDBACK. Turning the
resonator off, or a synth starting, returns the CV to FEEDBACK. It is not
saved yet: a power cycle starts at FEEDBACK. The controls are updated once
a millisecond. While the CV moves damping the audio callback still adds it
to FEEDBACK, as for destination 0; the resonator's block plays FEEDBACK at
its knob value instead, so the paths without the resonator are unchanged.

Its own limiter stages it before the mix: a full-scale sine
tuned to the fundamental or the second mode, C3 to C7, peaks at -6.0 to
-5.0 dBFS, and full-scale noise at -12.9 to -3.9 dBFS, at every rate.
Spectral restarts it from silence; after the synths an old tail can
resume. Granular and delay; spectral and both synths are unchanged. All 32
grains still play: on the development
board, 32 grains plus the resonator measured 77.1% at Bright digital, an
estimated 88.9% on a module against the 90% limit then in force.

### [A] gestures in profiles

A Hygrostat profile chooses what the [A] gestures do (#301). The factory
layout and every earlier profile keep their gestures.

- [A]+SEED and [A]+FREEZE can take Monophonic grains, Buffer trim, Synth
  start or a two-value toggle: Reverb character, Processor, Frozen feedback,
  Synth or TIME modulation. A toggle flips its setting (Synth steps through
  the playable synths), saves it, and [A]
  shows the new value's options page colour for half a second (green first
  value, yellow second). Processor clears the recording, as from its page;
  while FREEZE is on, the change waits for its release. In Plaits synth
  play [A]+FREEZE still steps the bank, so a toggle there waits for the
  synth to stop, and Synth goes on [A]+SEED only. The [A] hold stays Synth
  start only.
- [A]+FEEDBACK, [A]+DRY/WET and [A]+REVERB can each take Manual gain,
  Stereo width or Diffusion, one per knob. Hygrostat feature ID 40, Manual
  gain knob, places the gain: Off keeps it on FEEDBACK; Fixed takes it off
  the knobs. Releasing [A] saves the gain wherever it is. Holding [A] still
  restarts automatic gain when it doesn't start the synth, and ID 27 still
  chooses the power-on gain.

Firmware before Beta 4 refuses these layouts. The format is in
[SCALE-PROFILES.md](SCALE-PROFILES.md#a-gestures-beta-4).

### Gain ceiling

Hygrostat feature ID 52 limits automatic input gain to Stock (+32 dB),
+24 dB, +18 dB or +12 dB (#59). The gain driver multiplies samples
digitally; the codec has no programmable gain steps. Stock keeps the
original 40× limit. The lower limits are 15.849×, 7.943× and 3.981×.
Manual gain and [A] hold restart keep their usual behaviour.

A custom profile can assign this setting to a page knob, with green,
yellow, red and slowly pulsing yellow indications. Changes preview immediately and save
in options record byte 28. The factory pages stay unchanged and use Stock.
