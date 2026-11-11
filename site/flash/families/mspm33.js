// Cortex-M33 family: LP-MSPM33C321A and LP-AM13E230 (same NVMNW flash controller, same flash read interface, same CS_DAP).
// Facts and where they come from:
//  - FRI FRD_INTF_CTRL (0x4002900C on both chips; hw_fri.h in the MSPM33 and AM13 SDKs): bit 1 data cache (AM13: data line
//    buffer), bit 2 code cache (AM13: cache). A program that turned the caches on (TI's examples do at 160 MHz) leaves
//    stale flash lines in them; a reset after a write then fetches the OLD vector table and the old program runs
//    (measured 2026-10-22, LP-MSPM33C321A: PC inside the previous image after a correct write). The caches go off before
//    the first write; the debugger's own reads never pass through them, so the read-back cannot show this.
//  - Safe connect: the wait-for-debug method of TI's GEL files (pwrap.js) holds the core in its reset handler.
import * as pwrap from "./pwrap.js";

// RAM routines (tools/webflash/loader/mspm33_loader.c and crc_sectors.c, built by build_m33.sh): a flash loader that programs
// a whole buffer per run, and a per-sector CRC-32 over a flash range.
const LOADER = "8eb072b6816800295dd00168099142681121090204690d1d0b46d03308930b46d43307930b46243306930b46203305930b462c3304930b46303303930b46343302930b46383301930c913c31009100261f4f0d900b950521615101230c996350e1594907fcd4099971180020089d6051079d0a9060510b9823500d98c56806982550059821508d06ad0f04982550156803982550556802982550956801982550d568009825500c982350e059c507fcd080070d980b9d07d51036103281688e420a99c8d300e000214161084600befde7d0130000";
const CRC_SECTORS = "83b072b6c2680190416889000292521800231c4c08261d46690801273d406d4225404d40761ef7d1990055505b1c39028b42efd1019e7168002920d0306800250095b468002c12d00023df4326460346591c1b787b40dbb29b00d3583f0a5f40761e0b46f4d10019fb43019e00e0009bac0002990b516d1c73689d42e1d300befde7c0462083b8ed";

export const family = {
  id: "mspm33",
  cacheReg: 0x4002900C,       // cleared while the page writes; the previous value is put back before the start
  words: 16,
  ramStart: 0x30000000,        // secure alias of SRAM (the core is in the secure state at reset; the 0x20000000 alias is non-secure)
  crc: true,                  // the on-chip CRC-32 routine checks a write (tools/webflash/loader/crc32.c)
  release: pwrap.release,
  // Measured on the LP-MSPM33C321A, 2026-10-22 (64-byte debug packets): read about 23 kB a second, write through the RAM loader about 9 kB a second
  timing: { readBps: 23000, writeBps: 9000, eraseSectorMs: 10 },
  coreFc: 0x50042000,         // flash controller as the secure core sees it (the semaphore matches on the secure attribute)
  loader: LOADER, crcSectors: CRC_SECTORS,
  // Called by Session.start() after the plain connect: true when the core was caught in its reset handler.
  async safeConnect(dap) {
    if (!(await pwrap.hasPwrap(dap))) return false;
    return await pwrap.waitForDebug(dap);
  },
  // Start the written program. The caches are already off or restored by the engine.
  async start(dap, how) {
    if (how === "pwrap" && (await pwrap.hasPwrap(dap))) { await pwrap.restart(dap); return true; }
    return false;           // the engine falls back to the Arm system reset request
  },
};

// LP-AM13E230: the same controller, caches and power wrapper (AP 4, measured 2026-10-22); SRAM at 0x20000000 only (no secure alias),
// no TrustZone. Its read interface calls the bits data line buffer (bit 1) and cache (bit 2).
export const am13 = { ...family, id: "am13", ramStart: 0x20000000, coreFc: 0x40042000 };
