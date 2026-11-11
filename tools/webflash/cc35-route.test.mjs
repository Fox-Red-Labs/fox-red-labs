// The CC35X1 stays route C: recognised, console at 115200,
// the exact Toolbox command, and nothing the page could use to activate, burn fuses, set rollback protection or enter RMA.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const b = JSON.parse(readFileSync(join(ROOT, "site/flash/bench/boards.json"), "utf8")).boards.find((x) => x.id === "cc35x1");

test("CC35X1: recognised, never written by the page, console 115200, Toolbox command given", () => {
  assert.equal(b.programmable, false); assert.equal(b.saci, false);
  assert.equal(b.identity.kind, "cfgap"); assert.equal(b.identity.ap, 0);
  assert.equal(b.console.baud, 115200);
  assert.match(b.toolbox.command, /simplelink-wifi-toolbox programmer -i XDS110 -param1 <probe serial> programming --tool_settings /);
});
test("CC35X1: no activation, fuse, rollback, RMA or erase command anywhere in what the page shows", () => {
  const all = JSON.stringify([b.toolbox, b.console, b.note, b.recovery]);
  for (const word of ["activation", "activate", "fuse", "rollback", "rma", "full_flash_erase", "custom_activation", "get_fuse_data"])
    assert.ok(!all.toLowerCase().includes(word), `the CC35X1 entry mentions ${word}`);
});
