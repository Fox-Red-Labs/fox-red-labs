// Drive site/motor-console.html in Chrome against the real LP-AM13E230 + DRV8376EVM, through the page's own buttons.
// Chrome's Web Serial chooser cannot be clicked by automation, so the page gets a stand-in navigator.serial that forwards to the
// board's real serial port (node serialport, 9,200,000 baud) over a local WebSocket. Everything else is the page as a reader sees it.
//
//   python3 tools/frl/lock.py exec --serial AL130001 --op console-bench -- \
//     node tools/motorlink/console-bench-test.mjs --port /dev/cu.usbmodemAL1300011 --steps connect,cal,run,step,loss,stop --shots <folder>
//
// Steps (each opens a fresh page and connects; the Hall table stays valid on the board between steps):
//   connect  idle readout and walkthrough, nothing written
//   cal      the Calibrate button, until the guard reports reason 10 (about 3 minutes, the motor turns slowly)
//   run      the Run button at the page defaults, until the guard's cap stops it; saves the comparison row and the CSV download
//   step     a run with a longer cap, then the Step button once the loop is closed (--step-hz 5, --cap 25)
//   loss     a run, then the bridge is cut mid-run (--cut silent|drop); the page must say "Link lost"; reconnect, read the guard reason
//   stop     a run, then the STOP button; the guard must report reason 9
//
// Needs serialport, ws and playwright-core in NODE_PATH, and Google Chrome. Take the probe lock first.
import { createRequire } from "module";
import { createServer } from "http";
import { readFileSync, existsSync, statSync, mkdirSync, writeFileSync } from "fs";
import { dirname, join, extname } from "path";
import { fileURLToPath } from "url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH, "x.js"));
const { SerialPort } = require("serialport");
const { WebSocketServer } = require("ws");
const { chromium } = require("playwright-core");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const ttyPath = arg("--port", "/dev/cu.usbmodemAL1300011"), shots = arg("--shots", null), steps = arg("--steps", "connect").split(",");
const cutMode = arg("--cut", "silent"), stepHz = Number(arg("--step-hz", "5")), stepCap = Number(arg("--cap", "25")), cutAfterS = Number(arg("--cut-after", "4"));
if (shots) mkdirSync(shots, { recursive: true });
const DEBUG = process.argv.includes("--debug");   // exposes the page's session and controller to this script as window.__dbg()
const T0 = Date.now();
const log = (...a) => console.log(`[${((Date.now() - T0) / 1000).toFixed(1).padStart(6)}s]`, ...a);

const types = { ".html": "text/html", ".js": "text/javascript", ".json": "application/json", ".svg": "image/svg+xml", ".css": "text/css", ".woff2": "font/woff2", ".txt": "text/plain" };
const http = createServer((req, res) => {
  const p = join(ROOT, "site", decodeURIComponent(req.url.split("?")[0]).replace(/\/$/, "/index.html"));
  if (!p.startsWith(join(ROOT, "site")) || !existsSync(p) || statSync(p).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "Content-Type": types[extname(p)] || "application/octet-stream" });
  let body = readFileSync(p);
  if (DEBUG && p.endsWith("motor-console.html")) body = Buffer.from(body.toString().replace("let session = null, ctl = null,", "window.__dbg = () => ({ session, ctl }); let session = null, ctl = null,"));
  res.end(body);
}).listen(0, "127.0.0.1");
await new Promise((r) => http.on("listening", r));

