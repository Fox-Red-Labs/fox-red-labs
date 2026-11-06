# iot-chain: board-independent device logic in C

C code with no hardware calls (a credential record, a command parser, topic and telemetry helpers) that the radio demos share. `test/run.sh` builds and runs `test/host_test.c` on the Mac and needs only `cc`. Not here: any board's drivers or secrets. Terms: [docs/vocab.md](../../docs/vocab.md).
