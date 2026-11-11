// The bench's recovery promises and backup-file checks, without a browser or a board.
//   node --test tools/webflash/recovery.test.mjs
// Reads site/flash/bench/boards.json (tools/webflash/build_page.py) and site/flash/bench/recovery.js. No hardware.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { bannerFor, summaryFor, checkBackup, BACKUP_FORMAT } from "../../site/flash/bench/recovery.js";
import { sha256 } from "../../site/flash/mspm0-dap.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const boards = JSON.parse(readFileSync(join(ROOT, "site/flash/bench/boards.json"), "utf8")).boards;
const byId = Object.fromEntries(boards.map((b) => [b.id, b]));
const l1306 = byId.mspm0l1306, cc27 = byId.cc2755p10, cc23 = byId.cc2340r5, cc35 = byId.cc35x1;

async function copyOf(board, bytes = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8])) {
  return { format: BACKUP_FORMAT, kit: board.kit, traceid: "0x12345678", deviceid: "0x0BB8A02F", start: "0x0",
    bytes: bytes.length, sha256: await sha256(bytes.buffer), saved: "2026-10-15T12:00:00.000Z", data: Buffer.from(bytes).toString("base64") };
}

test("every board says how it recovers, and the kind matches what the page can do", () => {
  for (const b of boards) {
    assert.ok(b.recovery, `${b.id} has no recovery entry`);
    if (b.recovery.kind === "page") assert.equal(b.programmable, true, `${b.id}: page restore on a board the page cannot program`);
    if (b.recovery.kind === "vendor") { assert.ok(b.saci, `${b.id}: vendor route without a SACI write`); assert.ok(b.recovery.steps.length && b.recovery.files.length && b.recovery.tested); }
    if (b.recovery.kind === "none") { assert.equal(b.programmable, false); assert.ok(!b.saci, `${b.id}: writes through SACI but says nothing is written`); }
  }
  assert.equal(cc27.recovery.kind, "vendor"); assert.equal(cc23.recovery.kind, "vendor"); assert.equal(cc35.recovery.kind, "none");
});

test("the banner makes no restore promise before a board is known, and the right one after", () => {
  const none = bannerFor(null);
  assert.equal(none.tone, "neutral");
  assert.doesNotMatch(none.text, /and can put it back/);
  assert.match(bannerFor(l1306).text, /can put it back/);
  for (const b of [cc27, cc23]) {
    const v = bannerFor(b);
    assert.equal(v.tone, "caution");
    assert.match(v.text, /cannot put it back/);
    assert.match(v.text, /UniFlash/);
  }
  assert.match(bannerFor(cc35).lead, /Nothing is written/);
});

test("the summary beside the write button follows the board and the recipe", () => {
  assert.equal(summaryFor(l1306, { check: { kind: "pattern" } }).restoreHere, true);
  assert.match(summaryFor(l1306, { check: { kind: "colors" } }).lines[1][1], /only the one sector/);
  const v = summaryFor(cc27, { check: { kind: "ble" } });
  assert.equal(v.restoreHere, false);
  assert.match(v.lines[0][1], /\.bin/);
  assert.match(v.lines[2][1], /not from this page/);
  assert.match(v.lines[2][1], /Not tested with files this page saved/);
  assert.equal(summaryFor(null, null), null);
});

test("a .bin from a Bluetooth board is refused with the reason", async () => {
  const r = await checkBackup("\x00\x01", "cc2755p10-20261015-main.bin", l1306, sha256);
  assert.equal(r.ok, false); assert.match(r.message, /cannot put it back/);
});

test("unreadable, foreign and incomplete files are refused before anything is written", async () => {
  assert.equal((await checkBackup("not json", "x.json", l1306, sha256)).ok, false);
  assert.match((await checkBackup(JSON.stringify({ format: "other" }), "x.json", l1306, sha256)).message, /not a copy this page saved/);
  const c = await copyOf(l1306); delete c.traceid;
  assert.match((await checkBackup(JSON.stringify(c), "x.json", l1306, sha256)).message, /no traceid/);
});

