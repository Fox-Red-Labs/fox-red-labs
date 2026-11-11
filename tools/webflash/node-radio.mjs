// The Digital Lab's wireless-board flash path (site/flash/cc-saci.js, the same code the page runs) driven from Node through
// node-hid, for the CC27xx and CC23xx boards. Same setup as node-run.mjs (node-hid in NODE_PATH; take the probe lock first).
//
//   node tools/webflash/node-radio.mjs identify --probe LS5600RE --family cc27
//   node tools/webflash/node-radio.mjs backup   --probe LS5600RE --family cc27 --out <dir>
//   node tools/webflash/node-radio.mjs flash    --probe LS5600RE --family cc27 --hex <file.hex> --out <dir>   (backs up first when the chip holds something else)
//   node tools/webflash/node-radio.mjs set      --probe LS5600RE --family cc27 --hex <file.hex> --manifest <manifest.json> --values on_ms=100,off_ms=100   (the settings ride with a whole write)
//   node tools/webflash/node-radio.mjs restore  --probe LS5600RE --family cc27 --out <dir>                    (writes the saved files back, reads the chip again)
//   node tools/webflash/node-radio.mjs peek     --probe LS5600RE --family cc27 --addr 0x20000000 --words 4 [--again 2]
import { createRequire } from "module";
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { createHash } from "crypto";
import { Dap } from "../../site/flash/mspm0-dap.js";
import { Saci, parseHex, crc32, FAMILY, readBins, putBack, imageWithParams } from "../../site/flash/cc-saci.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const mode = process.argv[2], serial = arg("--probe"), fam = FAMILY[arg("--family", "cc27")], out = arg("--out");
if (!serial || !fam) { console.log("name the probe (--probe) and the family (--family cc27|cc23)"); process.exit(2); }
const log = (x) => { if (!/^NOP/.test(x)) console.log("  " + x); };
const sha = (b) => createHash("sha256").update(b).digest("hex");

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
if (!info) { console.log(`probe ${serial} has no CMSIS-DAP HID interface on USB`); process.exit(2); }
const t = new NodeHidTransport(info.path); await t.open();
const dap = new Dap(t); dap.dormant = true;
const sc = new Saci(dap, log, fam);
const save = (dir, stamp, b) => { mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "main.bin"), b.main); writeFileSync(join(dir, "ccfg.bin"), b.ccfg); if (b.scfg) writeFileSync(join(dir, "scfg.bin"), b.scfg);
  writeFileSync(join(dir, "backup.json"), JSON.stringify({ stamp, family: arg("--family"), sha256: { main: sha(b.main), ccfg: sha(b.ccfg), scfg: b.scfg ? sha(b.scfg) : null } }, null, 1)); };
const load = (dir) => ({ main: new Uint8Array(readFileSync(join(dir, "main.bin"))), ccfg: new Uint8Array(readFileSync(join(dir, "ccfg.bin"))),
  scfg: existsSync(join(dir, "scfg.bin")) ? new Uint8Array(readFileSync(join(dir, "scfg.bin"))) : null });

try {
  const idr = await dap.connect();
  const id = await dap.apRead(1, 0x0);
  console.log("DPIDR 0x" + idr.toString(16), "CFG-AP DEVICEID 0x" + id.toString(16), JSON.stringify(await sc.status()));
  if (mode === "identify") { /* done */ }
  else if (mode === "peek") {
    const a = parseInt(arg("--addr"), 16), n = +arg("--words", 1);
    for (let k = 0; k < +arg("--again", 1); k++) { console.log((await dap.readBlock(a, n)).map((v) => "0x" + (v >>> 0).toString(16).padStart(8, "0")).join(" ")); await new Promise((r) => setTimeout(r, 1000)); }
  } else if (mode === "backup") {
    const st = await sc.status();
    if (!st.ahbOpen) throw new Error("the chip's memory port is closed: nothing can be read to save");
    const b = await readBins(dap, fam); save(out, new Date().toISOString(), b);
    console.log("saved to", out, "main sha256", sha(b.main).slice(0, 16));
  } else if (mode === "flash") {
    const img = parseHex(readFileSync(arg("--hex"), "utf8"));
    await sc.enter();
    const same = (await sc.verifyMain(img.main.length, crc32(img.main))) === 0;
    if (same) console.log("the chip's own check matches this program: nothing to write");
    else {
      const st0 = await sc.bootAndWait();
      if (!st0.ahbOpen) throw new Error("memory port closed: cannot save first");
      if (!out) throw new Error("--out is needed so the chip's flash is saved before the write");
      if (!existsSync(join(out, "main.bin"))) { save(out, new Date().toISOString(), await readBins(dap, fam)); console.log("saved a copy to", out); }
      else console.log("a saved copy already exists in", out, "(kept)");
      await sc.enter();
      let last = -1;
      await sc.program(img, (p) => { const q = Math.floor(p * 10); if (q !== last) { last = q; console.log("  writing " + q * 10 + "%"); } });
    }
    const st = await sc.bootAndWait();
    console.log("started; debug port", st.ahbOpen ? "open" : "closed");
  } else if (mode === "set") {
    // node tools/webflash/node-radio.mjs set --probe S --family cc27 --hex <file.hex> --manifest <manifest.json> --values on_ms=100,off_ms=100 --out <backup dir>
    // The settings ride with a whole write (the chip has no sector erase over SACI): the page's Change tab does exactly this.
    const block = JSON.parse(readFileSync(arg("--manifest"), "utf8")).params, vals = {};
    for (const kv of arg("--values", "").split(",").filter(Boolean)) { const [k, v] = kv.split("="); vals[k] = +v; }
    const img = imageWithParams(parseHex(readFileSync(arg("--hex"), "utf8")), block, vals);
    await sc.enter();
    await sc.program(img);
    const st = await sc.bootAndWait();
    console.log("settings written with the image; debug port", st.ahbOpen ? "open" : "closed", JSON.stringify(vals));
  } else if (mode === "restore") {
    const bins = load(out);
    await sc.enter();
    let last = -1;
    const res = await putBack(sc, bins, (p) => { const q = Math.floor(p * 10); if (q !== last) { last = q; console.log("  writing " + q * 10 + "%"); } });
    console.log("READ-BACK", JSON.stringify(res), res.ok ? "IDENTICAL" : "DIFFERENT");
    process.exitCode = res.ok ? 0 : 1;
  } else console.log("modes: identify backup flash restore peek");
} catch (e) { console.log("STOPPED:", e.message); process.exitCode = 1; }
finally { if (["flash", "restore"].includes(mode) && process.exitCode) await sc.close(); else await dap.disconnect().catch(() => {}); await t.close(); }
