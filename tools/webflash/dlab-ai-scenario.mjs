// The Digital Lab's "Pick an AI model", Change (shared parameter block) and Go deeper, against one real board in real Chrome.
//   node tools/webflash/dlab-browser-run.mjs --probe MG000001 --scenario ai --kit LP-MSPM0G3519 --out <folder>
// Called by dlab-scenarios.mjs. Flashes the demos it names (the page saves the board's flash first), draws one digit by mouse, asks one occupancy window,
// sends new blink settings, and records what the page shows.
const connected = (page) => page.waitForFunction(() => ["on", "bad"].includes(document.getElementById("found").dataset.state) && !/Looking/.test(document.getElementById("found").textContent), null, { timeout: 60000 });
const finished = (page, ms = 240000) => page.waitForFunction(() => !document.getElementById("result").classList.contains("hidden") && !document.getElementById("go").disabled && !/Writing|Flashing|%/.test(document.getElementById("go").textContent) && document.getElementById("result-card").dataset.state !== "run", null, { timeout: ms });

export async function ai(ctx) {
  const { page, rec, text, shot, url } = ctx;
  const kit = ctx.arg("--kit", "LP-MSPM0G3519"), mode = ctx.arg("--only", "all");
  await page.goto(url);
  await page.waitForFunction(() => document.querySelectorAll("#recipes .recipe").length > 0, null, { timeout: 30000 });
  const use = await page.waitForSelector('#boards button:has-text("Use this one")', { timeout: 8000 }).catch(() => null);
  if (use) await use.click(); else await page.click("#connect");
  await connected(page);
  rec(`${kit}: connected`, (await page.getAttribute("#found", "data-state")) === "on", await text("#found"));
  // best first and the picker
  const first = await page.$$eval("#recipes .recipe", (els) => els.slice(0, 4).map((e) => e.querySelector(".t").textContent));
  const nModels = await page.$$eval("#ai-models .recipe", (e) => e.length);
  rec(`${kit}: the picker lists its models`, !(await page.$eval("#ai-pick", (e) => e.classList.contains("hidden"))) && nModels >= 7, `${nModels} models; grid starts: ${first.join(" | ")}`);
  await shot("picker");
  const run = async (id, name, expect, ms = 240000) => {
    if (!(await page.locator(`.recipe[data-id="${id}"]`).isVisible())) { if (id.startsWith("digit") || id.startsWith("occ")) await page.click("[data-ai-picker]"); }
    await page.click(`.recipe[data-id="${id}"]`);
    await page.click("#go"); await page.waitForSelector("#c-plan:not([hidden])", { timeout: 5000 }).catch(() => {});
    await page.click("#c-write"); await finished(page, ms);
    const v = await text("#verdict");
    rec(`${name}: flashed and the page's own check passed`, expect.test(v), `${v} ${await text("#measured")}`);
  };
  if (mode === "all" || mode === "digit") {
    await run("digit-1k", "digit 1k", /read 10 of 10/);
    await page.waitForSelector(".dg-pad.live", { timeout: 15000 }).catch(() => {});
    // draw a 1 by mouse
    // a real mouse stroke: the pad scrolled to the middle of the window (under the sticky header a click lands on the header), then a vertical "1"
    await page.locator("#dg-canvas").evaluate((e) => e.scrollIntoView({ block: "center" }));
    const box = await page.locator("#dg-canvas").boundingBox();
    const x = box.x + box.width * 0.5, y0 = box.y + box.height * 0.2, y1 = box.y + box.height * 0.8;
    rec("digit 1k: the pad is the element under the pointer", await page.evaluate(([x, y]) => document.elementFromPoint(x, y).id === "dg-canvas", [x, y0]), `canvas at ${Math.round(box.x)},${Math.round(box.y)}`);
    await page.mouse.move(x, y0); await page.mouse.down(); await page.mouse.move(x, y1, { steps: 25 }); await page.mouse.up();
    await page.waitForFunction(() => document.getElementById("dg-ans").textContent !== "?" && /ms/.test(document.getElementById("dg-time").textContent), null, { timeout: 15000 }).catch(() => {});
    rec("digit 1k: the chip answers a drawn digit", /The chip read \d in/.test(await text("#dg-time")), `${await text("#dg-ans")} | ${await text("#dg-time")} | pad class: ${await page.getAttribute(".dg-pad", "class")} | lock: ${await text("#dg-lock-h")} | grid cells: ${await page.evaluate(() => document.getElementById("dg-in").getContext("2d").getImageData(0, 0, 160, 160).data.some((v) => v > 0))}`);
    await page.click("#t-ai");
    const aiTxt = await text("#c-ai-rows");
    rec("digit 1k: Edge AI tab shows this run beside our measured row", /Cycles per answer/.test(aiTxt) && /89,336 at 32 MHz|no row for this board yet/.test(aiTxt), aiTxt);
    await shot("digit-1k-ai");
    await run("digit-cnn_16small", "digit cnn_16small", /read 10 of 10/);
    await run("digit-12k", "digit 12k", /read 10 of 10/);
  }
  if (mode === "all" || mode === "occ") {
    await run("occ-500", "occupancy 500", /all 20 stored windows match/);
    await page.click("#oc-wins button:nth-child(6)");
    await page.waitForFunction(() => /the chip says \d/.test(document.getElementById("oc-time").textContent), null, { timeout: 15000 }).catch(() => {});
    rec("occupancy 500: a window gets an answer with cycles", /the chip says \d, the room had \d/.test(await text("#oc-time")), await text("#oc-time"));
    await page.click("#t-ai"); await shot("occ-500-ai");
    rec("occupancy 500: Edge AI tab shows our measured row", /ms at (80|32) MHz/.test(await text("#c-ai-rows")), await text("#c-ai-rows"));
  }
  if (mode === "all" || mode === "change") {
    const cid = ctx.arg("--change-id", "blink-150-150-150-900");
    if (cid.startsWith("blink")) {
      await run(cid, "blink", /It works/);
      await page.click("#t-change");
      rec("blink: Change shows the settings", (await page.$$("#cp-rows input")).length === 4, await text("#c-params"));
      const vals = [300, 200, 100, 700];
      for (let i = 0; i < 4; i++) await page.fill(`#cp-rows input[data-i="${i}"]`, String(vals[i]));
      await shot("change-before-send");
      await page.click("#cp-send"); await finished(page, 120000);
      rec("blink: Send applies the new pattern and the LED follows it", /It works: the LED repeats 300, 200, 100, 700/.test(await text("#verdict")), `${await text("#verdict")} ${await text("#measured")}`);
      await shot("change-after-send");
      for (const [i, v] of [150, 150, 150, 900].entries()) await page.fill(`#cp-rows input[data-i="${i}"]`, String(v));
      await page.click("#cp-send"); await finished(page, 120000);
      rec("blink: back to the default pattern", /It works: the LED repeats 150, 150, 150, 900/.test(await text("#verdict")), `${await text("#verdict")} ${await text("#measured")}`);
    } else {   // any other image: send its first value changed and its defaults after
      await page.click(`.recipe[data-id="${cid}"]`); await page.click("#t-change");
      const n0 = (await page.$$("#cp-rows input")).length;
      rec(`${cid}: Change shows the settings`, n0 >= 1, await text("#c-params"));
      await page.fill('#cp-rows input[data-i="0"]', ctx.arg("--change-value", "300"));
      await shot("change-before-send");
      await page.click("#cp-send"); await finished(page, 180000);
      rec(`${cid}: Send writes the block and the program restarts`, /Sent: the program restarted/.test(await text("#verdict")), `${await text("#verdict")} ${await text("#measured")}`);
      await shot("change-after-send");
      await page.click("#cp-reset"); await page.click("#cp-send"); await finished(page, 180000);
      rec(`${cid}: defaults written back`, /Sent: the program restarted/.test(await text("#verdict")), await text("#verdict"));
    }
  }
  const links = await page.$$eval("#deeper-links a", (a) => a.map((x) => x.textContent + " " + x.href));
  rec("Go deeper has the repo, the board folder, AGENTS.md, demos and files", links.length >= 5 && links.some((l) => /AGENTS\.md/.test(l)), links.join(" | "));
  await shot("final");
}

// Page height and full-page pictures with a board connected, at two laptop sizes (the layout check, 2026-10-22).
export async function layout(ctx) {
  const { page, rec, url, out } = ctx;
  const label = ctx.arg("--label", "after");
  await page.goto(url);
  await page.waitForFunction(() => document.querySelectorAll("#recipes .recipe").length > 0, null, { timeout: 30000 });
  const use = await page.waitForSelector('#boards button:has-text("Use this one")', { timeout: 8000 }).catch(() => null);
  if (use) await use.click(); else await page.click("#connect");
  await connected(page);
  await page.waitForTimeout(1500);
  await page.click('.recipe[data-id="blink-150-150-150-900"]');   // the same demo on both versions, so the heights compare
  await page.waitForTimeout(800);
  for (const [w, h] of [[1280, 800], [1366, 768]]) {
    await page.setViewportSize({ width: w, height: h }); await page.waitForTimeout(500);
    const height = await page.evaluate(() => document.documentElement.scrollHeight);
    rec(`${label} ${w}x${h}: page height`, true, `${height} px`);
    await page.screenshot({ path: `${out}/${label}-${w}x${h}-full.png`, fullPage: true });
    await page.screenshot({ path: `${out}/${label}-${w}x${h}-top.png` });
  }
}
