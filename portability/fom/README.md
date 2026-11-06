# fom: control-loop figure-of-merit header for any board

Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes.

`fom.h` (with `fom_c28.h` and `fom_tb.h`) is a header-only C harness: a rate sweep and one result layout for a hardware-triggered control loop, the same on every board. The board program supplies one function. `host_test.sh` builds and runs the self-test on the Mac with `cc` and prints OK; it needs only a C compiler. The boards' benchmark folders include the header.
