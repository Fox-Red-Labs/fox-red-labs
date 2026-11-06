// Program a blank, debug-locked CC27xx (CCFG blank, SCFG erased) over SACI, with no CPU connection.
// TI's flow: CC27xx TRM SWCU195A section 9.2.4.1 "Initial Programming of a New Device", commands in section 9.3.
// Run with the CCStudio scripting runner and ABSOLUTE paths:
//   /Applications/ti/ccs2101/ccs/scripting/run.sh <repo>/cc2755p10/recovery/saci_program.js <ccxml> <app.hex> <ccfg.bin> <scfg.bin> check|write
// check: enter SACI, NOP, CCFG blank check, blank check of the MAIN sectors the app covers. Writes nothing.
// write: the same checks (stops before any write if one fails), then PROG_MAIN_SECTOR per sector, VERIFY_MAIN_SECTORS (CRC32),
//        PROG_SCFG_SECTOR, PROG_CCFG_SECTOR (user record skipped, stays blank), VERIFY_CCFG and VERIFY_SCFG (embedded CRCs),
//        then BLDR_APP_RESET_DEVICE so the chip boots with the new configuration.
// It never sends SACI_CMD_FLASH_ERASE_CHIP: with the CCFG invalid, a chip erase would no longer keep the HSM firmware (TRM 9.3.2.3.1).
// It never touches the HSM area (0xE8000 to 0xFFFFF) and refuses an app that reaches it.
// It refuses a CCFG or SCFG that is not debug-open with every permission allowed, has a bad CRC, or sets secure boot or keys.
// Every SACI result is printed as it arrives.
const fs = require("fs");
const a = process.argv.slice(process.argv.findIndex(x => x.endsWith("saci_program.js")) + 1);
const [ccxml, appHex, ccfgPath, scfgPath, mode] = a;
if (!["check", "write"].includes(mode)) { console.log("usage: saci_program.js <ccxml> <app.hex> <ccfg.bin> <scfg.bin> check|write"); process.exit(2); }
const t0 = Date.now(); const log = (...x) => console.log(((Date.now() - t0) / 1000).toFixed(2).padStart(7) + "s", ...x);
const hex = v => "0x" + (v >>> 0).toString(16).padStart(8, "0");

