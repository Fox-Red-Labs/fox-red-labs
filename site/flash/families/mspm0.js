// MSPM0 family (LP-MSPM0L1306 and the other MSPM0 kits the bench writes). Code cache and prefetch switch: CPUSS CTL
// (hw_cpuss.h). Start: a boot reset through SYSCTL (RESETLEVEL, RESETCMD, hw_sysctl_mspm0l11xx_l13xx.h).
// Parts with a CS_DAP (MSPM0G3519, G5187) also have the power-wrapper port used by the wait-for-debug connect.

export const family = {
  id: "mspm0",
  cacheReg: 0x40401300,
  words: 8,
  crc: true,                  // the on-chip CRC-32 routine (tools/webflash/loader/crc32.c, Thumb-1) checks a write; RAM start comes from the board's data
  // Measured 2026-10-22 over the XDS110's 64-byte packets: bytes a second (L1306: 64 kB read in 2.8 s, put back in 3.1 s)
  timing: { readBps: 23000, writeBps: 20000, eraseSectorMs: 10 },
  // No safeConnect: the MSPM0 parts keep the plain halt and the nRESET wake-up (Session.start). The CS_DAP parts' power wrapper
  // (pwrap.js) is detected by pwrap.hasPwrap but not used here: not run through the wait-for-debug sequence yet.
  async start() { return false; },
};
