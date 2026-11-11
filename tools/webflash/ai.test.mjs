// The AI picker, the shared parameter block, best-first order and Go deeper: pure parts and data. node tools/webflash/ai.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";
import { stateText, flowIds } from "../../site/flash/bench/ota-ui.js";
import { terminalText } from "../../site/flash/bench/take-home.js";
import { paramsClamp, paramsWords, aiCompare, aiCardLine, bestFirst, deeperLinks, changeNote, estimateText, timeLine, usedPins, pinCells } from "../../site/flash/bench/stage.js";
const rd = (p) => JSON.parse(readFileSync(new URL("../../site/flash/bench/" + p, import.meta.url), "utf8"));
const recipes = rd("recipes.json").recipes, best = rd("best.json");

test("best.json names only demos that exist on that board", () => {
  for (const [b, ids] of Object.entries(best)) {
    if (b === "note") continue;
    for (const id of ids) assert.ok(recipes.some((r) => r.board === b && r.id === id), `${b}/${id}`);
  }
});
test("best first keeps the order of the list and flags those demos", () => {
  const rs = recipes.filter((r) => r.board === "mspm0g3519");
  const o = bestFirst("mspm0g3519", rs, best);
  assert.deepEqual(o.slice(0, best.mspm0g3519.length).map((r) => r.id), best.mspm0g3519);
  assert.ok(o.slice(0, best.mspm0g3519.length).every((r) => r.best) && o.slice(best.mspm0g3519.length).every((r) => !r.best));
  assert.equal(o.length, rs.length);
});
test("every image with a parameter block has the same block shape and a default inside its limits", () => {
  const withP = recipes.filter((r) => r.params);
  assert.ok(withP.length >= 20);
  for (const r of withP) {
    const p = r.params;
    assert.equal(p.kind, "params"); assert.equal(p.format, 1); assert.equal(p.magic, "0x50524C46");
    assert.match(p.addr, /^0x[0-9A-F]{8}$/);
    for (const it of p.items) { assert.ok(it.min <= it.default && it.default <= it.max, `${r.board}/${r.id}/${it.id}`); assert.ok(it.id && it.label && it.unit); }
    assert.equal(paramsWords(p, p.items.map((i) => i.default)).length, 4 + p.items.length);
    assert.match(changeNote(r.check.kind, p), /last flash sector/);
  }
  const ids = new Set(); for (const r of withP) ids.add(`${r.recipe}:${r.params.image_id}`);
  const byId = {}; for (const r of withP) (byId[r.params.image_id] ||= new Set()).add(r.recipe === "blink-pattern" ? "blink" : r.recipe === "occupancy" ? "occ" : r.recipe);
  for (const [id, kinds] of Object.entries(byId)) assert.equal(kinds.size, 1, `image id ${id} shared by ${[...kinds]}`);
});
test("a written block clamps, rounds and falls back to the default", () => {
  const items = [{ min: 10, max: 5000, default: 150 }, { min: 0, max: 1000, default: 80 }];
  assert.deepEqual(paramsClamp(items, [5, 99999]), [10, 1000]);
  assert.deepEqual(paramsClamp(items, ["x", 12.6]), [150, 13]);
  const w = paramsWords({ magic: "0x50524C46", format: 1, image_id: 4, items }, [300, 40]);
  assert.deepEqual(w, [0x50524C46, 1, 2, 4, 300, 40]);
});
test("the picker rows hold this run beside our measured row and say when there is none", () => {
  const g = recipes.find((r) => r.board === "mspm0g5187" && r.id === "digit-1k").ai;
  const rows = aiCompare(g, { right: 10, nTest: 10, cycles: 89336, clkMhz: 32 });
  assert.ok(rows.some(([k, a, b]) => k === "Cycles per answer" && /89,336/.test(a) && /89,336 at 32 MHz/.test(b)));
  assert.ok(rows.some(([k, a]) => /10 of 10/.test(a)));
  const l = recipes.find((r) => r.board === "mspm0l1306" && r.id === "digit-12k").ai;
  const lr = aiCompare(l, { cycles: 412861, clkMhz: 32, right: 10, nTest: 10 });
  assert.ok(lr.some(([k, , b]) => k === "Cycles per answer" && /412,861 at 32 MHz/.test(b)), "the L1306 picker run is a row now");
  assert.ok(lr.some(([k, , b]) => k === "Flash" && /no row for this board yet/.test(b)));
  assert.match(aiCardLine(l), /12\.9 ms at 32 MHz, measured/);
  assert.match(aiCardLine({ size: "x", measured: {} }), /not measured on this board yet/);
  const o = recipes.find((r) => r.board === "mspm0g3519" && r.id === "occ-500").ai;
  const orows = aiCompare(o, { right: 17, sameRef: 20, nTest: 20, cycles: 300000, clkMhz: 32 });
  assert.ok(orows.some(([k, a]) => /17 of 20/.test(a) && /20 of 20 match/.test(a)));
  assert.ok(orows.some(([, , b]) => /3\.91 ms at 80 MHz/.test(b)));
});
test("every model in the picker has its facts, its image and the windows file", () => {
  const ai = recipes.filter((r) => r.ai);
  for (const b of ["mspm0l1306", "mspm0g3519", "mspm0g5187"]) {
    const ids = ai.filter((r) => r.board === b).map((r) => r.id);
    for (const id of ["digit", "digit-12k", "digit-1k", "digit-cnn_16small", "occ-500", "occ-2k", "occ-8k"]) assert.ok(ids.includes(id), `${b}/${id}`);
  }
  assert.ok(!ai.some((r) => r.board === "mspm0l1306" && r.id === "occ-30k"), "the 30k model does not fit the L1306's 4 kB");
  for (const r of ai) { assert.ok(r.ai.name && r.ai.size && r.ai.accuracy && r.ai.image.flash_b > 0 && r.ai.image.ram_b > 0, r.id); assert.ok(existsSync(new URL("../../site/flash/bench/" + r.image, import.meta.url))); }
  const w = rd("occ-windows.json"); assert.equal(w.x.length, 20); assert.equal(w.x[0].length, 128); assert.deepEqual(w.label.slice(0, 6), [0, 0, 0, 0, 0, 1]);
  for (const r of ai.filter((x) => x.check.kind === "occ")) assert.deepEqual(r.check.labels, w.label);
});
test("Go deeper is one short block of real links", () => {
  const l = deeperLinks("LP-MSPM0L1306", "mspm0l1306");
  assert.ok(l.length <= 5 && l.every(([k, h]) => k && /^https:\/\/github\.com\/Fox-Red-Labs\/fox-red-labs/.test(h)));
  assert.ok(l.some(([k, h]) => k === "AGENTS.md" && /mspm0l1306\/AGENTS\.md$/.test(h)));
  assert.equal(deeperLinks("x", "").length, 3);
});

