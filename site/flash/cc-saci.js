// TI CC23xx / CC27xx flash programming through SACI, the chip's device-management mailbox on the SEC-AP (access port 2),
// with no CPU connection. Uses the Dap from mspm0-dap.js with dap.dormant = true.
// Commands, layout and order: TI's cc23xx_dap_saci_util.gel (CCStudio 21.0.1) and CC27xx TRM SWCU195A section 9.3,
// as proven on our LP-EM-CC2755P10 by cc2755p10/recovery/saci_program.js.
// Never: a write into the HSM area (0xE8000 and up on the CC27xx), a CCFG or SCFG that is not debug-open with every
// permission allowed, lifecycle, password or key commands.
export const HSM_START = 0xE8000, SECTOR = 2048, KEY = 0xB7E3A08F;
// Per family: where main flash ends for this page, and whether the chip erase keeps the HSM sectors.
// CC27xx: 1 MB, TI's security firmware at 0xE8000 and up, kept (retain bit, as TI's cc23xx_dap.gel sets for CC27xx).
// CC23x0R5: 512 kB main flash (hw_memmap.h), no HSM, no SCFG; the chip erase clears CCFG and every main sector.
export const FAMILY = { cc27: { name: "CC27xx", mainEnd: HSM_START, retain: 1 }, cc23: { name: "CC23x0R5", mainEnd: 0x80000, retain: 0 } };
const CFGAP = 1, SECAP = 2;
const RES = { 0: "SUCCESS", 0x80: "INVALID_CMD_ID", 0x81: "INVALID_ADDRESS_PARAM", 0x82: "INVALID_SIZE_PARAM", 0x83: "INVALID_KEY_PARAM",
  0x84: "FLASH_FSM_ERROR", 0x85: "PARAM_BUFFER_OVERFLOW", 0x86: "NOT_ALLOWED", 0x87: "CRC32_MISMATCH", 0x89: "BLANK_CHECK_FAILED", 0xFF: "CMD_FAILED" };

const T = new Uint32Array(256);
for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; T[n] = c >>> 0; }
export function crc32(b) { let c = 0xFFFFFFFF; for (const x of b) c = T[(c ^ x) & 0xff] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; }
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const words = (b) => { const w = []; for (let i = 0; i < b.length; i += 4) w.push(u32(b, i)); return w; };

// Intel hex -> { main: Uint8Array padded with 0xFF to whole sectors, ccfg, scfg (or null) }
export function parseHex(text) {
  const mem = new Map(); let base = 0;
  for (const line of text.split(/\r?\n/)) {
    if (!line.startsWith(":")) continue;
    const b = line.slice(1).match(/../g).map((x) => parseInt(x, 16)), n = b[0], ad = (b[1] << 8) | b[2], ty = b[3];
    // every record's length and checksum (Codex review 2026-10-16: a corrupted record was written and then "verified")
    if (b.length !== n + 5 || b.some(Number.isNaN) || (b.reduce((x, y) => x + y, 0) & 0xff) !== 0) throw new Error("the program file is damaged (an Intel hex record fails its checksum)");
    // 04: extended linear address (CC2755P10 image); 02: extended segment address, x16 (CC2340R5 image). Missing the 02s
    // put the CC2340R5's upper data on top of its vector table (2026-10-15); vectorCheck() now refuses such an image.
    if (ty === 4) base = ((b[4] << 8) | b[5]) * 65536; else if (ty === 2) base = ((b[4] << 8) | b[5]) * 16;
    else if (ty === 0) for (let i = 0; i < n; i++) mem.set(base + ad + i, b[4 + i]);
  }
  let end = 0; for (const a of mem.keys()) if (a < 0x4E000000 && a + 1 > end) end = a + 1;
  const main = new Uint8Array(Math.ceil(end / SECTOR) * SECTOR).fill(0xff);
  const area = (start, size) => { let any = false; const out = new Uint8Array(size).fill(0xff); for (let i = 0; i < size; i++) if (mem.has(start + i)) { out[i] = mem.get(start + i); any = true; } return any ? out : null; };
  for (const [a, v] of mem) if (a < end) main[a] = v;
  return { main, end, ccfg: area(0x4E020000, 2048), scfg: area(0x4E040000, 1024) };
}

