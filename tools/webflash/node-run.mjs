// Run the browser flasher's own code (site/flash/mspm0-dap.js) against a real board from Node, through
// node-hid instead of WebHID, so the page's logic is tested on hardware without a browser.
//
//   node tools/webflash/node-run.mjs identify --probe ML130001
//   node tools/webflash/node-run.mjs flash --probe ML130001 [--image prebuilt/mspm0l1306/blink-150-150-150-900]
//   node tools/webflash/node-run.mjs colors --probe ML130001 --steps 1:500,6:250,0:250
//   node tools/webflash/node-run.mjs save --probe ML130001 --file copy.json
//   node tools/webflash/node-run.mjs restore --probe ML130001 --file copy.json [--pattern 150,150,150,900]
//
// node-hid is not a dependency of this repository: install it in a scratch folder and set NODE_PATH to its
// node_modules. Take the probe lock first (python3 tools/frl/lock.py exec --serial <s> --op webflash -- node ...).
import { createRequire } from "module";
import { readFileSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { Dap, Mspm0, Session, run, elfSegments, sha256, patternCheck } from "../../site/flash/mspm0-dap.js";
import { writeFileSync } from "fs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");

const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const mode = process.argv[2];
const serial = arg("--probe");
if (!serial) { console.log("name the probe with --probe <serial>"); process.exit(2); }

class NodeHidTransport {
  constructor(path) { this.path = path; }
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

const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === serial && d.interface === 5);
if (!info) { console.log(`probe ${serial} has no CMSIS-DAP HID interface on USB`); process.exit(2); }

const routes = JSON.parse(readFileSync(join(ROOT, "data", "routes.json")));
const b = routes.boards.mspm0l1306, led = b.leds[0], v = b.recipes["blink-pattern"].verify;
const n = (s) => parseInt(s, 16);
const board = {
  kit: b.kit,
  identity: { addr: n(b.target.identity.addr), deviceid: n(b.target.identity.deviceid), deviceidMask: n(b.target.identity.deviceid_mask) },
  flash: { start: n(b.target.writable[0].start), end: n(b.target.writable[0].end) },
  led: { gpio: n(led.gpio_base), bit: led.bit, label: led.label },
  gpio: { dout: n(b.gpio_offsets.dout), doe: n(b.gpio_offsets.doe) },
  check: { seconds: v.seconds || 4, tolMs: v.tolerance_ms || 40 },
  ram: 4096, ramStart: 0x20000000, cmdbyten: 0xFF,
};
// --board <id>: another board from site/flash/bench/boards.json, with everything the page's engine reads (flash controller,
// semaphore, alias, family). Hex strings become numbers. The LED comes from the image's manifest (--image).
const num = (v) => (typeof v === "string" && /^0x[0-9a-f]+$/i.test(v) ? parseInt(v, 16) : v);
const deep = (o) => (o && typeof o === "object" && !Array.isArray(o) ? Object.fromEntries(Object.entries(o).map(([k, x]) => [k, deep(x)])) : num(o));
if (arg("--board")) {
  const bb = JSON.parse(readFileSync(join(ROOT, "site", "flash", "bench", "boards.json"))).boards.find((x) => x.id === arg("--board"));
  if (!bb || !bb.flash) { console.log("no such programmable board"); process.exit(2); }
  const { resources, recovery, picture, printed, note, readOnly, ...rest } = bb;
  Object.assign(board, deep(rest));
  const img = arg("--image");
  if (img) { const m = JSON.parse(readFileSync(join(ROOT, img, "manifest.json"))); board.led = { gpio: n(m.led.gpio), bit: m.led.bit, label: m.led.label }; board.led.activeHigh = m.active_high; }
}
if (arg("--start")) board.start = arg("--start");   // "pwrap": start through the power wrapper instead of the Arm reset request
const ui = { say: (s) => console.log("  " + s), now: () => performance.now(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)) };
const t = new NodeHidTransport(info.path);