test("a damaged copy fails its checksum", async () => {
  const c = await copyOf(l1306);
  c.data = Buffer.from([9, 9, 9, 9, 9, 9, 9, 9]).toString("base64");
  const r = await checkBackup(JSON.stringify(c), "x.json", l1306, sha256);
  assert.equal(r.ok, false); assert.match(r.message, /checksum/);
});

test("a copy from another kit is refused; the right one is described and accepted", async () => {
  const other = await copyOf(byId.mspm0g5187);
  const r = await checkBackup(JSON.stringify(other), "x.json", l1306, sha256);
  assert.equal(r.ok, false); assert.match(r.message, /Plug in the board it came from/);
  const good = await checkBackup(JSON.stringify(await copyOf(l1306)), "x.json", l1306, sha256);
  assert.equal(good.ok, true); assert.match(good.message, /same chip/);
  const before = await checkBackup(JSON.stringify(await copyOf(l1306)), "x.json", null, sha256);
  assert.equal(before.ok, true, "a good copy can be checked before a board is connected");
});

test("a page-format copy is refused on a board this page cannot restore", async () => {
  const r = await checkBackup(JSON.stringify(await copyOf(cc27)), "x.json", cc27, sha256);
  assert.equal(r.ok, false); assert.match(r.message, /cannot put a copy back/);
});

test("a vendor route marked proven names a script that exists in the repository", () => {
  for (const b of boards.filter((x) => x.recovery.kind === "vendor" && x.recovery.proven)) {
    const script = b.recovery.steps.join(" ").match(/(cc[0-9a-z-]+\/[\w./-]+\.sh)/);
    assert.ok(script, `${b.id}: no script named`);
    assert.ok(existsSync(join(ROOT, script[1])), `${b.id}: ${script[1]} missing`);
    assert.match(b.recovery.tested, /Passed/);
  }
});


test("no board is read only in the Digital Lab since the MSPM33 starts after a write (2026-10-22, families/mspm33.js)", async () => {
  const { readOnlyReason } = await import("../../site/flash/bench/recovery.js");
  const boards = JSON.parse(readFileSync(new URL("../../site/flash/bench/boards.json", import.meta.url), "utf8")).boards;
  for (const b of boards) assert.equal(readOnlyReason(b), "", b.id);
});

test("put-back tells how long it takes before it starts, and closing the tab mid-write asks first", async () => {
  const { restoreEstimate, guardUnload } = await import("../../site/flash/bench/recovery.js");
  assert.match(restoreEstimate(65536).text, /about \d+ seconds/);
  assert.match(restoreEstimate(1048576).text, /about 45 seconds/); assert.match(restoreEstimate(4 * 1048576).text, /minutes/);
  assert.match(restoreEstimate(1048576).text, /closing it halfway/);
  let handler = null, writing = false;
  guardUnload({ addEventListener: (t, f) => { assert.equal(t, "beforeunload"); handler = f; } }, () => writing);
  const ev = () => ({ prevented: false, preventDefault() { this.prevented = true; } });
  let e = ev(); handler(e); assert.equal(e.prevented, false, "idle: no prompt");
  writing = true; e = ev(); handler(e); assert.equal(e.prevented, true, "writing: the browser asks");
  assert.ok(e.returnValue);
});

test("an unplug part way says what state the board is in and the one step that recovers it", async () => {
  const { interruptedMessage } = await import("../../site/flash/bench/recovery.js");
  const w = interruptedMessage("writing", { saved: true, kit: "LP-MSPM0L1306" });
  assert.match(w.message, /partly written/); assert.match(w.next, /Put it back/); assert.match(w.next, /Downloads/);
  assert.match(interruptedMessage("writing", { saved: false }).next, /run the same demo again/);
  assert.match(interruptedMessage("restoring").next, /Put it back again with the same file/);
  assert.match(interruptedMessage("saving").message, /before anything was written/);
  assert.match(interruptedMessage("connecting").next, /unchanged/);
  assert.match(interruptedMessage("written").next, /read the new program back and it matched/);
  for (const p of ["connecting", "saving", "writing", "written", "restoring"]) {
    const m = interruptedMessage(p);
    assert.ok(!/—/.test(m.message + m.next), "no em dashes");
  }
});