// ---- the bridge: page <-> WebSocket <-> real serial port ----
const MCmod = DEBUG ? require(join(ROOT, "tools/motor/console.js")) : null;
const dbgParser = DEBUG ? new MCmod.Parser(MCmod.makeVars(JSON.parse(readFileSync(join(ROOT, "site/data/motorlink-vars.json"), "utf8")))) : null;
const wss = new WebSocketServer({ port: 0, host: "127.0.0.1" });
await new Promise((r) => wss.on("listening", r));
let tty = null, cur = null, cut = false, bytesIn = 0, bytesOut = 0;
const closeTty = () => new Promise((r) => { if (!tty) return r(); const t = tty; tty = null; t.removeAllListeners("data"); t.close(() => r()); });
wss.on("connection", (ws) => {
  ws.on("message", async (m, isBin) => {
    if (isBin) { if (tty && cur === ws && !cut) { bytesOut += m.length; if (DEBUG) log("page write", m.length === 16 ? "packet cmd " + m[1] : m.length + " byte(s)"); tty.write(m); } return; }
    const msg = JSON.parse(m.toString());
    if (msg.op === "sopen") {
      await closeTty(); cur = ws; cut = false;
      log("bridge: opening", ttyPath, "at", msg.baud, "baud");
      tty = new SerialPort({ path: ttyPath, baudRate: msg.baud }, (err) => {
        if (err) { log("bridge: OPEN FAILED", err.message); ws.send(JSON.stringify({ op: "serror", message: err.message })); return; }
        ws.send(JSON.stringify({ op: "sopened" }));
      });
      tty.on("data", (d) => { bytesIn += d.length; if (DEBUG) dbgParser.push(new Uint8Array(d), (f) => { if (f.cmd !== 77) log("  board frame cmd", f.cmd); }); if (!cut && cur === ws && ws.readyState === 1) ws.send(d); });
      tty.on("error", (e) => log("bridge: serial error", e.message));
    }
    if (msg.op === "sclose") { if (cur === ws) await closeTty(); }
  });
});
const cutBridge = async (mode) => {
  log("bridge: CUT", mode);
  if (mode === "silent") { cut = true; return; }              // traffic stops both ways, nothing says why
  cut = true; if (cur) cur.terminate(); await closeTty();     // the port and the socket go away
};

const shim = `(() => {
  const URLW = "ws://127.0.0.1:${wss.address().port}"; let ws = null, sctl = null;
  const port = { readable: null, writable: null,
    open: async ({ baudRate }) => {
      ws = new WebSocket(URLW); ws.binaryType = "arraybuffer";
      await new Promise((res, rej) => { ws.onopen = res; ws.onerror = () => rej(new Error("bridge down")); });
      port.readable = new ReadableStream({ start: (c) => { sctl = c; } });
      const mine = ws;
      port.writable = new WritableStream({ write: (chunk) => { if (mine.readyState === 1) mine.send(chunk); } });
      mine.onclose = () => { try { sctl && sctl.error(new Error("The device has been lost.")); } catch (e) {} };
      await new Promise((res, rej) => { mine.onmessage = (e) => {
        if (typeof e.data === "string") { const m = JSON.parse(e.data); if (m.op === "sopened") res(); if (m.op === "serror") rej(new Error(m.message)); return; }
        try { sctl && sctl.enqueue(new Uint8Array(e.data)); } catch (x) {} };
        mine.send(JSON.stringify({ op: "sopen", baud: baudRate })); });
    },
    close: async () => { try { ws.send(JSON.stringify({ op: "sclose" })); ws.close(); } catch (e) {} sctl = null; } };
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port, getPorts: async () => [port] } });
})();`;

