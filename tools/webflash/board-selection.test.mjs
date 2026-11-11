import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import vm from 'node:vm';
const root = new URL('../../', import.meta.url);
const read = p => readFileSync(new URL(p, root), 'utf8');
const catalogue = JSON.parse(read('site/flash/bench/catalog.json')).boards;
const boards = JSON.parse(read('site/flash/bench/boards.json')).boards;
const recipes = JSON.parse(read('site/flash/bench/recipes.json')).recipes;
test('every inventory board has an honest capability entry and real instructions', () => {
  const inventory = read('data/boards.csv').trim().split('\n').slice(1).map(l => l.split(',')[0]);
  assert.deepEqual(catalogue.map(b => b.id).sort(), inventory.sort());
  for (const b of catalogue) {
    const adapter = boards.find(x => x.id === b.id);
    const demos = recipes.filter(x => x.board === b.id);
    assert.equal(b.recognized, !!adapter);
    assert.equal(b.browserFlash, !!(adapter && (adapter.programmable || adapter.saci) && demos.length));
    assert.equal(b.demos, demos.length);
    assert.ok(existsSync(new URL(b.readme, root)), b.readme);
    if (b.page) assert.ok(existsSync(new URL('site/' + b.page, root)), b.page);
  }
});
const source = read('site/flash/bench/bench.js');
const fn = source.slice(source.indexOf('async function connectBoard()'), source.indexOf('// A Session on the connected board'));
async function connect(selected, identities) {
  const seen = [], used = []; let clears = 0;
  class FlashError extends Error { constructor(kind, text) { super(text); this.kind = kind; } }
  const ctx = { chooseDevice: async () => selected ? [selected] : [],
    clearConnection: () => { clears++; }, identify: async d => { seen.push(d); return identities[d]; },
    // This other authorized board must never be selected as a fallback.
    allowed: async () => ['OTHER'], known: new Map(), drawBoards: async () => {},
    useBoard: (...args) => used.push(args), FlashError, trail: { add() {} } };
  let error;
  try { await vm.runInNewContext(fn + '\nconnectBoard()', ctx); } catch (e) { error = e; }
  return { seen, used, clears, error };
}
test('unknown selected probe never falls back to another authorized board', async () => {
  const r = await connect('UNKNOWN', { UNKNOWN: { board: null, what: 'unknown chip' }, OTHER: { board: boards[0] } });
  assert.deepEqual(r.seen, ['UNKNOWN']); assert.deepEqual(r.used, []);
  assert.equal(r.clears, 1); assert.equal(r.error.kind, 'wrong-chip');
});
test('recognized selected probe is the only one used', async () => {
  const g = boards.find(b => b.id === 'mspm0g3507');
  const r = await connect('CHOSEN', { CHOSEN: { board: g } });
  assert.equal(r.error, undefined); assert.deepEqual(r.seen, ['CHOSEN']);
  assert.equal(r.used[0][0], 'CHOSEN'); assert.equal(r.used[0][1].board.id, g.id);
});
test('cancelling the chooser leaves the existing connection untouched', async () => {
  const r = await connect(null, {});
  assert.equal(r.clears, 0); assert.deepEqual(r.seen, []); assert.deepEqual(r.used, []);
});
