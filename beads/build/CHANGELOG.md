# Changelog

Changes from the vanilla Mutable Instruments Beads source (`f4cfa76`). Every
change, one line each, is in [README-BASE.md](../../README-BASE.md). Changes
from the 2021 and 2024 firmware to the vanilla source are in the
[beads-build-kit changelog](https://github.com/insolentconstruct/eurorack/blob/beads-build-kit/beads/build/CHANGELOG.md).

## beads-tuneup-2026.11.09
- A module whose settings flash is damaged no longer crashes at every start. The calibration is kept in a second sector and restored from it when needed, so a save interrupted by a power cut keeps the calibration; at worst the settings return to their defaults. If both copies of the calibration are lost, the module plays on the factory calibration and default settings, blinks all LEDs for 3 s at each power-up and writes nothing to flash; hold SEED at power-up to repair it, then recalibrate. A damaged frozen buffer is skipped instead of crashing (#148).
- A damaged calibration record no longer resets the calibration and starts the factory test; a record with no saved settings after it keeps the calibration and resets only the settings (#148).
- No flash erase or write starts while the module's 3.3 V supply is below about 2.8 V. An operation already underway cannot be stopped; an interrupted save is handled like a power cut (#175).
- The build refuses an application large enough to reach the frozen buffer and settings flash (#155).
- Quality, mono/stereo and synth changes now fade the output out over 10 ms instead of cutting it at full level, and fade back in afterwards. The stock synth reverb tail is retained when the synth stops (#53).
- Fix a settings save that could, rarely, skip erasing the flash and write over the old settings, leaving a module that crashes at every start. Each flash operation is now checked, and settings are only written onto erased flash (#149).
- Quality, mono/stereo and synth changes that keep the sample rate no longer stop the converter, which removes the pop it made; changes to or from vintage digital (32 kHz) and dirty tape (24 kHz) still restart it (#53).
- Changes to or from vintage digital (32 kHz) and dirty tape (24 kHz) hold the converter in reset while its clocks change, as its datasheet specifies. The same change cut the click by about 25 dB in Drizzlet Beta 2; a small click remains (#53).
- Less flash wear from a modulated FREEZE: each interrupted buffer save doubles the wait before the next, from 10 s up to 320 s; a completed save, a minute unfrozen or a reboot resets it (#56).
- Frozen-buffer backups carry a CRC and are range-checked; stock backups still load. An interrupted or failed backup is never restored, and a setting saved during a backup no longer breaks it (#36).
- Delay mode: no tempo glitch on the first clock after about 12 hours without one (#57).
- Frozen delay with negative FM no longer reads outside the buffer, and very fast SEED clocks no longer create a zero-length slice (#33).
- Fixed undefined reads in the dummy CPU-load grains and past the last wavetable; output unchanged (#34, #35).
- App built with GCC 9 instead of 4.9.3; the sound is unchanged.
- More CPU headroom, with the same sound: about 16,600 fewer cycles per audio block in the heaviest stock case on a development board. On a module, the peak load of a heavy granular patch falls from 91% to 76–78%.
- The wavetable synth uses less CPU, with the same sound: about 6,800 fewer cycles per audio block in its busiest case on a development board. On a module, the peak load of a heavy synth patch falls from 85–86% to 82–83%.

Controls, settings and the default sound are those of the vanilla source; no features are added.
