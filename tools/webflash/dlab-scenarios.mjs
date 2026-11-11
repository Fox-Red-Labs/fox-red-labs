// Scenarios for dlab-browser-run.mjs. Each runs the Digital Lab page against one real board and records pass or fail per check.
import { readFileSync } from "fs";

const connected = (page) => page.waitForFunction(() => ["on", "bad"].includes(document.getElementById("found").dataset.state) && !/Looking/.test(document.getElementById("found").textContent), null, { timeout: 60000 });
const finished = (page, ms = 240000) => page.waitForFunction(() => !document.getElementById("result").classList.contains("hidden") && !document.getElementById("go").disabled && !/Writing|Flashing|%/.test(document.getElementById("go").textContent), null, { timeout: ms });

// "Take it home" starts folded (ui.js adds .folded; only its head or "Run the full version" opens it), so its Copy buttons are hidden until opened.
export async function openTakeHome(page) {
  await page.evaluate(() => { const t = document.getElementById("takehome"); if (t) t.classList.remove("folded"); });
  await page.locator("#th-term-copy").scrollIntoViewIfNeeded();
}

async function connect(page, rec, shot, kit) {
  await page.click("#connect");
  await connected(page);
  const found = await page.textContent("#found");
  rec(`${kit}: Connect lists and names the board`, (await page.getAttribute("#found", "data-state")) === "on" && found.includes(kit), found);
  const list = (await page.textContent("#boards")).replace(/\s+/g, " ");
  rec(`${kit}: boards list shows it In use`, list.includes(kit) && /In use/.test(list), list);
}

// Pick a demo, press Flash it, read the Before it writes panel, press Write it, wait for the verdict.
async function flashDemo(page, rec, text, shot, { title, name, serial, expect, ms }) {
  await page.click(`.recipe:has-text("${title}")`);
  await page.click("#go");
  await page.waitForSelector("#c-plan:not([hidden])", { timeout: 5000 }).catch(() => {});
  const plan = await text("#c-plan");
  rec(`${name}: Before it writes panel shows, names the serial`, !(await page.$eval("#c-plan", (e) => e.hidden)) && plan.includes(serial), plan);
  await shot(`${name}-plan`);
  const t0 = Date.now();
  await page.click("#c-write");
  await finished(page, ms);
  const verdict = await text("#verdict");
  rec(`${name}: Write it runs and the page's own check passes`, expect.test(verdict), `${verdict} (${((Date.now() - t0) / 1000).toFixed(1)} s) ${await text("#measured")}`);
  await shot(`${name}-result`);
  return verdict;
}

