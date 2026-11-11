// The Digital Lab page in headless Chrome against the fake XDS110 (sim-xds110.mjs), no board: the chooser cancelled, WebHID
// blocked, a probe another program holds, an unknown chip, the whole save, write, check and put-back path, an unplug part way
// through a write followed by reconnect and put-back, and the diagnostic record. Nothing here touches USB.
//
//   NODE_PATH=<folder with playwright-core> node tools/webflash/dlab-sim-run.mjs --out <folder> [--port 8799]
//
// Starts tools/webflash/serve.py itself. Writes results.json, a PNG per scenario and the files the page downloaded. A pass
// here is a software result: the real chooser, USB stack and board are not involved.
import { createRequire } from "module";
import { spawn } from "child_process";
import { readFileSync, writeFileSync, mkdirSync } from "fs";
import { basename, dirname, join } from "path";
import { fileURLToPath } from "url";

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, "..", "..");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const OUT = arg("--out"), PORT = Number(arg("--port", "8799"));
if (!OUT) { console.error("--out <folder> is required"); process.exit(2); }
mkdirSync(OUT, { recursive: true });
const require = createRequire(join(process.env.NODE_PATH || HERE, "x.js"));
const { chromium } = require("playwright-core");

// the simulator as a classic script: window.FrlSim.makeFakeXds110 / makeFakeHid
const SIM = "window.FrlSim = (() => {\n" + readFileSync(join(HERE, "sim-xds110.mjs"), "utf8").replace(/^export /gm, "") + "\nreturn { makeFakeXds110, makeFakeHid };\n})();";
const L1306 = { deviceid: 0x0BB8202F, idAddr: 0x41C40000, flashBytes: 0x10000, led: { gpio: 0x400A0000, dout: 0x1280, doe: 0x12C0, bit: 26, pattern: [150, 150, 150, 900] } };