const browser = await chromium.launch({ channel: "chrome", headless: true });
const ctx = await browser.newContext({ acceptDownloads: true, viewport: { width: 1280, height: 900 } });
await ctx.addInitScript(shim);
const out = {}; const downloads = [];
let page = null;
const text = (sel) => page.textContent(sel).then((x) => (x || "").replace(/\s+/g, " ").trim());
const shot = async (name, sel) => {
  if (!shots) return;
  const el = sel ? await page.$(sel) : null;
  // the compact layout folds detail panels away; a hidden target gets the visible page instead of a 30 s timeout
  const shown = el ? await el.isVisible().catch(() => false) : false;
  if (shown) await el.screenshot({ path: join(shots, name + ".png"), timeout: 5000 }).catch(() => page.screenshot({ path: join(shots, name + ".png") }));
  else await page.screenshot({ path: join(shots, name + ".png"), fullPage: false });
};
const readout = async () => { const o = {}; for (const r of await page.$$("#mc-readout tr")) o[(await r.$eval("th", (e) => e.textContent))] = (await r.$eval("td", (e) => e.textContent)); return o; };
const tab = (id) => page.$$eval(`#${id} tbody tr`, (rs) => rs.map((r) => [...r.children].map((c) => c.textContent.trim()).join(" | ")));
const resultRows = () => page.$$eval("#mc-results tbody tr", (rs) => rs.map((r) => [...r.children].map((c) => c.textContent.trim())));
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const statusNow = async () => `${await text("#mc-status")} || ${await text("#mc-state-word")}`;
// compact layout helpers: the lights and the stage strip are in the top bar; tables live in folds that must be opened first
const lightsNow = () => page.$$eval("#mc-lights .lt", (bs) => bs.map((b) => ({ id: b.dataset.id, status: b.className.replace("lt", "").trim(), tip: b.title })));
const lightsLine = async () => (await lightsNow()).map((l) => `${l.id}=${l.status}`).join(" ");
const stageLit = () => page.$$eval("#mc-stages .stg", (es) => es.filter((e) => e.classList.contains("lit")).map((e) => e.textContent.trim()).join(",")).catch(() => "");
const topbar = async () => ({ state: await text("#mc-state-word"), ti: await text("#mc-state-ti"), guard: await text("#mc-guard-word"), status: await text("#mc-status"), now: await text("#mc-now"), stage: await stageLit(), stageExp: await text("#mc-stage-exp") });
async function openFold(title) {   // folds are exclusive; opening one closes the others
  const sum = page.locator(".mcf > summary", { hasText: title });
  const open = await sum.evaluate((e) => e.parentElement.open);
  if (!open) await sum.click();
  await sleep(250);
}
async function closeFolds() { await page.$$eval(".mcf", (ds) => ds.forEach((d) => (d.open = false))); }
const foldShot = async (title, name, sel) => { await openFold(title); await shot(name, sel || ".mcf[open] .mcpanel"); await closeFolds(); };

async function open(freshPage) {
  if (page) await page.close().catch(() => {});
  page = await ctx.newPage();
  page.on("pageerror", (e) => log("PAGE ERROR:", e.message));
  page.on("dialog", async (d) => { log("DIALOG:", d.message()); out.dialogs = (out.dialogs || []).concat(d.message()); await d.accept(); });
  page.on("download", async (d) => { const p = join(shots || process.env.TMPDIR || "/tmp", d.suggestedFilename()); await d.saveAs(p); downloads.push(p); log("download saved", p); });
  await page.goto(`http://127.0.0.1:${http.address().port}/motor-console.html`);
  await page.waitForSelector("#mc-connect");
}
async function connect() {
  await page.click("#mc-connect");
  await page.waitForFunction(() => document.body.classList.contains("mc-on") || /Could not/.test(document.getElementById("mc-status").textContent), null, { timeout: 60000 });
  const st = await text("#mc-status"); log("status:", st);
  if (/Could not/.test(st)) throw new Error(st);
  await sleep(1500);
}
async function disconnect() { await page.click("#mc-connect"); await sleep(500); log("status:", await text("#mc-status")); }
async function waitEnded(maxS, every = 1000, trace = true) {
  const t = Date.now(); let last = "", lastStage = null, lastLights = null;
  while (Date.now() - t < maxS * 1000) {
    const ro = await readout();
    const line = `state=${ro["Motor state (TI)"]} | guard=${ro["Guard state"]} | reason=${ro["Guard reason"]} | run=${ro["Run time"]} | speed=${ro["Speed"]} | peak=${ro["Peak current this run"]} | bus=${ro["Bus voltage"]}`;
    if (trace && line !== last) log(line);
    last = line;
    if (trace) {
      const sg = await stageLit(), lt = await lightsLine();
      if (sg !== lastStage) { lastStage = sg; log(`stage lit: ${sg} | state word: ${await text("#mc-state-word")}`); }
      if (lt !== lastLights) { lastLights = lt; log("lights:", lt); }
    }
    const s = await statusNow();
    if (/Link lost/.test(s)) { log("LINK LOST:", s); return "linklost"; }
    if (await page.$eval("#mc-stop", (e) => !e.classList.contains("on"))) return "ended";
    await sleep(every);
  }
  return "timeout";
}
const startRun = async () => {
  await page.waitForFunction(() => !document.getElementById("mc-run-btn").disabled, null, { timeout: 10000 });
  log("click:", await text("#mc-run-btn")); await page.click("#mc-run-btn");
  await page.waitForFunction(() => document.getElementById("mc-stop").classList.contains("on"), null, { timeout: 8000 });
};
const waitClosed = async (maxS) => {
  const t = Date.now();
  while (Date.now() - t < maxS * 1000) { if (!(await page.$eval("#mc-step-btn", (e) => e.disabled))) return (Date.now() - t) / 1000; await sleep(200); }
  return null;
};