export async function run(ctx) {
  const { page, rec, text, shot, url, probe, downloads, out, scenario } = ctx;
  await page.goto(url);
  await page.waitForSelector(".recipe", { timeout: 30000 });
  await page.waitForTimeout(1200);
  if (scenario === "connect") {
    await connect(page, rec, shot, "LP-MSPM0L1306");
    return;
  }
  if (scenario === "l1306") return l1306(ctx);
  if (scenario === "cc2340r5") return cc2340(ctx);
  if (scenario === "restore") {   // put a saved copy back through the page: --kit <name> --file <copy.json>
    await connect(page, rec, shot, ctx.arg("--kit"));
    await page.click('summary:has-text("Put back")'); await page.setInputFiles("#file", ctx.arg("--file"));
    await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 30000 });
    rec("copy accepted", true, await text("#file-check")); const t0 = Date.now(); await page.click("#restore");
    await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 900000 });
    const rr = await text("#rresult"); rec("Put it back restores and reads back", /Put back and read back/.test(rr), `${rr} (${((Date.now() - t0) / 1000).toFixed(1)} s)`); return;
  }
  if (scenario === "ai") return (await import("./dlab-ai-scenario.mjs")).ai(ctx);   // Pick an AI model, Change, Go deeper
  if (scenario === "ai-layout") return (await import("./dlab-ai-scenario.mjs")).layout(ctx);   // page height with a board connected
  if (scenario === "two") return two(ctx);
  if (scenario === "family") return family(ctx);
  if (scenario === "dbg") {
    await page.evaluate((s) => { window.__frlAllow = s; }, [probe, ctx.arg("--also")]);
    await connect(page, rec, shot, ctx.arg("--kit")); await page.click(".recipe >> nth=0"); await page.waitForTimeout(800);
    rec("dbg takehome", true, await page.evaluate(() => { const t = document.getElementById("takehome"); const a = document.querySelector("#takehome #th-ai"); return "hidden=" + t.classList.contains("hidden") + " ai=" + !!a + " prev=" + (a && a.querySelector(".k-prev") ? a.querySelector(".k-prev").textContent.slice(0, 80) : "none") + " labels=" + (a ? a.querySelectorAll("label").length : -1) + " checked=" + (a ? a.querySelectorAll("input:checked").length : -1) + " attrs=" + (a && a.querySelector("input") ? [...a.querySelector("input").getAttributeNames()].join(",") : "") + " html=" + t.innerHTML.length; }));
    rec("dbg", true, await page.evaluate(async () => { const t = performance.now(); const d = await navigator.hid.getDevices(); return d.length + " devices in " + (performance.now() - t) + " ms; map " + JSON.stringify([...window.__frlSerials.values()]) + " all " + (await navigator.hid.getDevices()).length; }));
    return;
  }
  if (scenario === "printloop") {
    await connect(page, rec, shot, "LP-MSPM0L1306");
    for (let i = 1; i <= Number(ctx.arg("--reps", "5")); i++) await flashDemo(page, rec, text, shot, { title: "Print a message", name: `L1306 print #${i}`, serial: probe, expect: /^It works/, ms: 120000 });
    return;
  }
  throw new Error("unknown scenario " + scenario);
}

