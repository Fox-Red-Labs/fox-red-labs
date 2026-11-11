// Drive site/flash/bench/ in Chrome against a real board: connect, the colour player (two sends), the blink and the
// message recipe, then put the first saved copy back. Chrome's WebHID and Web Serial choosers cannot be clicked by
// automation, so the page gets stand-ins for navigator.hid and navigator.serial that forward to the real XDS110
// (node-hid) and its serial port (serialport) over a local WebSocket. Everything else is the page as a reader sees it.
//
//   node tools/webflash/bench-test.mjs --probe ML130001 [--quick | --connect-only | --recovery | --controls] [--shots <folder>]
//
// --examples [--ids a,b]: the starter examples from build_examples.py, each flashed and checked through the page, then the saved copy put back.
// --recovery (persona review B01, MSPM0 boards): the board's promise and write summary after connecting; a blink whose
// USB link the harness cuts in the middle of flashing (it must end "Stopped", never "It works"); a copy from another
// chip (trace ID changed) and a damaged copy, both refused with nothing written; then the real copy put back and read back.
//
// Needs node-hid, serialport, ws and playwright-core in NODE_PATH, and Google Chrome. Take the probe lock first.
import { createRequire } from "module";
import { createServer } from "http";
import { readFileSync, existsSync, statSync, mkdirSync } from "fs";
import { dirname, join, extname } from "path";
import { fileURLToPath } from "url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH, "x.js"));
const HID = require("node-hid");
const { SerialPort } = require("serialport");
const { WebSocketServer } = require("ws");
const { chromium } = require("playwright-core");
const arg = (k) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : null; };
const serial = arg("--probe"), shots = arg("--shots");
if (shots) mkdirSync(shots, { recursive: true });
const info = HID.devices().find((d) => d.vendorId === 0x451 && d.serialNumber === serial && d.interface === 5);
if (!info) { console.log(`probe ${serial} not found`); process.exit(2); }
const ttyPath = `/dev/cu.usbmodem${serial}1`;

const types = { ".html": "text/html", ".js": "text/javascript", ".json": "application/json", ".svg": "image/svg+xml", ".out": "application/octet-stream", ".txt": "text/plain" };
const http = createServer((req, res) => {
  const p = join(ROOT, "site", decodeURIComponent(req.url.split("?")[0]).replace(/\/$/, "/index.html"));
  if (!p.startsWith(join(ROOT, "site")) || !existsSync(p) || statSync(p).isDirectory()) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "Content-Type": types[extname(p)] || "application/octet-stream" });
  res.end(readFileSync(p));
}).listen(0, "127.0.0.1");
await new Promise((r) => http.on("listening", r));

const wss = new WebSocketServer({ port: 0, host: "127.0.0.1" });
await new Promise((r) => wss.on("listening", r));
let dev = null, tty = null, cutAfter = -1, sent = 0;   // --recovery: drop the probe after cutAfter more reports
wss.on("connection", (ws) => {
  ws.on("message", (m) => {
    const msg = JSON.parse(m);
    if (msg.op === "open") { dev = new HID.HID(info.path); ws.send(JSON.stringify({ op: "opened" })); return; }
    if (msg.op === "close") { if (dev) dev.close(); dev = null; return; }
    if (msg.op === "send") {
      if (!dev) { console.log("harness: the page sent a report to a closed device:", msg.data.slice(0, 4).join(",")); ws.send(JSON.stringify({ op: "report", data: [] })); return; }
      if (cutAfter >= 0 && ++sent > cutAfter) { if (dev) dev.close(); dev = null; cutAfter = -1; console.log("harness: USB link cut"); ws.send(JSON.stringify({ op: "report", data: [] })); return; }
      const out = [0, ...msg.data]; while (out.length < 65) out.push(0);
      dev.write(out);
      ws.send(JSON.stringify({ op: "report", data: dev.readTimeout(3000) }));
      return;
    }
    if (msg.op === "sopen") {
      tty = new SerialPort({ path: ttyPath, baudRate: msg.baud }, () => ws.send(JSON.stringify({ op: "sopened" })));
      tty.on("data", (d) => ws.send(JSON.stringify({ op: "sdata", data: Array.from(d) })));
      return;
    }
    if (msg.op === "swrite") { if (tty) tty.write(Buffer.from(msg.data)); return; }
    if (msg.op === "sclose") { if (tty) tty.close(() => {}); tty = null; }
  });
});

