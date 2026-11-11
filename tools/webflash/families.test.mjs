// Chip-family modules (site/flash/families/) and the engine's safe connect, cache switch, on-chip CRC, retries and exit path,
// against a fake debug port. The real boards: docs in mspm33c321a/AGENTS.md.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { Mspm0, Session, crc32, estimateSeconds, FlashError, ERR } from '../../site/flash/mspm0-dap.js';
import { familyOf } from '../../site/flash/families/index.js';
import * as pwrap from '../../site/flash/families/pwrap.js';

const root = new URL('../../', import.meta.url);
const boards = JSON.parse(readFileSync(new URL('site/flash/bench/boards.json', root), 'utf8')).boards;
const n = v => (typeof v === 'string' && /^0x/i.test(v) ? parseInt(v, 16) : v);
const deep = o => (o && typeof o === 'object' && !Array.isArray(o) ? Object.fromEntries(Object.entries(o).map(([k, v]) => [k, deep(v)])) : n(o));
const m33 = deep(boards.find(b => b.id === 'mspm33c321a'));
const l13 = deep(boards.find(b => b.id === 'mspm0l1306'));
const DHCSR = 0xE000EDF0, DEMCR = 0xE000EDFC, AIRCR = 0xE000ED0C, CACHE = 0x4002900C;

class FakeDap {
  constructor() { this.mem = new Map(); this.log = []; this.ap = new Map([[0xFC, 0x002E0002]]); this.halted = false; this.failNext = 0; this.select = 0; this.csw = 0; }
  async read32(a) {
    if (a === DHCSR) return this.halted ? 0x00130003 : 0x02100000;
    if (a === m33.fc.base + 0x13D0) return 3;
    if (a === m33.fc.semaphore.status) return m33.fc.semaphore.held;
    return this.mem.get(a) ?? 0;
  }
  async write32(a, v) { this.log.push(['w', a, v >>> 0]); if (a === DHCSR) this.halted = !!(v & 2); this.mem.set(a, v >>> 0); }
  async writeMany(pairs) {
    for (const [a, v] of pairs) { this.log.push(['w', a, v >>> 0]); this.mem.set(a, v >>> 0); }
    if (pairs.some(([a]) => a === m33.fc.base + 0x1100) && this.failNext > 0) { this.failNext--; this.statFail = true; } else this.statFail = false;
  }
  async readBlock(a, words) { return Array.from({ length: words }, (_, i) => this.mem.get(a + 4 * i) ?? 0); }
  async apRead(ap, reg) { return ap === 4 ? (this.ap.get(reg) ?? 0) : 0; }
  async apWrite(ap, reg, v) { this.log.push(['ap', ap, reg, v >>> 0]); this.ap.set(reg, v >>> 0); }
  async connect() {}
  async dpWrite() {}
  async disconnect() {}
}

test('family modules: M33 boards share one, MSPM0 has its own, with the cache register of each', () => {
  assert.equal(familyOf(m33).id, 'mspm33');
  assert.equal(familyOf({ family: 'am13' }).id, 'am13');
  assert.equal(familyOf({ id: 'am13e230', fc: { word: 16 } }).ramStart, 0x20000000);
  assert.equal(familyOf(m33).ramStart, 0x30000000);
  assert.equal(familyOf(l13).id, 'mspm0');
  assert.equal(familyOf(m33).cacheReg, CACHE);
  assert.equal(familyOf(l13).cacheReg, 0x40401300);
});

test('crc32 is the zlib CRC-32', () => {
  assert.equal(crc32(new TextEncoder().encode('123456789')), 0xCBF43926);
  assert.equal(crc32(new Uint8Array(0)), 0);
});

test('wait-for-debug writes the register sequence CCStudio writes, catches the core, ends halted', async () => {
  const d = new FakeDap();
  d.read32 = async a => (a === DHCSR ? 0x00130003 : 0);
  assert.equal(await pwrap.hasPwrap(d), true);
  assert.equal(await pwrap.waitForDebug(d), true);
  const dprec = d.log.filter(x => x[0] === 'ap' && x[2] === 0).map(x => x[3]);
  const tail = dprec.slice(-7);
  assert.deepEqual(tail, [0x002C4027, 0x000C6027, 0x000C602F, 0x000C202F, 0x0048202F, 0x0008602F, 0x00120008]);
  assert.ok(dprec.slice(0, 10).every(v => (v & 0x4000) !== 0), 'hold bit set in the reset loop');
  assert.equal(d.log.filter(x => x[0] === 'ap' && x[2] === 0xF0).length, 20);   // SPREC reset requests
  assert.ok(d.log.some(x => x[0] === 'w' && x[1] === DEMCR && (x[2] & 1)), 'vector catch on reset');
});

test('a chip without the power wrapper is left alone', async () => {
  const d = new FakeDap(); d.ap.set(0xFC, 0);
  assert.equal(await pwrap.hasPwrap(d), false);
  const m = new Mspm0(d, l13);
  assert.equal(m.fam.safeConnect, undefined, 'MSPM0 keeps the plain halt and the nRESET wake-up');
  assert.equal(d.log.length, 0);
});

test('caches go off before the first write and come back before the start reset', async () => {
  const d = new FakeDap(); d.mem.set(CACHE, 6);
  d.read32 = (orig => async a => (a === DHCSR ? (d.halted ? 0x00130003 : 0x02100000) : orig(a)))(d.read32.bind(d));
  const m = new Mspm0(d, m33);
  await m.halt();
  assert.equal(d.mem.get(CACHE), 0, 'caches off while the flash is written');
  await m.resetRun();
  const writes = d.log.filter(x => x[0] === 'w');
  const back = writes.findIndex(x => x[1] === CACHE && x[2] === 6), reset = writes.findIndex(x => x[1] === AIRCR);
  assert.ok(back >= 0 && reset > back, 'cache value put back, then the Arm system reset request');
});

