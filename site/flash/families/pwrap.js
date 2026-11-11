// The power-wrapper access port (PWR-AP) that TI's CS_DAP debug subsystem puts at AP 4: the MSPM33C321A, the AM13E230
// and the MSPM0 parts with a CS_DAP (MSPM0G1518, G3519, G5187, MSP32C031) have it; the MSPM0L1306 does not.
// What TI's debug servers do with it comes from the device GEL files in CCStudio 21.0.1
// (ccs_base/emulation/gel/mspm33c321a.gel: OnPreTargetConnect, OnPreFileLoaded, OnRestart, OnReset,
// MSPM33_ResetCtl_WaitForDebug) and the register list in targetdb/Modules/AP/PWRAP.xml:
//   DPREC0 (AP4 reg 0x00) and SPREC (AP4 reg 0xF0), IDR (reg 0xFC) reads 0x002E0002.
//   wait for debug: see waitForDebug() below
//   restart:        DPREC0 |= 0x20000, SPREC |= 1                 (reset cause 0x1A, "debug triggered SYSRST")
// Nothing here writes flash.

export const PWRAP_AP = 4, DPREC0 = 0x00, SPREC = 0xF0, IDR = 0xFC, PWRAP_ID = 0x002E0002;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export async function hasPwrap(dap) {
  try { return ((await dap.apRead(PWRAP_AP, IDR)) >>> 0) === PWRAP_ID; } catch (e) { return false; }
}
export const readDprec0 = (dap) => dap.apRead(PWRAP_AP, DPREC0);
export async function orDprec0(dap, bits) { await dap.apWrite(PWRAP_AP, DPREC0, ((await dap.apRead(PWRAP_AP, DPREC0)) | bits) >>> 0); }
export async function orSprec(dap, bits) { await dap.apWrite(PWRAP_AP, SPREC, ((await dap.apRead(PWRAP_AP, SPREC)) | bits) >>> 0); }

// Catch the core before the program on the chip runs, even one that faults at boot. The register sequence is what
// CCStudio's debug server writes on a wait-for-debug connect, taken from its own log (TI_DS_ENABLE_LOGGING) on the
// LP-MSPM33C321A, 2026-10-22: hold the chip in its boot wait (10 reset requests with the hold bit 0x4000), three
// DPREC0 values, a vector catch on reset (DEMCR VC_CORERESET) with halt requests, then DPREC0 values that release the
// hold, so the core stops at its first instruction. (The GEL file alone, hold then 0x00120008, left a chip that faults
// at boot with its memory port shut.) The core ends halted; the caller's DEMCR write of 0 removes the vector catch.
const DHCSR = 0xE000EDF0, DEMCR = 0xE000EDFC;
export async function waitForDebug(dap) {
  for (let i = 0; i < 10; i++) {
    await orSprec(dap, 1);
    await orDprec0(dap, 0x4000);
    await orSprec(dap, 1);
  }
  await sleep(150);                                   // the chip re-initialises before the debugger's next access
  dap.select = -1; dap.csw = -1;
  await dap.connect();
  const w = (v) => dap.apWrite(PWRAP_AP, DPREC0, v);
  for (const v of [0x002C4027, 0x000C6027, 0x000C602F]) await w(v);
  await dap.write32(DHCSR, 0xA05F0001);
  await dap.write32(DEMCR, ((await dap.read32(DEMCR)) | 1) >>> 0);
  for (let i = 0; i < 10; i++) await dap.write32(DHCSR, 0xA05F0003);
  for (const v of [0x000C202F, 0x0048202F, 0x0008602F]) { await w(v); await sleep(20); }
  await w(0x00120008);
  for (let i = 0; i < 50; i++) {                       // wait for the halt at the first instruction
    if (((await dap.read32(DHCSR).catch(() => 0)) & (1 << 17)) !== 0) return true;
    await sleep(10);
  }
  return false;
}

// Restart through the debug subsystem: a system reset the debugger asked for (reset cause 0x1A), the program runs from its vector table.
export async function restart(dap) {
  await dap.apWrite(PWRAP_AP, DPREC0, (((await dap.apRead(PWRAP_AP, DPREC0)) & ~0x4000) | 0x20000) >>> 0);   // hold bit off, so the next reset runs the program
  await orSprec(dap, 1);
}

// Let the next reset run the program: the hold bit of the wait-for-debug connect off.
export async function release(dap) {
  await dap.apWrite(PWRAP_AP, DPREC0, ((await dap.apRead(PWRAP_AP, DPREC0)) & ~0x4000) >>> 0);
}
