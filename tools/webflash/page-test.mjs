// Drive site/flash/index.html in Chrome against a real board. Chrome's WebHID device chooser cannot be
// clicked by automation, so the page gets a stand-in navigator.hid whose one device forwards every report
// to the real XDS110 through node-hid over a local WebSocket. Everything else is the page as a reader sees it.
//
//   node tools/webflash/page-test.mjs --probe ML130001 [--restore] [--shot page.png]
//
// Needs node-hid, ws and playwright-core in NODE_PATH, and Google Chrome. Take the probe lock first.
import { createRequire } from "module";
import { createServer } from "http";
import { readFileSync, existsSync, statSync } from "fs";
import { dirname, join, extname } from "path";
import { fileURLToPath } from "url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH, "x.js"));
const HID = require("node-hid");
const { WebSocketServer } = require("ws");
const { chromium } = require("playwright-core");
const arg = (k) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : null; };
const serial = arg("--probe");
const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === serial && d.interface === 5);
if (!info) { console.log(`probe ${serial} not found`); process.exit(2); }

const types = { ".html": "text/html", ".js": "text/javascript", ".json": "application/json", ".out": "application/octet-stream", ".txt": "text/plain" };
const http = createServer((req, res) => {
  const p = join(ROOT, "site", decodeURIComponent(req.url.split("?")[0]).replace(/\/$/, "/index.html"));
  if (!p.startsWith(join(ROOT, "site")) || !existsSync(p) || statSync(p).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "Content-Type": types[extname(p)] || "application/octet-stream" });
  res.end(readFileSync(p));
}).listen(0, "127.0.0.1");
await new Promise((r) => http.on("listening", r));
const port = http.address().port;

const wss = new WebSocketServer({ port: 0, host: "127.0.0.1" });
await new Promise((r) => wss.on("listening", r));
let dev = null, reports = 0;
wss.on("connection", (ws) => {
  ws.on("message", (m) => {
    const msg = JSON.parse(m);
    if (msg.op === "open") { dev = new HID.HID(info.path); ws.send(JSON.stringify({ op: "opened" })); return; }
    if (msg.op === "close") { if (dev) dev.close(); dev = null; return; }
    const out = [0, ...msg.data]; while (out.length < 65) out.push(0);
    dev.write(out);
    const r = dev.readTimeout(3000);
    reports++;
    ws.send(JSON.stringify({ op: "report", data: r }));
  });
});
const wsPort = wss.address().port;

const shim = `(() => {
  const ws = new WebSocket("ws://127.0.0.1:${wsPort}");
  const ready = new Promise((r) => ws.addEventListener("open", r));
  const listeners = [];
  let opened = null;
  ws.addEventListener("message", (e) => {
    const m = JSON.parse(e.data);
    if (m.op === "opened" && opened) { opened(); opened = null; return; }
    if (m.op === "report") { const d = new Uint8Array(m.data); for (const f of listeners) f({ data: new DataView(d.buffer) }); }
  });
  const device = { productName: "XDS110 (stand-in for test)", opened: false,
    open: async () => { await ready; await new Promise((r) => { opened = r; ws.send(JSON.stringify({ op: "open" })); }); device.opened = true; },
    close: async () => { ws.send(JSON.stringify({ op: "close" })); device.opened = false; },
    sendReport: async (id, data) => { ws.send(JSON.stringify({ op: "send", data: Array.from(data) })); },
    addEventListener: (t, f) => { if (t === "inputreport") listeners.push(f); } };
  Object.defineProperty(navigator, "hid", { value: { requestDevice: async () => [device], getDevices: async () => [] } });
})();`;

const browser = await chromium.launch({ channel: "chrome", headless: true });
const ctx = await browser.newContext({ acceptDownloads: true });
await ctx.addInitScript(shim);
const page = await ctx.newPage();
page.on("pageerror", (e) => console.log("page error:", e.message));
await page.goto(`http://127.0.0.1:${port}/flash/`);
await page.waitForFunction(() => document.getElementById("about").textContent.length > 0);
const t0 = Date.now();
const dl = page.waitForEvent("download", { timeout: 60000 }).catch(() => null);
await page.click("#go");
const download = await dl;
const copy = download ? join(process.env.TMPDIR || "/tmp", download.suggestedFilename()) : null;
if (download) await download.saveAs(copy);
await page.waitForFunction(() => !document.getElementById("result").classList.contains("hidden") && !document.getElementById("go").disabled, null, { timeout: 120000 });
const verdict = await page.textContent("#verdict"), detail = await page.textContent("#detail");
console.log(JSON.stringify({ verdict, detail, seconds: (Date.now() - t0) / 1000, copy, reports }));
const shot = arg("--shot");
if (shot) { await page.setViewportSize({ width: 820, height: 1000 }); await page.screenshot({ path: shot, fullPage: true }); }
const steps = await page.$$eval("#steps li", (l) => l.map((x) => x.textContent));
console.log(steps.join("\n"));

if (process.argv.includes("--restore")) {
  const t1 = Date.now();
  await page.setInputFiles("#file", copy);
  await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 180000 });
  console.log(JSON.stringify({ restore: await page.textContent("#rresult"), seconds: (Date.now() - t1) / 1000 }));
}
await browser.close();
wss.close(); http.close();
process.exit(verdict.startsWith("It works") ? 0 : 1);
