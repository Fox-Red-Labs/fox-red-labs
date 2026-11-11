// Draw a digit, without a board: the page's preparation of a drawing, the mailbox protocol against a stand-in for the chip's RAM
// (laid out as tools/webflash/player/digit.c lays it out), and the slow-answer rule. node --test tools/webflash/digit.test.mjs
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";
const read = (p) => readFileSync(new URL("../../" + p, import.meta.url), "utf8");
const src = read("site/flash/bench/bench.js");
const prep = src.slice(src.indexOf("export function dgPrepare"), src.indexOf("function dgShowInput")).replace("export ", "");
const ask = src.slice(src.indexOf("class DgTimeout"), src.indexOf("async function dgLive"));
const N = 72;
const ctx = (extra = {}) => vm.runInNewContext("var DG_N = 72;" + prep + ask + ";({ dgPrepare, dgAsk, DgTimeout })", { Math, Float32Array, Int8Array, performance: { now: () => Date.now() }, ...extra });

function bar(x0, x1, y0, y1) { const g = new Uint8Array(N * N); for (let y = y0; y <= y1; y++) for (let x = x0; x <= x1; x++) g[y * N + x] = 1; return g; }

test("an empty pad is not sent", () => { assert.equal(ctx().dgPrepare(new Uint8Array(N * N)), null); });

test("a drawing becomes 256 int8 values, scaled to 127 and centred, wherever it sits on the pad", () => {
  const { dgPrepare } = ctx();
  const a = dgPrepare(bar(10, 16, 8, 62)), b = dgPrepare(bar(50, 56, 8, 62));   // the same vertical bar, far left and far right
  assert.equal(a.length, 256);
  assert.equal(Math.max(...a.map(Math.abs)), 127);
  assert.deepEqual(Array.from(a), Array.from(b));                                // position on the pad does not matter
  const cols = new Array(16).fill(0); a.forEach((v, i) => { if (v > 0) cols[i % 16]++; });
  const lit = cols.map((c, i) => (c ? i : -1)).filter((i) => i >= 0);
  assert.ok(lit.length && lit[0] >= 5 && lit[lit.length - 1] <= 10, "the stroke sits in the middle columns: " + cols);
});

// the mailbox as digit.c declares it: seven uint32 words, then input[256]
const LAYOUT = { magic: 0, cpuclk_hz: 4, selftest: 8, n_test: 12, seq_in: 16, seq_out: 20, pred: 24, cycles: 28, input: 32 };
function chip(answer, { answers = true } = {}) {
  const mem = new Map(), base = 0x20000000; let writes = [];
  const dap = {
    async read32(a) { return mem.get(a) >>> 0; },
    async write32(a, v) { mem.set(a, v >>> 0); if (a === base + LAYOUT.seq_in && answers) { mem.set(base + LAYOUT.pred, answer); mem.set(base + LAYOUT.cycles, 2600000); mem.set(base + LAYOUT.seq_out, v); } },
    async writeBlock(a, words) { writes.push([a, words.length]); words.forEach((w, i) => mem.set(a + 4 * i, w)); },
  };
  return { s: { dap }, base, mem, writes };
}

test("the protocol writes the input at +32, bumps seq_in at +16 and reads the answer from +24 and +28", async () => {
  const { dgPrepare, dgAsk } = ctx({ ui: { sleep: (ms) => new Promise((r) => setTimeout(r, 1)) } });
  const inp = dgPrepare(bar(10, 16, 8, 62)), c = chip(7);
  c.mem.set(c.base + LAYOUT.seq_in, 41); c.mem.set(c.base + LAYOUT.seq_out, 41);
  const a = await dgAsk(c.s, c.base, inp);
  assert.equal(JSON.stringify(a), JSON.stringify({ pred: 7, cycles: 2600000 }));
  assert.equal(JSON.stringify(c.writes), JSON.stringify([[c.base + 32, 64]]));
  assert.equal(c.mem.get(c.base + LAYOUT.seq_in), 42);
  const w = c.mem.get(c.base + 32);                                              // four int8 per word, first value lowest
  assert.equal(w, ((inp[0] & 0xFF) | ((inp[1] & 0xFF) << 8) | ((inp[2] & 0xFF) << 16) | ((inp[3] & 0xFF) << 24)) >>> 0);
});

test("the offsets match the layout the image's manifest records", () => {
  const m = JSON.parse(read("site/flash/bench/recipes.json")).recipes.find((r) => r.check.kind === "digit");
  const man = JSON.parse(read("site/flash/bench/" + m.manifest));
  assert.deepEqual(man.digit.layout.slice(0, 8).map((_, i) => 4 * i), Object.values(LAYOUT).slice(0, 8));
  assert.equal(man.digit.layout[8], "input[256]");
  assert.match(read("tools/webflash/player/digit.c"), /magic;[\s\S]*cpuclk_hz;[\s\S]*selftest;[\s\S]*n_test;[\s\S]*seq_in;[\s\S]*seq_out;[\s\S]*pred;[\s\S]*cycles;[\s\S]*input\[256\]/);
});

test("a chip that does not answer gives a timeout, which the live loop tolerates", async () => {
  let t = 0;
  const { dgAsk, DgTimeout } = ctx({ performance: { now: () => (t += 1500) }, ui: { sleep: async () => {} } });
  const c = chip(0, { answers: false });
  await assert.rejects(() => dgAsk(c.s, c.base, new Int8Array(256)), DgTimeout);
  assert.match(src.slice(src.indexOf("async function dgLive"), src.indexOf("function dgLift")), /!\(e instanceof DgTimeout\) \|\| \+\+misses >= 3/);
});

test("the pad is locked until the reader runs, and the Start button is on the lock", () => {
  const html = read("site/flash/bench/index.html");
  assert.match(html, /class="dg-lock" id="dg-lock"[\s\S]*id="dg-start"[\s\S]*<canvas id="dg-canvas"/);
  assert.match(src, /pointerdown", \(e\) => \{ if \(!dg\.live\) return;/);
});
