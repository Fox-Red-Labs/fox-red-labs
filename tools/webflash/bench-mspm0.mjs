// Hardware check of the browser bench's own module (site/flash/mspm0-dap.js, unchanged) on one MSPM0 LaunchPad that
// is not the L1306, from Node through node-hid. Per board, each phase on a fresh connection:
//   1 identify and save all of main flash   2 flash the blink image and time the LED   3 flash the color player,
//   write two sequences, time the LEDs   4 restore the saved copy and read it back.
// A fault stops the board where it is: nothing tries to recover it. Never writes NONMAIN, never mass-erases.
//
//   python3 tools/frl/lock.py exec --serial MG000001 --op webflash-mspm0g3519 -- \
//     node tools/webflash/bench-mspm0.mjs mspm0g3519 --work <scratch dir>
//
// The board object comes from site/flash/bench/boards-draft/<board>.json (ram, cmdbyten, flash, leds, gpio).
// Needs NODE_PATH to hold node-hid (see tools/webflash/node-run.mjs).
import { createRequire } from "module";
import { readFileSync, writeFileSync, mkdirSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { createHash } from "crypto";
import { Dap, Session, elfSegments, sequenceCheck } from "../../site/flash/mspm0-dap.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const boardId = process.argv[2], work = arg("--work");
mkdirSync(work, { recursive: true });
const draft = JSON.parse(readFileSync(join(ROOT, "site/flash/bench/boards-draft", boardId + ".json")));
const n = (s) => parseInt(s, 16);
const hex = (v) => "0x" + (v >>> 0).toString(16).toUpperCase().padStart(8, "0");
const sha = (b) => createHash("sha256").update(b).digest("hex");
const chans = draft.check_channels.map((c) => ({ ...c, addr: n(c.gpio) }));
const red = chans[0];
const board = {
  kit: draft.kit,
  identity: { addr: n(draft.identity.addr), deviceid: n(draft.identity.deviceid), deviceidMask: n(draft.identity.deviceid_mask) },
  flash: { start: n(draft.flash.start), end: n(draft.flash.end) },
  ram: draft.ram.bytes, ramStart: n(draft.ram.start), cmdbyten: n(draft.cmdbyten),
  led: { gpio: red.addr, bit: red.bit, label: red.label },
  gpio: { dout: n(draft.gpio_offsets.dout), doe: n(draft.gpio_offsets.doe) },
  check: { seconds: 4, tolMs: 40 },
};
const now = () => performance.now();
const ui = { say: () => {}, now, sleep: (ms) => new Promise((r) => setTimeout(r, ms)) };
const t00 = now();
const log = (s) => console.log(`[${((now() - t00) / 1000).toFixed(1)}s] ${s}`);
const res = { board: boardId, probe: draft.probe_serial, module: "site/flash/mspm0-dap.js unchanged", started: new Date().toISOString(), steps: [] };
const step = async (name, fn) => { const t0 = now(); const r = await fn(); const ms = Math.round(now() - t0); res.steps.push({ name, ms, ...(r || {}) }); log(`${name}: ${ms} ms ${r ? JSON.stringify(r) : ""}`); return r; };

class T {
  constructor(p) { this.path = p; }
  async open() { this.dev = new HID.HID(this.path); }
  async close() { try { this.dev.close(); } catch (e) { /* closed */ } }
  async transfer(bytes) {
    const out = new Array(65).fill(0);
    bytes.forEach((b, i) => { out[i + 1] = b; });
    this.dev.write(out);
    const r = this.dev.readTimeout(3000);
    if (!r.length) throw new Error("no reply from the debugger");
    return Uint8Array.from(r);
  }
}
async function session(phase, fn) {
  const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === draft.probe_serial && d.interface === 5);
  if (!info) throw new Error("probe not on USB");
  const s = new Session(new T(info.path), board, ui);
  await step(`${phase}: connect and identify`, async () => ({ ...(await s.start()), dap_firmware: await s.dap.info(0x04) }));
  try { return await fn(s); } finally { await s.end().catch(() => {}); await ui.sleep(300); }
}
const load = (dir) => {
  const man = JSON.parse(readFileSync(join(ROOT, dir, "manifest.json"))), raw = readFileSync(join(ROOT, dir, man.image.file));
  return { man, segments: elfSegments(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength)) };
};

