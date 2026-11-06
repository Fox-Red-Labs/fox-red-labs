// Screenshot a page after it has had time to load live data, with headless Chrome and nothing else installed.
//   node sim/shot.mjs <url> <out.png> [width] [height] [wait ms] [light|dark]
// Starts Chrome with a throwaway profile, waits, captures the whole page, and closes Chrome.
import { spawn } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const [url, out, w = "390", h = "844", wait = "7000", scheme = "light"] = process.argv.slice(2);
const chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const port = 9300 + Math.floor(Math.random() * 500);
const proc = spawn(chrome, ["--headless=new", "--disable-gpu", `--remote-debugging-port=${port}`,
  `--user-data-dir=${mkdtempSync(join(tmpdir(), "frl-shot-"))}`, "--hide-scrollbars", "about:blank"], { stdio: "ignore" });
const sleep = ms => new Promise(r => setTimeout(r, ms));
setTimeout(() => { proc.kill(); console.error("timed out"); process.exit(2); }, +wait + 30000);
let target;
for (let i = 0; i < 50 && !target; i++) {
  await sleep(200);
  try { target = (await (await fetch(`http://127.0.0.1:${port}/json`)).json()).find(t => t.type === "page"); } catch {}
}
const ws = new WebSocket(target.webSocketDebuggerUrl);
await new Promise(r => ws.addEventListener("open", r));
let id = 0;
const pending = new Map();
ws.addEventListener("message", e => { const m = JSON.parse(e.data); if (pending.has(m.id)) { pending.get(m.id)(m.result); pending.delete(m.id); } });
const send = (method, params = {}) => new Promise(r => { pending.set(++id, r); ws.send(JSON.stringify({ id, method, params })); });
await send("Emulation.setDeviceMetricsOverride", { width: +w, height: +h, deviceScaleFactor: 2, mobile: +w < 600 });
await send("Emulation.setEmulatedMedia", { features: [{ name: "prefers-color-scheme", value: scheme }] });
await send("Page.enable");
await send("Page.navigate", { url });
await sleep(+wait);
const { result } = await send("Runtime.evaluate", { expression: "document.documentElement.scrollHeight", returnByValue: true });
const fullH = process.env.FIXED ? +h : Math.min(4000, Math.max(+h, result.value));   // FIXED=1 keeps the given height
await send("Emulation.setDeviceMetricsOverride", { width: +w, height: fullH, deviceScaleFactor: 2, mobile: +w < 600 });
await sleep(400);
const shot = await send("Page.captureScreenshot", { format: "png" });
writeFileSync(out, Buffer.from(shot.data, "base64"));
ws.close();
proc.kill();
console.log(`${out}: ${w}x${fullH}`);
process.exit(0);
