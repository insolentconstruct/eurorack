# Host tests

Author: Insolent Construct

Run from the repository root: `make -j"$(nproc)" -f beads/test/<suite>.mk test`. Native tests use `-O1` with the existing sanitizer settings; spectral also runs a build with Clouds' FFT.

Check the firmware harness's simulated power-on reset with optimized,
unsanitized GCC. Use a separate target for the build flags and keep the
downsampler's `-Wmaybe-uninitialized` diagnostic visible as a warning.

```sh
make -j2 -f beads/test/firmware.mk -f - \
  TARGET=firmware_unsanitized_test HOST_SANITIZERS=0 test <<'EOF'
$(BUILD_DIR)processor.o $(BUILD_DIR)processor_options.o: EXTRA_FLAGS = -Wno-error=maybe-uninitialized
EOF
```

Delay mode, Clouds and spectral accept exact `TestXxx` names:

```sh
make -j"$(nproc)" -f beads/test/clouds.mk test TEST_ARGS='TestStockHashes TestReverbRates'
./clouds_test --list
```

No arguments runs everything. Unknown names fail before any test runs; several names run in the normal test order, each printing its elapsed time. Delay quantizer checks are split into Digital (the two digital qualities) and Tape (the two tape qualities), each covering mono and stereo.

`ci-jobs.json` defines the ten CI jobs and their shell commands. When changing it, keep every suite and every listed function in exactly one job. The `list-host-tests` workflow job supplies the list to the host matrix.

`codec.mk` compiles the real codec driver as C++98 against its own fake HAL
(`codec_stubs/`), not `common_stubs`.