test('a write is checked with the chip CRC, and a mismatch names the first wrong word by reading back', async () => {
  const d = new FakeDap(), m = new Mspm0(d, m33);
  const data = Uint8Array.from({ length: 512 }, (_, i) => i & 0xFF);
  let asked = null;
  m.runStub = async (code, regs) => { asked = regs; return crc32(data); };
  d.readBlock = async () => { throw new Error('no read-back when the CRC matches'); };
  await m.verify([{ addr: 0x10000000, data }]);
  assert.deepEqual(asked, [0, 512]);                        // alias moved to the controller's address; bytes checked
  m.runStub = async () => 0x12345678;                        // wrong checksum
  d.readBlock = async (a, w) => Array.from({ length: w }, (_, i) => (i === 3 ? 0xBAD : data[4 * i] | data[4 * i + 1] << 8 | data[4 * i + 2] << 16 | data[4 * i + 3] << 24) >>> 0);
  await assert.rejects(m.verify([{ addr: 0x10000000, data }]), e => e.kind === ERR.VERIFY && /0x0000000C/.test(e.message));
  let crc = 0; m.runStub = async () => { crc++; return 0; };
  await assert.rejects(m.verify([{ addr: 0, data }], { full: true }), e => e.kind === ERR.VERIFY);
  assert.equal(crc, 0, 'the proof after a put-back reads everything back');
});

test('a failed flash block is tried three times with a 150 ms pause, then reported', async () => {
  const d = new FakeDap(), m = new Mspm0(d, m33);
  let calls = 0;
  m.command1 = async () => { calls++; if (calls < 3) throw new FlashError(ERR.FLASH, 'not passed', ''); };
  const t0 = Date.now();
  await m.command(0x42, 0x2000);
  assert.equal(calls, 3);
  assert.ok(Date.now() - t0 >= 290, 'two pauses of 150 ms');
  assert.equal(m.retries, 2);
  calls = 0; m.command1 = async () => { calls++; throw new FlashError(ERR.FLASH, 'not passed', ''); };
  await assert.rejects(m.command(0x42, 0x2000), e => e.kind === ERR.FLASH);
  assert.equal(calls, 3);
  calls = 0; m.command1 = async () => { calls++; throw new FlashError(ERR.IMAGE, 'wrong image', ''); };
  await assert.rejects(m.command(0x42, 0x2000), e => e.kind === ERR.IMAGE);
  assert.equal(calls, 1, 'an image error is not retried');
});

test('every exit path resets the core and disconnects, errors included', async () => {
  const d = new FakeDap(); d.read32 = (orig => async a => (a === DHCSR ? (d.halted ? 0x00130003 : 0x02100000) : orig(a)))(d.read32.bind(d));
  let closed = 0, disc = 0;
  const s = new Session({ open: async () => {}, close: async () => { closed++; } }, m33, { say() {}, now: () => 0, sleep: async () => {} });
  s.dap = d; s.m = new Mspm0(d, m33); d.disconnect = async () => { disc++; };
  await s.m.halt();
  assert.equal(s.m.halted, true);
  await s.end();                                             // as in the finally of run(): no start was reached
  assert.ok(d.log.some(x => x[0] === 'w' && x[1] === AIRCR), 'core reset');
  assert.equal(d.halted, false);
  assert.equal(disc, 1); assert.equal(closed, 1);
});

test('timing per family for the UI estimate: a 1 MB M33 put-back is minutes, an L1306 one is seconds', () => {
  const t33 = estimateSeconds(m33, { bytes: 540000, total: 1048576 });
  assert.ok(t33 > 60 && t33 < 200, String(t33));
  assert.ok(estimateSeconds(l13, { bytes: 65536 }) < 10);
  assert.ok(estimateSeconds(m33, { bytes: 2048 }) < 2);
  assert.ok(familyOf(m33).timing.readBps > 0 && familyOf(l13).timing.writeBps > 0);
});

test('the M33 loader writes whole buffers: a put-back programs through the RAM loader, not word by word', async () => {
  const d = new FakeDap(), m = new Mspm0(d, m33);
  assert.equal(m.fast, true);
  let runs = 0, words = 0;
  m.runRam = async (code, params) => { runs++; return [...params.slice(0, 5), 0]; };
  d.writeBlock = async (a, w) => { words += w.length; };
  const data = new Uint8Array(40000).fill(0xAA);
  await m.programFast(0x10000, data);
  assert.equal(runs, 3, '16 kB per run');
  assert.equal(words, 10000);
  await m.programFast(0x40000, new Uint8Array(4096).fill(0xFF));
  assert.equal(runs, 3, 'erased data is not written');
});

test('a chip asleep under the out-of-box image is woken by the nRESET pulse and halted on the same call', async () => {
  const d = new FakeDap(), m = new Mspm0(d, l13);
  let asleep = true, pulses = 0;
  d.pulseReset = async () => { pulses++; asleep = false; };
  const rd = d.read32.bind(d);
  d.read32 = async a => { if (asleep) throw new FlashError(ERR.CONNECT, 'the chip did not answer the debugger (ack 4)', ''); return a === DHCSR ? (d.halted ? 0x00130003 : 0x02100000) : rd(a); };
  await m.halt();
  assert.equal(pulses, 1);
  assert.equal(m.halted, true);
});
