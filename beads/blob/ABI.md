# Plaits model blobs: interface

`plaits_blob.h`, `plaits_region.h` and `plaits_libraries.h` are the source of truth. Version 1 is the frozen proof's blob format; its blobs still load. Version 2 is in code since phase 1, libraries and dependencies since phase 2. Since phase 3 the release app (`PLAITS_BLOBS=1`, the default) has no Plaits engine: all 24 stock models are blobs in its default region (`build/tools/build_region.sh`), and the WAV and combo image write app and region together. Since phase 4 Beads' wavetables are the default region's `WAVE` section too. `PLAITS_BLOBS=0` builds Plaits' engines in, as before; no release uses it.

# Version 1 (frozen)

## Blob

- 64-byte header (`PlaitsBlobHeader`), code and constants, the imports (GOT), then a CRC-32. ABI 1 fixes the entry signatures and `plaits::EngineParameters` (24 bytes).
- Position independent: no absolute relocations, no mutable data, no branch through a register except returns. `build_blob.sh` and `blob_pack.py` gate this. `.blob` is flagged writable only because the linker's GOT is; `blob.ld` leaves writable input out of `.blob` (the gate then refuses its section), `build_blob.sh` refuses objects with their own `.got`, `.got.plt` or `.blob` section, and `blob_pack.py` checks that every GOT word the linker resolved is an export.
- `init(state, services, arena, arena_size)` constructs the blob's state object in `state` with placement new and returns a pointer to it. `reset` and `render` take that pointer. The object is not polymorphic (a vtable holds absolute addresses), at most 256 bytes, aligned to at most 8. A port keeps its DSP in a plain class; the compiled-in engine wraps the same class.

## Services: frozen exports

The firmware places these at fixed addresses, after the profile:

| Export | Address | Size | Align | SHA-256 |
|---|---|---:|---:|---|
| services header | 0x08021400 | 64 | 4 | (fields checked) |
| `plaits::lut_sine` | 0x08021440 | 2,564 | 4 | `71fef7c6…62b1eb` |
| `stmlib::lut_pitch_ratio_high` | 0x08021e44 | 1,028 | 4 | `b53783e5…683b28` |
| `stmlib::lut_pitch_ratio_low` | 0x08022248 | 1,028 | 4 | `dbc5c7f5…3182ca` |

- Address, size and alignment: linker asserts in `services_section.ld`; size also by `STATIC_ASSERT` in `plaits_blob.cc`.
- Contents: `PlaitsBlob::CheckServices` hashes each export at boot (about 0.3 ms) and compares it with `kPlaitsBlobExportDigests`. If anything differs, no blob runs.
- A new export or a changed table needs services version 2. Version 1 stays as it is for as long as any release supports version-1 blobs.

## Region

Version 1's single-table region (`DZRG`) was never shipped; version 2's sections replace it.

## Identity

One release covers the app and its approved blobs. A support report or qualification run identifies its configuration by the release number and `app.bin` SHA-256, the profile block's CRC, and version 2's identity record (below).

## Not reserved

No 8 KiB slot for I-cache alignment. Reserve one only if a mixed-model worst-case block (several blobs and compiled-in models switching in one voice set) shows placement costs well above the 0.5-point spread measured for one engine.

# Version 2

Version 2 moves all Plaits content out of the app: the stock 24 models, Palette models and the libraries they share become blobs, and Beads' wavetables become a section of their own. Version 1's rules stay unless a line below changes them.

## IDs

Both tables are permanent data in `beads/build/profile-features.json`, which Hygrostat copies verbatim. A profile or region stores IDs, never positions. An ID the firmware doesn't know makes the profile invalid (factory defaults); it never selects another model.