async function l1306(ctx) {
  const { page, rec, text, shot, probe, downloads, out } = ctx;
  const kit = "LP-MSPM0L1306";
  await connect(page, rec, shot, kit);
  await shot("l1306-connected");
  // Blink
  await flashDemo(page, rec, text, shot, { title: "Blink a pattern", name: "L1306 blink", serial: probe, expect: /^It works/, ms: 120000 });
  rec("L1306: the page saved a backup before the first write", downloads.length >= 1 && /\.json$/.test(downloads[0] || ""), downloads.join(","));
  // Talk tab on Print a message
  await flashDemo(page, rec, text, shot, { title: "Print a message", name: "L1306 print", serial: probe, expect: /^It works/, ms: 120000 });
  await page.click("#t-talk");
  await page.click("#c-con-open");
  await page.waitForTimeout(6000);
  let con = await text("#c-con");
  if (!/Hello/.test(con)) { await page.waitForTimeout(8000); con = await text("#c-con"); }
  rec("L1306: Talk tab console shows the board's message", /Hello from my LaunchPad/.test(con), con.slice(0, 200));
  await shot("l1306-talk");
  await page.click("#c-con-open"); // close
  // Copy buttons
  await openTakeHome(page);
  await page.click("#th-term-copy"); const term = await page.evaluate(() => navigator.clipboard.readText());
  rec("L1306: Copy for your terminal text carries the serial", term.includes("--probe " + probe), term.replace(/\n/g, " | ").slice(-260));
  await page.click("#th-ai-copy"); const ai = await page.evaluate(() => navigator.clipboard.readText());
  rec("L1306: Copy for your AI text carries the serial", ai.includes(probe), ai.slice(0, 500));
  // Digit: write the reader through the page, then draw with the real mouse
  await flashDemo(page, rec, text, shot, { title: "Draw a digit", name: "L1306 digit", serial: probe, expect: /^Ready: the chip read 10 of 10/, ms: 240000 });
  await page.waitForTimeout(2000);
  await shot("l1306-digit-ready");
  const live = await page.evaluate(() => document.getElementById("dg-canvas").parentElement.className + " | " + document.getElementById("dg-lock").className);
  await page.evaluate(() => document.getElementById("dg-canvas").scrollIntoView({ block: "center" })); await page.waitForTimeout(400);
  const box = await page.locator("#dg-canvas").boundingBox();
  const stroke = async (pts) => { await page.mouse.move(box.x + pts[0][0] * box.width, box.y + pts[0][1] * box.height); await page.mouse.down();
    for (const [x, y] of pts.slice(1)) await page.mouse.move(box.x + x * box.width, box.y + y * box.height, { steps: 4 }); await page.mouse.up(); };
  const answers = [];
  const ask = async (label, pts) => {
    await page.click("#dg-clear"); await page.waitForTimeout(300);
    await stroke(pts);
    await page.waitForFunction(() => /^\d$/.test(document.getElementById("dg-ans").textContent) || /Stopped/.test(document.getElementById("dg-time").textContent), null, { timeout: 15000 }).catch(() => {});
    answers.push(`${label}->${await text("#dg-ans")} (${await text("#dg-time")})`);
  };
  await ask("1", [[0.5, 0.15], [0.5, 0.85]]);
  await shot("l1306-digit-drew-1");
  await ask("0", Array.from({ length: 25 }, (_, k) => [0.5 + 0.3 * Math.cos((k / 24) * 2 * Math.PI), 0.5 + 0.38 * Math.sin((k / 24) * 2 * Math.PI)]));
  await shot("l1306-digit-drew-0");
  rec("L1306: Draw a digit answers a mouse-drawn 1 and 0 with a digit", answers.every((a) => /->\d /.test(a)), live + " :: " + answers.join(" ; "));
  // the page-level restore
  await page.click('summary:has-text("Put back")');
  await page.setInputFiles("#file", downloads[0]);
  await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 15000 });
  rec("L1306: the first saved copy passes the page's own check", true, await text("#file-check"));
  const t0 = Date.now();
  await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 300000 });
  const rr = await text("#rresult");
  rec("L1306: Put it back restores the first copy and reads it back", /Put back and read back/.test(rr), `${rr} (${((Date.now() - t0) / 1000).toFixed(1)} s)`);
  await shot("l1306-restored");
}

async function two(ctx) {
  const { page, rec, text, shot, probe, downloads } = ctx;
  const also = ctx.arg("--also-dap", ctx.also);   // the CMSIS-DAP serial the page shows; on an LP-XDS110 it differs from the USB serial
  const kitA = "LP-MSPM0L1306", kitB = "LP-EM-CC2340R5";
  await connect(page, rec, shot, kitA);
  // a second probe is allowed now: the page names it with its survey, then Use this one switches
  await page.evaluate((s) => { window.__frlAllow = s; }, [probe, also]);
  await page.click("#survey");
  await page.waitForFunction(() => /LP-EM-CC2340R5/.test(document.getElementById("boards").textContent), null, { timeout: 60000 }).catch(() => {});
  const list = await text("#boards");
  rec("two boards: the survey names the second board", list.includes(kitB) && list.includes(kitA), list);
  await shot("two-list");
  await page.click('#boards li:has-text("' + kitB + '") button:has-text("Use this one")');
  await page.waitForTimeout(800);
  const f = await text("#found");
  rec(`two boards: Use this one switches to ${kitB} on ${also}`, f.includes(kitB) && f.includes(also), f);
  rec("two boards: the demo list changes with the board", (await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent))).some((t) => /Bluetooth/.test(t)), (await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent))).join(" | "));
  await page.click('.recipe:has-text("Bluetooth")');
  await page.waitForTimeout(500);
  await openTakeHome(page);
  await page.click("#th-ai-copy"); const aiB = await page.evaluate(() => navigator.clipboard.readText());
  rec(`${kitB}: Copy for your AI carries ${also} and not ${probe}`, aiB.includes(also) && !aiB.includes(probe), aiB.slice(-300));
  await page.click("#th-term-copy"); const termB = await page.evaluate(() => navigator.clipboard.readText());
  rec(`${kitB}: Copy for your terminal text`, true, `serial in text: ${termB.includes(also)} :: ` + termB.replace(/\n/g, " | ").slice(-300));
  const goVisible = await page.$eval("#go", (b) => !b.classList.contains("hidden") && !b.disabled);
  rec(`${kitB}: Before it writes panel opens and Cancel writes nothing`, await (async () => {
    if (!goVisible) return false;
    await page.click("#go"); await page.waitForSelector("#c-plan:not([hidden])", { timeout: 5000 }).catch(() => {});
    const plan = await text("#c-plan"); await shot("two-cc2340-plan"); await page.click("#c-cancel");
    ctx.planB = plan; return plan.includes(also) && downloads.length === 0;
  })(), (ctx.planB || "go button not available").slice(0, 400));
  await page.click('#boards li:has-text("' + kitA + '") button:has-text("Use this one")');
  await page.waitForTimeout(800);
  const f2 = await text("#found");
  rec(`two boards: Use this one switches back to ${kitA} on ${probe}`, f2.includes(kitA) && f2.includes(probe), f2);
  await page.click(`.recipe:has-text("Blink a pattern")`); await page.click("#th-ai-copy");
  const aiA = await page.evaluate(() => navigator.clipboard.readText());
  rec(`${kitA}: Copy for your AI after switching back carries ${probe} and not ${also}`, aiA.includes(probe) && !aiA.includes(also), aiA.slice(-200));
  await shot("two-back");
}

