# Scale profiles

Beta 2 scale profiles let Hygrostat replace the musical scale bank, choose its
cycle order, and assign each scale an RGB colour and indication pattern.
Off and Chromatic remain the first two direct knob choices. The Scale choice
uses the editable bank. This document describes the development format; final
release qualification is pending.

## Identity and limits

The bank has up to 14 entries, with stable IDs 2 through 15. Factory scales keep
IDs 2 through 9. Bank order controls SEED stepping, not saved identity: moving
an entry does not change the selected scale. An omitted selection falls back
to Off. Editing a definition deliberately changes what that ID plays.

Each entry has 1–24 degrees, including a zero-degree root, and a repeat
interval from 0.01 to 128 semitones (at least one cent). Degrees are finite, strictly increasing and
less than the repeat interval. Values are normal float32 semitones or positive zero; subnormals and negative
zero are rejected. Cents are converted
by the editor. Names hold at most 31 UTF-8 bytes. LED colours have three 8-bit
channels; indication patterns are steady, slow, fast or two pulses.

## Wire format

The profile remains at application offset 1024 and grows to 4096 bytes. Version 2
keeps every six-byte feature entry and permanent packed-settings nibble meaning.
The save marker remains c1; the wire-profile version and saved-state format are
separate. Version 1 profiles retain their legacy 512-byte parser bounds.

| Header byte(s) | Meaning |
| --- | --- |
| 0–3 | DZPF magic, little endian 0x46505a44 |
| 4 | Version 2 |
| 5 | Feature entry count |
| 6–7 | Payload bytes: 6 × feature count + 144 × scale count |
| 8–11 | Release identity |
| 12 | Custom bank: 0 factory, 1 explicit bank |
| 13 | Scale count, 0–14 |
| 14–15 | Zero |

Six-byte feature entries follow the header, then the scale records, then CRC32
of the header and payload. Factory mode requires no scale records. An explicit
empty bank has custom-bank byte 1 and count 0. Payload bounds are checked before
the checksum or any record reads.

| Scale record byte(s) | Meaning |
| --- | --- |
| 0 | Stable ID, 2–15; unique in the bank |
| 1 | Degree count, 1–24 |
| 2–4 | Red, green, blue |
| 5 | Pattern: 0 steady, 1 slow, 2 fast, 3 double |
| 6–7 | Zero |
| 8–11 | Repeat interval, little-endian IEEE float32 semitones |
| 12–43 | NUL-terminated UTF-8 name, zero padding |
| 44–139 | 24 little-endian float32 degrees; unused values positive zero |
| 140–143 | Zero |

Invalid names, values, IDs, lengths, reserved fields or checksums are rejected.
The fixed Fifths and octaves random-interval option remains independent of an
edited scale with ID 2. Generated guides and shared links must carry the same
bank used by the firmware. Custom banks need their own CPU assessment; historical
factory-scale costs do not establish the maximum cost of a 24-degree bank.
