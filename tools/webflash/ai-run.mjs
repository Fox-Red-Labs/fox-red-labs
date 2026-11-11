// Run the Digital Lab's AI images and parameter block on a real board from Node (the page's own debugger code,
// site/flash/mspm0-dap.js, through node-hid), so the picker's images are tested on hardware without a browser.
//
//   python3 tools/frl/lock.py exec --serial MG000001 --op webflash -- \
//     node tools/webflash/ai-run.mjs --probe MG000001 --board mspm0g3519 save --file backup.json
//   ... ai-run.mjs ... model --image prebuilt/mspm0g3519/digit-1k        flash it, read the self-test, ask the ten stored digits
//   ... ai-run.mjs ... model --image prebuilt/mspm0g3519/occ-500         flash it, read the self-test, ask the 20 stored windows
//   ... ai-run.mjs ... params --image prebuilt/mspm0g3519/blink-150-150-150-900 --values 300,300,300,300   write the shared block, reset, time the LED
//   ... ai-run.mjs ... restore --file backup.json                        put the saved copy back and read it back
//
// node-hid is not a dependency of this repository: install it in a scratch folder and set NODE_PATH to its node_modules.
// Take the probe lock first. Prints one JSON line per step; exit code 0 only when every check passed.
import { createRequire } from "module";
import { readFileSync, writeFileSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { Session, elfSegments, sha256, sequenceCheck } from "../../site/flash/mspm0-dap.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const mode = process.argv[2], serial = arg("--probe"), boardId = arg("--board");
if (!serial || !boardId || !mode) { console.log("usage: <mode> --probe SERIAL --board ID ... (modes: save, restore, model, params, read)"); process.exit(2); }
const n = (s) => parseInt(s, 16);
const out = (o) => console.log(JSON.stringify(o));

class NodeHidTransport {
  constructor(path) { this.path = path; }
  async open() { this.dev = new HID.HID(this.path); }
  async close() { try { this.dev.close(); } catch (e) { /* closed */ } }
  async transfer(bytes) {
    const o = new Array(65).fill(0); bytes.forEach((b, i) => { o[i + 1] = b; });
    this.dev.write(o);
    const r = this.dev.readTimeout(3000);
    if (!r.length) throw new Error("no reply from the debugger");
    return Uint8Array.from(r);
  }
}
const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === serial && d.interface === 5);
if (!info) { out({ error: `probe ${serial} has no CMSIS-DAP HID interface on USB` }); process.exit(2); }

const bb = JSON.parse(readFileSync(join(ROOT, "site", "flash", "bench", "boards.json"))).boards.find((x) => x.id === boardId);
if (!bb || !bb.flash) { out({ error: "no such programmable board" }); process.exit(2); }
const board = { kit: bb.kit, identity: { addr: n(bb.identity.addr), deviceid: n(bb.identity.deviceid), deviceidMask: n(bb.identity.deviceidMask) },
  flash: { start: n(bb.flash.start), end: n(bb.flash.end) }, led: { gpio: 0x400A0000, bit: 0, label: "", activeHigh: true }, gpio: { dout: n(bb.gpio.dout), doe: n(bb.gpio.doe) },
  check: { seconds: 4, tolMs: 40 }, ram: bb.ram, ramStart: n(bb.ramStart), cmdbyten: n(bb.cmdbyten) };
const ui = { say: (s) => { if (process.env.VERBOSE) console.log("  " + s); }, now: () => performance.now(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)) };
const s = new Session(new NodeHidTransport(info.path), board, ui);
let ok = true;
const check = (name, pass, detail) => { ok = ok && pass; out({ check: name, pass, ...detail }); };

async function imageOf(dir) {
  const man = JSON.parse(readFileSync(join(ROOT, dir, "manifest.json")));
  const raw = readFileSync(join(ROOT, dir, man.image.file));
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength);
  if ((await sha256(buf)) !== man.image.sha256) throw new Error("image hash does not match its manifest");
  return { man, image: { segments: elfSegments(buf) } };
}
const waitFor = async (addr, magic, ms) => { const t0 = performance.now(); while (performance.now() - t0 < ms) { if ((await s.dap.read32(addr)) >>> 0 === magic) return true; await ui.sleep(100); } return false; };

// BitNetMCU's ten stored digits (16 x 16 int8) and their labels, from the header the images were built with
function storedDigits() {
  const t = readFileSync(join(ROOT, "third-party", "bitnetmcu", "BitNetMCU_MNIST_test_data.h"), "utf8");
  const ins = [], labels = [];
  for (let k = 0; k < 10; k++) {
    const m = t.match(new RegExp(`input_data_${k}\\[[^\\]]*\\]\\s*=\\s*\\{([^}]*)\\}`));
    ins.push(m[1].split(",").map((v) => v.trim()).filter(Boolean).map((v) => (Number(v) << 24) >> 24));   // 0xEC is -20
    labels.push(parseInt(t.match(new RegExp(`label_${k}\\s*=\\s*(\\d+)`))[1], 10));
  }
  return { ins, labels };
}

async function ask(addr, seqOff, outOff, fill, readOff, readN) {
  await fill();
  const seq = ((await s.dap.read32(addr + seqOff)) + 1) >>> 0;
  await s.dap.write32(addr + seqOff, seq);
  const t0 = performance.now();
  while (performance.now() - t0 < 3000) {
    if ((await s.dap.read32(addr + outOff)) === seq) return await s.dap.readBlock(addr + readOff, readN);
    await ui.sleep(20);
  }
  throw new Error("the chip did not answer within 3 s");
}