async function cc2340(ctx) { throw new Error("todo"); }

// Any MSPM0 or M33 board that browser-flashes: every demo the page lists for it, the Copy texts, then the first saved copy put back.
async function family(ctx) {
  const { page, rec, text, shot, probe, downloads } = ctx;
  const kit = ctx.arg("--kit"), dap = ctx.arg("--dap", probe);
  await connect(page, rec, shot, kit);
  const titles = await page.$$eval(".recipe .t", (l) => l.map((x) => x.textContent));
  rec(`${kit}: demos listed`, titles.length > 0, titles.join(" | "));
  const bad = /^(Stopped|Flashed, but|Could not|The chip did not|It did not|Nothing|The fade did not|The LED did not)/;
  const skip = ctx.arg("--skip", ""), only = ctx.arg("--only", "");
  for (const title of titles) {
    if (only && !new RegExp(only, "i").test(title)) continue;
    if (skip && new RegExp(skip, "i").test(title)) { rec(`${kit} ${title.slice(0, 26)}: skipped`, true, "needs a jumper or a button press the bench does not supply"); continue; }
    const name = `${kit} ${title.slice(0, 26)}`;
    try {
      await page.click(`.recipe:has-text("${title}")`);
      const go = await page.$eval("#go", (b) => !b.disabled && !b.classList.contains("hidden"));
      if (!go) { rec(`${name}: Flash it available`, false, "button not available"); continue; }
      await page.click("#go");
      await page.waitForSelector("#c-plan:not([hidden])", { timeout: 5000 }).catch(() => {});
      const plan = await text("#c-plan");
      rec(`${name}: Before it writes shows the serial`, plan.includes(dap), plan.slice(0, 160));
      await page.click("#c-write");
      await finished(page, 300000);
      const verdict = await text("#verdict");
      // The page's own judgement is the class show() sets on #verdict ("ok" or "bad"); the phrase list is only a
      // second guard. A run on 2026-11-08 logged "The fade did not check out" as a pass because that phrase was not listed.
      const pageOk = await page.$eval("#verdict", (e) => e.classList.contains("ok") && !e.classList.contains("bad")).catch(() => false);
      rec(`${name}: Write it, page check`, pageOk && !bad.test(verdict) && verdict.length > 0, `${verdict} ${await text("#measured")}`.slice(0, 380));
      await shot(name.replace(/\W+/g, "-"));
      if (/digit/i.test(title)) {
        await page.evaluate(() => document.getElementById("dg-canvas").scrollIntoView({ block: "center" })); await page.waitForTimeout(400);
        const box = await page.locator("#dg-canvas").boundingBox();
        const stroke = async (pts) => { await page.mouse.move(box.x + pts[0][0] * box.width, box.y + pts[0][1] * box.height); await page.mouse.down();
          for (const [x, y] of pts.slice(1)) await page.mouse.move(box.x + x * box.width, box.y + y * box.height, { steps: 4 }); await page.mouse.up(); };
        const ans = [];
        for (const [l, pts] of [["1", [[0.5, 0.15], [0.5, 0.85]]], ["0", Array.from({ length: 25 }, (_, k) => [0.5 + 0.3 * Math.cos((k / 24) * 2 * Math.PI), 0.5 + 0.38 * Math.sin((k / 24) * 2 * Math.PI)])]]) {
          await page.click("#dg-clear"); await page.waitForTimeout(300); await stroke(pts);
          await page.waitForFunction(() => /^\d$/.test(document.getElementById("dg-ans").textContent), null, { timeout: 15000 }).catch(() => {});
          ans.push(`${l}->${await text("#dg-ans")}`);
        }
        rec(`${name}: mouse-drawn 1 and 0 answered`, ans.join(" ") === "1->1 0->0", ans.join(" "));
      }
      if (/thermometer/i.test(title)) {
        await page.waitForTimeout(4000);
        rec(`${name}: live reading shows`, /\d/.test(await text("#th-num")), await text("#th-num") + " " + await text("#th-note"));
        if (await page.$eval("#th-stop", (b) => !b.classList.contains("hidden"))) await page.click("#th-stop");
      }
    } catch (e) { rec(`${name}: crashed`, false, e.message.slice(0, 300)); }
  }
  const copyOf = async (btn) => { await page.bringToFront(); await openTakeHome(page); await page.evaluate(() => navigator.clipboard.writeText("")).catch(() => {}); await page.locator(btn).scrollIntoViewIfNeeded();
    for (let i = 0; i < 3; i++) { await page.click(btn); await page.waitForTimeout(600); const t = await page.evaluate(() => navigator.clipboard.readText()).catch(() => ""); if (t) return t; } return ""; };
  const term = await copyOf("#th-term-copy"), ai = await copyOf("#th-ai-copy");
  const prev = await page.evaluate(() => { const e = document.querySelector("#takehome .k-prev"); return e ? e.textContent : "(no preview)"; });
  rec(`${kit}: Take it home preview text carries the serial`, prev.includes(dap), prev.slice(-200));
  rec(`${kit}: Copy for your AI carries the serial`, ai.includes(dap), ai.slice(-260));
  rec(`${kit}: Copy for your terminal names the serial with the lookup note when it runs start.sh`, (/frl detect/.test(term) && term.includes("--probe " + dap)) || !/\.\/start\.sh/.test(term), term.replace(/\n/g, " | ").slice(-260));
  if (!downloads.length) { rec(`${kit}: a backup was saved`, false, "none"); return; }
  rec(`${kit}: a backup was saved before the first write`, /\.json$/.test(downloads[0]), downloads[0].split("/").pop());
  await page.click('summary:has-text("Put back")');
  await page.setInputFiles("#file", downloads[0]);
  await page.waitForFunction(() => !document.getElementById("restore").disabled, null, { timeout: 15000 });
  const t0 = Date.now(); await page.click("#restore");
  await page.waitForFunction(() => !document.getElementById("rresult").classList.contains("hidden"), null, { timeout: 400000 });
  const rr = await text("#rresult");
  rec(`${kit}: Put it back restores and reads back`, /Put back and read back/.test(rr), `${rr} (${((Date.now() - t0) / 1000).toFixed(1)} s)`);
}
