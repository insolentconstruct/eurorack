# Feature allocation

**TL;DR:** which buttons, gestures, settings bytes, flash and CPU Drizzlet already uses (TAKEN) and what is still free (FREE), with the rules for adding options.

The [options guide](OPTIONS.md) explains the controls, zones and LED patterns.

The `drizzlet` branch, based on the Mutable Instruments source `d8f4739`. TAKEN means existing behaviour; FREE means no dedicated action or allocation, with the limits below. Planned uses come from the tentative roadmap in [#32](https://github.com/insolentconstruct/eurorack/issues/32); they can change until built. <!-- source: git trees fc549d7, d8f4739 -->
Source comments use repository-relative paths; `stock:` means `d8f4739`; Drizzlet pins stmlib `d18def8` (stock `e3bd7c9`; the stmlib files cited here are identical in both); “linker script” means `stmlib/linker_scripts/stm32h7xx_flash_application_large.ld`, and `manual.md` is the Beads manual. <!-- source: git ls-tree fc549d7 stmlib; git ls-tree d8f4739 stmlib -->

## Buttons and startup

| Gesture | Allocation / action | Saved? |
|---|---|---|
| the quality button [A] tap | TAKEN: cycle quality downward, 3 → 2 → 1 → 0 | Yes <!-- beads/ui.cc:472-495 --> |
| [A] hold, 1.5 s | TAKEN: with no input, FREEZE off and Synth start on Manual, toggle wavetable synth; otherwise restart automatic gain and clear manual gain | Gain only; manual says 1 s <!-- beads/ui.cc:48-50,478-484; manual.md:44-45 --> |
| [A] + feedback knob | TAKEN: manual input gain; release saves. Planned: manual gain improvements (#19) | Yes <!-- beads/ui.cc:174-177,491-495 --> |
| [A] + dry/wet knob | TAKEN by Drizzlet: stereo width; release saves | Yes |
| FREEZE tap | TAKEN: toggle freeze | Buffer/state save starts after 10 s (with interruption backoff below) if autosave enabled, outside wavetable synth and spectral modes <!-- beads/ui.cc:402-404; beads/background_buffer_backup.cc:144-161; beads/beads.cc:406-408 --> |
| FREEZE hold | FREE dedicated long action; tap already toggles, 4 s event has no extra action | — <!-- beads/ui.cc:48-50,402-404,461-520 --> |
| SEED press / release | TAKEN: unlatch, then generate while held; release stops | No; starts latched at boot <!-- beads/ui.cc:81-83,406-411,463-469 --> |
| SEED hold, 4 s | TAKEN: latch generation | No <!-- beads/ui.cc:48-50,463-469; manual.md:59 --> |
| the CV-assign button [M] tap | TAKEN: cycle feedback / dry/wet / reverb CV destination | Yes <!-- beads/ui.cc:499-515 --> |
| [M] + mix knobs | TAKEN: set three independent CV amounts; release saves | Yes <!-- beads/ui.cc:178-180,499-515 --> |
| [M] alone, 3 s | TAKEN by Drizzlet: options entry | Save gesture below <!-- beads/options.h:125-140; beads/build/README.md:97-128 --> |
| Hold SEED, press FREEZE | TAKEN: latch SEED; consumes both releases | No <!-- beads/ui.cc:320-324; manual.md:59 --> |
| Hold [M], press SEED | TAKEN: toggle R output between audio and grain gate | Yes <!-- beads/ui.cc:331-339; manual.md:138 --> |
| Hold [M], press FREEZE | TAKEN: toggle buffer autosave; briefly display status | Yes <!-- beads/ui.cc:346-355,560-562 --> |
| [M] + [A], either order | WARNING: calibration entry; do not allocate | Calibration can be overwritten <!-- beads/ui.cc:390-398,428-453 --> |
| Hold [A], press SEED | TAKEN: toggle monophonic grains; consumes both releases; [A] first | Yes <!-- beads/ui.cc:358-369 --> |
| [A] + FREEZE | TAKEN: hold [A] first, then press FREEZE to trim the recording window to zero and clear the button freeze latch | Mutable Instruments’ original gesture; saves keep the length; stock loads full length |
| Power on holding FREEZE | TAKEN: audio updater; FREEZE release retries after an error; TIME sets update gain | Writes firmware <!-- beads/bootloader/bootloader.cc:76-78,160-162,315,377-381; manual.md:247-253 --> |
| Power on holding FREEZE + [M] | WARNING: erase app sectors 1–7, then receive update; enabled | Erase chord keeps boot/settings sectors; reception has no upper bound <!-- beads/bootloader/bootloader.cc:28,266-280,317-322 --> |
| Power on holding SEED | WARNING: reset State to defaults, including limiter; retain valid calibration. With both copies of the calibration lost: repair with the factory calibration; result shown 3 s | Rewrites settings sector <!-- beads/settings.cc; beads/settings_storage.h --> |
| Other power-on chords | No dedicated action; inherit FREEZE/SEED checks and may reach normal combos after startup | Not safe unused gestures <!-- beads/bootloader/bootloader.cc:315-322; beads/beads.cc:319-345; beads/ui.cc:315-425 --> |

All six pairs are covered above. Combo checks use live debounced state; event order is FREEZE, SEED, [A], [M], but simultaneous presses need not produce a combo. SEED+FREEZE takes priority over [M]+FREEZE; extra-button chords are not free slots. <!-- beads/drivers/switches.h:40-45,62-67; beads/ui.cc:140-168,315-399 -->
The manual agrees on the main gestures; autosave toggle and erase/reset chords are source-only here. Use source timing for [A] (1.5 s versus the manual's 1 s). <!-- manual.md:44-49,59,132,138,247-253; beads/ui.cc:48-50,346-355; beads/bootloader/bootloader.cc:317-322; beads/beads.cc:329 -->

## Drizzlet options

Enter with [M] alone for 3 s, mix knobs still, then release; entry needs normal mode and 20 ms with all buttons up. Tap [M] cycles pages 1 → 2 → 3 → 4 → 1; the last page is kept in RAM, with page 1 at power-up. A knob must move >10% to edit. Saves one second after the last qualified movement or value change; no idle close. Hold [M] 3 s or press another button to close, keeping changes. On page 1 with Scale selected, short SEED taps step the scale instead of closing.

| Page / pulsing LED | FEEDBACK | DRY/WET | REVERB |
|---|---|---|---|
| 1 / FEEDBACK | Pitch scale | Random intervals | Attacks |
| 2 / DRY/WET | Delay mode | Reverb character | Frozen feedback |
| 3 / REVERB | Processor | Synth | Synth start |
| 4 / all three | Reserved | Reserved | Limiter |

Reserved knobs have no effect on [A] or saving. On entry and on each page change, [A] shows each stored value for 2.048 s, left to right, skipping reserved knobs, with that knob's mix LED lit steady; a qualified knob or a SEED scale step ends it. [A] then shows the first available setting, then the last qualified knob moved. Blink patterns start when the shown setting changes. Every first value, including Pitch Off, is steady green; other values follow the green/yellow/red ladder with steady, slow (0.98 Hz) or fast (3.91 Hz) blinking; Raag Shri uses two yellow pulses then a pause. The page LED flashes for 100 ms on entry, close and after a changed batch is written. README.txt's Options reference lists every colour and pulse; `make -f beads/build/tools/options_tables.mk test` checks it and `./build/options_tables` prints its TSV from the firmware tables. Pitch scale values and grain behaviour: [options guide](OPTIONS.md).

## Saved data

Hygrostat format v1 keeps State at 24 bytes. Bytes 0–9 retain their stock
meaning. Width (14), CV destination (16) and diffusion (17) use full-byte
encoding.
Settings from stock carry over exactly as stock reads them, including
monophonic mode. Drizzlet's options from earlier releases (alphas and Beta 1)
start from the profile's defaults. Calibration is always kept.
Byte 23 is the format marker: 0 is the old layout, 0xC1 is packed v1, and
every other marker selects the profile defaults. Old records are decoded in
RAM; the next save writes packed v1.

Bytes 10–13, 15 and 18–22 hold twenty permanent nibble fields. The low
nibble admits 0–14, with 15 meaning not set; the high admits 0–8, with 9
meaning not set. Pitch uses a low nibble. No packed byte can be 0xA0–0xAF,
so Beta 1 reads the packed choices as Stock. Fields never move or get reused;
Off and Fixed features keep their dormant saved fields. Unsupported or
not-set values use the profile default.

| Feature ID | Feature | Permanent field |
|---|---|---|
| 1 | Pitch scale | 0 (byte 10 low) |
| 2 | Reverb character | 1 (byte 10 high) |
| 3 | Delay mode | 2 (byte 11 low) |
| 4 | Processor | 3 (byte 11 high) |
| 5 | Limiter | 4 (byte 12 low) |
| 6 | Frozen feedback | 5 (byte 12 high) |
| 7 | Synth | 6 (byte 13 low) |
| 8 | Attacks | 7 (byte 13 high) |
| 9 | Synth start | 8 (byte 15 low) |
| 10 | Random intervals | 9 (byte 15 high) |
| 11 | Stereo width | Full byte 14 |
| 12 | Diffusion | Full byte 17 |
| 13 | Monophonic grains | Stock byte 9 |
| 14 | Buffer trim | No saved value |
| 15 | Version at boot (#54) | No saved value |
| 16 | Input attenuation (#44) | No saved value |
| 17 | Delay indication (#42) | No saved value |
| 18 | LED brightness (#40) | 10 (byte 18 low) |
| 19 | Live assignment LEDs (#27) | No saved value |
| 20 | FREEZE during saves (#25) | No saved value |
| 21 | SEED latch (#13) | 11 (byte 18 high) |
| 22 | Pitch notch indication (#7) | No saved value |
| 23 | TIME modulation (#141) | 12 (byte 19 low) |

Fields 13–19 are unallocated in v1. Fields 10–12 are permanent from this
release.
The profile table in `beads/profile.cc` owns identities and allowed controls.
Missing or invalid profiles use the release layout and lock settings writes;
the boot guard's own recoveries still run from storage state. SEED at boot
with a valid profile resets to its defaults, preserving calibration.

### Legacy and runtime settings layout

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
| 10 | `padding[0]` | Drizzlet limiter: 0 = Stock, 0xA1 = Fast, 0xA2 = Off; everything else decodes to Stock <!-- beads/settings.h:74-85 --> |
| 11 | `padding[1]` | Synth start: 0xA1 = Manual; 0 and all unknown values = Automatic (Stock); canonicalized on load/save |
| 12 | `padding[2]` | Pitch: 0 Off; 0xA1 Chromatic; 0xA2–0xA9 Fifths, Major, Minor, Major pentatonic, Minor pentatonic, Pelog, Bhairav, Shri; unknown values read Off |
| 13 | `padding[3]` | Reverb character: 0xA2 Clouds-style; 0 and every other value Stock. 0xA1 reserved for future Grains only; canonicalized on load/save |
| 14 | `padding[4]` | Stereo width: 0 Stock; 1..255 enabled, zero to full displacement |
| 15 | `padding[5]` | Delay mode: 0 Auto (Stock), 0xA1 Disabled, 0xA2 Wide/smooth filter; unknown values read Auto (Stock) |
| 16 | `padding[6]` | ASSIGN extra destination: 0 off, 1 WIDTH, 2 DIFFUSION; other values read off; stock destination stays FEEDBACK |
| 17 | `padding[7]` | Grain diffusion: 0 follows the reverb character (none with Stock, 0.7 with Clouds-style); 1..255 = amount 0..1 set with [A]+REVERB |
| 18 | `frozen_feedback` | Frozen feedback: 0xA1 = Evolving; 0 and all unknown values = Stock; canonicalized on load/save |
| 19 | `attacks` | Attacks: 0xA1 = Snap, 0xA2 = Snap and trigger; 0 and all unknown values = Off; canonicalized on load/save |
| 20 | `engine` | Processor: 0xA1 = Spectral; 0 and all unknown values = Granular (Stock); canonicalized on load/save |
| 21 | `synth_engine` | Synth: 0 Beads (Stock), 0xA1 Plaits; unknown values read Beads (Stock); canonicalized on load/save |
| 22 | `random_intervals` | Random intervals: 0xA1 = Fifths and octaves; 0 and all unknown values = Free (Stock); canonicalized on load/save |
| 23 | `padding[0]` | RESERVED: variant ID. 0 = main Drizzlet or stock; zeroed on every save |

Stock (2021 and 2024 releases) keeps byte 9 and shows the half-bright [A] cue, but plays grains polyphonically. <!-- stock:beads/ui.cc:191; stock:beads/settings.cc:94-96; stock:beads/beads.cc:203-207 -->

Stock loads and checksums the whole State, ignores padding semantically, then clears all 14 padding bytes on SaveState, which appends the full State (or rewrites the sector when full). Saving on stock loses Drizzlet options (wavetable synth returns to Automatic (Stock)). Drizzlet canonicalizes bytes 10–16 and 18–21 on load/save, keeps byte 17 and clears 22 and 23. <!-- stock:beads/settings.cc:75-83,94-96; stmlib/system/sector_storage_h7xx.h:133-141,168-209; beads/settings.cc:84,96-99 -->
Keep size, tags, offsets and checksum format compatible: records require matching tag and `sizeof(T)`, with an 8-byte header, 32-byte flash-word rounding, and latest valid sequential State loading. No general migration exists here. <!-- stmlib/system/sector_storage_h7xx.h:86-99,106-123,168-175,213-233 -->

Damaged storage (boot guard). Bank 2 is read with faults masked and the ECC error flag checked (`beads/drivers/flash.h`), so an uncorrectable word no longer crashes the module. At power-up the settings sector is read as stock reads it: the CALI record, then STAT records up to the first that fails its checks or reads with an uncorrectable error, as an interrupted save leaves it; nothing after that one is read, and the next save rewrites the sector. A CALI that is unreadable or fails its checks in a sector that isn't blank, or reads clean but is more than 25% from the factory values, is restored from the mirror if the mirror reads clean, passes its checks, and is within 25%, keeping the states that loaded; an out-of-range CALI with no such mirror loads as stock loads it. A blank sector takes the calibration from the mirror (a power cut after an erase); with a blank mirror it is a new module (factory test). A valid CALI with no valid STAT keeps the calibration, resets the state and never starts the factory test. Only a lost CALI with no usable mirror, or a blank sector with a mirror that isn't blank but holds no CALI, is damage: every bank-2 erase and program is locked and the module plays on the factory calibration and default settings. Only SEED at power-up writes then: the mirror first, then the sector, read back as the firmware loads it; damaged frozen-buffer sectors are erased after a successful repair. A repaired sector is in the stock format. Erases name the sector within bank 2 (settings 7, mirror 6, buffer 0-5) and are checked blank, masked, before anything is written. Each erase and program first checks the programmable voltage detector (level 6, 2.69-2.83 V falling) and is refused while it shows VDD low: a state save is skipped, and any other refused operation fails like a flash error, so a save cut short loads as a power cut at that point would. The detector is set at each start-up; no option byte is written. <!-- beads/settings_storage.h; beads/settings.cc; beads/background_buffer_backup.cc; beads/drivers/flash.h; beads/test/storage_test.cc -->

| Flash area | TAKEN | FREE / limit |
|---|---|---|
| Bootloader, 0x08000000–0x0801FFFF | Reserved 128 KiB sector; updater starts app at 0x08020000 | Not free app space <!-- beads/bootloader/bootloader.cc:53,266-280 --> |
| CALI + STAT, 0x081E0000–0x081FFFFF | One 128 KiB sector; CALI payload 120 B (6×8 B calibration + 72 B padding), record 128 B; STAT payload 24 B, record 32 B | Append space is owned by storage, not a new feature area; CALI padding[0–71] FREE format bytes, but cleared on save; change clearing before use <!-- beads/settings.h:40-61,101; beads/drivers/cv_adc.h:45-52; beads/settings.cc:88-99; stmlib/system/sector_storage_h7xx.h:58-59,213-224 --> |
| FRZZ, 0x08100000–0x081BFFFF | Intended reservation: six 128 KiB sectors; 32 B header word + 768,000 B audio | 18,400 B tail reserved with backup; header layout below <!-- beads/background_buffer_backup.cc; beads/background_buffer_backup.h; beads/dsp/processor.h:51; stmlib/system/sector_storage_h7xx.h:51-57 --> |
| Calibration mirror, 0x081C0000–0x081DFFFF | TAKEN by Drizzlet: a CALI record, as in the settings sector, written before each settings-sector erase when missing or different. Stock ignores it | Restores the calibration when the settings sector is blank <!-- beads/settings_storage.h --> |
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

Buffer trim and monophonic grains use Mutable Instruments’ original gestures and are always enabled. Automatic (Stock) input mono/stereo detection remains active.
`PROFILE_INTERRUPT` and bootloader `ENABLE_ERROR_DETECTION` are off; their debug-pin/error-check paths are excluded. `ENABLE_HARDWARE_24K`, `ENABLE_BANDWIDTH_CONTROL` and bootloader `ENABLE_ERASE_BUTTON_COMBO` are ON; their alternate branches are excluded. `TEST` hooks belong to host tests. <!-- beads/beads.cc:74,109-111,338-340; beads/bootloader/bootloader.cc:27-28,190-264; beads/dsp/processor.h:46,79-92; beads/dsp/multitap_delay.h:38,90; beads/ui.cc:546-549 -->
`WasteCPUCycles(16)` is always called in main, including wavetable synth and spectral modes: it renders a dummy vintage-digital grain workload with memory accesses, discarding output. In spectral mode the main loop also computes the frames (`Processor::Buffer()`), one per channel per hop. Main-loop occupancy is not audio CPU headroom. <!-- beads/beads.cc:403-410; beads/dsp/processor.h:150-153; beads/dsp/granular_processor.h:116-147 -->

## Budgets

Earlier combined pitch-scale and reverb build: 273,456 B of app flash, excluding the later trim changes. Off matches Stock audio in emulator fixtures; instruction counts vary with code generation.

Historical: Drizzlet CI run `38491527444`, commit `0ca3a69`, GCC 4.9.3, before the GCC 9 app build and this cycle's features; remeasure before allocating this headroom. Values are bytes from `app.elf` sections and the link map, not the debug ELF file size. <!-- measurement: github.com/insolentconstruct/eurorack/actions/runs/38491527444; app.elf SHA256 f4b9d4903b63f26ba40cc5bfbad216cd6b47a321f950276d576aeb3dd2b49efb; objects/beads/link.txt -->

| Region | TAKEN | FREE / qualification |
|---|---|---|
| App flash before FRZZ (896 KiB) | 271,292: vectors 664 + text/rodata 269,432 + exidx 8 + data load 1,188 | 646,212; app ends 0x080623BC. Linker alone reports 1,694,788 free, but includes storage areas above <!-- measurement: CI 38491527444 app.elf sections, beads.map:2682,3664-3716; stmlib/linker_scripts/stm32h7xx_flash_application_large.ld:36 --> |
| DTCM (128 KiB) | `.data` 1,188 + `.bss` 31,508; minimum heap/stack reservation 1,536 | 96,840 after reservation; actual stack grows from 0x20020000 and must also fit; no Drizzlet high-water measurement <!-- measurement: CI 38491527444 app.elf sections 4,5,10,12; linker script:29-32,170-178 --> |
| ITCM (64 KiB) | 0 | 65,536 for explicitly placed code; adds flash load bytes <!-- measurement: CI 38491527444 app.elf section 11; linker script:37,152-162 --> |
| D1 / D2 / D3 RAM | 524,288 delay / 262,144 delay / 65,536 reverb | 0 / 0 / 0 linker space; not interchangeable with DMA or TCM <!-- measurement: CI 38491527444 app.elf sections 7-9; beads/beads.cc:54-56; linker script:39-42 --> |
| D2 DMA RAM (32 KiB) | 1,216 | 31,552 static space; preserve uncached DMA placement <!-- measurement: CI 38491527444 app.elf section 6; linker script:41 --> |

The GCC 9 Clouds-style build uses 270,808 app flash bytes and 35,700 DTCM
`.bss` bytes, plus 1,188 `.data` bytes and the 1,536-byte minimum heap/stack
reservation. D1/D2/D3 and DMA reservations are unchanged. Actual module stack
high-water remains unverified; module callback timing is in TESTING.md.

Audio callback period is 250 µs (4,000 blocks/s): nominal budget 120,000 cycles at 480 MHz, before interrupt overhead. The makefile's 400 MHz `F_CPU` is not what the clock setup programs. Module timing, whole callback: delay mode 60% of budget; heaviest grain case with default options 85–86%; Clouds-style 88.1%; wavetable synth mode 90.5%; no underruns (TESTING.md). <!-- beads/beads.cc:50,295; beads/drivers/system.cc:124-148 (8 MHz HSE, PLLM 1, PLLN 120, PLLP 2); beads/makefile:27-28 -->
The dummy workload keeps the main loop busy, so main-loop activity is not idle time. Synth mode leaves 9.5% of the budget; time dense stereo grains, synth, delay transitions, backup and interrupt overlap before budgeting new DSP. <!-- beads/beads.cc:403-410 -->

## Design aims

Mutable Instruments built Beads as a real-time granular texture tool around one model: "a fixed-length tape loop. This is not a sampler." The panel should say what the module does ("I want to keep this module WYSIWYG"), and the stock firmware was to get "no further additional feature that will require a button combo". Drizzlet adds some things Mutable Instruments declined for stock (diffusion, scales, trim, mono grains), so a new feature is judged by these tests instead:

- It shapes the sound live (texture), not edits or files it (studio).
- It is off by default and hard to reach by accident.
- One manual paragraph explains it; no hidden state the panel can't show.
- Patching can't already do it.
- CPU: the worst measured block stays at or under 90% of the budget. This is a working limit; the real one comes from timing the click recipe (#1) on the 2021 and 2024 firmware against the vanilla source.

Prefer, in order: no control (a better default or an indicator), a new value in an existing option, a gesture, then a new option replacing another (the page is full once the planned options land).

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
  [Saved data](#saved-data); byte 23 holds the format marker.
- **One firmware.** Plaits (#50) and spectral mode (#3) fit in the main firmware as options, so no separate variants are planned. Plaits plays in place of the wavetable synth and borrows the first 168 KB of delay bank 0 while synth mode runs; every exit from synth mode clears the banks.
- **The options page stays small.** An option has to earn its place: it fixes something users hear as a defect, or several users ask for it, and patching can't already do it. Cap: four pages with three settings each, including the limiter and wavetable synth; monophonic grains use [A]+SEED outside the page. Other requests stay open as issues.
- **Buffer trim:** grains stay within the trimmed window at their start; release can run about 10 ms past it (the envelope minimum release).
- **Gestures:** [A]+SEED is taken by monophonic grains; [A]+FREEZE = buffer trim; [A]+REVERB = grain diffusion amount (#2); the FREEZE long hold is unassigned; flash sector 14 holds the calibration mirror.

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

## Exact option values

The user guides (OPTIONS.md, README) describe positions and timing in words. The numbers behind them are below; `tools/options_tables_test.py` checks each against the firmware export.

- **Open and close:** hold [M] 3 s. A knob must move more than 10% of its travel to take over. Zone boundaries have a 3% dead band: thirds switch at 1/3 ± 3% and 2/3 ± 3%; halves switch below 47% and above 53%. Pitch scale, Delay mode, Attacks and Limiter use thirds; the other settings use halves. Opening a page shows each stored value for 2.048 s each.
- **Stereo width ([A] + DRY/WET):** Stock below 7%; leaves Stock above 13%, returns below 7%. Enabled width runs from zero spread at 13% to full spread at 100%, saved as 1..255 (0 = Stock). Releasing [A] saves it. The assignable CV input (7) adds to the saved width, clamped to zero and full spread.
- **Saving and LEDs:** changes save after 1 second without movement that changes the setting or passes the movement threshold; pending changes also save on close. The page LED flashes for 100 ms on entry, close and saving. Slow blink is 0.98 Hz; fast blink is 3.91 Hz; two yellow pulses repeat as a 2.048 s group.
- **Delay mode, Wide/smooth filter:** unfiltered zone from 40% to 60% of the SIZE attenurandomizer plus SIZE CV range.
- **Synth start, Manual:** hold [A] 1.5 s to start or stop (the manual says 1 s).
- **Processor, Spectral:** takes effect once the page has closed: a 10 ms fade out, the codec restarts with 250 ms of silence, then a 10 ms fade in. Spectral frames are 4096 samples with a 1024-sample hop at the quality's rate (48, 32 or 24 kHz).
- **Monophonic grains:** the [A] LED is half as bright when on; the recording reserve can dim it further.
- **Frozen-buffer autosave:** a save cut short after erasing doubles the next wait, up to 320 s.

## Off-by-default features

A lightweight requested feature may ship Off by default. Its Off state must
leave the audio unchanged and add no measurable audio-path cost; branches
belong outside sample loops. Measure each feature and interacting controls.
Stock Beads' measured worst case is the ceiling (provisionally 92%). Warn from
91%; block above 92%, for measured and estimated combinations alike. The
ceiling must stay below the earliest measured DAC deadline at every sample
rate. New features still require flash, permanent storage fields, controls and
maintenance.

### Small Beta 2 profile options

IDs 15–23 default to Off. LED brightness Off means full brightness. Brightness,
SEED latch and TIME modulation allow Off, Fixed or Adjustable; an Adjustable
choice can occupy any of the twelve page slots. They have no amount, action,
SEED-stepping or CV-assignment slot. All other new features allow Off or Fixed
only, with no control placement. Omitted entries are Off. The default profile
block is unchanged. The page-facing catalogue is
`beads/build/hygrostat/static/profile-features.json`; costs are unknown until measured.

Brightness uses 100% / 66% / 33% (0 / 1 / 2), selected in thirds of a page knob
when Adjustable or set via Hygrostat when Fixed. SEED latch uses Off / Ignore /
Sync / Add (0 / 1 / 2 / 3). TIME modulation uses Random / Scan (0 / 1).
Fixed-only features use Off / On (0 / 1). Brightness scales the final LED
frame, including options and warning displays, and previews while turning. Off
and Fixed 100% preserve every stock LED value. At 66% and 33%, RGB white also
receives the half-intensity balance correction. UI priority is options and
storage indications, manual meters, boot cue, delay cue, pitch notch, then
quality/overwrite. Delay cues last 150 ms on both actual transitions. The boot
cue is a 100 ms light-blue flash identifying Drizzlet, followed by 150 ms
light-blue pulses separated by 150 ms gaps. One release constant sets the pulse
count (2 for Beta 2); audio startup never waits for this cue. Notch indication
follows the detune law's unison and octave plateaus and both two-octave knob
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
flashes. Sync free-runs and restarts the internal grain-clock phase on each
SEED edge (meaningful on the periodic side of DENSITY). Add free-runs and fires
an extra grain per edge. The divider is unavailable in every non-Off value.
These choices act only while latched with a cable patched; unpatched
attack-triggered grains keep working. Synth grain clocks use the same choices;
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
