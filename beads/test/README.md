# Host tests

Author: Insolent Construct

From the repository root, run `make -j"$(nproc)" -f beads/test/<suite>.mk test`.
Native tests use `-O1`; existing sanitizer settings apply. Options also runs its advanced build.

Delay mode and Clouds accept exact `TestXxx` function names as arguments.
For example:

```sh
make -j"$(nproc)" -f beads/test/clouds.mk test TEST_ARGS='TestStockHashes TestReverbRates'
./clouds_test --list
```

No arguments runs the full program. Unknown names fail before running any test;
multiple names select their union in the normal test order. Each selected test
prints its elapsed time. Delay quantizer checks are split into Digital (the two
digital qualities) and Tape (the two tape qualities), each covering mono/stereo.

`ci-jobs.json` defines the eight CI jobs and their exact shell commands. Keep
every suite and every listed function in exactly one job when changing it.
The `list-host-tests` workflow job supplies this list to the host matrix.
