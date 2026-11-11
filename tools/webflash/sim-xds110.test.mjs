// The Digital Lab's engine (site/flash/mspm0-dap.js) against the fake XDS110 (sim-xds110.mjs): the whole connect, save,
// write, read-back, run and put-back path, and what happens when the probe is held, the chip is wrong, an answer is lost or
// the cable comes out part way. Simulated transport only: no real board, USB stack or Chrome is involved.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { WebHidTransport, Session, elfSegments, FlashError, ERR } from '../../site/flash/mspm0-dap.js';
import { makeFakeXds110 } from './sim-xds110.mjs';

const root = new URL('../../', import.meta.url);
const boards = JSON.parse(readFileSync(new URL('site/flash/bench/boards.json', root), 'utf8')).boards;
const recipes = JSON.parse(readFileSync(new URL('site/flash/bench/recipes.json', root), 'utf8')).recipes;
const n = (s) => parseInt(s, 16);
const b = boards.find((x) => x.id === 'mspm0l1306');
const blink = recipes.find((r) => r.id === 'blink-150-150-150-900');
const c = blink.check;
// the same shape bench.js sessionBoard() builds
const board = { kit: b.kit, identity: { ...b.identity, addr: n(b.identity.addr), deviceid: n(b.identity.deviceid), deviceidMask: n(b.identity.deviceidMask) },
  flash: { start: n(b.flash.start), end: n(b.flash.end) }, led: { gpio: n(c.led.gpio), bit: c.led.bit, label: c.led.label, activeHigh: true },
  gpio: { dout: n(b.gpio.dout), doe: n(b.gpio.doe) }, check: { seconds: 2, tolMs: 40 }, ram: b.ram, ramStart: n(b.ramStart), cmdbyten: n(b.cmdbyten) };
const image = () => {
  const buf = readFileSync(new URL('site/flash/bench/' + blink.image, root));
  return { segments: elfSegments(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length)) };
};
const ui = { now: () => performance.now(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)), say: () => {} };
const sim = (o = {}) => makeFakeXds110({ deviceid: n(b.identity.deviceid), idAddr: n(b.identity.addr), flashBytes: board.flash.end,
  led: { gpio: board.led.gpio, dout: board.gpio.dout, doe: board.gpio.doe, bit: board.led.bit, pattern: c.pattern }, ...o });
const transport = (s) => { const t = new WebHidTransport(s.device); t.timeoutMs = 200; return t; };

test('save, write, read back, run, check the LED, put back: byte for byte against the fake board', async () => {
  const x = sim(), s = new Session(transport(x), board, ui);
  const id = await s.start();
  assert.equal(id.deviceid, '0x' + n(b.identity.deviceid).toString(16).toUpperCase().padStart(8, '0'));
  const copy = await s.save();
  assert.equal(copy.bytes, board.flash.end);
  assert.ok(Buffer.from(copy.data, 'base64').equals(Buffer.from(x.original)), 'the saved copy is what was on the board');
  await s.flashImage(image());
  assert.ok(!Buffer.from(x.flash).equals(Buffer.from(x.original)), 'the write changed the flash');
  await s.run();
  const res = await s.check(c.pattern);
  assert.equal(res.ok, true, 'the simulated LED follows the pattern');
  await s.restore(copy);
  assert.ok(Buffer.from(x.flash).equals(Buffer.from(x.original)), 'put back is byte-exact');
  await s.end();
});

test('a chip with another ID is refused before anything is erased', async () => {
  const x = sim({ deviceid: 0x0BB9F02F }), s = new Session(transport(x), board, ui);
  await assert.rejects(s.start(), (e) => e.kind === ERR.WRONG_CHIP && /do not match/.test(e.message));
  assert.equal(x.counts.erase + x.counts.program, 0);
  await s.end().catch(() => {});
});

test('a put-back refuses a copy saved from another chip', async () => {
  const a = sim(), s = new Session(transport(a), board, ui);
  await s.start(); const copy = await s.save(); await s.end();
  const other = sim({ traceid: 0x11112222 }), s2 = new Session(transport(other), board, ui);
  await s2.start();
  await assert.rejects(s2.restore(copy), (e) => e.kind === ERR.WRONG_CHIP && /another board/.test(e.message));
  assert.equal(other.counts.erase, 0);
  await s2.end();
});

test('a probe another program holds fails at open with the browser error the page explains', async () => {
  const x = sim({ fault: { busy: true } }), t = transport(x);
  await assert.rejects(t.open(), (e) => e.name === 'NotAllowedError' && /Failed to open/.test(e.message));
});

test('one lost answer costs one retry, not every command after it', async () => {
  const x = sim(), t = transport(x), s = new Session(t, board, ui);
  await s.start();
  x.setFault({ dropAnswer: x.counts.transfers + 3 });
  const copy = await s.save();   // the read that lost its answer is retried (Mspm0.retry)
  assert.ok(Buffer.from(copy.data, 'base64').equals(Buffer.from(x.original)));
  assert.ok(s.m.retries >= 1);
  await s.end();
});

test('a late answer left waiting is dropped instead of answering the next command', async () => {
  const x = sim(), t = transport(x);
  await t.open();
  t.queue.push(new Uint8Array([0x00, 3, 0x41, 0x42, 0x43]));   // a stale DAP_Info answer
  const r = await t.transfer([0x02, 0x01]);
  assert.equal(r[0], 0x02);
  await t.close();
  assert.equal(t.onReport, null, 'close removes the report listener');
});

test('unplugged part way through a write: a connect error, the board half written, and put back recovers it', async () => {
  const x = sim(), s = new Session(transport(x), board, ui);
  await s.start(); const copy = await s.save();
  x.setFault({ unplugAt: { kind: 'program', after: 1 } });
  await assert.rejects(s.flashImage(image()), (e) => e instanceof FlashError && e.kind === ERR.CONNECT);
  await s.end().catch(() => {});
  assert.ok(x.gone);
  x.replug();
  const s2 = new Session(transport(x), board, ui);
  await s2.start();
  await s2.restore(copy);
  assert.ok(Buffer.from(x.flash).equals(Buffer.from(x.original)), 'put back after the interrupted write is byte-exact');
  await s2.end();
});

test('unplugged while saving: nothing erased or programmed', async () => {
  const x = sim(), s = new Session(transport(x), board, ui);
  await s.start();
  x.setFault({ unplugAt: { kind: 'read', after: x.counts.read + 200 } });
  await assert.rejects(s.save(), (e) => e.kind === ERR.CONNECT);
  assert.equal(x.counts.erase + x.counts.program, 0);
  assert.ok(Buffer.from(x.flash).equals(Buffer.from(x.original)));
  await s.end().catch(() => {});
});