// The image must start with a vector table: a stack pointer in SRAM (0x2000_0000 to 0x2010_0000) and a Thumb reset
// address inside the image.
export function vectorCheck(img) {
  const sp = u32(img.main, 0), rv = u32(img.main, 4), bad = [];
  if (sp < 0x20000000 || sp > 0x20100000 || sp & 3) bad.push(`the first word 0x${sp.toString(16)} is not a stack pointer in SRAM`);
  if (!(rv & 1) || (rv & ~1) >= img.end) bad.push(`the reset address 0x${rv.toString(16)} is not Thumb code inside the image`);
  return bad;
}

// The same refusals as saci_program.js: debug open, every permission allowed, no keys, no secure boot, valid CRCs.
export function checkConfig(ccfg, scfg, family = FAMILY.cc27) {
  const bad = [];
  if (!ccfg || ccfg.length !== 2048) bad.push("no 2 kB CCFG in the image");
  else {
    if (u32(ccfg, 0x0C) !== crc32(ccfg.subarray(0, 0x0C))) bad.push("CCFG boot CRC");
    if (u32(ccfg, 0x08) !== 0) bad.push("CCFG application vector is not at 0");   // as saci_program.js checks
    if (u32(ccfg, 0x74C) !== crc32(ccfg.subarray(0x10, 0x74C))) bad.push("CCFG main CRC");
    if (u32(ccfg, 0x7FC) !== crc32(ccfg.subarray(0x7D0, 0x7FC))) bad.push("CCFG debug CRC");
    if (u32(ccfg, 0x18) !== 0xAAAAAAAA) bad.push("CCFG permissions not all allowed");
    if (family === FAMILY.cc27 && ((u32(ccfg, 0x1C) >>> 4) & 0xf) !== 0xA) bad.push("CCFG main-app erase not allowed");   // 0x1C is a timeout word on the CC23
    if (ccfg[0x7D0] !== 0x5A) bad.push("CCFG debug port not open");
  }
  if (scfg) {
    if (scfg.length !== 1024) bad.push("SCFG is not 1 kB");
    else {
      if (u32(scfg, 228) !== crc32(scfg.subarray(0, 228))) bad.push("SCFG CRC");
      if (u32(scfg, 208) !== 0xAAAAAAAA || (u32(scfg, 212) & 0xf) !== 0xA) bad.push("SCFG permissions not all allowed");
      if (u32(scfg, 164) !== 0x2F8EA5A5) bad.push("SCFG secure boot on");
      if (!scfg.subarray(0, 112).every((x) => x === 0xff) || !scfg.subarray(176, 208).every((x) => x === 0xff)) bad.push("SCFG holds keys");
    }
  }
  return bad;
}

export class Saci {
  constructor(dap, log, family = FAMILY.cc27) { this.dap = dap; this.log = log || (() => {}); this.seq = 0x40; this.family = family; }
  async status() { const v = await this.dap.apRead(CFGAP, 0xC); return { v, boot: (v >>> 8) & 0xff, ahbOpen: !!((v >>> 24) & 1) }; }
  async reconnect() { this.dap.select = -1; this.dap.csw = -1; await this.dap.connect(); }

  // Reset into SACI (SECAP RSTCTL = 0x5C) and send a NOP at once, which also stops the inactivity timeout.
  async enter() {
    if ((await this.dap.apRead(SECAP, 0xC)) & 1) await this.dap.apRead(SECAP, 0x8);
    await this.dap.apWrite(SECAP, 0x18, 0x5C).catch(() => {});        // the port drops as the chip resets
    const t = Date.now();
    while (Date.now() - t < 900) {
      try { await this.reconnect(); if ((await this.status()).boot === 0x20) { await this.must("NOP", [0x01], 2000); return; } } catch (e) { /* still resetting */ }
    }
    throw new Error("the chip did not enter its programming mode (SACI)");
  }

  async waitTx(ms = 2000) { const t = Date.now(); while ((await this.dap.apRead(SECAP, 0x4)) & 1) if (Date.now() - t > ms) throw new Error("SACI: the chip stopped reading commands"); }
  async waitRx(ms) { const t = Date.now(); while (!((await this.dap.apRead(SECAP, 0xC)) & 1)) if (Date.now() - t > ms) throw new Error("SACI: no answer"); }