test("the time said before a flash or put-back and the timer line while it runs", () => {
  assert.equal(estimateText(0), "a few seconds"); assert.equal(estimateText(5), "a few seconds");
  assert.equal(estimateText(26), "about 25 seconds"); assert.equal(estimateText(44), "about 45 seconds");
  assert.equal(estimateText(226), "about 4 minutes"); assert.equal(estimateText(90), "about 1.5 minutes"); assert.equal(estimateText(60), "about 60 seconds");
  const a = timeLine(40, 10); assert.match(a.text, /^10 s so far of about 40 seconds\. Still/); assert.equal(a.over, false); assert.ok(a.pct > 0 && a.pct < 95);
  const b = timeLine(10, 40); assert.equal(b.over, true); assert.match(b.text, /a little longer than usual/); assert.equal(b.pct, 95);
  assert.notEqual(timeLine(40, 8).text.split(". ").pop(), timeLine(40, 12).text.split(". ").pop(), "the closing line changes so it never looks frozen");
  assert.equal(timeLine(0, 3).pct, 0);
});
test("a build-route demo builds from source in the reader copy: folders first, then the script, then where the image goes", () => {
  const further = { route: "build", command: "python3 tools/webflash/build_digit.py --model cnn_64 mspm0g5187", dirs: ["ports/bitnetmcu", "third-party/bitnetmcu", "tools"],
    image: "prebuilt/mspm0g5187/digit/firmware.out" };
  const lines = terminalText({ os: "mac", title: "Draw a digit", kit: "LP-MSPM0G5187", further, serial: "S", tools: null }).split("\n");
  const add = lines.indexOf("git sparse-checkout add ports/bitnetmcu third-party/bitnetmcu tools"), run = lines.indexOf(further.command);
  assert.ok(add > 0 && run > add, "the sparse clone gets the folders before the script runs");
  assert.match(lines[run + 1], /^# The build writes prebuilt\/mspm0g5187\/digit\/firmware\.out\. Write it with UniFlash or CCStudio/);
  const l1306 = terminalText({ os: "mac", further: { ...further, image: "prebuilt/mspm0l1306/digit/firmware.out", frl: true }, serial: "M301", tools: null });
  assert.match(l1306, /\npython3 tools\/frl\/frl\.py flash --board mspm0l1306 --image prebuilt\/mspm0l1306\/digit\/firmware\.out --probe M301$/);
});

test("the pin panel lists every pin of the package and marks only the pins a demo's record names", () => {
  const po = rd("pinouts.json").boards;
  const boards = rd("boards.json").boards;
  for (const [id, n] of [["mspm0l1306", 32], ["mspm0g5187", 64], ["mspm0g3519", 100]]) {
    assert.equal(po[id].pins.length, n, id); assert.match(po[id].source, /SysConfig .* device data/);
    const r = recipes.find((x) => x.board === id && x.check.kind === "pattern" || x.board === id && x.check.kind === "colors");
    const used = usedPins(r, boards.find((b) => b.id === id));
    assert.ok(used.size >= 1);
    for (const name of used) assert.ok(po[id].pins.some((p) => p.name === name), `${id} names ${name}, which the package table has`);
    assert.equal(pinCells(po[id], used).filter((c) => c.used).length, used.size);
  }
  const uart = recipes.find((x) => x.board === "mspm0l1306" && x.check.kind === "uart");
  assert.deepEqual([...usedPins(uart, boards.find((b) => b.id === "mspm0l1306"))].sort(), ["PA8", "PA9"]);
  assert.equal(usedPins(null, null).size, 0);
});

test("the CC35X1 update panel follows the data's flow in order and names the menu the board is in", () => {
  const d = JSON.parse(readFileSync(new URL("../../site/flash/cc35-ota.json", import.meta.url), "utf8"));
  assert.deepEqual(flowIds(d), ["connect", "scan", "join", "server", "download", "install", "reboot", "keep", "revert"]);
  assert.match(stateText("trial", d.states), /keep it or go back/); assert.equal(stateText("candidate", d.states), d.states.candidate); assert.equal(stateText("", d.states), d.states.silent);
  const b = rd("boards.json").boards.find((x) => x.id === "cc35x1");
  assert.ok(b.toolbox.command && b.console.baud === 115200); assert.doesNotMatch(b.note, /cannot program this one/);
});
test("the radio images' settings ids are the ones params.py documents (6 blink, 7 message, 8 button)", () => {
  const ids = new Set(recipes.filter((r) => r.board.startsWith("cc") && r.params).map((r) => `${r.id}:${r.params.image_id}`));
  for (const x of ["radio-blink:6", "radio-message:7", "radio-button:8"]) assert.ok(ids.has(x), x);
});
