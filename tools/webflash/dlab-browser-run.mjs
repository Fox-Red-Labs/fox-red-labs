// The Digital Lab in a real, headed Chrome on this Mac, against a real board, with no chooser click.
// WebHID is real: a Chrome device policy (WebHidAllowDevicesForUrls) grants the XDS110 to the page's origin and the page's ?auto=1 path
// (allowed(), survey(), useBoard()) picks the probe named by &probe=<serial>. Web Serial is a stand-in over a local WebSocket
// (node serialport), because Chrome accepts the Serial allow policy only at mandatory level, which needs root on macOS.
//
//   defaults write com.google.Chrome WebHidAllowDevicesForUrls -array '<dict><key>devices</key><array><dict><key>vendor_id</key><integer>1105</integer><key>product_id</key><integer>48883</integer></dict></array><key>urls</key><array><string>http://127.0.0.1:8777</string></array></dict>'
//   python3 -m http.server 8777 --bind 127.0.0.1 --directory site
//   python3 tools/frl/lock.py exec --serial ML130001 -- node tools/webflash/dlab-browser-run.mjs --probe ML130001 --scenario l1306 --out <folder>
//   defaults delete com.google.Chrome WebHidAllowDevicesForUrls      (remove the policy when done)
//
// Needs playwright-core, serialport and ws in NODE_PATH, and Google Chrome. Take the probe lock first. Writes a results JSON and screenshots to --out.
import { createRequire } from "module";
import { mkdirSync, writeFileSync, readFileSync, existsSync } from "fs";
import { join } from "path";

const require = createRequire(join(process.env.NODE_PATH.split(":")[0], "x.js"));
const { chromium } = require("playwright-core");
const { SerialPort } = require("serialport");
const WS = require("ws"), WebSocketServer = WS.WebSocketServer || WS.Server;
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const probe = arg("--probe"), also = arg("--also"), scenario = arg("--scenario", "connect"), out = arg("--out"), origin = arg("--origin", "http://127.0.0.1:8777");
if (!probe || !out) { console.log("usage: --probe SERIAL [--also SERIAL2] --scenario NAME --out DIR"); process.exit(2); }
mkdirSync(out, { recursive: true });

const wss = new WebSocketServer({ port: 0, host: "127.0.0.1" });
await new Promise((r) => wss.on("listening", r));
wss.on("connection", (ws) => {
  let tty = null;
  ws.on("message", (m) => {
    const msg = JSON.parse(m);
    if (msg.op === "sopen") {
      if (process.env.SERIAL_LOG) console.log(`serial ${Date.now() % 1000000}: open ${msg.baud}`);
      tty = new SerialPort({ path: `/dev/cu.usbmodem${probe}1`, baudRate: msg.baud }, (e) => ws.send(JSON.stringify({ op: "sopened", error: e ? String(e) : "" })));
      tty.on("data", (d) => { if (process.env.SERIAL_LOG) console.log(`serial ${Date.now() % 1000000}: ${JSON.stringify(d.toString())}`); ws.send(JSON.stringify({ op: "sdata", data: Array.from(d) })); });
    } else if (msg.op === "swrite") { if (tty) tty.write(Buffer.from(msg.data)); }
    else if (msg.op === "sclose") { if (process.env.SERIAL_LOG) console.log(`serial ${Date.now() % 1000000}: close`); if (tty) tty.close(() => {}); tty = null; }
  });
  ws.on("close", () => { if (tty) tty.close(() => {}); });
});
const shim = `(() => {
  const ws = new WebSocket("ws://127.0.0.1:${wss.address().port}");
  const ready = new Promise((r) => ws.addEventListener("open", r));
  let sopened = null, sctl = null;
  ws.addEventListener("message", (e) => { const m = JSON.parse(e.data);
    if (m.op === "sopened" && sopened) { sopened(); sopened = null; }
    if (m.op === "sdata" && sctl) sctl.enqueue(new Uint8Array(m.data)); });
  const port = { readable: null, writable: null, getInfo: () => ({ usbVendorId: 0x451, usbProductId: 0xBEF3 }),
    open: async ({ baudRate }) => { await ready; port.readable = new ReadableStream({ start: (c) => { sctl = c; } });
      port.writable = new WritableStream({ write: (chunk) => { ws.send(JSON.stringify({ op: "swrite", data: Array.from(chunk) })); } });
      await new Promise((r) => { sopened = r; ws.send(JSON.stringify({ op: "sopen", baud: baudRate })); }); },
    close: async () => { ws.send(JSON.stringify({ op: "sclose" })); sctl = null; } };
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port, getPorts: async () => [port] } });
})();`;