  async cmd(w, rxMs = 30000, resp = true) {
    const sq = (this.seq++) & 0xff, w0 = ((w[0] & 0xffff00ff) | (sq << 8)) >>> 0;
    await this.waitTx(); await this.dap.apWrite(SECAP, 0x4, 2); await this.dap.apWrite(SECAP, 0x0, w0);
    if (w.length > 1) { await this.waitTx(); await this.dap.apWrite(SECAP, 0x4, 0); for (let i = 1; i < w.length; i++) { if (i > 1) await this.waitTx(); await this.dap.apWrite(SECAP, 0x0, w[i] >>> 0); } }
    await this.waitTx();
    if (!resp) return 0;
    await this.waitRx(rxMs); const hd = await this.dap.apRead(SECAP, 0x8);
    for (let i = 0; i < hd >>> 24; i++) { await this.waitRx(2000); await this.dap.apRead(SECAP, 0x8); }
    if ((hd & 0xff) !== (w0 & 0xff) || ((hd >>> 8) & 0xff) !== sq) throw new Error("SACI: the answer does not match the command");
    return (hd >>> 16) & 0xff;
  }
  async must(name, w, rxMs) { const r = await this.cmd(w, rxMs); this.log(`${name}: ${RES[r] || "0x" + r.toString(16)}`); if (r !== 0) throw new Error(`SACI ${name}: ${RES[r] || "0x" + r.toString(16)}`); }