// ---------- CRC32 (same as zlib; the SDK SCFG and the board's CCFG CRCs match it) ----------
const T = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; T[n] = c >>> 0; }
const crc32 = b => { let c = 0xFFFFFFFF; for (const x of b) c = T[(c ^ x) & 0xff] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const words = b => { const w = []; for (let i = 0; i < b.length; i += 4) w.push(u32(b, i)); return w; };

// ---------- inputs and safety checks ----------
const SECTOR = 2048, HSM_START = 0xE8000;
const mem = new Map(); let base = 0;
for (const line of fs.readFileSync(appHex, "utf8").split(/\r?\n/)) {
  if (!line.startsWith(":")) continue;
  const b = Buffer.from(line.slice(1), "hex"), n = b[0], ad = (b[1] << 8) | b[2], ty = b[3];
  if (ty === 4) base = ((b[4] << 8) | b[5]) << 16; else if (ty === 0) for (let i = 0; i < n; i++) mem.set(base + ad + i, b[4 + i]);
}
const mainAddrs = [...mem.keys()].filter(x => x < 0x4E000000);
const appEnd = Math.max(...mainAddrs) + 1;
if (appEnd > HSM_START) throw new Error("app reaches the HSM area; refusing");
const nSect = Math.ceil(appEnd / SECTOR);
const app = Buffer.alloc(nSect * SECTOR, 0xff); for (const x of mainAddrs) app[x] = mem.get(x);
const ccfg = fs.readFileSync(ccfgPath), scfg = fs.readFileSync(scfgPath);
const bad = [];
if (ccfg.length !== 2048) bad.push("CCFG is not 2048 bytes");
if (scfg.length !== 1024) bad.push("SCFG is not 1024 bytes");
if (u32(ccfg, 0x0C) !== crc32(ccfg.subarray(0, 0x0C))) bad.push("CCFG bootCfg CRC");
if (u32(ccfg, 0x74C) !== crc32(ccfg.subarray(0x10, 0x74C))) bad.push("CCFG main CRC");
if (u32(ccfg, 0x7FC) !== crc32(ccfg.subarray(0x7D0, 0x7FC))) bad.push("CCFG debugCfg CRC");
if (u32(ccfg, 0x18) !== 0xAAAAAAAA) bad.push("CCFG permissions not all ALLOW (allowDebugPort, allowChipErase, ...)");
if (((u32(ccfg, 0x1C) >>> 4) & 0xf) !== 0xA) bad.push("CCFG allowMainAppErase not ALLOW");
if (ccfg[0x7D0] !== 0x5A) bad.push("CCFG debugCfg.authorization is not DBGOPEN (0x5A)");
if (u32(ccfg, 0x08) !== 0) bad.push("CCFG pAppVtor is not 0x0 (where the app's vector table is)");
if (u32(scfg, 228) !== crc32(scfg.subarray(0, 228))) bad.push("SCFG CRC");
if (u32(scfg, 208) !== 0xAAAAAAAA || (u32(scfg, 212) & 0xf) !== 0xA) bad.push("SCFG permissions not all ALLOW");
if (u32(scfg, 164) !== 0x2F8EA5A5) bad.push("SCFG secure boot is not NO_AUTH");
if (!scfg.subarray(0, 112).every(x => x === 0xff)) bad.push("SCFG has an HSM key hash or debug keys");
if (!scfg.subarray(176, 208).every(x => x === 0xff)) bad.push("SCFG has a key-update key hash");
if (bad.length) { console.log("REFUSED:", bad.join("; ")); process.exit(1); }
const appCrc = crc32(app);
log("app", appHex, "end", hex(appEnd), "->", nSect, "sectors, CRC32", hex(appCrc));
log("CCFG ok: debug open, permissions all ALLOW, no write/erase protection beyond the board's shipped values; SCFG ok: no keys, secure boot off");

// ---------- SACI over SEC-AP (TRM 9.3.1) ----------
const ds = initScripting(); ds.setScriptingTimeout(120000);
ds.configure(ccxml);
const s = ds.openSession(/CS_DAP/); s.target.connect();
const rd = r => Number(BigInt.asUintN(32, BigInt(s.expressions.evaluate("'REG'::" + r))));
const wr = (r, v) => s.expressions.evaluate("'REG'::" + r + " = 0x" + (v >>> 0).toString(16));
const status = () => { const v = rd("CFGAP_DEVICESTATUS"); return { v, boot: (v >>> 8) & 0xff, ahb: (v >>> 24) & 1 }; };
const waitTx = (ms = 2000) => { const t = Date.now(); while (rd("SECAP_TXCTL") & 1) if (Date.now() - t > ms) throw new Error("TXD_FULL timeout, TXCTL " + hex(rd("SECAP_TXCTL"))); };
const waitRx = (ms) => { const t = Date.now(); while (!(rd("SECAP_RXCTL") & 1)) if (Date.now() - t > ms) throw new Error("no response, RXCTL " + hex(rd("SECAP_RXCTL"))); };
let seq = 0x40;
const RES = { 0: "SUCCESS", 0x80: "INVALID_CMD_ID", 0x81: "INVALID_ADDRESS_PARAM", 0x82: "INVALID_SIZE_PARAM", 0x83: "INVALID_KEY_PARAM", 0x84: "FLASH_FSM_ERROR",
  0x85: "PARAM_BUFFER_OVERFLOW", 0x86: "NOT_ALLOWED", 0x87: "CRC32_MISMATCH", 0x89: "BLANK_CHECK_FAILED", 0xFF: "CMD_FAILED" };
function cmd(name, w, { resp = true, rxMs = 30000 } = {}) {
  const sq = (seq++) & 0xff, w0 = ((w[0] & 0xffff00ff) | (sq << 8)) >>> 0;
  waitTx(); wr("SECAP_TXCTL", 2); wr("SECAP_TXD", w0);
  if (w.length > 1) { waitTx(); wr("SECAP_TXCTL", 0); for (let i = 1; i < w.length; i++) { if (i > 1) waitTx(); wr("SECAP_TXD", w[i]); } }
  waitTx();
  if (!resp) { log(name, "sent (no response by design)"); return null; }
  waitRx(rxMs); const h = rd("SECAP_RXD"); const n = (h >>> 24) & 0xff, data = [];
  for (let i = 0; i < n; i++) { waitRx(2000); data.push(rd("SECAP_RXD")); }
  const r = (h >>> 16) & 0xff;
  log(name, "hdr", hex(h), "result 0x" + r.toString(16), RES[r] || "?", data.length ? "data " + data.map(hex).join(" ") : "");
  if ((h & 0xff) !== (w0 & 0xff) || ((h >>> 8) & 0xff) !== sq) throw new Error("response does not match the command");
  return r;
}
const must = (name, r) => { if (r !== 0) { const st = status(); log("STOP after", name, "DEVICESTATUS", hex(st.v)); s.target.disconnect(); ds.shutdown(); process.exit(1); } };
const KEY = 0xB7E3A08F;

let st = status(); log("start DEVICESTATUS", hex(st.v), "boot 0x" + st.boot.toString(16), "AHB-AP", st.ahb ? "open" : "closed",
  "TXCTL", hex(rd("SECAP_TXCTL")), "RXCTL", hex(rd("SECAP_RXCTL")));if (st.ahb) { log("AHB-AP is already open: this chip is not locked, nothing to do"); s.target.disconnect(); ds.shutdown(); process.exit(0); }
// Enter SACI with TI's own EnterSaci() (CC23xx/CC27xx flash library, called by cc23xx_dap.gel and cc23xx_xds.gel): an SWD reset
// through SEC-AP (TRM Table 9-1, "writing value 0x5C to SWD:SECAP.RSTCTL.RST"); TRM 9.1.4: "if SWD is connected, the device
// management interface SACI is always entered"; cc23xx_dap.gel: the device "will therefore stay in SACI mode" afterwards.
// It runs on the Cortex_M33_0 session without connecting the CPU (no OnPreTargetConnect, no Secure Debug Manager).
// A raw RSTCTL write from this script (2026-10-09 18:09) lost the DAP for 3 s and the 1 s SACI timeout ran out.
if (rd("SECAP_RXCTL") & 1) log("stale RXD word read and dropped:", hex(rd("SECAP_RXD")));
log("EnterSaci() (TI flash library) to reset into SACI");
const m = ds.openSession(/Cortex_M33_0/);
try { log("EnterSaci() returned", m.expressions.evaluate("EnterSaci()")); } catch (e) { log("EnterSaci() error:", e.message); }
{ const t = Date.now(); let ok = false; while (Date.now() - t < 3000) { try { st = status(); if (st.boot === 0x20) { ok = true; break; } } catch (e) { } }
  if (!ok) { try { s.target.disconnect(); s.target.connect(); st = status(); ok = st.boot === 0x20; } catch (e) { log("DAP reconnect:", e.message); } }
  if (!ok) { log("did not reach SACI (0x20); last DEVICESTATUS", hex(st.v)); ds.shutdown(); process.exit(1); } }
must("NOP", cmd("NOP", [0x01], { rxMs: 2000 }));
log("in SACI, DEVICESTATUS", hex(status().v));
must("VERIFY_CCFG blank check", cmd("VERIFY_CCFG blank check", [(0x11 | (1 << 31)) >>> 0, 0, 0, 0, 0]));
must("VERIFY_MAIN blank check", cmd("VERIFY_MAIN blank check " + hex(0) + "+" + hex(nSect * SECTOR), [(0x10 | (1 << 31)) >>> 0, 0, nSect * SECTOR, 0]));
if (mode === "check") { log("check done: SACI answers, CCFG blank, app sectors blank. Nothing written. DEVICESTATUS", hex(status().v)); s.target.disconnect(); ds.shutdown(); process.exit(0); }

for (let k = 0; k < nSect; k++) {
  const a0 = k * SECTOR; let end = Math.min(a0 + SECTOR, appEnd); end = a0 + Math.ceil((end - a0) / 4) * 4;
  const d = app.subarray(a0, end);
  must("PROG_MAIN_SECTOR", cmd("PROG_MAIN_SECTOR " + hex(a0) + " " + d.length + " B", [(0x0E | (d.length << 16)) >>> 0, KEY, a0, ...words(d)]));
}
must("VERIFY_MAIN crc", cmd("VERIFY_MAIN CRC32 " + hex(0) + "+" + hex(nSect * SECTOR), [0x10, 0, nSect * SECTOR, appCrc]));
// SCFG before CCFG: TRM 9.2.3 says SCFG programming is "typically invoked near the end of a flash image update, just before programming
// the CCFG", and the CCFG last "so that the device does not boot into an incomplete image". If the SCFG step fails, the CCFG stays blank.
must("PROG_SCFG_SECTOR", cmd("PROG_SCFG_SECTOR 1024 B", [(0x1A | (1024 << 16)) >>> 0, KEY, ...words(scfg)]));
must("PROG_CCFG_SECTOR", cmd("PROG_CCFG_SECTOR (user record skipped)", [(0x0C | (1 << 16)) >>> 0, KEY, ...words(ccfg)]));
must("VERIFY_CCFG", cmd("VERIFY_CCFG embedded CRCs, user record skipped", [(0x11 | (1 << 17)) >>> 0, 0, 0, 0, 0]));
must("VERIFY_SCFG", cmd("VERIFY_SCFG embedded CRC", [0x1B, 0]));
cmd("BLDR_APP_RESET_DEVICE (reset, SACI times out, boot)", [0x14], { resp: false });
{ const t = Date.now(); let last = -1; while (Date.now() - t < 8000) { try { st = status(); if (st.v !== last) { log("DEVICESTATUS", hex(st.v), "boot 0x" + st.boot.toString(16), "AHB-AP", st.ahb ? "open" : "closed"); last = st.v; } } catch (e) { } } }
log(st.ahb ? "DONE: debug port (AHB-AP) open" : "AHB-AP still closed");
s.target.disconnect(); ds.shutdown();
