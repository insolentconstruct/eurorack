# How the released source differs from earlier Beads firmware

Two older firmware builds are in circulation:

- **The 2021 forum release**: `beads.wav`, posted on the Mutable Instruments forum in April 2021.
- **The 2024 recovery firmware**: `beads_patch_firmware.wav` and `beads_golden_bootloader_combo.bin`, shared in the ModWiggler Beads thread in early 2024 for the "stuck on boot" fix.

Both were rebuilt **byte-for-byte** from Émilie Gillet's released source (eurorack `d8f4739`) with this kit's toolchain plus a small patch. The builds were compared against the application images decoded from those update files. So the lists below are not guesses: they are the source changes **needed to reproduce each older firmware image exactly**, and undoing exactly these changes does that. A byte-exact rebuild proves these changes are sufficient; it cannot recover the author's original history, and other source routes to the same bytes can't be excluded (for example, several stmlib revisions compile to identical bytes). The release's bootloader is byte-identical to the bootloader in the 2024 combo file.

Effects are described from the code. Nothing here was measured by ear unless stated.

## 2024 recovery firmware → released source (this build)

One change, in `beads/dsp/granular_player.h` (`GranularPlayer`, polyphonic mode only):

- **How a new grain picks which playing grain to cut short.** When a new grain starts, Beads fades out a few "safety" grains early, so a voice is always free.
  - The older firmware uses a cheap "steal the oldest grain" method only when grains arrive less than 5 ms apart. At 5 ms and slower, it sorts the grains by the time each has left.
  - The released code uses the cheap method whenever grains arrive less than **25 ms** apart, which is roughly 40 or more grains per second instead of 200 or more.
  - In the cheap method, each safety grain gets a fade-out cap. When grains arrive more than 5 ms (and less than 25 ms) apart, the released code swaps two of those caps, so the grain being stolen fades out within at most 5 ms instead of within one grain interval.
- Monophonic mode is unchanged. That mode is also compiled out of all shipped builds (`ENABLE_ADVANCED_FEATURES` is commented out).

**Observed.** In emulation, with identical input and state:
- outputs are bit-identical when grains start 25 ms or more apart;
- outputs differ when grains start between 5 and 25 ms apart, where the two versions run different code;
- below 5 ms both versions run the same code, so outputs only differ if an earlier passage through the 5–25 ms range has already changed the grain history. Such differences can persist briefly afterwards.

Two bench tests hit the changed range: "freeze gate" and "gated density". The likely motivation, lower CPU use in dense textures, is **not** measured yet.

## 2021 forum release → 2024 recovery firmware

Five changes:

1. **Settings recovery at power-on (SEED held).** Holding SEED while powering on rewrites the settings sector, keeping a valid calibration; this fixes modules stuck on boot. The 2021 release has no such path. **Do not hold SEED at power-on unless you mean to.**
2. **Simpler smoothing of knobs and CV.** The stmlib `HysteresisFilter` lost its short tracking mode after a change (upstream stmlib commit `742b6df`). Each pot/CV channel's state shrinks from 28 to 20 bytes.
3. **Delay density/FM input smoothing.**
   - The dead band is narrowed from 0.005 to 0.0025.
   - The FM follower's brief tracking after a change (the next 7 calls) is removed.
4. **Steadier slice selection while frozen (delay modes).** An extra hysteresis filter (0.0025 dead band) sits on the slice/TIME selection during frozen playback, in both manual and clocked delay.
5. **Clocked delay: both FM inputs now move the clock divider.**
   - In the 2021 release, exponential FM separately bent the tap ratios.
   - In the 2024 firmware and in this release, it is added to the divider selection instead.
   - Manual delay is unaffected.

## Notes

- Upgrading from the 2021 release to this build brings all six changes.
- The patches and the comparison method are not part of this kit; this kit builds only the released source. They are available on request.