  // Best-effort end of a write run, errors included: ask the chip to reset and run, then let go of the debugger. Never throws.
  async close() {
    try { await this.boot(); } catch (e) { /* the port may already be gone */ }
    try { await this.dap.disconnect(); } catch (e) { /* ignore */ }
  }
  verifyMain(len, crc) { return this.cmd([0x10, 0, len, crc]); }                   // 0 = SUCCESS, 0x87 = CRC32_MISMATCH
  // Program a whole image: chip erase (the flash key as the second word, as TI's cc23xx_dap.gel ChipErase sends it; on the
  // CC27xx with the retain bit, which needs the valid CCFG on the chip now), main sectors, CRC check, SCFG then CCFG last
  // (CC27xx TRM 9.2.3: the device must not boot into an incomplete image), their own CRC checks, then the caller boots it.
  async program(img, progress = () => {}) {
    const f = this.family;
    if (img.end > f.mainEnd) throw new Error(`the image reaches past 0x${f.mainEnd.toString(16)} on the ${f.name}; refusing`);
    if (img.scfg && f !== FAMILY.cc27) throw new Error(`the image has an SCFG, which the ${f.name} does not have; refusing`);
    if (!img.scfg && f === FAMILY.cc27) throw new Error("the image has no SCFG; a CC27xx needs one to open its debug port again; refusing");
    const bad = [...vectorCheck(img), ...checkConfig(img.ccfg, img.scfg, f)]; if (bad.length) throw new Error("refused: " + bad.join("; "));
    await this.must(f.retain ? "ERASE_CHIP keeping the HSM sectors" : "ERASE_CHIP", [(0x09 | (f.retain << 16)) >>> 0, KEY], 60000);
    const n = img.main.length / SECTOR;
    for (let k = 0; k < n; k++) {
      const d = img.main.subarray(k * SECTOR, (k + 1) * SECTOR);
      if (d.every((x) => x === 0xff)) continue;
      // Each block gets 3 tries with 150 ms between (esptool-js does the same); the chip's CRC32 check below proves the whole write, so no read-back is needed.
      let r = -1;
      for (let a = 1; a <= 3 && r !== 0; a++) {
        try { r = await this.cmd([(0x0E | (SECTOR << 16)) >>> 0, KEY, k * SECTOR, ...words(d)]); } catch (e) { if (a === 3) throw e; r = -1; }
        if (r !== 0 && a < 3) { this.log(`PROG_MAIN_SECTOR 0x${(k * SECTOR).toString(16)}: ${RES[r] || r}, trying again (${a} of 3)`); await new Promise((x) => setTimeout(x, 150)); }
      }
      if (r !== 0) throw new Error(`SACI PROG_MAIN_SECTOR 0x${(k * SECTOR).toString(16)}: ${RES[r] || r} (3 tries)`);
      progress((k + 1) / n);
    }
    await this.must("VERIFY_MAIN CRC32", [0x10, 0, img.main.length, crc32(img.main)]);
    if (img.scfg) await this.must("PROG_SCFG_SECTOR", [(0x1A | (1024 << 16)) >>> 0, KEY, ...words(img.scfg)]);
    await this.must("PROG_CCFG_SECTOR (user record skipped)", [(0x0C | (1 << 16)) >>> 0, KEY, ...words(img.ccfg)]);
    await this.must("VERIFY_CCFG", [(0x11 | (1 << 17)) >>> 0, 0, 0, 0, 0]);
    if (img.scfg) await this.must("VERIFY_SCFG", [0x1B, 0]);
  }
  // BLDR_APP_RESET_DEVICE: reset and run. The chip resets as it takes the command, so the port may drop before a read
  // of TXCTL answers (ack 7); the caller reconnects and reads the boot state, which is the real check.
  async boot() {
    const sq = (this.seq++) & 0xff;
    await this.waitTx(); await this.dap.apWrite(SECAP, 0x4, 2);
    await this.dap.apWrite(SECAP, 0x0, (0x14 | (sq << 8)) >>> 0).catch(() => {});
  }
  // Boot, then wait until the chip has left SACI and its memory port is open. The CC2755P10 is back within 1.5 s; the
  // CC2340R5 (LS470GTU) was not, and read as closed at 1.5 s while open on a plain connection: its SACI timeout runs first.
  async bootAndWait(ms = 12000) {
    if (this.family === FAMILY.cc23) return this.bootQuiet();
    await this.boot();
    const t = Date.now(); let st = null;
    while (Date.now() - t < ms) {
      await new Promise((r) => setTimeout(r, 400));
      try { await this.reconnect(); st = await this.status(); if (st.ahbOpen && st.boot !== 0x20) return st; } catch (e) { /* still restarting */ }
    }
    return st || { ahbOpen: false, boot: -1 };
  }
  // The CC23x0R5's ROM enters SACI again if a debugger is active while it boots (PMCTL BOOTSTA 0x20; 0x36 while it waits
  // for the debugger to let go). TI's ResetWithoutEnteringSaci (cc23xx_xds.gel) resets after the SWD disconnect: send
  // BLDR_APP_RESET_DEVICE with 1, drop the debug power-up requests, put the port back to dormant (line reset, then 0xE3BC),
  // release the pins, keep quiet 2 s, then wake the port again and read the state.
  async bootQuiet() {
    await this.must("BLDR_APP_RESET_DEVICE after the debugger lets go", [(0x14 | (1 << 16)) >>> 0], 3000);
    await this.dap.dpWrite(0x4, 0).catch(() => {});
    await this.dap.cmd([0x12, 56, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF], "DAP_SWJ_Sequence");
    await this.dap.cmd([0x12, 16, 0xBC, 0xE3], "DAP_SWJ_Sequence");
    await this.dap.disconnect();
    await new Promise((r) => setTimeout(r, 2000));
    await this.reconnect();
    return this.status();
  }
}