const shim = `(() => {
  const ws = new WebSocket("ws://127.0.0.1:${wss.address().port}");
  const ready = new Promise((r) => ws.addEventListener("open", r));
  const listeners = []; let opened = null, sopened = null, sctl = null;
  ws.addEventListener("message", (e) => {
    const m = JSON.parse(e.data);
    if (m.op === "opened" && opened) { opened(); opened = null; return; }
    if (m.op === "sopened" && sopened) { sopened(); sopened = null; return; }
    if (m.op === "report") { const d = new Uint8Array(m.data); for (const f of listeners) f({ data: new DataView(d.buffer) }); }
    if (m.op === "sdata" && sctl) sctl.enqueue(new Uint8Array(m.data));
  });
  const device = { productName: "XDS110 (stand-in for test)", vendorId: 0x0451, productId: 0xBEF3, collections: [{ usagePage: 0xFF00 }], opened: false,
    open: async () => { await ready; await new Promise((r) => { opened = r; ws.send(JSON.stringify({ op: "open" })); }); device.opened = true; },
    close: async () => { ws.send(JSON.stringify({ op: "close" })); device.opened = false; },
    sendReport: async (id, data) => { ws.send(JSON.stringify({ op: "send", data: Array.from(data) })); },
    addEventListener: (t, f) => { if (t === "inputreport") listeners.push(f); } };
  let granted = [];
  Object.defineProperty(navigator, "hid", { value: { requestDevice: async () => { granted = [device]; return [device]; }, getDevices: async () => granted } });
  const port = { readable: null, writable: null,
    open: async ({ baudRate }) => { await ready; port.readable = new ReadableStream({ start: (c) => { sctl = c; } });
      port.writable = new WritableStream({ write: (chunk) => { ws.send(JSON.stringify({ op: "swrite", data: Array.from(chunk) })); } });
      await new Promise((r) => { sopened = r; ws.send(JSON.stringify({ op: "sopen", baud: baudRate })); }); },
    close: async () => { ws.send(JSON.stringify({ op: "sclose" })); sctl = null; } };
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port, getPorts: async () => [port] } });
})();`;

const browser = await chromium.launch({ channel: "chrome", headless: true });
const ctx = await browser.newContext({ acceptDownloads: true, viewport: { width: 1440, height: 900 } });
await ctx.addInitScript(shim);
const page = await ctx.newPage();
page.on("pageerror", (e) => console.log("page error:", e.message));
const downloads = [];
page.on("download", async (d) => { const p = join(process.env.TMPDIR || "/tmp", d.suggestedFilename()); await d.saveAs(p); downloads.push(p); });
await page.goto(`http://127.0.0.1:${http.address().port}/flash/bench/`);
await page.waitForSelector(".recipe");
const out = {};
let t;
const done = () => page.waitForFunction(() => !document.getElementById("result").classList.contains("hidden") && !document.getElementById("go").disabled, null, { timeout: 600000 });
const grab = async (k) => { out[k] = { verdict: await page.textContent("#verdict"), measured: await page.textContent("#measured") }; if (shots) await page.screenshot({ path: join(shots, k + ".png"), fullPage: false }); };

await page.click("#connect");
await page.waitForFunction(() => ["on", "bad"].includes(document.getElementById("found").dataset.state), null, { timeout: 30000 });
out.connect = (await page.$eval("#found", (e) => e.dataset.state)) === "on" ? "Connected: " + await page.textContent("#found") : await page.textContent("#found");
console.log("connect:", out.connect);
await page.click("#survey");
await page.waitForFunction(() => !document.getElementById("survey").disabled && /program|recogni/i.test(document.getElementById("boards").textContent), null, { timeout: 30000 })
  .catch(async (e) => { console.log("survey did not finish; boards:", await page.textContent("#boards"), "survey disabled:", await page.$eval("#survey", (b) => b.disabled)); throw e; });
out.boards = await page.textContent("#boards");

