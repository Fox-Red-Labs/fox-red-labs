import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import vm from 'node:vm';
import { bannerFor, summaryFor } from '../../site/flash/bench/recovery.js';
import { Mspm0, matchesIdentity, elfSegments } from '../../site/flash/mspm0-dap.js';

const root = new URL('../../', import.meta.url);
const read = p => readFileSync(new URL(p, root), 'utf8');
const boards = JSON.parse(read('site/flash/bench/boards.json')).boards;
const g = boards.find(b => b.id === 'mspm0g3507');
const recipes = JSON.parse(read('site/flash/bench/recipes.json')).recipes;

test('every draft reaches the board catalogue, including G3507', () => {
  for (const f of readdirSync(new URL('site/flash/bench/boards-draft/', root)).filter(f => f.endsWith('.json'))) {
    assert.ok(boards.some(b => b.id === f.slice(0, -5)), f);
  }
  assert.ok(g?.programmable);
  assert.equal(Number(g.flash.end), 128 * 1024);
  assert.equal(g.ram, 32 * 1024);
  assert.equal(Number(g.ramStart), 0x20200000);
  assert.equal(Number(g.cmdbyten), 0x1ff);
  assert.match(g.note, /hardware check/);
});

test('G3507 blink and colour images are present, checksummed and within its flash', () => {
  const mine = recipes.filter(r => r.board === g.id);
  assert.deepEqual(mine.map(r => r.check.kind).sort(), ['colors', 'pattern']);
  for (const r of mine) {
    const image = readFileSync(new URL('site/flash/bench/' + r.image, root));
    assert.equal(createHash('sha256').update(image).digest('hex'), r.sha256);
    const segments = elfSegments(image.buffer.slice(image.byteOffset, image.byteOffset + image.byteLength));
    assert.ok(segments.length);
    for (const s of segments) assert.ok(s.addr >= 0 && s.addr + s.data.length <= Number(g.flash.end));
  }
});

test('G3507 family match accepts silicon revisions but rejects smaller variants and unreadable USERID', () => {
  for (const revision of [0, 1, 3, 15]) assert.ok(matchesIdentity(g.identity, (revision << 28) | 0x0bb8802f, 0x00c7ae2d));
  for (const userid of [0x00d4151f, 0x001dc504, null, 0]) assert.equal(matchesIdentity(g.identity, 0x0bb8802f, userid), false);
  assert.equal(matchesIdentity(g.identity, 0x0bba902f, 0x00c7ae2d), false);
});

test('pre-write identity check independently refuses the smaller G3506 without writing', async () => {
  let writes = 0;
  const dap = { read32: async a => ({ [0x41c40000]: 123, [0x41c40004]: 0x0bb8802f, [0x41c40008]: 0x00d4151f })[a],
    write32: async () => { writes++; } };
  const board = { ...g, identity: { ...g.identity, addr: Number(g.identity.addr) }, flash: { start: 0, end: 0x20000 } };
  await assert.rejects(new Mspm0(dap, board).identify(), e => e.kind === 'wrong-chip');
  assert.equal(writes, 0);
  dap.read32 = async a => ({ [0x41c40000]: 123, [0x41c40004]: 0x1bb8802f, [0x41c40008]: 0x00c7ae2d })[a];
  assert.equal((await new Mspm0(dap, board).identify()).userid, '0x00C7AE2D');
});

const source = read('site/flash/bench/bench.js');
const identifyCode = source.slice(source.indexOf('async function identify('), source.indexOf('async function survey('));
async function discover(userid, deviceid = 0x1bb8802f) {
  class Dap {
    async info() { return 'ANY-SERIAL'; }
    async connect() {}
    async disconnect() {}
    async apRead(ap) { return ap === 0 ? 0x24770011 : 0; }
    async read32(addr) { return addr === 0x41c40004 ? deviceid : addr === 0x41c40008 ? userid : 0; }
  }
  const context = { safeSerial: v => (typeof v === 'string' && /^[A-Za-z0-9]{1,16}$/.test(v) ? v : ''), boards, Dap, matchesIdentity, n: s => parseInt(s, 16), transportFor: () => ({ open: async () => {}, close: async () => {} }) };
  return vm.runInNewContext(identifyCode + '\nidentify({serialNumber: "not-the-lab-probe"}, false)', context);
}
test('browser discovery selects G3507 by chip identity, regardless of probe serial', async () => {
  assert.equal((await discover(0x00c7ae2d)).board?.id, g.id);
  assert.equal((await discover(0x00d4151f)).board, null);
  assert.equal((await discover(null)).board, null);
  assert.equal((await discover(0, 0x0bba902f)).board?.id, 'mspm0g3519');
});