// ---- Put-back (2026-10-22) ---------------------------------------------------------------------------------------
// The page saves a board's main flash, CCFG and (CC27xx) SCFG before it writes. These two turn those saved files into the
// image program() takes and prove the put-back by reading the chip again.
// bins = { main: Uint8Array (0 to the family's mainEnd), ccfg: 2048 bytes, scfg: 1024 bytes or null }
export function imageFromBackup(bins, family = FAMILY.cc27) {
  const m = bins.main;
  if (!m || m.length < SECTOR || m.length > family.mainEnd) throw new Error("the saved main flash is not the size of this chip's main flash");
  let end = m.length; while (end > 0 && m[end - 1] === 0xff) end--;
  if (end === 0) throw new Error("the saved main flash is blank: there is no program to put back");
  const main = new Uint8Array(Math.ceil(end / SECTOR) * SECTOR).fill(0xff); main.set(m.subarray(0, end));
  return { main, end, ccfg: bins.ccfg, scfg: bins.scfg || null };
}
const bytesOfWords = (w) => { const b = new Uint8Array(w.length * 4); w.forEach((x, i) => { b[i * 4] = x & 0xff; b[i * 4 + 1] = (x >>> 8) & 0xff; b[i * 4 + 2] = (x >>> 16) & 0xff; b[i * 4 + 3] = x >>> 24; }); return b; };
// Read the chip's main flash, CCFG and SCFG through its debug port (open after a boot): what the page saves before a write.
export async function readBins(dap, family = FAMILY.cc27) {
  return { main: bytesOfWords(await dap.readBlock(0, family.mainEnd / 4)), ccfg: bytesOfWords(await dap.readBlock(0x4E020000, 2048 / 4)),
    scfg: family === FAMILY.cc27 ? bytesOfWords(await dap.readBlock(0x4E040000, 1024 / 4)) : null };
}
// First differing byte of two arrays, or -1.
export function firstDiff(a, b) { if (a.length !== b.length) return Math.min(a.length, b.length); for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return i; return -1; }
// Write the saved image back (the chip must be in SACI), start it, read the chip again and compare with the saved files.
// Returns { ok, main, ccfg, scfg } with the offset of the first difference (-1 = identical) for each area.
export async function putBack(sc, bins, progress = () => {}) {
  const fam = sc.family, img = imageFromBackup(bins, fam);
  await sc.program(img, progress);
  const st = await sc.bootAndWait();
  if (!st.ahbOpen) return { ok: false, reason: "the debug port did not open after the put-back" };
  const now = await readBins(sc.dap, fam);
  const res = { ok: true, main: firstDiff(now.main, bins.main), ccfg: firstDiff(now.ccfg, bins.ccfg), scfg: bins.scfg ? firstDiff(now.scfg, bins.scfg) : -1 };
  res.ok = res.main < 0 && res.ccfg < 0 && res.scfg < 0;
  return res;
}

// ---- Radio demo settings table (2026-10-22) ----------------------------------------------------------------------
// The shared parameter block (tools/webflash/params.py, format 1) of the radio demo images (cc2755p10/radio-demos/frl_demo.c) lives in one
// 2 kB sector of main flash, outside the image: words magic 0x50524C46, format 1, count, image id, then one value per setting.
// The Change tab calls writeParams(): one SACI sector write and a reset; no chip erase, no rebuild, and the image's own CRC32 does not cover it.
export const PARAMS_MAGIC = 0x50524C46;
export function paramSector(imageId, values) {
  const w = new Uint8Array(SECTOR).fill(0xff);
  [PARAMS_MAGIC, 1, values.length, imageId, ...values].forEach((v, i) => { w[i * 4] = v & 0xff; w[i * 4 + 1] = (v >>> 8) & 0xff; w[i * 4 + 2] = (v >>> 16) & 0xff; w[i * 4 + 3] = (v >>> 24) & 0xff; });
  return w;
}
// The chip has no single-sector erase over SACI (only ERASE_CHIP), so programming a changed table over an old one would AND the bits.
// A change is therefore a whole write: imageWithParams() returns the image with its settings sector filled, and the page runs program() as usual
// (about 3 s for a small demo). block = the manifest's params block ({addr, image_id, items: [{id, label, min, max, default}]}); values = {id: number}.
export function imageWithParams(img, block, values) {
  const addr = parseInt(block.addr, 16), list = block.items.map((it) => {
    const v = Number(values[it.id] ?? it.default);
    if (!Number.isInteger(v) || v < it.min || v > it.max) throw new Error(`${it.label || it.id} must be a whole number from ${it.min} to ${it.max}`);
    return v;
  });
  if (addr % SECTOR) throw new Error("the settings table is not on a sector boundary; refusing");
  if (addr < img.main.length) throw new Error("the settings table overlaps the program; refusing");
  const main = new Uint8Array(addr + SECTOR).fill(0xff); main.set(img.main);
  main.set(paramSector(block.image_id, list), addr);
  return { ...img, main };      // end stays the program's end: program() refuses an image past the family's main flash by img.end
}
