# Porting the base DSP

The DSP remains fixed-rate: 48, 32 and 24 kHz according to recording quality.
A host at another rate needs resampling around the engine. Define `TEST` to
select the support library's portable arithmetic instead of ARM assembly.
Packed sample structures and embedded memory placement remain upstream code.

`MulAdd()` calls standard `fmaf()` to preserve one random-pan calculation's
fused rounding. On a processor without FMA it may call the math library, once
per grain start. Avoid introducing contraction elsewhere in a host port;
match the chosen target's floating-point rounding and flush-to-zero behavior.

Delay-line modulo assumes readers never advance ahead of the write head.
New readers must preserve that non-negative index invariant. Mono/stereo
specialization and local grain accumulation preserve the original arithmetic
order; initializing the caller's output frame remains required.

Drivers, UI, settings, flash backups and the bootloader are firmware services,
separate from a desktop DSP port. Host tests use small hardware stubs and run
the real processor, synth and firmware callback. No desktop plugin format is
provided.
