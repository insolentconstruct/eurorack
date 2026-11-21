# Plaits model blobs: interface

Version 1 is the frozen proof, not linked into the firmware; `plaits_blob.h` is its source of truth. Version 2 is a draft for Beta 4: phase 1 fixes it in code, and nothing in it ships before then.

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

After the app: `PlaitsBlobRegionHeader` (magic, version, release, services version, size, count), one `PlaitsBlobRegionEntry` per blob (model, offset, size, CRC), the blobs verbatim at 32-byte aligned offsets, then a CRC-32 of everything before it.

- `PlaitsBlob::ReadRegion` checks services, then the table (release, bounds, CRC, the profile's selected CRC, ascending non-overlapping entries, one per model), then every blob. One refused blob refuses the whole region.
- Interrupted update: the region CRC fails, so no blob runs. Stale tail: bytes after `size` are never read. Stale region (another install or release): refused by release or by the selected CRC. A selected CRC of 0 means no blobs.

## Identity

One release covers the app and its approved blobs. A support report or qualification run identifies its configuration by:

1. the release number (the tag's date, compiled into the app) and its `app.bin` SHA-256 from the release manifest (Hygrostat's decode checks the app bytes against it);
2. services version and `CheckServices` status;
3. the region CRC, which is also the profile's selection;
4. in order: each blob's model, address, size, status and SHA-256;
5. the profile block's CRC.

`ReadRegion` fills 1 (the number) to 4 in a `PlaitsBlobIdentity` record (408 bytes of RAM), which can be read over SWD by symbol, or later shown by a diagnostic.

## Not reserved

No 8 KiB slot for I-cache alignment. Reserve one only if a mixed-model worst-case block (several blobs and compiled-in models switching in one voice set) shows placement costs well above the 0.5-point spread measured for one engine.

# Version 2 (draft)

Version 2 moves all Plaits content out of the app: the stock 24 models, Palette models and the libraries they share become blobs, and Beads' wavetables become a section of their own. Version 1's rules stay unless a line below changes them.

## IDs

Both tables are permanent data in `beads/build/profile-features.json`, which Hygrostat copies verbatim. A profile or region stores IDs, never positions. An ID the firmware doesn't know makes the profile invalid (factory defaults); it never selects another model.

- **Model IDs** (`plaits_models`, one byte): 0–23 are the stock models in Plaits 1.2 order, as today. 28–97 are Plaits Palette catalogue numbers at its commit `0390557b`. Speech Sounds is 98 and LPC Words 99. 24–27 (Palette's numbers for four stock models) and 255 are never assigned. Later models take the next free ID from 100.
- **Library IDs** (`plaits_libraries`, one byte, 0 = none): 1 Plaits wavetables, 2 chord bank, 3 speech, 4 6-op FM, 5 scale voices, 6 wavetable scale voices (needs 1). A library may depend on another; Hygrostat installs the whole chain. Code used by one engine stays in that engine's blob; 6-op FM is a library anyway, because its unpacked banks are shared by all voices.

## Blob

The header grows to 96 bytes (24 little-endian words). The blob still ends with its CRC-32.

| Word | Model blob | Library blob |
|---:|---|---|
| 0–3 | magic `DZMB`, header size 96, ABI 2, services version 2 | same |
| 4 | kind 1 | kind 2 |
| 5–6 | first model ID, number of consecutive IDs it serves | library ID, 1 |
| 7 | size | size |
| 8–9 | state size, arena size | RAM size, 0 |
| 10–12 | `init`, `reset`, `render` offsets | `init` offset, export table offset, export count |
| 13–14 | GOT offset and size | same |
| 15–16 | dependency list offset (one word per library ID), count | same |
| 17–19 | flags, OUT gain, AUX gain | 0 |
| 20–23 | reserved, 0 | same |

- **Model entries:** `init(state, services, arena, arena_size, libraries)`, where `libraries` holds one context per dependency in the listed order. `reset(object, services, argument)`; the argument is the selected ID minus the first ID (the 6-op bank). `render` as in version 1.
- **Library entries:** `init(ram, ram_size, services)` runs once at synth entry, outside the audio interrupt, and returns the library's context. A library has no writable statics; shared state (decoded word banks, unpacked 6-op banks) lives in its declared RAM. Its export table lists offsets of the functions and tables dependents may call or read; the loader turns them into the dependents' import table. Calls through that table are the gate's one exception to "no branch through a register".
- **State slot:** at least the largest offered model's state. Palette's largest is 1,080 B (Dual Sync); the stock engines' plain-class sizes are measured in phase 1.
- **Still refused:** writable data, libc functions that write `errno` (`sinf`, `tanhf`), and absolute pointer tables. A port turns pointer tables into offsets. `memcpy` and `memmove` are copied into the blob.

## Services version 2

- Version 1's three exports keep their addresses and indexes.
- New: `stmlib::Random`'s shared state, so a ported stock model draws from the same generator as the compiled-in engine. Bit identity needs this.
- New table exports: any table that two or more blobs read and no library owns. The phase 0 links found seven: `lut_fold`, `lut_ws_bump`, `lut_ws_linear`, `lut_ws_inverse_sin`, `lut_ws_inverse_tan`, `lut_fm_frequency_quantizer`, `lut_4x_downsampler_fir` (4,656 B). Each is hashed at boot like version 1's.
- Version-1 blobs keep loading as long as a release lists them.

## Region

From the first 4 KiB boundary after the app to 0x08100000. Sections come in a fixed order, `WAVE`, `TERR`, `PLTS`, each starting on a 32-byte flash word (the H7's ECC unit), so no flash word holds bytes of two sections.

- **Section header** (32 bytes): magic `DZSC`, kind (`WAVE`, `TERR` or `PLTS` as a four-character code), version, release, size (header to CRC), item count, two reserved words. Then the payload, then a CRC-32 of everything before it. A section is valid on its own, without the profile.
- **Directory, in the profile block:** for each section, its kind, offset, size and CRC, plus the selection CRC. The firmware checks every entry lies inside the region, is aligned and doesn't overlap another before it reads anything. The profile also carries the bank table (up to 15 rows of 8 model IDs) and the synth lock.
- **Without a valid profile:** the firmware uses the release's own default layout, compiled into the app as fixed offsets and the SHA-256 of each default section. It never trusts a region offset from anywhere else.
- **`PLTS` payload:** one 16-byte entry per blob (kind and ID, offset, size, CRC), libraries first, then models by first ID; blobs follow at 32-byte offsets. Every dependency must be present and approved, or the dependent is refused. All or none within the section.
- **Faults:** a damaged section never hides another. With no valid `PLTS`, Synth uses the Beads wavetables; with no valid `WAVE`, it uses Plaits; with neither, the synth is silent and [A] shows the error cue until a good install.
- **Install order:** Hygrostat writes `WAVE` before `PLTS`.

## Content limits (Beta 4)

Firmware and Hygrostat both refuse content outside these limits.

**`WAVE` (Beads wavetables, the reader in `dsp/wavetable_grain.h` and `dsp/wavetable_voice.h`):**
- Exactly 12 banks of 16 waves, plus one guard row: 193 rows of 260 little-endian `int16`, 100,360 bytes. The bank count is fixed: FEEDBACK's quantizer has 12 steps (`kNumBanks`).
- Each row is one integrated cycle, as `resources/wavetables.py` makes it: two copies of the 256-sample wave, mean removed, normalized to a peak of 1, summed, mean removed again, scaled by 512 (4 × 32768 / 256) and rounded; the row keeps the last 260 samples. So samples 0–3 repeat samples 256–259, which the four-point Hermite reader needs past the cycle's end.
- Row 192 repeats row 191: the last wave of bank 11 interpolates towards it.
- Every sample fits `int16`. The factory table's peak is 26,345, so a new table that would overflow is refused, not clipped. The voices differentiate the data at 1 / (frequency × 131,072).

**`TERR` (terrain grids for Wave terrain):**
- 1 to 16 grids, each 64 × 64 `int8` in rows of x (4,096 bytes). The value is the sample / 128.
- Same format as Plaits' own user terrain (`LoadUserData`, `TerrainLookup` in `wave_terrain_engine.cc`) and Palette's `WAVE_TERRAIN_CUSTOM`. The reader maps x and y from [−1, 1] to [0, 62] and interpolates bilinearly, so index 63 is read only as the upper neighbour. No guard samples; all 256 values are valid.
- No `TERR` section means the eight stock terrains only, bit-identical to today.

## Identity

The version 1 record grows with every blob, so version 2 keeps digests in flash (the app's allowlist) and only statuses in RAM: release, services status, one status and CRC per section, and one status byte per `PLTS` entry. It is readable over SWD by symbol.
