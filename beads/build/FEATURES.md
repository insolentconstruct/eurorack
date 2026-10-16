# Feature allocation

**TL;DR:** which buttons, gestures, settings bytes, flash and CPU Drizzlet already uses (TAKEN) and what is still free (FREE), with the rules for adding options.

The `drizzlet` branch, based on the Mutable Instruments source `d8f4739`. TAKEN means existing behaviour; FREE means no dedicated action or allocation, with the limits below. Planned uses come from the tentative roadmap in [#32](https://github.com/insolentconstruct/eurorack/issues/32); they can change until built. <!-- source: git trees fc549d7, d8f4739 -->
Source comments use repository-relative paths; `stock:` means `d8f4739`; Drizzlet pins stmlib `d18def8` (stock `e3bd7c9`; the stmlib files cited here are identical in both); “linker script” means `stmlib/linker_scripts/stm32h7xx_flash_application_large.ld`, and `manual.md` is the Beads manual. <!-- source: git ls-tree fc549d7 stmlib; git ls-tree d8f4739 stmlib -->

## Buttons and startup

| Gesture | Allocation / action | Saved? |
|---|---|---|
| MEDIA tap | TAKEN: cycle quality downward, 3 → 2 → 1 → 0 | Yes <!-- beads/ui.cc:472-495 --> |
| MEDIA hold, 1.5 s | TAKEN: with no input, FREEZE off and On request, toggle self-play; otherwise restart automatic gain and clear manual gain | Gain only; manual says 1 s <!-- beads/ui.cc:48-50,478-484; manual.md:44-45 --> |
| MEDIA + feedback knob | TAKEN: manual input gain; release saves. Planned: manual gain improvements (#19) | Yes <!-- beads/ui.cc:174-177,491-495 --> |
| FREEZE tap | TAKEN: toggle freeze | Buffer/state save starts after 10 s (with interruption backoff below) if autosave enabled, outside synth mode <!-- beads/ui.cc:402-404; beads/background_buffer_backup.cc:144-161; beads/beads.cc:406-408 --> |
| FREEZE hold | FREE dedicated long action; tap already toggles, 4 s event has no extra action | — <!-- beads/ui.cc:48-50,402-404,461-520 --> |
| SEED press / release | TAKEN: unlatch, then generate while held; release stops | No; starts latched at boot <!-- beads/ui.cc:81-83,406-411,463-469 --> |
| SEED hold, 4 s | TAKEN: latch generation | No <!-- beads/ui.cc:48-50,463-469; manual.md:59 --> |
| ASSIGN tap | TAKEN: cycle feedback / dry/wet / reverb CV destination | Yes <!-- beads/ui.cc:499-515 --> |
| ASSIGN + mix knobs | TAKEN: set three independent CV amounts; release saves | Yes <!-- beads/ui.cc:178-180,499-515 --> |
| ASSIGN alone, 3 s | TAKEN by Drizzlet: options entry | Save gesture below <!-- beads/options.h:125-140; beads/build/README.md:23-30 --> |
| Hold SEED, press FREEZE | TAKEN: latch SEED; consumes both releases | No <!-- beads/ui.cc:320-324; manual.md:59 --> |
| Hold ASSIGN, press SEED | TAKEN: toggle R output between audio and grain gate | Yes <!-- beads/ui.cc:331-339; manual.md:138 --> |
| Hold ASSIGN, press FREEZE | TAKEN: toggle buffer autosave; briefly display status | Yes <!-- beads/ui.cc:346-355,560-562 --> |
| ASSIGN + MEDIA, either order | WARNING: calibration entry; do not allocate | Calibration can be overwritten <!-- beads/ui.cc:390-398,428-453 --> |
| Hold MEDIA, press SEED | TAKEN: toggle monophonic grains; consumes both releases; MEDIA first | Yes <!-- beads/ui.cc:358-369 --> |
| MEDIA + FREEZE | FREE dedicated action in this build; reserved by disabled trim feature. Planned for alpha 3: buffer trim (#31), once a saved buffer keeps its trimmed length | Would trim/unfreeze, not save directly <!-- beads/ui.cc:372-384; beads/ui.h:44 --> |
| Power on holding FREEZE | TAKEN: audio updater; FREEZE release retries after an error; TIME sets update gain | Writes firmware <!-- beads/bootloader/bootloader.cc:76-78,160-162,315,377-381; manual.md:247-253 --> |
| Power on holding FREEZE + ASSIGN | WARNING: erase app sectors 1–7, then receive update; enabled | Erase chord keeps boot/settings sectors; reception has no upper bound <!-- beads/bootloader/bootloader.cc:28,266-280,317-322 --> |
| Power on holding SEED | WARNING: reset State to defaults, including limiter; retain valid calibration | Rewrites settings sector <!-- beads/beads.cc:329-330; beads/settings.cc:60-76; stmlib/system/sector_storage_h7xx.h:126-130 --> |
| Other power-on chords | No dedicated action; inherit FREEZE/SEED checks and may reach normal combos after startup | Not safe unused gestures <!-- beads/bootloader/bootloader.cc:315-322; beads/beads.cc:319-345; beads/ui.cc:315-425 --> |

All six pairs are covered above. Combo checks use live debounced state; event order is FREEZE, SEED, MEDIA, ASSIGN, but simultaneous presses need not produce a combo. SEED+FREEZE takes priority over ASSIGN+FREEZE; extra-button chords are not free slots. <!-- beads/drivers/switches.h:40-45,62-67; beads/ui.cc:140-168,315-399 -->
The manual agrees on the main gestures; autosave toggle and erase/reset chords are source-only here. Use source timing for MEDIA (1.5 s versus the manual's 1 s). <!-- manual.md:44-49,59,132,138,247-253; beads/ui.cc:48-50,346-355; beads/bootloader/bootloader.cc:317-322; beads/beads.cc:329 -->

## Drizzlet options

Enter with ASSIGN alone for 3 s, mix knobs still, then release. Opens on the dry/wet category; tap ASSIGN cycles three categories (feedback/dry-wet/reverb LEDs); in each, FEEDBACK, DRY/WET and REVERB are its three settings, each moved >10% then selected by zones (limiter thirds, self-play halves). Hold ASSIGN 3 s, or 5 s idle, saves a change and exits; another button cancels. Entry needs normal mode and 20 ms with all buttons up. <!-- beads/options.h:63-140; beads/build/README.md:23-30 -->

| Resource | TAKEN | FREE / reserved |
|---|---|---|
| Feedback LED category, FEEDBACK | Self-play: lower half Automatic (stock), upper half On request; midpoint hysteresis ±3% | Steady cyan / one magenta pulse per 1.2 s |
| Feedback LED category, DRY/WET and REVERB | Reserved | Planned: DRY/WET delay mode (#26, #9); REVERB stereo width (#5) |
| Dry/wet LED category, DRY/WET | Limiter (`kLimiterPage`); left/middle/right thirds = Stock/Fast/Off | No spare zone <!-- beads/options.h:41,114,139; beads/ui.h:69-73 --> |
| Dry/wet LED category, FEEDBACK and REVERB | Reserved settings, no setter (knobs locked during editing) | Planned: FEEDBACK mix law (#21, #48); REVERB reverb character, Clouds-style with grain diffusion (#20, #22, #2) <!-- beads/options.h:90-117; beads/ui.cc:128-131 --> |
| Reverb LED category, all three knobs | Reserved category, no setter | Planned: FEEDBACK pitch scale (#6, #8); DRY/WET LED brightness (#40); REVERB grain starts on recorded attacks (#58, #47) <!-- beads/options.h:90-117; beads/build/README.md --> |
| Assignment LEDs | All pulse at entry; only selected category pulses when editing | No extra category LED <!-- beads/ui.cc:283-289 --> |
| MEDIA RGB | Stock steady green; Fast one blue pulse; Off two amber pulses per 1.2 s | Other colours/patterns unassigned in options only; Feedback shows self-play: Automatic steady cyan, On request one magenta pulse; reserved categories show limiter <!-- beads/ui.cc:290-299 --> |
| Mix knobs | All three normal values locked while editing; FEEDBACK or DRY/WET selects; smooth catch-up after movement/exit | No untouched mix-knob slot to assume <!-- beads/ui.cc:123-134; beads/pot_controller.h:72-93,106-140 --> |

## Saved settings

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
| 11 | `padding[1]` | Self-play: 0xA1 = On request; 0 and all unknown values = Automatic; canonicalized on load/save |
| 12–22, individually | `padding[2]` … `padding[12]` | FREE; zeroed on every save |
| 23 | `padding[13]` | RESERVED: variant ID. 0 = main Drizzlet or stock; zeroed on every save |

Stock (2021 and 2024 releases) keeps byte 9 and shows the half-bright MEDIA cue, but plays grains polyphonically. Turn monophonic grains off before downgrading. Drizzlet uses byte 9 without changing the settings layout. <!-- stock:beads/ui.cc:191; stock:beads/settings.cc:94-96; stock:beads/beads.cc:203-207 -->

Stock loads/checksums the whole State, ignores padding semantically, then clears all 14 padding bytes on SaveState, which appends the full State (or rewrites the sector when full). Returning to stock and saving loses Drizzlet options. Drizzlet canonicalizes bytes 10 and 11 on load/save. Stock ignores byte 11; a downgrade-and-save resets self-play to Automatic. Bytes 12–23 are cleared on save. <!-- stock:beads/settings.cc:75-83,94-96; stmlib/system/sector_storage_h7xx.h:133-141,168-209; beads/settings.cc:84,96-99 -->
Keep size, tags, offsets and checksum format compatible: records require matching tag and `sizeof(T)`, with an 8-byte header, 32-byte flash-word rounding, and latest valid sequential State loading. No general migration exists here. <!-- stmlib/system/sector_storage_h7xx.h:86-99,106-123,168-175,213-233 -->

| Flash area | TAKEN | FREE / limit |
|---|---|---|
| Bootloader, 0x08000000–0x0801FFFF | Reserved 128 KiB sector; updater starts app at 0x08020000 | Not free app space <!-- beads/bootloader/bootloader.cc:53,266-280 --> |
| CALI + STAT, 0x081E0000–0x081FFFFF | One 128 KiB sector; CALI payload 120 B (6×8 B calibration + 72 B padding), record 128 B; STAT payload 24 B, record 32 B | Append space is owned by storage, not a new feature area; CALI padding[0–71] FREE format bytes, but cleared on save; change clearing before use <!-- beads/settings.h:40-61,101; beads/drivers/cv_adc.h:45-52; beads/settings.cc:88-99; stmlib/system/sector_storage_h7xx.h:58-59,213-224 --> |
| FRZZ, 0x08100000–0x081BFFFF | Intended reservation: six 128 KiB sectors; 32 B header word + 768,000 B audio | 18,400 B tail reserved with backup; header layout below <!-- beads/background_buffer_backup.cc; beads/background_buffer_backup.h; beads/dsp/processor.h:51; stmlib/system/sector_storage_h7xx.h:51-57 --> |
| 0x081C0000–0x081DFFFF | No stock storage owner | FREE 128 KiB sector by current code; linker permits app here, so reserve explicitly before using <!-- beads/settings.h:101; beads/background_buffer_backup.cc:157; stmlib/linker_scripts/stm32h7xx_flash_application_large.ld:36; stmlib/system/sector_storage_h7xx.h:57-58 --> |

Frozen-buffer backup header: one 32-byte flash word at 0x08100000, little-endian, followed by the 768,000 audio bytes. Stock firmware reads bytes 0–9 only, so stock and Drizzlet load each other's backups.

| Bytes | Field | Rule |
|---|---|---|
| 0–3 | `tag` | `FRZZ` (0x5a5a5246), as stock |
| 4–7 | `write_ptr` | Signed frame index of the next write, as stock; must be below the buffer length: 384,000 / 256,000 / 256,000 / 192,000 stereo frames for media 0–3, twice that in mono |
| 8, 9 | `media`, `mono` | As stock; 0–3 and 0–1 |
| 10–11 | `padding` | Zero |
| 12–15 | `crc` | stmlib `crc32` (CRC-32/ISO-HDLC) of the audio, continued over this header with `crc` = 0 |
| 16–19 | `crc_tag` | `CRC1` (0x31435243): `crc` is valid. Stock backups hold leftover stack bytes in 10–31, so without this tag a backup loads as stock did, after the range checks |
| 20–31 | `reserved` | Zero. A later release may use a byte only if older firmware can ignore it safely; anything else needs a new `crc_tag` |

Autosave normally starts after FREEZE has been on for 10 seconds. A save cut
short after erasing doubles the next wait, up to 320 seconds, until a save
completes or FREEZE has been off for a minute; power-up starts at 10 seconds.
The thaw reset is checked only outside synth mode (backup processing is skipped there): if FREEZE is released during synth mode, the minute starts when synth mode ends.
Backoff is RAM-only and runs in the main loop.

Saving erases the header's sector first and writes the header last, after the audio, so a header means a complete backup; a failed erase or program abandons the save. Loading checks the ranges, then the CRC when tagged, before touching the processor; any failure starts with an empty buffer. Known limit: power lost while a flash word is being programmed can leave an ECC error that faults when read, as in stock. <!-- beads/background_buffer_backup.h; beads/background_buffer_backup.cc; beads/test/backup_test.cc -->

Updater reception has no end-address guard; extending app/update files can overwrite FRZZ or settings. Reserve an update boundary as well as a linker boundary before assigning new flash storage. <!-- beads/bootloader/bootloader.cc:234-259,291,345-355 -->

## Panel I/O

| Resource | Granular / delay | Synth / other ownership |
|---|---|---|
| DENSITY knob + CV | Grain rate; noon stops latched grains; clock divider/probability; unlatched noon triggers single grains. Delay: base time or clock/tap ratio | Synth grain rate, with density CV also used for exponential FM at high rates; TAKEN <!-- manual.md:63-77,150-162; beads/dsp/synth.cc:70-81 --> |
| TIME, PITCH, SIZE, SHAPE knobs + CV | Position, transposition, duration/direction, envelope. SIZE fully CW enters delay; TIME then delay multiple/frozen slice, PITCH pitch-shift, SHAPE tap envelopes | Synth TIME wave position, PITCH root/grain pitch, SIZE duration, SHAPE envelope; all TAKEN <!-- manual.md:87-97,144-174; beads/dsp/wavetable_voice.h:84-94; beads/dsp/wavetable_granular_player.h:65-68; beads/dsp/delay_controller.cc:173-175,293-294,346 --> |
| Four attenurandomizers | Patched: CW CV amount, CCW CV-controlled randomness; unpatched: peaky/uniform randomness. Delay: slow random LFOs; SIZE amount + SIZE CV also set bandwidth | Synth PITCH CV always 1 V/oct root; PITCH attenurandomizer remains grain randomness; TAKEN <!-- manual.md:102-106,174,192-194; beads/dsp/granular_processor.h:277-282 --> |
| Feedback, dry/wet, reverb knobs; ASSIGN CV | Feedback, audio blend, reverb; destinations 0/1/2 or custom three-way amounts | Synth feedback selects 12 banks (manual says 8), dry/wet oscillator/grains; options borrow all three mix knobs (FEEDBACK and DRY/WET in use); TAKEN <!-- beads/beads.cc:219-234; beads/dsp/synth.h:43; beads/dsp/synth.cc:52,83,98-108; manual.md:182-184; beads/options.h:94-113 --> |
| FREEZE + SEED gate inputs | Record stop; grain clock/gate, delay clock. Button freeze ORs with gate; SEED input delayed 2.5 ms | Synth freeze stops new grains/holds envelopes; SEED still drives grains; TAKEN <!-- beads/beads.cc:132-151; manual.md:158,186; beads/dsp/synth.cc:73-81 --> |
| L/R audio in, L/R audio out | Input normalization selects mono/stereo; output always stereo, unpatched R sums to L; optional R grain gate replaces audio | Automatic self-play: both inputs empty, FREEZE off and quiet output for 10 s selects synth; On request: a MEDIA hold instead; patching an input exits once FREEZE is off; R gate costs stereo output; no spare dedicated gate output <!-- manual.md:36-40,134-138; beads/beads.cc:360-394; beads/dsp/synth.cc:138-142 --> |
| MEDIA RGB LED | 0 Scorched cassette red/pink, 1 Sunny tape amber, 2 Cold digital teal, 3 Bright digital white; dims with overwrite reserve; manual gain overrides with meter | Same palette marks synth resolutions 4/4, 16/6, 7/7, 16/16 bits (continuous/grains); options override colours; TAKEN <!-- beads/ui.cc:190-195,238-245; beads/dsp/processor.h:58-63; beads/dsp/synth.cc:84-85 --> |
| FREEZE / SEED LEDs | Freeze status (half-bright during backup); SEED gate/held or breathing latch. Autosave display FREEZE dim=disabled, brighter=enabled | Same ownership in synth; no spare status LED <!-- beads/ui.cc:247-262 --> |
| Three assignment LEDs / input-level LED | Destination or custom amounts / input VU, blinking during gain adjustment | Options page markers / VU retained; TAKEN <!-- beads/ui.cc:264-272,283-306 --> |
| Calibration, panic, factory test, updater LEDs | Calibration: channel/step + pulsing assignment LEDs; panic blinks all; factory test fades all | Updater alternates FREEZE/SEED, MEDIA level meter; error blinks all, writing lights all. These are reserved overrides <!-- beads/ui.cc:207-231,276-280,301-307; beads/bootloader/bootloader.cc:99-148 --> |

No physical knob, CV jack, gate jack or LED is unallocated. New normal-mode controls need a deliberate shared gesture; only MEDIA is RGB, and R is already shared with gate output. <!-- beads/beads.cc:178-234; beads/drivers/leds.h:42-52; manual.md:138 -->

## Compiled out

`ENABLE_ADVANCED_FEATURES` is commented out: buffer trim has no active button entry and MEDIA+FREEZE stays reserved for it. Monophonic grains (MEDIA+SEED) are enabled independently of that flag. Automatic input mono/stereo detection remains active. <!-- beads/ui.h:44; beads/ui.cc:358-384; beads/beads.cc:203-207,385-394 -->
`PROFILE_INTERRUPT` and bootloader `ENABLE_ERROR_DETECTION` are off; their debug-pin/error-check paths are excluded. `ENABLE_HARDWARE_24K`, `ENABLE_BANDWIDTH_CONTROL` and bootloader `ENABLE_ERASE_BUTTON_COMBO` are ON; their alternate branches are excluded. `TEST` hooks belong to host tests. <!-- beads/beads.cc:74,109-111,338-340; beads/bootloader/bootloader.cc:27-28,190-264; beads/dsp/processor.h:46,79-92; beads/dsp/multitap_delay.h:38,90; beads/ui.cc:546-549 -->
`WasteCPUCycles(16)` is always called in main, including synth mode: it renders a dummy vintage-digital grain workload with memory accesses, discarding output. Main-loop occupancy is not audio CPU headroom. <!-- beads/beads.cc:403-410; beads/dsp/processor.h:150-153; beads/dsp/granular_processor.h:116-147 -->

## Budgets

Historical: Drizzlet CI run `38491527444`, commit `0ca3a69`, GCC 4.9.3, before the GCC 9 app build and this cycle's features; remeasure before allocating this headroom. Values are bytes from `app.elf` sections and the link map, not the debug ELF file size. <!-- measurement: github.com/insolentconstruct/eurorack/actions/runs/38491527444; app.elf SHA256 f4b9d4903b63f26ba40cc5bfbad216cd6b47a321f950276d576aeb3dd2b49efb; objects/beads/link.txt -->

| Region | TAKEN | FREE / qualification |
|---|---|---|
| App flash before FRZZ (896 KiB) | 271,292: vectors 664 + text/rodata 269,432 + exidx 8 + data load 1,188 | 646,212; app ends 0x080623BC. Linker alone reports 1,694,788 free, but includes storage areas above <!-- measurement: CI 38491527444 app.elf sections, beads.map:2682,3664-3716; stmlib/linker_scripts/stm32h7xx_flash_application_large.ld:36 --> |
| DTCM (128 KiB) | `.data` 1,188 + `.bss` 31,508; minimum heap/stack reservation 1,536 | 96,840 after reservation; actual stack grows from 0x20020000 and must also fit; no Drizzlet high-water measurement <!-- measurement: CI 38491527444 app.elf sections 4,5,10,12; linker script:29-32,170-178 --> |
| ITCM (64 KiB) | 0 | 65,536 for explicitly placed code; adds flash load bytes <!-- measurement: CI 38491527444 app.elf section 11; linker script:37,152-162 --> |
| D1 / D2 / D3 RAM | 524,288 delay / 262,144 delay / 65,536 reverb | 0 / 0 / 0 linker space; not interchangeable with DMA or TCM <!-- measurement: CI 38491527444 app.elf sections 7-9; beads/beads.cc:54-56; linker script:39-42 --> |
| D2 DMA RAM (32 KiB) | 1,216 | 31,552 static space; preserve uncached DMA placement <!-- measurement: CI 38491527444 app.elf section 6; linker script:41 --> |

Audio callback period is 250 µs (4,000 blocks/s): nominal budget 120,000 cycles at 480 MHz, before interrupt overhead. The makefile's 400 MHz `F_CPU` is not what the clock setup programs. No worst-case block timing has been measured for Drizzlet yet. <!-- beads/beads.cc:50,295; beads/drivers/system.cc:124-148 (8 MHz HSE, PLLM 1, PLLN 120, PLLP 2); beads/makefile:27-28 -->
The dummy workload keeps the main loop busy, so main-loop activity is not idle time. Treat CPU headroom as UNKNOWN: time dense stereo grains, synth, delay transitions, backup and interrupt overlap before budgeting new DSP. <!-- beads/beads.cc:403-410 -->

## Planning rules

- Allocate a reserved category knob before borrowing another normal gesture; record category, knob, colours and saved bytes together. <!-- derived: beads/options.h:90-113; beads/ui.cc:283-299; beads/settings.h:60 -->
- Give zero/unknown settings the stock default; preserve the 24-byte State layout and accept that stock saves erase extension bytes. <!-- derived: beads/settings.h:74-85; stock:beads/settings.cc:94-96; stmlib/system/sector_storage_h7xx.h:168-175 -->
- Change load/save clearing when assigning padding; do not claim byte 9 or flash journal/backup tails as free. <!-- derived: beads/settings.cc:84,96-99; beads/settings.h:59; beads/background_buffer_backup.cc:95-116,157 -->
- Check button order, long holds, startup and disabled-feature collisions; keep calibration/reset/update chords reserved. <!-- derived: beads/ui.cc:315-399; beads/bootloader/bootloader.cc:315-322; beads/beads.cc:329 -->
- Keep new options stock by default; account for both processor and synth, and restore borrowed controls/LEDs after editing. <!-- beads/build/STYLE.md:42; derived: beads/beads.cc:244-261; beads/ui.cc:123-134,198-308 -->
- Reserve flash by erase sector; measure CPU and stack on the actual build before spending static size headroom. <!-- derived: stmlib/system/sector_storage_h7xx.h:43-59,149-164; linker script:29-42; measurement: CI 38491527444 app.elf -->

## Decisions

Agreed 2026-10-14; see the [roadmap](https://github.com/insolentconstruct/eurorack/issues/32).

- **Saved options default to stock.** Byte 11 stores self-play (0xA1 On request, otherwise Automatic). Bytes 12–22 remain free for options. Byte 23 is reserved for a variant ID (0 = main Drizzlet or stock), so options never use it up. The limiter keeps byte 10 as it is (0 Stock, 0xA1 Fast, 0xA2 Off).
- **Big engines ship as separate variants.** Engines that replace the granular engine (Plaits models, a spectral mode) are separate images, installed by `.wav`, each owning its panel. Variants share calibration and the common settings (quality, gain, routing, limiter). A variant that finds another variant's ID in byte 23 keeps the common bytes and resets its own.
- **The options page stays small.** An option has to earn its place: it fixes something users hear as a defect, or several users ask for it, and patching can't already do it. Cap: three categories with three settings each, including the limiter and self-play; monophonic grains use MEDIA+SEED outside the page. Other requests stay open as issues.
- **Gestures:** MEDIA+SEED is taken by monophonic grains; MEDIA+FREEZE is reserved for buffer trim; the FREEZE long hold and flash sector 14 are unassigned.
