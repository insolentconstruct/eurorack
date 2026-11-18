# Insolent Construct's eurorack

Firmware built on Mutable Instruments' open source; not a Mutable Instruments product.

| Branch | What it is |
|---|---|
| `drizzlet` (default) | Drizzlet: community firmware for Beads with new features ([install and controls](beads/build/README.md), [releases](https://github.com/insolentconstruct/eurorack/releases)) |
| [`beads-tuneup`](https://github.com/insolentconstruct/eurorack/tree/beads-tuneup) | Beads as Mutable Instruments made it, plus bug fixes and CPU optimizations; no new features |
| [`beads-build-kit`](https://github.com/insolentconstruct/eurorack/tree/beads-build-kit) | The vanilla Beads source, unmodified, with a build kit for its original GCC 4.9 compiler |
| `peaks-tuneup`, `plaits-tuneup`, `rings-tuneup`, `yarns-tuneup` | Work in progress: the tuneup treatment for other modules |
| [`master`](https://github.com/insolentconstruct/eurorack/tree/master) | Mutable Instruments' upstream, unchanged |

## Drizzlet

- [Install and controls](beads/build/README.md)
- [Options guide](beads/build/docs/OPTIONS.md) ([printable](beads/build/docs/options-guide.svg))

## Credits and licence

Drizzlet and the tuneups are built on Mutable Instruments' source. Mutable Instruments
isn't involved in these forks; please report problems here, not to them.
Their original licence and guidelines follow, unchanged.

---

License
=======

Code (AVR projects): GPL3.0.

Code (STM32F projects): MIT license.

Hardware: cc-by-sa-3.0

By: Emilie Gillet (emilie.o.gillet@gmail.com)

Guidelines for derivative works
===============================

**Mutable Instruments is a registered trademark.**

The name "Mutable Instruments" should not be used on any of the derivative works you create from these files.

We do not recommend you to keep the original name of the Mutable Instruments module for your derivative works.

For example, your 5U adaptation of Mutable Instruments Clouds can be called "Foobar Modular - Particle Generator".
