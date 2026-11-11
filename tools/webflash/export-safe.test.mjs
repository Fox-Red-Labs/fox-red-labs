// site/flash/bench/export-safe.js: what leaves the Digital Lab as a file (Codex review 2026-10-25, findings 8, 10, 11).
// Board text in a CSV cannot run as a spreadsheet formula; the console and live logs keep a stated number of entries and
// a save says when it is partial; the copy behind ".bin" and "Put back" is always the board just saved, even when its
// flash matched a copy saved earlier this visit.
import { test } from "node:test";
import assert from "node:assert/strict";
import { csvCell, capPush, partialNote, CON_MAX, LIVE_MAX, rememberCopy, binName } from "../../site/flash/bench/export-safe.js";

test("a cell that starts like a formula is neutralized", () => {
  for (const s of ["=1+1", "+cmd", "-2+3", "@SUM(A1)", "\t=x", "\r=x"]) {
    const c = csvCell(s);
    assert.ok(c.startsWith(`"'`), `${JSON.stringify(s)} -> ${c}`);
  }
  assert.equal(csvCell("hello"), '"hello"');
  assert.equal(csvCell('say "hi"'), '"say ""hi"""');
  assert.equal(csvCell(12.5), '"12.5"');
});

test("a negative number from the board stays a number", () => {
  assert.equal(csvCell(-12.5), '"-12.5"');
  assert.equal(csvCell("-12.5"), '"-12.5"');
});

test("the logs keep the newest entries up to their limit and count what they dropped", () => {
  const log = [];
  for (let i = 0; i < 12; i++) capPush(log, i, 10);
  assert.deepEqual([...log], [2, 3, 4, 5, 6, 7, 8, 9, 10, 11]);
  assert.equal(log.dropped, 2);
  assert.ok(CON_MAX >= 10000 && LIVE_MAX >= 10000);
});

test("a partial save says so, with the limit", () => {
  const log = []; for (let i = 0; i < 12; i++) capPush(log, i, 10);
  assert.match(partialNote(log, 10, "lines"), /oldest 2/);
  assert.match(partialNote(log, 10, "lines"), /last 10/);
  assert.equal(partialNote([], 10, "lines"), "");
});

test("switching boards and back points the last copy at the board just saved", () => {
  const st = { saved: new Map(), last: null };
  const a1 = rememberCopy(st, "mspm0l1306-0xA", { sha256: "aa" }, "a.json", "LP-MSPM0L1306");
  assert.equal(a1.save, true);
  rememberCopy(st, "mspm0g3519-0xB", { sha256: "bb" }, "b.json", "LP-MSPM0G3519");
  assert.equal(st.last.kit, "LP-MSPM0G3519");
  const a2 = rememberCopy(st, "mspm0l1306-0xA", { sha256: "aa" }, "a-again.json", "LP-MSPM0L1306");
  assert.equal(a2.save, false);                              // same content: not downloaded again
  assert.equal(st.last.kit, "LP-MSPM0L1306");                // but the last copy is this board's again
  assert.equal(st.last.file, "a.json");                      // the file already in Downloads
});

test("the .bin is named for the board and the start address", () => {
  assert.equal(binName({ name: "mspm0l1306-0xA-20261025", kit: "LP-MSPM0L1306", start: 0 }), "LP-MSPM0L1306-mspm0l1306-0xA-20261025-at-0x00000000.bin");
});
