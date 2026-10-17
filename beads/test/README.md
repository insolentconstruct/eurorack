# Base tests

Run `beads/test/run.sh OUTPUT_DIR` from the repository root in a container with
g++, make and sanitizer runtimes. Ten independent suites cover the stock
signal path, the included delay fixes, backup failures and firmware callbacks.
No option or feature suites are included. Tests of existing stock delay and
monophonic DSP code do not enable new gestures or advanced firmware features.

`beads/test/storage_compat.sh OUTPUT_DIR` extracts four storage sources from
`f4cfa76` and builds the same test against both readers. Each writes a real
settings record and frozen buffer, then the other reads it, for all four media
in mono and stereo. A second argument can name a previously exported stock
source directory when the container has no Git metadata. Stock is compiled
without sanitizers because its original backup writer reads a full flash word
from a smaller header; the base failure suite uses the checked format.

`audio_reference.sh OUTPUT_DIR` (also run by `run.sh` and CI) compiles the
current DSP and the checked-in pre-optimization versions of the three changed
headers in `reference/` into separate host executables. The reference headers
come from the build-kit source before the three CPU optimizations; their
original licences are retained. No Git history or external oracle is needed.
The same compiler, sanitizer and floating point flags build both versions.
The test compares every captured float byte, rejects non-finite output and
requires nonzero output in each scenario. Four media times stereo grains,
mono grains, delay, synth and unpatched-SEED synth give 20 scenarios, each
with 2,400 warm-up blocks and 2,000 captured blocks. Synth cases call the
real synth engine at 48 kHz for each wavetable resolution. Controls exercise feedback,
reverb, random modulation and freeze; saved options remain at stock defaults.
These native-host comparisons cover the CPU optimizations, not ARM instruction
rounding or every possible signal/state. Historical linked-ARM emulator
comparisons are recorded only in the validation report.

CI runs the ten host suites plus this audio reference comparison and the
bidirectional stock storage compatibility suite. It also performs two full
pinned firmware/WAV release builds, compares release hashes, and checks WAV
round-trip decoding at 48/44.1 kHz with corruption, missing-EOT and wrong-payload
negative controls. CI does not run a linked-ARM emulator or hardware tests.
