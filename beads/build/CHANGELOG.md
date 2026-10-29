# Changelog

Changes from the vanilla Mutable Instruments Beads source (`f4cfa76`). Every
change, one line each, is in [README-BASE.md](../../README-BASE.md). Changes
from the 2021 and 2024 firmware to the vanilla source are in the
[beads-build-kit changelog](https://github.com/insolentconstruct/eurorack/blob/beads-build-kit/beads/build/CHANGELOG.md).

## Unreleased
- Quality, mono/stereo and synth changes now fade the output out over 10 ms instead of cutting it at full level, and fade back in afterwards. The synth's reverb tail no longer plays on after the synth stops (#53).
- Fix a settings save that could, rarely, skip erasing the flash and write over the old settings, leaving a module that crashes at every start. Each flash operation is now checked, and settings are only written onto erased flash (#149).
- Quality, mono/stereo and synth changes that keep the sample rate no longer stop the converter, which removes the pop it made; changes to or from vintage digital (32 kHz) and dirty tape (24 kHz) still restart it (#53).
- Less flash wear from a modulated FREEZE: each interrupted buffer save doubles the wait before the next, from 10 s up to 320 s; a completed save, a minute unfrozen or a reboot resets it (#56).
- Frozen-buffer backups carry a CRC and are range-checked; stock backups still load. An interrupted or failed backup is never restored, and a setting saved during a backup no longer breaks it (#36).
- Delay mode: no tempo glitch on the first clock after about 12 hours without one (#57).
- Frozen delay with negative FM no longer reads outside the buffer, and very fast SEED clocks no longer create a zero-length slice (#33).
- Fixed undefined reads in the dummy CPU-load grains and past the last wavetable; output unchanged (#34, #35).
- App built with GCC 9 instead of 4.9.3; the sound is unchanged.
- More CPU headroom: about 16,600 fewer cycles per audio block in the heaviest stock case, with the same sound.
- The wavetable synth uses less CPU: about 6,800 fewer cycles per audio block in its busiest case, with the same sound.

Controls, settings and the default sound are those of the vanilla source; no features are added.