// WebHID shows no serial number, so the run limits what getDevices() returns to the probes under test. Each real HIDDevice is asked one
// stateless DAP_Info (serial) command, and the search stops as soon as every wanted probe is found, so the other probes on the bench
// are touched as little as possible. The devices returned are Chrome's own HIDDevice objects, granted by the policy.
const hidShim = `(() => {
  window.__frlAllow = ${JSON.stringify([probe])};
  const real = navigator.hid.getDevices.bind(navigator.hid), serials = new Map();
  const dapSerial = async (d) => {
    const opened = d.opened; if (!opened) await d.open();
    try {
      return await new Promise((res, rej) => {
        const t = setTimeout(() => { d.removeEventListener("inputreport", on); rej(new Error("no answer")); }, 2000);
        const on = (e) => { clearTimeout(t); d.removeEventListener("inputreport", on); const b = new Uint8Array(e.data.buffer, e.data.byteOffset, e.data.byteLength);
          res(new TextDecoder().decode(b.slice(2, 2 + Math.max(0, b[1] - 1)))); };
        d.addEventListener("inputreport", on);
        const o = new Uint8Array(64); o[0] = 0x00; o[1] = 0x03; d.sendReport(0, o).catch(rej);
      });
    } finally { if (!opened) await d.close().catch(() => {}); }
  };
  window.__frlSerials = serials;
  navigator.hid.getDevices = async () => {
    const all = await real(), want = new Set(window.__frlAllow), got = [];
    for (const d of all) {
      if (!serials.has(d) && [...want].some((w) => ![...serials.values()].includes(w))) { try { serials.set(d, await dapSerial(d)); } catch (e) { serials.set(d, "?"); } }
      if (want.has(serials.get(d))) got.push(d);
      if (got.length === want.size) break;
    }
    return got;
  };
})();`;

const results = [];
const rec = (check, pass, detail = "") => { results.push({ check, pass, detail }); console.log(`${pass ? "PASS" : "FAIL"}  ${check}${detail ? "  :: " + String(detail).replace(/\s+/g, " ").slice(0, 300) : ""}`); };
const profile = join(out, "chrome-profile");
const ctx = await chromium.launchPersistentContext(profile, { channel: "chrome", headless: false, acceptDownloads: true, viewport: { width: 1440, height: 1000 } });
await ctx.addInitScript(shim);
await ctx.addInitScript(hidShim);
await ctx.grantPermissions(["clipboard-read", "clipboard-write"], { origin });
const page = ctx.pages()[0] || await ctx.newPage();
page.on("pageerror", (e) => console.log("page error:", e.message));
page.on("console", (m) => { if (m.type() === "error") console.log("console error:", m.text().slice(0, 200)); });
const downloads = [];
page.on("download", async (d) => { const p = join(out, d.suggestedFilename()); await d.saveAs(p); downloads.push(p); console.log("download:", p); });
const text = (sel) => page.textContent(sel).then((x) => (x || "").replace(/\s+/g, " ").trim());
const shot = (n) => page.screenshot({ path: join(out, n + ".png") });
const url = `${origin}/flash/bench/?auto=1&probe=${probe}`;   // the page names no second probe: WebHID shows no serial, the run limits getDevices() (window.__frlAllow)

export { page, ctx, rec, results, text, shot, downloads, probe, also, out, url, wss };
if (scenario === "none") process.exit(0);
const { run } = await import(`./dlab-scenarios.mjs`);
try { await run({ page, ctx, rec, text, shot, downloads, probe, also, out, url, arg, scenario }); }
catch (e) { rec("scenario crashed", false, e.stack || e.message); await shot("crash").catch(() => {}); }
writeFileSync(join(out, `results-${scenario}.json`), JSON.stringify(results, null, 1));
await ctx.close(); wss.close();
process.exit(results.some((r) => !r.pass) ? 1 : 0);
