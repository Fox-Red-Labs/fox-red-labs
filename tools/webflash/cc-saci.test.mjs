// cc-saci.js without a board: the radio demo images pass the page's own refusals, a saved backup turns back into the image
// that was written, a failed block is retried 3 times, and close() never throws.
import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync, existsSync } from "fs";
import { dirname, join } from "path";
import { fileURLToPath } from "url";
import { Saci, paramSector, imageWithParams, parseHex, vectorCheck, checkConfig, imageFromBackup, firstDiff, FAMILY } from "../../site/flash/cc-saci.js";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FAMS = { cc2755p10: FAMILY.cc27, cc2340r5: FAMILY.cc23 };

test("every radio demo image passes vectorCheck and checkConfig and carries its parameter block", () => {
  for (const [board, fam] of Object.entries(FAMS)) for (const d of ["radio-blink", "radio-message", "radio-button"]) {
    const dir = join(ROOT, "prebuilt", board, d), hex = join(dir, d + ".hex");
    if (!existsSync(hex)) continue;           // prebuilt images are built by cc2755p10/radio-demos/build.sh
    const img = parseHex(readFileSync(hex, "utf8")), man = JSON.parse(readFileSync(join(dir, "manifest.json")));
    assert.deepEqual([...vectorCheck(img), ...checkConfig(img.ccfg, img.scfg, fam)], [], `${board} ${d}`);
    const a = parseInt(man.params.addr, 16);
    assert.ok(a % 2048 === 0 && a >= img.main.length && a + 2048 <= fam.mainEnd, `${board} ${d}: the settings sector ${man.params.addr} is outside the image and inside main flash`);
  }
});

test("a saved backup becomes the image that was written", () => {
  const hex = join(ROOT, "prebuilt", "cc2755p10", "radio-blink", "radio-blink.hex");
  if (!existsSync(hex)) return;
  const img = parseHex(readFileSync(hex, "utf8"));
  const full = new Uint8Array(FAMILY.cc27.mainEnd).fill(0xff); full.set(img.main);
  const back = imageFromBackup({ main: full, ccfg: img.ccfg, scfg: img.scfg }, FAMILY.cc27);
  assert.equal(firstDiff(back.main, img.main), -1);
  assert.throws(() => imageFromBackup({ main: new Uint8Array(FAMILY.cc27.mainEnd).fill(0xff), ccfg: img.ccfg, scfg: img.scfg }), /blank/);
  assert.throws(() => imageFromBackup({ main: new Uint8Array(100), ccfg: img.ccfg }), /size/);
});

test("a failed block is tried 3 times, then the run stops; close() never throws", async () => {
  const hex = join(ROOT, "prebuilt", "cc2755p10", "radio-blink", "radio-blink.hex");
  if (!existsSync(hex)) return;
  const img = parseHex(readFileSync(hex, "utf8"));
  const dap = { disconnect: async () => { throw new Error("gone"); }, apWrite: async () => { throw new Error("gone"); }, apRead: async () => 0 };
  const sc = new Saci(dap, () => {});
  let tries = 0;
  sc.must = async () => {};                               // erase and the later checks are not under test
  sc.cmd = async (w) => { if ((w[0] & 0xff) === 0x0E) { tries++; return 0x84; } return 0; };
  await assert.rejects(sc.program(img), /3 tries/);
  assert.equal(tries, 3);
  tries = 0; let fails = 2;
  sc.cmd = async (w) => { if ((w[0] & 0xff) === 0x0E) { tries++; if (fails-- > 0) return 0x84; } return 0; };
  await sc.program(img);                                  // succeeds on the third try of the first block
  assert.ok(tries >= 3);
  await sc.close();                                       // the port is gone: must not throw
});

test("the settings sector is the shared block, filled into the image, and out-of-range values are refused", () => {
  const sec = paramSector(6, [100, 200]);
  const w = (i) => (sec[i * 4] | (sec[i * 4 + 1] << 8) | (sec[i * 4 + 2] << 16) | (sec[i * 4 + 3] << 24)) >>> 0;
  assert.deepEqual([w(0), w(1), w(2), w(3), w(4), w(5), w(6)], [0x50524C46, 1, 2, 6, 100, 200, 0xFFFFFFFF]);
  assert.equal(sec.length, 2048);
  const block = { addr: "0x000DF800", image_id: 6, items: [{ id: "on_ms", min: 10, max: 60000, default: 500 }, { id: "off_ms", min: 10, max: 60000, default: 500 }] };
  const hex = join(ROOT, "prebuilt", "cc2755p10", "radio-blink", "radio-blink.hex");
  if (!existsSync(hex)) return;
  const img = parseHex(readFileSync(hex, "utf8"));
  const out = imageWithParams(img, block, { on_ms: 100 });
  assert.equal(out.main.length, 0xE0000);
  assert.equal(firstDiff(out.main.subarray(0, img.main.length), img.main), -1);
  assert.equal(out.main[0xDF800 + 16], 100); assert.equal(out.main[0xDF800 + 20], 500 & 0xff);
  assert.throws(() => imageWithParams(img, block, { on_ms: 5 }), /from 10 to 60000/);
  assert.throws(() => imageWithParams(img, { ...block, addr: "0x0" }, {}), /overlaps/);
});
