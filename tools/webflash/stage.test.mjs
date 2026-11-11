// The stage's pure parts: tabs per demo, Share links, Edge AI rows, the write plan. node --test tools/webflash/stage.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { tabState, changeNote, shareLink, reportUrl, aiRows, planRows, TABS } from "../../site/flash/bench/stage.js";
const recipes = JSON.parse(readFileSync(new URL("../../site/flash/bench/recipes.json", import.meta.url), "utf8")).recipes;

test("Edge AI appears only for the digit reader; Talk only where a serial message is printed", () => {
  for (const k of new Set(recipes.map((r) => r.check.kind))) {
    const t = tabState(k);
    assert.deepEqual(Object.keys(t), TABS);
    assert.equal(t.ai.show, k === "digit" || k === "occ"); assert.equal(t.talk.on, k === "uart");
    if (!t.talk.on) assert.match(t.talk.why, /not for this demo/);
  }
  assert.match(tabState("uart", false).talk.why, /serial/);
});
test("Share links name the board and demo and go to the issue form", () => {
  assert.equal(shareLink("https://x/b/", "mspm0l1306", "digit"), "https://x/b/?board=mspm0l1306&recipe=digit");
  const u = new URL(reportUrl("mspm0l1306", "handwritten-digits", "Draw a digit"));
  assert.equal(u.searchParams.get("template"), "built-it.yml"); assert.equal(u.searchParams.get("board"), "mspm0l1306");
});
test("Edge AI rows say what was measured and never invent a number", () => {
  const none = aiRows({ flashKb: 64, ramKb: 4, bytes: 20712 });
  assert.ok(none.some(([k, v]) => k === "Self-test" && /not run yet/.test(v)) && none.some(([k, v]) => k === "Last answer" && /draw a digit/.test(v)));
  const got = aiRows({ flashKb: 64, ramKb: 4, bytes: 20712, selftest: 10, nTest: 10, clkMhz: 32, cycles: 2600000, pred: 7 });
  assert.ok(got.some(([, v]) => /81\.3 ms at 32 MHz, read 7/.test(v)) && got.some(([, v]) => /10 of 10/.test(v)) && got.some(([, v]) => /32 %/.test(v)));
  assert.match(changeNote("colors"), /No compiler/);
});
test("the plan names the board, what is written and what the recovery record says", () => {
  const p = planRows("LP-MSPM0L1306", "M3010001", { title: "Blink", bytes: 1234 }, [["Saves", "the flash"]]);
  assert.equal(p[0][0], "Board"); assert.match(p[0][1], /M3010001/); assert.match(p[1][1], /1,234 bytes/); assert.deepEqual(p[2], ["Saves", "the flash"]);
});
