// The chip view's feed from Node, for checks without a browser: prints what a running MSPM0 is doing and can save it.
//   python3 tools/frl/lock.py exec --serial <probe> --op live -- node tools/webflash/live-mspm0.mjs <probe> --secs 10 [--elf x.out] [--json out.json]
// Never halts, resets or writes the chip. The chip is picked by its DEVICEID from site/flash/chip/maps/.
// Needs NODE_PATH to hold node-hid (see tools/webflash/node-run.mjs).
import { createRequire } from "module";
import { readFileSync, writeFileSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { Dap } from "../../site/flash/mspm0-dap.js";
import { ChipFeed, elfFunctions, matchMap } from "../../site/flash/chip/feed.js";
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", ".."), MAPS = join(ROOT, "site/flash/chip/maps");
const require = createRequire(join(process.env.NODE_PATH || ROOT, "x.js"));
const HID = require("node-hid");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === process.argv[2] && d.interface === 5);
if (!info) { console.log("probe not on USB"); process.exit(2); }
const t = { async open() { this.dev = new HID.HID(info.path); }, async close() { this.dev.close(); }, async transfer(b) { const o = new Array(65).fill(0); b.forEach((x, i) => { o[i + 1] = x; }); this.dev.write(o); const r = this.dev.readTimeout(3000); if (!r.length) throw new Error("no reply"); return Uint8Array.from(r); } };
await t.open();
const dap = new Dap(t); await dap.connect();
const id = (await dap.read32(0x41C40004)) >>> 0, hit = matchMap(JSON.parse(readFileSync(join(MAPS, "index.json"))), id);
if (!hit) { console.log("no map for DEVICEID 0x" + id.toString(16)); await dap.disconnect(); await t.close(); process.exit(2); }
const feed = new ChipFeed(dap, JSON.parse(readFileSync(join(MAPS, hit.board + ".json"))), () => performance.now());
if (arg("--elf")) { const r = readFileSync(arg("--elf")); feed.fns = elfFunctions(r.buffer.slice(r.byteOffset, r.byteOffset + r.byteLength)); }
const secs = +arg("--secs", 10), t0 = performance.now(); let st, k = 0;
await feed.slow();
while (performance.now() - t0 < secs * 1000) { st = await feed.fast(1000); if (++k % 5 === 0) await feed.slow(); }
console.log(hit.kit, "| awake %", st.history.join(" "));
console.log("powered", Object.entries(st.power).filter(([, v]) => v).map(([n]) => n).join(" ") || "none", "| clock", st.clock.hz ?? st.clock.source, "| systick", JSON.stringify(st.systick));
console.log("irq lines on", st.enabled.join(",") || "none", "| running", JSON.stringify(st.interrupts));
console.log("pins", st.pins.filter((p) => p.connected).map((p) => `${p.name}=${p.fn}`).join(" "));
console.log("toggling", JSON.stringify(st.toggles));
console.log("functions", JSON.stringify(st.functions));
console.log("ram changed cells", st.ram?.changedCells, "of 1024 (", st.ram?.cellBytes, "B each ) | flash kB holding data", st.flashSectors.filter(Boolean).length, "| rate", st.sampleRate, "| detail", JSON.stringify(st.detail), "| faults", st.faults);
if (arg("--json")) writeFileSync(arg("--json"), JSON.stringify(st));
await dap.disconnect(); await t.close();