if (mode === "identify") {
  await t.open();
  const dap = new Dap(t);
  console.log("firmware", await dap.info(0x04), "packet", (await t.transfer([0x00, 0xFF]))[2]);
  const idr = await dap.connect();
  console.log("DPIDR", "0x" + idr.toString(16));
  console.log("AP0 IDR", "0x" + (await dap.apRead(0, 0xFC)).toString(16));
  const m = new Mspm0(dap, board);
  console.log(await m.identify());
  const dout = await dap.read32(board.led.gpio + board.gpio.dout);
  console.log("GPIOA DOUT", "0x" + dout.toString(16));
  const s = await dap.sampleWord(board.led.gpio + board.gpio.dout, 3000, ui.now);
  const pat = (arg("--pattern") || "150,150,150,900").split(",").map(Number);
  const r = patternCheck(s, board.led.bit, pat, board.check.tolMs);
  console.log(`samples ${r.samples} in ${r.span} ms (${r.rate}/s); levels`, JSON.stringify(r.levels.slice(0, 10)), "match", r.ok);
  await dap.disconnect();
  await t.close();
} else if (mode === "flash") {
  const dir = join(ROOT, arg("--image", "prebuilt/mspm0l1306/blink-150-150-150-900"));
  const man = JSON.parse(readFileSync(join(dir, "manifest.json")));
  const raw = readFileSync(join(dir, man.image.file));
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength);
  const h = await sha256(buf);
  if (h !== man.image.sha256) { console.log("image hash does not match its manifest"); process.exit(1); }
  const image = { segments: elfSegments(buf), pattern: man.pattern_ms };
  const t0 = performance.now();
  const r = await run(t, board, image, ui);
  console.log(JSON.stringify({ ...r, levels: r.levels.slice(0, 12), total_ms: Math.round(performance.now() - t0) }));
  process.exit(r.ok && r.outputEnabled ? 0 : 1);
} else if (mode === "colors") {
  // --steps 1:500,6:250,0:250   colour bits (1 red, 2 green, 4 blue) and milliseconds
  const steps = (arg("--steps") || "1:300,2:300,4:300,0:300").split(",").map((x) => x.split(":").map(Number));
  const dir = join(ROOT, "prebuilt", "mspm0l1306", "color-player");
  const man = JSON.parse(readFileSync(join(dir, "manifest.json")));
  const raw = readFileSync(join(dir, man.image.file));
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength);
  if ((await sha256(buf)) !== man.image.sha256) { console.log("image hash does not match its manifest"); process.exit(1); }
  const s = new Session(t, board, ui);
  try {
    await s.start();
    const sent = await s.sendColors({ segments: elfSegments(buf) }, man.table, steps);
    await s.run();
    const r = await s.checkColors({ ...man.table, gpio: "0x400A0000" }, steps);
    console.log(JSON.stringify({ sent, ok: r.ok, levels: r.levels.slice(0, 10), first: r.first, rate: r.rate }));
    process.exit(r.ok ? 0 : 1);
  } finally { await s.end(); }
} else if (mode === "save" || mode === "restore") {
  const file = arg("--file");
  if (!file) { console.log("name the file with --file"); process.exit(2); }
  const s = new Session(t, board, ui);
  try {
    await s.start();
    const t0 = performance.now();
    if (mode === "save") {
      const b = await s.save();
      writeFileSync(file, JSON.stringify(b));
      console.log(JSON.stringify({ saved: file, sha256: b.sha256, traceid: b.traceid, ms: Math.round(performance.now() - t0) }));
    } else {
      const ms = await s.restore(JSON.parse(readFileSync(file)));
      await s.run();
      console.log(JSON.stringify({ restored: file, ms }));
      const pat = arg("--pattern");
      if (pat) { const r = await s.check(pat.split(",").map(Number)); console.log(JSON.stringify({ ok: r.ok, levels: r.levels.slice(0, 8), outputEnabled: r.outputEnabled })); }
    }
  } finally { await s.end(); }
} else {
  console.log("modes: identify, flash, save, restore");
  process.exit(2);
}