// The colour check on the pins that sit on one GPIO port (all of them, except on the G5187: GPIOA only).
async function checkColorsOnePort(s, steps, port) {
  const mine = chans.filter((c) => c.addr === port), mask = mine.reduce((a, c) => a | (1 << c.ch), 0);
  const cycle = steps.reduce((a, [, ms]) => a + ms, 0), secs = Math.min(14, Math.max(4, (2.2 * cycle + 300) / 1000));
  const samples = await s.dap.sampleWord(port + board.gpio.dout, secs * 1000, now);
  const lvl = (v) => mine.reduce((a, c) => a | (((v >>> c.bit) & 1) << c.ch), 0);
  const r = sequenceCheck(samples, lvl, steps.map(([c, ms]) => [c & mask, ms]), board.check.tolMs);
  return { pass: r.ok, mask, levels: r.levels.slice(0, 8), samples: r.samples, rate_per_s: r.rate };
}

let backup, fault = false;
try {
  await session("1", async (s) => {
    await step("RAM alias check (SRAM only, no flash)", async () => {
      const lo = draft.ram.start === "0x20000000" ? 0x20000100 : 0x20000100, hi = n(draft.ram.start) + 0x100;
      await s.m.halt(); await s.dap.write32(hi, 0xA5C3F00D);
      const viaLow = await s.dap.read32(lo).catch(() => null);
      await s.dap.write32(hi, 0);
      return { wrote: hex(hi), read_at: hex(lo), value: viaLow === null ? null : hex(viaLow), alias_of_loader_base: viaLow === 0xA5C3F00D || lo === hi };
    });
    const sf = await s.dap.read32(0x41C40018);
    res.factory = { flash_kb: sf & 0xFFF, banks: ((sf >>> 12) & 3) + 1, sramflash: hex(sf) };
    await step("save main flash", async () => {
      backup = await s.m.readAll(); writeFileSync(join(work, `${boardId}-main-flash.bin`), backup);
      return { bytes: backup.length, sha256: sha(backup), ...res.factory };
    });
    await step("start the board's own program again (never leave a core halted)", async () => { await s.run(); });
  });
  const bl = load(draft.images.blink.dir);
  await session("2", async (s) => {
    await step("flash blink image", async () => ({ flash_ms: await s.flashImage({ segments: bl.segments }), image_sha256: bl.man.image.sha256 }));
    await step("run blink (boot reset)", async () => { await s.run(); });
    await step("check blink LED", async () => {
      const r = await s.check(bl.man.pattern_ms);
      return { pass: r.ok, outputEnabled: r.outputEnabled, levels: r.levels.slice(0, 8), samples: r.samples, rate_per_s: r.rate };
    });
  });
  const cp = load(draft.images.color_player.dir), table = cp.man.table;
  const seqs = [[[1, 500], [2, 500], [4, 500], [0, 500]], [[7, 250], [0, 250], [3, 400], [6, 400]]];
  const ports = [...new Set(chans.map((c) => c.addr))];
  await session("3", async (s) => {
    for (const [i, steps] of seqs.entries()) {
      await step(`send colors ${i + 1}`, async () => ({ ...(await s.sendColors({ segments: cp.segments }, table, steps)), image_sha256: cp.man.image.sha256 }));
      await step(`run color player ${i + 1}`, async () => { await s.run(); });
      for (const port of ports) await step(`check colors ${i + 1} on ${hex(port)}`, async () => checkColorsOnePort(s, steps, port));
    }
  });
} catch (e) {
  fault = true; res.fault = String(e.message || e) + (e.next ? " / " + e.next : "");
  log("FAULT, stopping this board: " + res.fault);
}
if (!fault) {
  try {
    await session("4", async (s) => {
      await step("restore saved flash", async () => { await s.m.restoreAll(backup); return {}; });
      await step("read back after restore", async () => { const b = await s.m.readAll(); if (sha(b) !== sha(backup)) throw new Error("readback differs"); return { identical: true, sha256: sha(b) }; });
      // The shipped image may put the chip to sleep before this write lands (G3519, 2026-10-15): the flash is already
      // verified, so a fault here is recorded, not treated as a failed restore. The following Session.start() wakes the chip.
      await step("start restored program", async () => { try { await s.run(); return { started: true }; } catch (e) { return { started: false, note: String(e.message || e) }; } });
    });
  } catch (e) { fault = true; res.fault = String(e.message || e); log("FAULT in restore: " + res.fault); }
}
res.finished = new Date().toISOString();
writeFileSync(join(work, `${boardId}-result.json`), JSON.stringify(res, null, 1));
console.log(fault ? "DONE WITH FAULT" : "DONE OK");
