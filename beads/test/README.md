# Host tests

Author: Insolent Construct

Run from the repository root: `make -j"$(nproc)" -f beads/test/<suite>.mk test`. Native tests use `-O1` with the existing sanitizer settings; options also runs its advanced build.

Delay mode and Clouds accept exact `TestXxx` names:

```sh
make -j"$(nproc)" -f beads/test/clouds.mk test TEST_ARGS='TestStockHashes TestReverbRates'
./clouds_test --list
```

No arguments runs everything. Unknown names fail before any test runs; several names run in the normal test order, each printing its elapsed time. Delay quantizer checks are split into Digital (the two digital qualities) and Tape (the two tape qualities), each covering mono and stereo.

`ci-jobs.json` defines the eight CI jobs and their shell commands. When changing it, keep every suite and every listed function in exactly one job. The `list-host-tests` workflow job supplies the list to the host matrix.