- **Model IDs** (`plaits_models`, one byte): 0–23 are the stock models in Plaits 1.2 order, as today. 28–97 are Plaits Palette catalogue numbers at its commit `0390557b`. Speech Sounds is 98 and LPC Words 99. 24–27 (Palette's numbers for four stock models) and 255 are never assigned. Later models take the next free ID from 100.
- **Library IDs** (`plaits_libraries`, one byte, 0 = none): 1 Plaits wavetables, 2 chord bank, 3 speech, 4 6-op FM, 5 scale voices, 6 wavetable scale voices (needs 1). A library may depend on another; Hygrostat installs the whole chain. Code used by one engine stays in that engine's blob; 6-op FM is a library anyway, because its unpacked banks are shared by all voices.
- **The stock libraries:** the Plaits wavetables (data: `wav_integrated_waves`, read by Wave terrain, Wavetable and Chords); the chord bank (`ChordBank`'s code, called by Chords, String machine and Chiptune, each keeping its `ChordBank` in its own state); speech (the synths and the decoded word banks); 6-op FM (its `init` unpacks the three banks and compiles the 32 algorithms once into its RAM, `SixOpLibrary`; the model reads both from the context). Its algorithms name each renderer by index (`IndexedAlgorithms`, `RunRenderer`), because Plaits' function pointers would be absolute words and indirect calls. The 6-op model clears its stagger buffer when built: compiled in, the engine's first blocks after a switch read the previous model's leftovers from the shared buffer; the blob starts clean (accepted, owner 2026-11-22). Wave terrain's terrains 5-7 clamp their coordinates to the bank (wave position to 0–62.9999, as Plaits' Wavetable clamps to 6.9999, and sample position to 0–128): compiled in, a path that overshoots read past the table; the blob's output is unchanged wherever the coordinates stay in range (owner 2026-11-22).

## Blob

The header grows to 96 bytes (24 little-endian words). The blob still ends with its CRC-32.

| Word | Model blob | Library blob |
|---:|---|---|
| 0–3 | magic `DZMB`, header size 96, ABI 2, services version 2 | same |
| 4 | kind 1 | kind 2 |
| 5–6 | first model ID, number of consecutive IDs it serves | library ID, 1 |
| 7 | size | size |
| 8–9 | state size, arena size | RAM size, scratch size |
| 10–12 | `init`, `reset`, `render` offsets | `init` offset (0: none), export table offset, export count |
| 13–14 | GOT offset and size | same |
| 15–16 | dependency list offset, count | same |
| 17–19 | flags, OUT gain, AUX gain | 0 |
| 20–23 | reserved, 0 | same |

- **Layout:** code and constants, the GOT, a library's export table (one offset per export), the dependency list, then the CRC-32. Each dependency is 36 bytes: the library's ID and the SHA-256 of the library blob it was built against, so a dependent pins one build of each library. At most 4 dependencies, 16 exports and 8 libraries per region.
- **Model entries:** `init(state, services, arena, arena_size, libraries)`, where `libraries` holds one `PlaitsBlobLibrary` per dependency in the listed order. `reset(object, services, argument)`; the argument is the selected ID minus the first ID (the 6-op bank). `render` as in version 1.
- **On every switch** the voice binds the selected blob, zeroes the declared state, runs `init`, then `reset`. Its first 48 samples of OUT (two Plaits blocks; the voice plays no AUX) then fade in on a squared ramp from 0 (`PlaitsBlobEngine`, outside the blob), so a restart from zeroed filters never clicks; compiled-in engines are not faded. Wavetable's restart transient (19.2 in its first block in the emulator) peaks at 0.68 with it. The arena keeps what the last engine left, as the compiled-in engines' shared buffer does (zeroing it doubled Particle Noise's switch on the Seed3); the emulator poisons it before every switch, so a blob that read it unwritten would differ. A compiled-in engine runs `Init` once at synth entry and only `Reset` on a switch, so it keeps state (filters, particles) that a blob rebuilds. The emulator reports where outputs differ after such a return; the blob's output matches the engine's first selection after power-on. Inharmonic string's strings (`BlobString`, `BlobDelayLine`) leave clearing their delay lines to `reset`, so a build clears them once, as the compiled-in `Reset` does.
- **Library entries:** `init(ram, ram_size, scratch, scratch_size, services, libraries)` runs once at synth entry, outside the audio interrupt, in table order (a library needs only libraries listed before it, so there is no cycle), and returns the library's context. A library has no writable statics; shared state (decoded word banks, unpacked 6-op banks) lives in its declared RAM, zeroed before `init` and owned by the library until the synth exits. Scratch is the first voice's RAM before the voices are built, the same for every library. A data library has no `init`.
- **Library RAM:** delay bank 0 after the six voices, 64 KiB (`kPlaitsLibraryRamSize`): one `PlaitsBlobLibrary` record per library, then each library's RAM, each part on 32 bytes. The stock libraries take 36,864 B (speech) and 18,048 B (6-op). The reader refuses a library whose RAM or scratch the synth cannot give, then its dependents. The bank holds the voices and these libraries while the synth runs; the granular processor and its resonator keep their state elsewhere and get the bank back when the synth exits.
- **Records and calls:** a record holds the context and each export's address (blob plus offset). Dependents read data exports (the Plaits wavetables) directly. They call code exports through functions named `plaits_blob_import_*`, in their own translation unit, each one load from the record in `r0` and a tail call; the export takes the record first and finds its context there. These are the gate's one exception to "no branch through a register": `blob_pack.py` accepts a register branch only inside such a function, only in that shape (`ldr rX, [r0, #4…64]; bx rX`, or GCC 9's `push {rX}; ldr rX, [r0, #k]; mov ip, rX; ldr.w rX, [sp], #4; bx ip` when arguments are on the stack), never entered past its start (each function checked by address, so two local ones of one name are both checked), and only in a blob that lists a library. A word that equals an address inside the blob is refused anywhere but in a library's exported data tables and Thumb code (the assembler's mapping symbols; an address is only ever data).
- **Export table:** a library defines `plaits_blob_exports[]` in section `.blob_exports`, one address per export; the packer turns each into an offset after checking it is one absolute relocation into the blob's code or constants. Models have none.
- **State slot:** 4,352 B, aligned to 8 (`kPlaitsBlobMaxStateSize2`). Compiled in, 6-op FM was 4,292 B; as a blob, with its algorithms in the library, it declares 1,208 B, the largest stock state; Palette's largest is 1,080 B (Dual Sync). The gate and the loader refuse a larger declared state. A voice is the slot and the 16 KiB shared buffer: 21 KiB (28 KiB with the engines compiled in).
- **Still refused:** writable data, libc functions that write `errno` (`sinf`, `tanhf`; a blob copies newlib's source instead, `beads/blob/libm`), and absolute pointer tables. A port turns pointer tables into offsets. `memcpy`, `memmove` and `memset` are copied into the blob from the app's own `libc.a`; the gate refuses any other archive member.
- **Build:** `build_blob.sh` includes `blob_visibility.h` first in every translation unit: the services' exports keep default visibility (reached through the GOT at their fixed addresses), everything declared after is hidden, so a table defined in another of the blob's files is reached PC-relative. `PLAITS_BLOB` is defined: Plaits' word bank then keeps the decoded banks per instance instead of in a static. `-fno-jump-tables` turns a switch into compares (a PC-relative jump table ends in a branch through a register; 6-op FM's LFO had one).

## Synth blobs (kind 3)

A whole synth that is not Plaits: Mutable Instruments' Rings (synth ID 1). `blob_pack.py` gates and packs the kind; the firmware's reader checks it in the `RNGS` section (Region, below), and the default region carries it.

| Word | Synth blob |
|---:|---|
| 0–3 | magic `DZMB`, 96, ABI 2, services version 2 |
| 4 | kind 3 |
| 5–6 | synth ID, 1 |
| 7 | size |
| 8–9 | RAM size (at most 256 KiB), 0 |
| 10–12 | `init`, `configure`, `render` offsets |
| 13–14 | GOT offset and size |
| 15–16 | 0, 0: a synth depends on no library |
| 17–19 | flags 0, control block size, block size |
| 20–23 | reserved, 0 |

- **Entries** (`beads/blob/synths/rings_synth_core.h`): `init(ram, ram_size, services)` builds the synth in RAM the app lends from the start of delay bank 0, zeroed first (Rings' objects start from zeroed `.bss`), and returns its context. `configure(context, target, controls, entry)` applies a target (core, model 0–5, polyphony 1, 2 or 4, FX 0–5) and renders one discarded block on the target's core (both on `entry`), so no reconfiguration reaches the audio interrupt; it refuses anything else and keeps what it had. Both run in the main loop with the codec paused. `render(context, controls, out, aux, size)` runs in the audio interrupt: Rings' strummer, then the target's core, with silent input. The control block is `rings::Patch` then `rings::PerformanceState`, 36 bytes.
- **The app checks** the kind, ID, RAM against what bank 0 can give, the control block size and the block size (12) before `init`.
- **No state slot or arena:** all of the synth's state lives in its RAM. One synth (Plaits with its libraries, or Rings) owns bank 0 at a time; an owner change goes through the synth's fade, a codec pause, zeroing and `init`.
- **Imports:** services version 2 only. Rings reaches `stmlib`'s pitch tables and the shared generator's state through the GOT, so its draws match a compiled-in Rings; its own tables are inside the blob.
- **Source:** vanilla Rings, except the block-size guard (`RINGS_BLOCK_SIZE`, default 24; Beads builds 12) and three tables made `const` under `RINGS_BLOB` (`rings_vanilla.py`, G1). `powf` comes from newlib 3.3.0's own source (`beads/blob/libm`), its error helpers built without the `errno` write; the toolchain's libm `powf` is refused (`neg_rings_errno`).

## Services version 2

Frozen as version 1's, in `services_v2.ld`, `services_section.ld` and `kPlaitsBlobExportDigests`:

| Export | Address | Size |
|---|---|---:|
| version 1's three | as above | |
| `stmlib::Random::rng_state_` | 0x20000000 (DTCM) | 4 |
| `plaits::lut_fold` | 0x0802264c | 2,064 |
| `plaits::lut_fm_frequency_quantizer` | 0x08022e5c | 520 |
| `plaits::lut_4x_downsampler_fir` | 0x08023064 | 16 |
| `plaits::lut_ws_inverse_tan` | 0x08023074 | 514 |
| `plaits::lut_ws_inverse_sin` | 0x08023278 | 514 |
| `plaits::lut_ws_linear` | 0x0802347c | 514 |
| `plaits::lut_ws_bump` | 0x08023680 | 514 |

- The services table (version 2, 11 exports) still fits the 64 bytes at 0x08021400. Tables are hashed at boot; the generator's state is checked by address.
- **The shared generator is a fixed RAM export,** not a function: a blob's GOT holds 0x20000000 and its inlined `Random::GetWord` updates the app's own state, at no cost per draw and with the same numbers as the compiled-in engine. The app places the state there (not loaded) and seeds it with stmlib's 0x21 at boot.
- A version 1 blob may import only version 1's exports; the gate and the loader refuse anything else (`neg_import`).
- **`R_ARM_REL32`** (a text-relative literal, as GCC emits for local tables under `-mpic-data-is-text-relative`) is accepted when the symbol it names lies inside the blob; `neg_rel32` names an export and is refused.

## Region

From the first 4 KiB boundary after the app to 0x08100000. Sections come in a fixed order, `WAVE`, `TERR`, `PLTS`, `RNGS`, each starting on a 32-byte flash word (the H7's ECC unit), so no flash word holds bytes of two sections.

- **Section header** (32 bytes): magic `DZSC`, kind (`WAVE`, `TERR`, `PLTS` or `RNGS` as a four-character code), version, release, size (header to CRC), item count, two reserved words. Then the payload, then a CRC-32 of everything before it. A section is valid on its own, without the profile.
- **Directory, in the profile block (version 3):** byte 14 is the bank count (0 = Plaits' three stock banks, else 1–15) and byte 15 the section count (0–4). After the entries and scale records come the bank table (8 model IDs per bank) and the directory (per section: kind, offset from the region start, size and the section's CRC, 16 bytes). The entry's CRC is the selection: a section written for another install fails it. Every ID must be known (0–23, 28–99) and the start bank (feature 31) below the bank count; a saved bank at or past it takes the start bank. The firmware checks every entry lies inside the region, is aligned, in order and apart before it reads anything; a bad directory faults every section. The synth lock follows from which sections are valid (below), not from a stored field.
- **Without a directory:** the firmware uses the release's own default layout, compiled into the app as fixed offsets and the SHA-256 of each default section (`plaits_catalog.cc`, written by `blob_catalog.py` and checked by `build_blobs_dev.sh`). It never trusts a region offset from anywhere else.
- **Reads:** a section is read only up to its size (a stale tail is never read), its header checked, then its CRC against the directory's (or, on the default layout, its SHA-256 alone), then its content. A blob's own CRC is not recomputed: its SHA-256 on the allowlist covers every byte (each bitwise CRC pass costs 55 cycles/B). The app reads the region at boot with the D-cache off and data bus faults ignored, then check `FLASH_SR1` DBECCERR and the bus fault status around each section, so a double ECC error in an interrupted write faults that section (`ECC`) instead of the boot (not yet exercised on hardware). Seed3: 0.97 M cycles (2.0 ms) for the 7,920 B development region, services check included.
- **`PLTS` payload:** one 16-byte entry per blob (byte kind, byte ID, two zero bytes, then offset from the section start, size and the blob's CRC), libraries first by ascending ID, then models by ascending first ID, at most 128; blobs follow at 32-byte offsets. Every blob is checked on its own first; then every dependency must be an earlier library entry with that ID, itself accepted, whose SHA-256 is the one recorded, or the dependent is refused (`DEPENDENCY`). All or none within the section.
- **`RNGS` payload:** exactly one synth blob (kind 3), at offset 32, filling the payload; the item count is 1. The blob passes `PlaitsBlob::Check` against the allowlist (its header rules, imports, services OK) or the section takes its status; another count or a size that does not fill the payload is `TABLE`, a model or library `KIND`. The reader does not compare the blob with the app: the app checks its ID, RAM, control block and block size before `init` (`PlaitsRegion::synth_blob`, `PlaitsBlobInfo`'s `controls_size` and `block_size`).
- **Faults:** a damaged section never hides another. Plaits may play when every model of every bank is served by a checked blob (`PlaitsVoice::Playable`), so it needs a valid `PLTS`; Beads' synth when `WAVE` is valid. With only one, Synth is forced to it; with neither, the synth is silent and [A] blinks fast until a good install. Profile and State are never rewritten. A `TERR` fault changes no synth: Wave terrain's custom grids are unavailable and its stock terrains play, with no cue (owner 2026-11-21). `PLAITS_BLOBS=0` builds keep Beads' wavetables compiled in.
- **Install order:** Hygrostat writes `WAVE` before `PLTS`, and `RNGS` last.

## Content limits (Beta 4)

Firmware and Hygrostat both refuse content outside these limits.

**`WAVE` (Beads wavetables, the reader in `dsp/wavetable_grain.h` and `dsp/wavetable_voice.h`):**
- Exactly 12 banks of 16 waves, plus one guard row: 193 rows of 260 little-endian `int16`, 100,360 bytes. The bank count is fixed: FEEDBACK's quantizer has 12 steps (`kNumBanks`).
- Each row is one integrated cycle, as `resources/wavetables.py` makes it: two copies of the 256-sample wave, mean removed, normalized to a peak of 1, summed, mean removed again, scaled by 512 (4 × 32768 / 256) and rounded; the row keeps the last 260 samples. So samples 0–3 repeat samples 256–259, which the four-point Hermite reader needs past the cycle's end.
- Row 192 repeats row 191: the last wave of bank 11 interpolates towards it.
- Every sample fits `int16`. The factory table's peak is 26,345, so a new table that would overflow is refused, not clipped. The voices differentiate the data at 1 / (frequency × 131,072).

**Reaching the content:** `PlaitsRegion::waves()` gives a valid `WAVE`'s rows to Beads' synth (`integrated_waves`, set at boot; NULL means it cannot play). A version 2 model with header flag `PLAITS_BLOB_FLAG_TERRAINS` (2) gets one more record in `init`'s `libraries`, after its dependencies: its context is a `PlaitsBlobTerrains` (the grids and their count, NULL and 0 without a valid `TERR`), valid while the synth runs. The loader, the gate and Hygrostat refuse any other model flag, and the flag on a version 1 blob.

**`TERR` (terrain grids for Wave terrain):**
- 1 to 16 grids, each 64 × 64 `int8` in rows of x (4,096 bytes). The value is the sample / 128.
- Same format as Plaits' own user terrain (`LoadUserData`, `TerrainLookup` in `wave_terrain_engine.cc`) and Palette's `WAVE_TERRAIN_CUSTOM`. The reader maps x and y from [−1, 1] to [0, 62] and interpolates bilinearly, so index 63 is read only as the upper neighbour. No guard samples; all 256 values are valid.
- No `TERR` section means the eight stock terrains only, bit-identical to today.
- HARMONICS sweeps the eight stock terrains, then the grids in section order (`8 + count` positions, as Plaits adds its user terrain as a ninth), so a valid `TERR` compresses the stock terrains into a smaller knob range (owner 2026-11-23).

## Identity

The version 1 record grew with every blob, so version 2 keeps digests in flash (the app's allowlist, `kPlaitsApprovedBlobs`) and only statuses in RAM: `PlaitsRegionIdentity`, 208 bytes since `RNGS` (release, services status, directory status, per section status, address, size and CRC for `WAVE`, `TERR`, `PLTS` and `RNGS`, the blob count and one status byte per `PLTS` entry), in `plaits_region` (beads.cc). It is readable over SWD by symbol.