test("new G3507 support never claims a hardware-proven restore", () => {
  assert.equal(bannerFor(g).tone, "caution");
  assert.match(bannerFor(g).text, /not yet been checked/);
  assert.match(summaryFor(g, null).lines[2][1], /not yet been checked/);
});

// The open aperture (2026-10-17): kits TI's files describe, predicted by tools/webflash/sdk_catalog.py, read-only.
const predicted = JSON.parse(read('site/flash/bench/boards-predicted.json'));
test('predicted boards never collide with tested ones and carry what the page needs', () => {
  assert.ok(predicted.boards.length >= 8, 'at least the six MSPM0 kits plus the R2, R53 and P20');
  for (const p of predicted.boards) {
    assert.ok(!boards.some(b => b.id === p.id), `${p.id} is already tested`);
    assert.equal(p.status, 'predicted'); assert.equal(p.programmable, false);
    assert.ok(['id-known', 'id-shared', 'id-pattern'].includes(p.confidence), p.confidence);
    assert.ok(p.basis && p.basis.sdk && p.note.startsWith('Predicted, not tested here'));
    if (p.identity.addr) {
      assert.equal(p.identity.addr, '0x41C40000');
      assert.ok(Number(p.flash.end) > 0 && p.ram > 0 && Number(p.ramStart) >= 0x20000000, p.id);
      assert.ok(['0xFF', '0x1FF'].includes(p.cmdbyten));
      assert.equal(p.check.sramflashKb * 1024, Number(p.flash.end) - Number(p.flash.start));
      assert.ok(p.leds.length >= 1 && p.leds.every(l => /^P[A-C]\d+$/.test(`P${l.port}${l.pin}`)));
      // a predicted ID that a tested board also carries would be shadowed; the generator must not emit one
      assert.ok(!boards.some(b => b.identity.deviceid && matchesIdentity({ ...b.identity, userid: undefined }, Number(p.identity.deviceid))), `${p.id} shares an ID with a tested board`);
    } else {
      assert.equal(p.identity.kind, 'cfgap'); assert.equal(p.identity.partid.offset, '0x4');
      assert.ok(!boards.some(b => (b.identity.deviceids || []).some(d => p.identity.deviceids.includes(d))), `${p.id} shares an ID with a tested board`);
    }
  }
  assert.deepEqual(predicted.reproduced.sort(), ['mspm0c1104', 'mspm0g3507', 'mspm0g3519', 'mspm0g5187', 'mspm0l1306']);
});
test('a CC2340R53 is named as itself, not as the LP-EM-CC2340R5 (routing fix 2026-10-17)', () => {
  const r5 = boards.find(b => b.id === 'cc2340r5'), r53 = predicted.boards.find(b => b.id === 'cc2340r53');
  assert.deepEqual(r5.identity.deviceids, ['0x0BB8402F']);
  assert.deepEqual(r53.identity.deviceids, ['0x0BBAE02F']);
  const all = [...boards, ...predicted.boards], id = 0x0BBAE02F;
  const hit = all.find(b => b.identity.kind === 'cfgap' && b.identity.ap === 1 && b.identity.deviceids.some(d => ((id & Number(b.identity.deviceidMask)) >>> 0) === ((Number(d) & Number(b.identity.deviceidMask)) >>> 0)));
  assert.equal(hit.id, 'cc2340r53');
  assert.ok(r53.siblings.includes('LP-EM-CC2340R53-Q1'));
});
test('the page loads the predicted file after boards.json and keeps tested entries first', () => {
  const src = read('site/flash/bench/bench.js');
  assert.ok(src.indexOf('loadData("boards.json"') < src.indexOf('loadData("boards-predicted.json"'));
  assert.match(src, /boards = \[\.\.\.boards, \.\.\.predicted\]/);
  assert.match(src, /function statusOf/);
  assert.match(src, /apRead\(ap, 0x4\)/);
  assert.match(src, /0x18\)/);
});