const pick = (title) => page.click(`.recipe:has-text("${title}")`);
const oobRead = async (what, k) => {
  await page.click(`[data-oob="${what}"]`);
  await page.waitForTimeout(3500);
  out[k] = { verdict: await page.textContent("#oob-value"), measured: await page.textContent("#oob-note") };
  if (shots) await page.screenshot({ path: join(shots, k + ".png"), fullPage: false });
};
if (process.argv.includes("--connect-only")) {   // identify the board and list its recipes; nothing is written
  out.recipes = await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent));
  await page.click(".recipe:not(:disabled) >> nth=0").catch(() => {});
  await page.waitForTimeout(500);
  out.detail = await page.textContent("#detail").catch(() => "");
  if (process.argv.includes("--press-go")) {   // run the chosen recipe's button once (on a CC27xx that already holds the image, nothing is written)
    const t1 = Date.now(); await page.click("#go"); await done();
    out.go = { verdict: await page.textContent("#verdict"), measured: await page.textContent("#measured"), seconds: (Date.now() - t1) / 1000,
               steps: await page.$$eval("#log li", (l) => l.map((x) => x.textContent)), downloads: downloads.length };
  }
  if (shots) await page.screenshot({ path: join(shots, "connect-only.png"), fullPage: true });
  console.log(JSON.stringify(out, null, 1));
  await browser.close(); wss.close(); http.close();
  process.exit(/Connected/.test(out.connect) ? 0 : 1);
}
if (process.argv.includes("--controls")) {   // the no-code controls card: rows added and removed with the plus and minus, sent, then the copy put back
  await pick("Pin, timer, bus");
  await page.waitForSelector("#controls:not(.hidden)");
  await page.waitForSelector("#ct-rows .ct-row");
  const rowsC = () => page.$$eval("#ct-rows .ct-row pre", (l) => l.map((x) => x.textContent));
  const add = async (label) => { await page.click("#ct-plus summary"); await page.click(`#ct-menu button:has-text("${label}")`); };
  const clearRows = async () => { while (await page.$("#ct-rows .ct-row .rm")) await page.click("#ct-rows .ct-row .rm >> nth=0"); };
  const setRow = async (n, f, v) => { const el = page.locator(`#ct-rows .ct-row >> nth=${n}`).locator(`[data-f="${f}"]`); if ((await el.evaluate((e) => e.tagName)) === "SELECT") await el.selectOption(String(v)); else await el.fill(String(v)); };
  const send = async (k) => { t = Date.now(); await page.click("#go"); await done(); await grab(k); out[k].seconds = (Date.now() - t) / 1000; out[k].c = await rowsC(); out[k].cardVerdict = await page.textContent("#ct-verdict"); out[k].stats = await page.$$eval("#stats div", (l) => l.map((x) => x.textContent));
    if (shots) await page.locator("#tools").screenshot({ path: join(shots, k + "-card.png") }).catch(() => {}); };
  out.start = { rows: await rowsC(), limit: await page.textContent("#ct-limit") };
  if (shots) await page.screenshot({ path: join(shots, "controls-start.png"), fullPage: false });
  const only = arg("--only");   // e.g. --only pwm: skip the others
  // A: a timer on PA26 and a bus scan
  await add("find every part"); await setRow(0, "on", 200); await setRow(0, "off", 300);
  await send("A-toggle-PA26-and-scan");
  // B: PWM at 1 kHz, 25 percent on PA26 (the LED2 red channel)
  await clearRows(); await add("PWM"); await setRow(0, "hz", 1000); await setRow(0, "duty", 25);
  await send("B-pwm-PA26-1kHz-25");
  await setRow(0, "hz", 100); await setRow(0, "duty", 50); await send("C-pwm-PA26-100Hz-50");
  await setRow(0, "hz", 20000); await setRow(0, "duty", 75); await send("D-pwm-PA26-20kHz-75");
  await setRow(0, "pin", 13); await setRow(0, "hz", 500); await setRow(0, "duty", 10); await send("E-pwm-PA13-500Hz-10");
  await setRow(0, "duty", 100); await send("F-pwm-PA13-100-steady");
  // G: a pin read and a register read at an address nothing answers at
  await clearRows(); await add("Pin: read"); await setRow(0, "pin", 22); await setRow(0, "mode", "read"); await send("G-read-PA22");
  await clearRows(); await add("Pin: read"); await setRow(0, "pin", 26); await setRow(0, "mode", "high"); await send("G2-drive-high-PA26");
  await setRow(0, "mode", "low"); await send("G3-drive-low-PA26");
  await clearRows(); await add("Timer"); await setRow(0, "pin", 13); await setRow(0, "on", 100); await setRow(0, "off", 400); await send("H-toggle-PA13-100-400");
  // I: the sensors BoosterPack's rows (predicted: it is not on this board)
  await clearRows(); await page.selectOption("#ct-bp", "bp-bassensorsmkii"); out.bpNote = await page.textContent("#ct-bp-note"); await send("I-bp-sensors");
  await page.selectOption("#ct-bp", "boostxl-edumkii"); await send("J-bp-edumkii");
  await page.selectOption("#ct-bp", "boostxl-sharp128"); await send("K-bp-sharp");
  for (let k = 0; k < 100 && !downloads.length; k++) await page.waitForTimeout(100);   // the first send's copy is saved by the download handler
  await page.$eval("#file", (f) => { f.closest("details").open = true; });
  await page.setInputFiles("#file", downloads[0]); await page.waitForTimeout(400);
  out.fileCheck = await page.textContent("#file-check");
  t = Date.now(); await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 600000 });
  out.restore = { text: await page.textContent("#rresult"), seconds: (Date.now() - t) / 1000, copy: downloads[0] };
  console.log(JSON.stringify(out, null, 1));
  await browser.close(); wss.close(); http.close();
  process.exit(/Applied/.test(out["A-toggle-PA26-and-scan"].verdict) && /Put back and read back/.test(out.restore.text) ? 0 : 1);
}
if (process.argv.includes("--recovery")) {
  const text = (sel) => page.textContent(sel).then((x) => (x || "").replace(/\s+/g, " ").trim());
  out.banner = await text("#safe");
  await pick("Blink"); out.summary = await text("#r-recovery"); out.goNote = await text("#go-note");
  // 1. an interrupted write: cut the link once the page is flashing
  t = Date.now(); await page.click("#go");
  await page.waitForFunction(() => /Flashing/.test(document.getElementById("log").textContent), null, { timeout: 120000 });
  sent = 0; cutAfter = 40;
  await done();
  out.interrupted = { verdict: await text("#verdict"), measured: await text("#measured"), next: await text("#next"), seconds: (Date.now() - t) / 1000, downloads: downloads.length };
  if (shots) await page.screenshot({ path: join(shots, "recovery-interrupted.png"), fullPage: false });
  // the probe is back: connect again, as a person would after replugging
  await page.click("#connect");
  await page.waitForFunction(() => ["on", "bad"].includes(document.getElementById("found").dataset.state) && !/Looking/.test(document.getElementById("found").textContent), null, { timeout: 60000 });
  out.reconnect = await text("#found");
  const copy = downloads[0], real = JSON.parse(readFileSync(copy, "utf8"));
  await page.click('summary:has-text("Put back")');
  const tryFile = async (name, obj) => {
    const f = join(process.env.TMPDIR || "/tmp", name); require("fs").writeFileSync(f, JSON.stringify(obj));
    await page.setInputFiles("#file", f); await page.waitForTimeout(400);
    const r = { check: await text("#file-check"), buttonEnabled: !(await page.$eval("#restore", (b) => b.disabled)) };
    if (r.buttonEnabled) { await page.click("#restore"); await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 600000 }); r.result = await text("#rresult"); }
    return r;
  };
  // 2. a copy from another chip: right kit and checksum, another trace ID; the chip check refuses it before any erase
  out.otherChip = await tryFile("frl-other-chip.json", { ...real, traceid: "0x00000001" });
  // 3. a damaged copy: the checksum fails, the button stays off
  const bad = { ...real, data: real.data.slice(0, -8) + "AAAAAAA=" };
  out.damaged = await tryFile("frl-damaged.json", bad);
  // 4. the real copy: put back and read back
  t = Date.now(); await page.setInputFiles("#file", copy); await page.waitForTimeout(400);
  out.realCheck = await text("#file-check");
  await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden") && !/^$/.test(document.getElementById("rresult").textContent), null, { timeout: 600000 });
  out.restore = { text: await text("#rresult"), seconds: (Date.now() - t) / 1000, copy, sha256: real.sha256, steps: await page.$$eval("#rlog li", (l) => l.map((x) => x.textContent)) };
  if (shots) await page.screenshot({ path: join(shots, "recovery-put-back.png"), fullPage: false });
  console.log(JSON.stringify(out, null, 1));
  await browser.close(); wss.close(); http.close();
  const ok = /Stopped/.test(out.interrupted.verdict) && !/It works/.test(out.interrupted.verdict) && /another board/.test(out.otherChip.result || "")
    && !out.damaged.buttonEnabled && /checksum/.test(out.damaged.check) && /Put back and read back/.test(out.restore.text);
  process.exit(ok ? 0 : 1);
}
if (process.argv.includes("--examples")) {   // the starter examples (build_examples.py): blink SOS and fast, button to LED, breathing LED, then the copy put back
  const text = (sel) => page.textContent(sel).then((x) => (x || "").replace(/\s+/g, " ").trim());
  const ids = (arg("--ids") || "blink-sos,blink-fast,button-led,breathe").split(",");
  await page.evaluate(() => { window.__frlTestPress = true; });   // nobody presses S2 here: the debugger drives the pin low (bench.js watchButton)
  out.recipes = (await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent))).join(" | ");
  for (const id of ids) {
    await page.click(`.recipe[data-id="${id}"]`);
    out[id] = { see: await text("#r-see"), proves: await text("#r-proves"), go: await text("#go-note") };
    t = Date.now(); await page.click("#go"); await done(); await grab(id); out[id].seconds = (Date.now() - t) / 1000;
    out[id].watch = { a: await text("#wt-a"), b: await text("#wt-b"), note: await text("#wt-note") };
    out[id].next = await text("#next"); out[id].stats = await page.$$eval("#stats div", (l) => l.map((x) => x.textContent));
  }
  await page.waitForTimeout(500);
  if (downloads.length) {
    await page.click('summary:has-text("Put back")');
    await page.setInputFiles("#file", downloads[0]);
    t = Date.now(); await page.click("#restore");
    await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 600000 });
    out.restore = { text: await text("#rresult"), seconds: (Date.now() - t) / 1000, copy: downloads[0] };
  }
  console.log(JSON.stringify(out, null, 1));
  await browser.close(); wss.close(); http.close();
  process.exit(ids.every((k) => /works|followed/i.test(out[k].verdict)) && /Put back/.test(out.restore?.text || "") ? 0 : 1);
}
const quick = process.argv.includes("--quick");   // another board: colours (if it has an RGB LED), blink, put back
if (!quick) {
await page.click(`.recipe[data-kind="oob"]`);
await oobRead("light", "oob-factory-light"); await oobRead("thermometer", "oob-factory-therm"); await page.click('[data-oob="stop"]');
t = Date.now(); await page.click("#go"); await done(); out["oob-flash"] = { verdict: await page.textContent("#verdict"), seconds: (Date.now() - t) / 1000 };
await oobRead("light", "oob-sdk-light"); await oobRead("thermometer", "oob-sdk-therm"); await page.click('[data-oob="stop"]');
}
const titles = await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent));
out.recipes = titles.join(" | ");
if (titles.some((x) => /colour/.test(x))) {
await pick("colour"); await page.click('#presets button:has-text("Police")');
t = Date.now(); await page.click("#go"); await done(); await grab("colors-1"); out["colors-1"].seconds = (Date.now() - t) / 1000;
await page.click('#presets button:has-text("Traffic light")');
t = Date.now(); await page.click("#go"); await done(); await grab("colors-2"); out["colors-2"].seconds = (Date.now() - t) / 1000;
}
if (!quick) { await pick("Speed test"); t = Date.now(); await page.click("#go"); await done(); await grab("speed"); out.speed.seconds = (Date.now() - t) / 1000;
}
if (titles.some((x) => /thermometer/i.test(x))) {   // the thermometer: first reading, then the live dial for 4 s
await pick("thermometer"); t = Date.now(); await page.click("#go"); await done(); await grab("thermo"); out.thermo.seconds = (Date.now() - t) / 1000;
await page.waitForTimeout(4000); out.thermo.dial = await page.textContent("#th-num"); out.thermo.note = await page.textContent("#th-note");
if (shots) await page.screenshot({ path: join(shots, "stream-live.png"), fullPage: false });
}
if (await page.$('.recipe[data-kind="loop"]')) {   // the first wire: one sweep, then the live chart (passes only with the wire in)
  await page.click('.recipe[data-kind="loop"]'); t = Date.now(); await page.click("#go"); await done(); await grab("loop"); out.loop.seconds = (Date.now() - t) / 1000;
  await page.waitForTimeout(3000); out.loop.note = await page.textContent("#lp-note");
  if (shots) await page.screenshot({ path: join(shots, "loop-live.png"), fullPage: false });
}
if (await page.$('.recipe[data-kind="digit"]')) {   // draw a digit: self-test, then BitNetMCU's 10 test digits through the page's drawing path
  await page.click('.recipe[data-kind="digit"]'); t = Date.now(); await page.click("#go"); await done(); await grab("digit"); out.digit.seconds = (Date.now() - t) / 1000;
  const h = readFileSync(join(ROOT, "third-party", "bitnetmcu", "BitNetMCU_MNIST_test_data.h"), "utf8");
  const ins = [...h.matchAll(/input_data_(\d+)\[256\] = \{([^}]*)\}/g)].map((m) => m[2].split(",").map((x) => x.trim()).filter(Boolean).map((x) => { const v = parseInt(x, 16); return v > 127 ? v - 256 : v; }));
  const labels = [...h.matchAll(/label_(\d+) = (\d+);/g)].map((m) => Number(m[2]));
  const N = 72, got = [];
  for (let k = 0; k < ins.length; k++) {   // the grid as ports/bitnetmcu/host/prep_test.c draws it: pixels above 40 become ink
    const grid = Array.from({ length: N * N }, (_, i) => (ins[k][Math.floor(Math.floor(i / N) * 16 / N) * 16 + Math.floor((i % N) * 16 / N)] > 40 ? 1 : 0));
    await page.evaluate((g) => window.dispatchEvent(new CustomEvent("frl-test-digit", { detail: g })), grid);
    await page.waitForFunction(() => /^\d$/.test(document.getElementById("dg-ans").textContent) || /Stopped/.test(document.getElementById("dg-time").textContent), null, { timeout: 15000 });
    got.push({ label: labels[k], read: Number(await page.textContent("#dg-ans")), note: await page.textContent("#dg-time") });
  }
  out.digit.drawn = got.map((g) => `${g.label}->${g.read}`).join(" "); out.digit.right = got.filter((g) => g.label === g.read).length;
  out.digit.time = got[got.length - 1].note;
  if (shots) await page.screenshot({ path: join(shots, "digit-live.png"), fullPage: false });
}
await pick("Blink"); t = Date.now(); await page.click("#go"); await done(); await grab("blink"); out.blink.seconds = (Date.now() - t) / 1000;
if (!quick) {
await pick("Print"); t = Date.now(); await page.click("#go"); await done(); await grab("message"); out.message.seconds = (Date.now() - t) / 1000;
}
await page.waitForTimeout(500);
if (downloads.length) {
  await page.click('summary:has-text("Put back")');
  await page.setInputFiles("#file", downloads[0]);
  t = Date.now(); await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 600000 });
  out.restore = { text: await page.textContent("#rresult"), seconds: (Date.now() - t) / 1000, copy: downloads[0],
    steps: await page.$$eval("#rlog li", (l) => l.map((x) => x.textContent)) };
}
console.log(JSON.stringify(out, null, 1));
await browser.close(); wss.close(); http.close();
const ok = quick
  ? ["colors-1", "colors-2", "blink", "thermo"].filter((k) => out[k]).every((k) => /works|playing/.test(out[k].verdict)) && (!out.digit || out.digit.right === 10) && /Put back/.test(out.restore?.text || "")
  : ["colors-1", "colors-2", "blink", "message"].every((k) => /works|playing/.test(out[k].verdict))
    && ["oob-factory-light", "oob-factory-therm", "oob-sdk-light", "oob-sdk-therm"].every((k) => /\d/.test(out[k].verdict)) && /CoreMark \d/.test(out.speed.verdict) && /Put back/.test(out.restore?.text || "");
process.exit(ok ? 0 : 1);