try {
  await s.start();
  if (mode === "save") {
    const b = await s.save(); writeFileSync(arg("--file"), JSON.stringify(b));
    out({ saved: arg("--file"), sha256: b.sha256, traceid: b.traceid });
  } else if (mode === "restore") {
    const ms = await s.restore(JSON.parse(readFileSync(arg("--file")))); await s.run();
    out({ restored: arg("--file"), ms });
  } else if (mode === "model") {
    const { man, image } = await imageOf(arg("--image"));
    const t0 = performance.now();
    const has = await s.m.holds(image.segments);
    if (!has) await s.flashImage(image);
    await s.run();
    out({ flashed: !has, ms: Math.round(performance.now() - t0), bytes: man.image.bytes });
    if (man.digit) {
      const a = n(man.digit.addr), magic = n(man.digit.magic) >>> 0;
      check("digit reader started", await waitFor(a, magic, 10000), {});
      const clk = await s.dap.read32(a + 4), right = await s.dap.read32(a + 8), nt = await s.dap.read32(a + 12);
      check("self-test", right === nt && nt === 10, { right, nt, clkMhz: clk / 1e6 });
      const { ins, labels } = storedDigits();
      let good = 0; const cyc = [];
      for (let d = 0; d < 10; d++) {
        const words = []; for (let k = 0; k < 256; k += 4) words.push(((ins[d][k] & 0xFF) | ((ins[d][k + 1] & 0xFF) << 8) | ((ins[d][k + 2] & 0xFF) << 16) | ((ins[d][k + 3] & 0xFF) << 24)) >>> 0);
        const r = await ask(a, 16, 20, () => s.dap.writeBlock(a + 32, words), 24, 2);
        good += r[0] === labels[d]; cyc.push(r[1]);
      }
      check("ten stored digits through the mailbox", good === 10, { good, cyclesMin: Math.min(...cyc), cyclesMax: Math.max(...cyc), cyclesMean: Math.round(cyc.reduce((a, b) => a + b, 0) / cyc.length), clkMhz: clk / 1e6, model: man.digit.model });
    } else if (man.occ) {
      const a = n(man.occ.addr), magic = n(man.occ.magic) >>> 0;
      check("occupancy program started", await waitFor(a, magic, 15000), {});
      const w = await s.dap.readBlock(a, 7);
      out({ clkMhz: w[1] / 1e6, nTest: w[2], sameRef: w[3], right: w[4], cycMin: w[5], cycMax: w[6] });
      check("self-test matches the reference on all 20", w[3] === 20 && w[2] === 20, { sameRef: w[3] });
      let agree = 0; const cyc = [];
      for (let v = 0; v < 20; v++) {
        const r = await ask(a, 28, 32, () => s.dap.write32(a + 36, v), 40, 6);
        agree += r[0] === man.occ.labels[v] ? 1 : 0; cyc.push(r[1]);
      }
      check("20 windows through the mailbox: cycles match the self-test range", Math.min(...cyc) >= w[5] - 2 && Math.max(...cyc) <= w[6] + 2, { rightOfTwenty: agree, cyclesMin: Math.min(...cyc), cyclesMax: Math.max(...cyc), selfTestRight: w[4] });
    }
  } else if (mode === "params") {
    const { man, image } = await imageOf(arg("--image"));
    const p = man.params; if (!p) throw new Error("this image has no parameter block");
    const vals = (arg("--values") || p.items.map((i) => i.default).join(",")).split(",").map(Number);
    if (!(await s.m.holds(image.segments))) await s.flashImage(image);
    await s.m.writeSector(n(p.addr), [n(p.magic) >>> 0, 1, vals.length, p.image_id, ...vals]);
    // read the sector back
    const back = await s.dap.readBlock(n(p.addr), 4 + vals.length);
    check("block read back", back.slice(4).every((v, i) => v === vals[i]) && back[0] === (n(p.magic) >>> 0), { back: back.map((v) => v >>> 0) });
    await s.run();
    if (man.pattern_ms) {
      const pat = vals.slice();
      const led = man.led, g = n(led.gpio);
      const samples = await s.dap.sampleWord(g + board.gpio.dout, 4500, ui.now);
      const r = sequenceCheck(samples, (v) => (v >>> led.bit) & 1, pat.map((ms, i) => [i % 2 === 0 ? 1 : 0, ms]), 40);
      check("LED follows the written pattern", r.ok, { asked: pat, seen: r.levels.slice(0, 8) });
    } else if (man.thermometer) {
      const m = n(man.thermometer.memres), c0 = await s.dap.read32(m);
      out({ memres: c0 });
      await ui.sleep(1200);
      out({ memres: await s.dap.read32(m) });
    } else if (man.digit || man.occ) {
      const a = n((man.digit || man.occ).addr);
      check("image still answers with the block applied", await waitFor(a, man.digit ? n(man.digit.magic) >>> 0 : n(man.occ.magic) >>> 0, 15000), {});
    }
  } else if (mode === "read") {
    const a = n(arg("--addr")), count = Number(arg("--words", "4"));
    out({ words: (await s.dap.readBlock(a, count)).map((v) => v >>> 0) });
  }
} catch (e) { out({ error: e.message || String(e) }); ok = false; }
finally { await s.end().catch(() => {}); }
process.exit(ok ? 0 : 1);