const results = [];
const rec = (name, ok, detail) => { results.push({ name, ok: !!ok, detail: String(detail || "").slice(0, 600) }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? "  | " + String(detail).slice(0, 200) : ""}`); };

const server = spawn("python3", [join(HERE, "serve.py"), "--port", String(PORT)], { cwd: ROOT, stdio: "ignore" });
await new Promise((r) => setTimeout(r, 1500));
const browser = await chromium.launch({ channel: "chrome", headless: true });
const URL0 = `http://localhost:${PORT}/site/flash/bench/?board=mspm0l1306`;

async function open(name, setup) {
  const ctx = await browser.newContext({ acceptDownloads: true, viewport: { width: 1366, height: 900 } });
  const page = await ctx.newPage();
  const downloads = [];
  page.on("download", async (d) => { const p = join(OUT, `${name}-${d.suggestedFilename()}`); await d.saveAs(p); downloads.push(p); });
  page.on("pageerror", (e) => rec(`${name}: no page error`, false, e.message));
  await page.addInitScript(SIM);
  await page.addInitScript(({ setup, L1306 }) => {
    const opts = { ...L1306, ...(setup.sim || {}) };
    const sim = window.FrlSim.makeFakeXds110(opts);
    const hid = window.FrlSim.makeFakeHid([sim], setup.mode || "grant");
    Object.defineProperty(navigator, "hid", { value: hid, configurable: true });
    window.__sim = sim; window.__hid = hid;
  }, { setup, L1306 });
  await page.goto(URL0);
  await page.waitForSelector(".recipe", { timeout: 30000 });
  await page.waitForFunction(() => !document.getElementById("connect").disabled, null, { timeout: 30000 });
  return { ctx, page, downloads };
}
const found = (page) => page.textContent("#found");
const connectAndWait = async (page) => {
  await page.click("#connect");
  await page.waitForFunction(() => { const f = document.getElementById("found"); return f.dataset.state !== "busy" && f.textContent.trim() && !/Looking/.test(f.textContent); }, null, { timeout: 60000 });
};
const write = async (page, ms = 120000) => {
  await page.click('.recipe:has-text("Blink")');
  await page.click("#go");
  await page.waitForSelector("#c-plan:not([hidden])", { timeout: 5000 });
  await page.click("#c-write");
  await page.waitForFunction(() => !document.getElementById("result").classList.contains("hidden") && !document.getElementById("go").disabled, null, { timeout: ms });
  return { verdict: await page.textContent("#verdict"), next: await page.textContent("#next") };
};
const flashMatches = (page) => page.evaluate(() => window.__sim.flash.every((v, i) => v === window.__sim.original[i]));

try {
  { // 1. the person closes Chrome's chooser
    const { ctx, page } = await open("cancel", { mode: "cancel" });
    await connectAndWait(page);
    const f = await found(page);
    rec("chooser closed: says no board was picked and how to retry, nothing written", /No board was picked/.test(f) && /Press Connect/.test(f), f);
    await page.screenshot({ path: join(OUT, "cancel.png") }); await ctx.close();
  }
  { // 2. a policy or an insecure origin blocks WebHID
    const { ctx, page } = await open("blocked", { mode: "blocked" });
    await connectAndWait(page);
    const f = await found(page);
    rec("WebHID blocked: names the block and chrome://policy", /blocked WebHID/.test(f) && /chrome:\/\/policy/.test(f), f);
    rec("WebHID blocked: Show what to check and the diagnostic button appear", await page.isVisible("#found-help") && await page.isVisible("#found-diag"), "");
    await ctx.close();
  }
  { // 3. another program holds the probe
    const { ctx, page } = await open("busy", { sim: { fault: { busy: true } } });
    await connectAndWait(page);
    const f = await found(page);
    rec("probe held by another program: says so and names CCStudio, not an unknown board", /could not open the board/.test(f) && /CCStudio/.test(f) && !/does not know/.test(f), f);
    await page.screenshot({ path: join(OUT, "busy.png") }); await ctx.close();
  }
  { // 4. an XDS110 on a chip this page does not know
    const { ctx, page } = await open("wrong", { sim: { deviceid: 0x0ABCD02F } });
    await connectAndWait(page);
    const f = await found(page);
    rec("unknown chip: refused, nothing written, says how to pick another", /does not know/.test(f) && /nothing was written/i.test(f), f);
    rec("unknown chip: no erase or program reached the chip", (await page.evaluate(() => window.__sim.counts.erase + window.__sim.counts.program)) === 0, "");
    await ctx.close();
  }
  { // 4b. a probe whose chip never answers (unpowered, asleep, jumpers off): a plain answer within 15 s, the
    // complaint users raise most about browser flashers is a connect that spins for minutes
    const { ctx, page } = await open("dead", { sim: { fault: { deadChip: true } } });
    const t0 = Date.now();
    await connectAndWait(page);
    const secs = (Date.now() - t0) / 1000, f = await found(page);
    rec(`dead chip: the connect answers within 15 s (took ${secs.toFixed(1)} s)`, secs <= 15, f);
    rec("dead chip: says no chip answered and to check power and jumpers, nothing written",
      /no chip did|did not answer/i.test(f) && /power/i.test(f) && /jumper/i.test(f) && (await page.evaluate(() => window.__sim.counts.erase + window.__sim.counts.program)) === 0, f);
    await page.screenshot({ path: join(OUT, "dead.png") }); await ctx.close();
  }
  { // 5. connect, save, write, check, put back
    const { ctx, page, downloads } = await open("happy", {});
    await connectAndWait(page);
    const f = await found(page);
    rec("connect names the board from its chip ID", /LP-MSPM0L1306/.test(f), f);
    const r = await write(page);
    rec("write and check: the page's own check passes against the simulated LED", /^It works/.test(r.verdict), r.verdict);
    rec("the backup was saved before the write", downloads.some((p) => /\.json$/.test(p) && !/diagnostic/.test(p)), downloads.join(", "));
    rec("the flash changed", !(await flashMatches(page)), "");
    rec("after the result, the way back is in view: Put the board back the way it was", await page.isVisible("#r-putback-btn"), "");
    await page.click("#r-putback-btn");
    const chosenOk = await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 30000 }).then(() => true).catch(() => false);
    rec("Put the board back: the copy saved this visit is already chosen and passes the page's checks", chosenOk, await page.textContent("#file-check"));
    await page.evaluate(() => { document.querySelector("section.more details").open = false; });   // the steps below open it themselves
    const backup = downloads.find((p) => /\.json$/.test(p) && !/diagnostic/.test(p));
    await page.click('summary:has-text("Put back")');
    { // the same copy as a raw .bin for other tools: byte for byte the .json's data
      await page.waitForSelector("#bin-save", { state: "visible", timeout: 5000 }).catch(() => {});
      const before = downloads.length;
      await page.click("#bin-save").catch(() => {});
      await page.waitForTimeout(1500);
      const bin = downloads.slice(before).find((p) => /\.bin$/.test(p));
      const js = backup && JSON.parse(readFileSync(backup, "utf8"));
      const same = !!bin && !!js && Buffer.compare(readFileSync(bin), Buffer.from(js.data, "base64")) === 0;
      rec("the saved copy also comes as a .bin, identical to the .json's data", same, bin || "no .bin downloaded");
    }
    // put back from the copy the page chose itself, not a file picked again by hand (Codex finding 12)
    const chosenName = await page.evaluate(() => (document.getElementById("file").files[0] || {}).name || "");
    rec("put back: the file chosen is the copy saved this visit", !!chosenName && !!backup && basename(backup).endsWith("-" + chosenName), `${chosenName} | ${backup}`);
    await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 30000 });
    await page.click("#restore");
    await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 120000 });
    const rr = await page.textContent("#rresult");
    rec("put back: the page reports it read back identical", /Put back and read back/.test(rr), rr);
    rec("put back: the simulated flash equals the original byte for byte", await flashMatches(page), "");
    await page.screenshot({ path: join(OUT, "happy.png"), fullPage: false });
    await ctx.close();
  }
  { // 6. unplugged part way through the write, reconnect, put back
    const { ctx, page, downloads } = await open("unplug", { sim: { fault: { unplugAt: { kind: "program", after: 1 } } } });
    await connectAndWait(page);
    const r = await write(page, 180000);
    const f = await found(page);
    rec("unplug mid-write: the result says partly written and how to put it back", /partly written/.test(r.verdict) && /Put it back/.test(r.next), `${r.verdict} | ${r.next}`);
    rec("unplug mid-write: the result card stays on screen", await page.isVisible("#result"), "");
    rec("unplug mid-write: the Connect line says the same", /partly written/.test(f), f);
    await page.screenshot({ path: join(OUT, "unplug.png") });
    await page.evaluate(() => window.__hid.replug(window.__sim));
    await page.waitForTimeout(300);
    await page.waitForFunction(() => !document.getElementById("connect").disabled, null, { timeout: 30000 });
    rec("replug: the automatic survey keeps the recovery steps on screen", /partly written/.test(await found(page)), await found(page));
    rec("replug: the result stays on screen until Connect", await page.isVisible("#result"), "");
    await connectAndWait(page);
    rec("reconnect after the unplug names the board again", /LP-MSPM0L1306/.test(await found(page)), await found(page));
    const backup = downloads.find((p) => /\.json$/.test(p) && !/diagnostic/.test(p));
    await page.click('summary:has-text("Put back")');
    await page.setInputFiles("#file", backup);
    await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 30000 });
    await page.click("#restore");
    await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 120000 });
    rec("after reconnect, put back restores the original image", /Put back and read back/.test(await page.textContent("#rresult")) && await flashMatches(page), await page.textContent("#rresult"));
    // the diagnostic record of this visit
    const before = downloads.length;
    await page.click(".more .diag-btn");
    await page.waitForFunction((n) => true, before);
    await page.waitForTimeout(800);
    const diag = downloads.slice(before).find((p) => /diagnostic/.test(p));
    const text = diag ? readFileSync(diag, "utf8") : "";
    const d = text ? JSON.parse(text) : { events: [] };
    const steps = d.events.map((e) => e.step);
    rec("diagnostic record: downloads, lists the unplug, the stop and the put-back", ["identified", "saved", "writing", "unplugged", "stopped", "put back"].every((s) => steps.includes(s)), steps.join(" > "));
    rec("diagnostic record: no full serial, no flash contents", !text.includes("SIM00001") && !/"data"/.test(text) && text.length < 60000, `${text.length} bytes`);
    await ctx.close();
  }
} catch (e) {
  rec("run finished without an exception", false, e.stack || e.message);
} finally {
  await browser.close();
  server.kill();
  const pass = results.filter((r) => r.ok).length;
  writeFileSync(join(OUT, "results.json"), JSON.stringify({ when: new Date().toISOString(), kind: "simulated transport, headless Chrome; no board", pass, total: results.length, results }, null, 2));
  console.log(`${pass} of ${results.length} passed`);
  process.exitCode = pass === results.length ? 0 : 1;
}
