# Plaits model blobs: frozen interface (version 1)

A proof, not linked into the firmware. `plaits_blob.h` is the source of truth; this page lists what may never change once a release ships blobs.

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
