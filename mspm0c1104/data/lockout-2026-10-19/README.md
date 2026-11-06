# LP-MSPM0C1104 lockout, 2026-10-19: what was read and written

Terms: [docs/vocab.md](../../../docs/vocab.md). Write-up and the recovery steps: [docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md](../../../docs/experiments/2026-10-19-mspm0c1104-lockout-root-cause.md) and the [FAQ](../../FAQ.md).

Two files are kept:

| File | What it is |
|---|---|
| `flash-as-found.bin` | All 16 kB of main flash as found, read twice identical (sha256 320acac6...e531e7). Sector 0 holds the first 896 B of the 936 B color player; 0x380 to 0x3FF are erased |
| `nonmain-as-found.bin` | NONMAIN, 256 B as found: TI's default boot configuration (0x00000003, 0xAABBAABB, 0xFFFFAABB, the rest 0xFF) |

What the reads showed, in order:

1. First read: nRESET reads 0, only AP1 (configuration) and AP2 (mailbox) answer, BOOTDIAG is 0.
2. After `adapter deassert srst`: nRESET reads 1, AP0 answers once, AP4 is back.
3. 30 reads over 10 s: AP0 faults every time and DPREC0 bit 17 toggles, so the chip keeps resetting.
4. TI's wait-for-debug hold, a vector catch and a release: the core halted and memory was readable.
5. The out-of-box image was written and verified (all 16 kB, 6.2 s). After a boot reset, PA22 toggled every 487 to 535 ms (out of the box: 495 ms).
6. Upstream `board/ti/mspm0-launchpad.cfg` connected normally after the restore.