try {
  await open(); out.start = { bytesIn: 0 };
  for (const s of steps) {
    log("=== step:", s);
    await open();
    await connect();
    const r = out[s] = {};
    if (DEBUG) for (let i = 0; i < 12; i++) { log("debug:", JSON.stringify(await page.evaluate(() => { const d = window.__dbg(); return { errors: d.session.errors, acks: d.session.acks, fillers: d.session.fillers, frames: d.session.frames, resyncs: d.ctl.resyncs, writes: d.session.writes.length, rotating: d.ctl.rotating }; }))); await sleep(250); }
    if (s === "connect") {
      r.readout = await readout(); r.idle = await tab("mc-idle-tab"); r.status = await text("#mc-status"); r.scheme = await text("#mc-scheme-why");
      r.topbar = await topbar(); r.lights = await lightsNow(); r.lightsOpenedTips = {};
      for (const l of r.lights) { await page.hover("#lt-" + l.id); await sleep(150); r.lightsOpenedTips[l.id] = await text("#mc-now"); }
      await page.mouse.move(5, 5);
      log(JSON.stringify(r, null, 1));
      await shot("01-connect-topbar", ".mcdock"); await foldShot("Diagnosis", "01-connect-idle-check");
    }
    if (s === "cal") {
      await page.waitForFunction(() => !document.getElementById("mc-cal-btn").disabled, null, { timeout: 10000 });
      log("click:", await text("#mc-cal-btn")); await page.click("#mc-cal-btn");
      await page.waitForFunction(() => document.getElementById("mc-stop").classList.contains("on"), null, { timeout: 8000 });
      const t = Date.now(); let shotMid = false;
      const res = await waitEnded(420, 5000);
      r.result = res; r.seconds = (Date.now() - t) / 1000; r.readout = await readout(); r.cal = await tab("mc-cal-tab"); r.status = await statusNow();
      log(JSON.stringify(r, null, 1));
      r.topbar = await topbar(); r.lights = await lightsNow();
      await shot("02-calibration-done-topbar", ".mcdock"); await foldShot("Diagnosis", "02-calibration-done-checks");
    }
    if (s === "run" || s === "step") {
      if (s === "step") { await page.fill("#mc-cap", String(stepCap)); }
      const before = (await resultRows()).length;
      await startRun();
      const t = Date.now();
      if (s === "step") {
        const tc = await waitClosed(25); r.closedAfterS = tc; log("closed loop after (s):", tc);
        if (tc === null) { log("loop did not close"); }
        else {
          await sleep(2500);   // let it settle at the reference before the step
          await shot("04-step-before", "#motor-console");
          await page.fill("#mc-step-d", String(stepHz));
          log("click:", await text("#mc-step-btn")); await page.click("#mc-step-btn");
          r.stepAtS = (Date.now() - t) / 1000;
          await sleep(6000); r.overlayMid = await text("#mc-overlay"); r.violMid = await text("#mc-viol-n"); await shot("04-step-mid-overlay", "#mc-overlay");
          log("overlay mid-run:", (r.overlayMid.match(/\d+ % inside; worst [^;]*/) || [""])[0]);
        }
      } else {
        const tc = await waitClosed(25); r.closedAfterS = tc; log("closed loop after (s):", tc);
        r.closedTopbar = await topbar(); r.closedLights = await lightsNow(); await shot("03-run-closed", "#motor-console");
      }
      const res = await waitEnded(90, 1000);
      r.result = res; r.seconds = (Date.now() - t) / 1000; await sleep(800);
      r.readout = await readout(); r.runTab = await tab("mc-run-tab"); r.results = await resultRows(); r.newRows = r.results.length - before;
      log(JSON.stringify(r, null, 1));
      const p = s === "run" ? "03-run" : "04-step";
      r.topbar = await topbar(); r.overlayText = (await text("#mc-overlay")).replace(/^.*?(step \d)/, "$1"); r.viol = await text("#mc-viol-n");
      r.accelMax = await page.evaluate(() => { const e = document.querySelector('[id^="se-"][id$="accelerationMax_Hzps"]'); return e ? e.textContent : null; });
      await shot(p + "-page", "#motor-console"); await foldShot("Diagnosis", p + "-walkthrough"); await foldShot("Run comparison", p + "-comparison");
      if (s === "step") await shot(p + "-overlay", "#mc-overlay");
      if (s === "run") {
        await openFold("Run comparison");
        await page.click("#mc-csv").catch((e) => log("csv click failed", e.message)); await closeFolds(); await sleep(800);
        if (downloads.length) { r.csv = readFileSync(downloads[downloads.length - 1], "utf8"); log("CSV:\n" + r.csv); }
      }
    }
    if (s === "loss") {
      await startRun();
      const t = Date.now();
      await sleep(cutAfterS * 1000);
      log("readout before cut:", JSON.stringify(await readout()));
      await cutBridge(cutMode);
      const tcut = Date.now(); let seen = null;
      while (Date.now() - tcut < 12000) { const st = await statusNow(); if (/Link lost|went away/.test(st)) { seen = st; break; } await sleep(100); }
      r.statusAfterCut = seen; r.secondsToMessage = (Date.now() - tcut) / 1000; log("page after cut (", r.secondsToMessage, "s ):", seen, "| full:", await statusNow());
      await sleep(1500);
      r.statusLater = await statusNow(); r.stopButtonOn = await page.$eval("#mc-stop", (e) => e.classList.contains("on"));
      r.topbarLost = await topbar(); r.lightsLost = await lightsNow();
      await shot("05-link-lost", ".mcdock");
      // reconnect as a reader would: Disconnect, then Connect again
      await sleep(2000);
      log("click Connect/Disconnect button:", await text("#mc-connect"));
      await page.click("#mc-connect").catch((e) => log("click failed", e.message));
      await sleep(1000); log("status after first click:", await statusNow(), "| button:", await text("#mc-connect"));
      if (/Disconnect/.test(await text("#mc-connect"))) { await page.click("#mc-connect"); await sleep(1000); }
      r.afterDisconnect = await statusNow();
      await connect();
      r.readoutAfter = await readout(); r.guardReason = r.readoutAfter["Guard reason"]; r.guardState = r.readoutAfter["Guard state"];
      log("after reconnect: guard reason", r.guardReason, "| state", r.guardState, "| motor state", r.readoutAfter["Motor state (TI)"]);
      await shot("05-after-reconnect", ".mcdock");
    }
    if (s === "stop") {
      await startRun();
      await sleep(3500);
      log("click STOP; readout:", JSON.stringify(await readout()));
      await shot("06-stop-before", ".mcdock");
      await page.click("#mc-stop"); const t = Date.now();
      await page.waitForFunction(() => !document.getElementById("mc-stop").classList.contains("on"), null, { timeout: 10000 });
      await sleep(1500);
      r.readout = await readout(); r.guardReason = r.readout["Guard reason"]; r.runTab = await tab("mc-run-tab"); r.results = await resultRows();
      r.secondsToStopped = (Date.now() - t) / 1000;
      log("after STOP: guard reason", r.guardReason, "| state", r.readout["Guard state"], "| motor", r.readout["Motor state (TI)"]);
      await shot("06-stop-after", ".mcdock"); await foldShot("Diagnosis", "06-stop-walkthrough");
    }
    if (await page.$eval("#mc-connect", (e) => /Disconnect/.test(e.textContent)).catch(() => false)) await disconnect();
  }
} catch (e) {
  log("FAILED:", e.stack || e.message); out.error = String(e.message);
} finally {
  out.bytesIn = bytesIn; out.bytesOut = bytesOut;
  if (shots) writeFileSync(join(shots, `result-${steps.join("-")}.json`), JSON.stringify(out, null, 1));
  await closeTty();
  await browser.close(); wss.close(); http.close();
  log("done; bytes in", bytesIn, "out", bytesOut);
  process.exit(out.error ? 1 : 0);
}
