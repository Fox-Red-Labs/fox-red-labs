# Board ports: the crypto pack on the LP-MSPM0G3519 and LP-MSPM0G5187

One line per board layer. The next port to a family reads its family's line first.

Terms: [docs/vocab.md](../../docs/vocab.md).

| Board | Started from | Notes | Date |
|---|---|---|---|
| mspm0g3519 | cc1354p10/pqc-join (Cortex-M33) and the MSPM0 bench harness | [mspm0g3519/PORT.md](mspm0g3519/PORT.md): post-quantum and classical crypto pack on the Cortex-M0+, SysTick counter, RAM log read by the debugger | 2026-10-17 |
| mspm0g5187 | mspm0g3519 (same core, same SDK) | [mspm0g5187/PORT.md](mspm0g5187/PORT.md): the same pack on 32 kB of RAM: ML-KEM-768 at -Os, no ML-DSA, join buffers dropped at link | 2026-10-17 |
