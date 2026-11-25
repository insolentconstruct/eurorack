<!--
Copyright 2026 Insolent Construct.

Author: Insolent Construct

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

**TL;DR** Tuneup better protects your calibration and settings during interrupted saves. Audio can recover after a failed start, and the automatic switch to synth mode is more reliable. Delay uses less processor time. The stock controls, default sound and saved formats are unchanged.

This is an unofficial community build based on the Beads source Mutable Instruments released under the MIT licence.

## What changed

- **Calibration.** Rejected and unfinished runs keep the previous calibration. Beads also rejects a measured scale or offset more than 25% from its factory value. Leaving before the first measurement with [M]+FREEZE resumes audio jack detection. That gesture still toggles autosave.
- **Failed calibration saves.** All LEDs blink. Beads keeps the calibration it would select at the next startup, or the previous calibration in memory if both flash copies are unusable. The factory test also reports a failed save.
- **Calibration backup.** Tuneup verifies a usable primary copy before replacing a usable backup. If the backup is the only measured copy, Tuneup saves and verifies it in the primary location first. This also protects it when an interrupted save is followed by a boot into stock firmware that writes factory values.
- **Damaged calibration.** An out-of-range record can trigger the storage warning even when its checksum is valid. Buttons held through the warning have to be released and pressed again before they can modify another button's action.
- **Settings and frozen audio.** When an old record immediately follows a blank slot, saving rewrites the settings area to keep the old record from resurfacing. Tuneup verifies that the settings area is blank after erasing and retries if needed. Frozen-buffer backups relock flash after each erase or write, and any backup still in progress is cancelled on entry to synth mode.
- **Frozen audio backup retries.** If a backup fails, Beads retries while FREEZE stays on. The FREEZE LED stays dim while a retry is pending.
- **Audio restart and synth mode.** Beads retries a detected audio start or restart failure even when audio has stopped. After ten quiet seconds with the inputs unplugged, a late output peak no longer stalls a synth fade already in progress.
- **Settings recovery utility.** The utility can take calibration and settings from separate saved sectors even when each holds only one usable record. It warns if the extracted calibration is outside Tuneup's accepted range.
- **Delay processing.** The delay taps share one SHAPE envelope per sample. The busiest delay blocks in development-board tests used 4–8% fewer cycles, and reference audio matched. The previous release’s clean-audio limits were 96.9% of the audio block at 48 kHz, 97.3% at 32 kHz and 97.7% at 24 kHz.

## Installing

Use the WAV ZIP for the normal audio update. Follow the included instructions, keep the original WAV level and disconnect speakers during the update. Compatible settings and usable calibration carry over. Tuneup checks calibration values as well as the checksum.

The combo ZIP and standalone HEX include the bootloader and are for an external programmer. The HEX carries its address. Load the combo BIN at `0x08000000`. Erase only the sectors you will program. A full-chip erase removes calibration and settings.

After a power cut during a save, let Tuneup start once before installing other firmware. If both calibration copies are unusable, all LEDs blink for three seconds at startup, Beads uses factory calibration and default settings, and saving is disabled. If you see that storage warning, hold SEED at power-on to attempt repair. A slow green [A] blink with all three [M] LEDs lit means repair succeeded and you should recalibrate. A fast red blink means repair failed.

## Known limits

- A small click can occur when changing between qualities that use different sample rates.
- A power cut at the wrong point in a settings rewrite can reset settings to defaults even when calibration survives. Until Tuneup has made its first backup on a module, there may be only one calibration copy.
- The audio updater retains the stock bootloader's handling of flash errors. If an update appears to finish but Beads will not start, follow the recovery instructions in the WAV ZIP.

## Verification

Host tests covered storage compatibility, calibration and reference audio. The build reproduced the pinned firmware and WAV hashes, and the reference decoder recovered the update payload at its native 48 kHz and after resampling to 44.1 kHz. This exact build was installed by audio on a Beads module. The application readback matched the build pin, and calibration was unchanged. Broader module qualification is pending.

## Downloads

| Download | SHA-256 |
| --- | --- |
| `beads-tuneup-2026.11.24-wav.zip` | `38d9a038f0fbc5dd6e5a2580b5bade914deb9ccecdb44b0c7896b06e15695491` |
| `beads-tuneup-2026.11.24-combo.zip` | `17aaa48966b26f1bb3fcd4f9ab928f37fec48c755f456f7b49a7de4b0bdca0fd` |
| `beads-tuneup-2026.11.24-combo.hex` | `13069060a4fd3c5aad3e42593f4e284c69dd89b0085766df799dc48fca4190a3` |
| `NOTICE.txt` | `0d38efed916a9e1ae0d9a0151436a2e728bb3a052caeef458397886df5dfab5a` |
| `SHA256SUMS` | `2cb5affe6ec0ae45940f364f8b47dce48dbf37fc83bc02f3abca287e68bd4695` |

The build pins below verify the firmware files and the WAV inside the ZIP.

| Build file | SHA-256 |
| --- | --- |
| `bootloader-padded.bin` | `014564367cf482122183c6e63c39f20cefed39c194c8a4b55dbe31a8855165de` |
| `app.bin` | `eeedbf93cb31aa414b3fb68e4067aa4b4a7312cef7b1fa5f9b546fd0fa49c1b3` |
| `combo.bin` | `c7f145c25a050337b7ad80ef106dab109d3920cc868b7d2c85ef476af48a11ac` |
| `bootloader.bin` | `9bff9f97fac6162d9c29620113186aae5ab4d4723b98fb1daf2b95d32b80da72` |
| `app.elf` | `5d0d46aa7e8ba97ccad4a827072f980f7bcaefd4199024f748c4bb845f871777` |
| `beads-tuneup-2026.11.24.wav` | `79b42279e7ab2ac29450892e1a20378f6e52cbe4ebf9175ddc1a03473dd47574` |
