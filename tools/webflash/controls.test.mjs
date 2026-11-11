import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { packTable, decodeStatus, cLines, expectedToggle, verdict, rowsToCfg, rowC, pwmSetup, pwmMatches, readAnswers, pwmRegs, pwmJudge } from '../../site/flash/bench/controls.js';
const root = new URL('../../', import.meta.url);
const man = JSON.parse(readFileSync(new URL('prebuilt/mspm0l1306/controls/manifest.json', root), 'utf8'));
const T = man.table, pins = man.pins;

test('the table packs the words the firmware reads, clamped to its limits', () => {
  const w = packTable(T, { seq: 7, pin: 26, mode: 'toggle', on: 200, off: 300, bus: 'none' });
  assert.deepEqual(w.slice(0, 9), [0x4C544346, 7, 26, 3, 200, 300, 0, T.no_reg, 0]);
  assert.deepEqual(packTable(T, { seq: 1, pin: null, mode: 'read', on: 1, off: 99999, bus: 'scan' }).slice(2, 9), [T.no_pin, 0, T.min_ms, T.max_ms, T.i2c_scan, T.no_reg, 0]);
  const r = packTable(T, { seq: 2, pin: 22, mode: 'read', on: 100, off: 100, bus: 'read', addr: 0x48, reg: 0, n: 2 });
  assert.deepEqual(r.slice(2, 9), [22, 0, 100, 100, 0x48, 0, 2]);
  assert.equal(packTable(T, { seq: 2, pin: 0, mode: 'read', on: 100, off: 100, bus: 'read', addr: 0x48, reg: '', n: 20 })[8], T.max_n);
  assert.equal(man.table.words.length, w.length); assert.equal(w.length, 21);
});
test('the status block decodes levels, toggles, bytes and the scan map', () => {
  const st = decodeStatus([0x4C545354, 7, 1, 42, 1, 0x04030201, 0x08070605, 0, 1 << 16, 0, 0, 32000000]);
  assert.ok(st.ok); assert.equal(st.seq, 7); assert.equal(st.level, 1); assert.equal(st.toggles, 42);
  assert.equal(st.i2c, 'acknowledged'); assert.deepEqual(st.bytes, [1, 2, 3, 4, 5, 6, 7, 8]); assert.deepEqual(st.scan, [48]);
  assert.equal(man.status.words.length, 21);
  assert.ok(!decodeStatus(new Array(12).fill(0)).ok);
});
test('the C lines name the pin, the pin-control index and the bus call', () => {
  const c = cLines({ pin: 26, mode: 'toggle', on: 200, off: 300, bus: 'scan' }, pins);
  assert.match(c, /IOMUX_PINCM27/); assert.match(c, /DL_GPIO_PIN_26/); assert.match(c, /200u \* CYCLES_PER_MS/); assert.match(c, /addr <= 0x77/);
  assert.match(cLines({ pin: 22, mode: 'read', bus: 'read', addr: 0x48, reg: 1, n: 2 }, pins), /DL_I2C_startControllerTransfer\(I2C0, 0x48, DL_I2C_CONTROLLER_DIRECTION_RX, 2\)/);
  assert.deepEqual(expectedToggle({ on: 200, off: 300 }), [[1, 200], [0, 300]]);
  assert.match(verdict({ pin: 26, mode: 'toggle', bus: 'scan' }, { toggles: 10, scan: [] }, pins), /PA26 \(LED2 red\) toggled 10 times so far; nothing answered/);
});
test('every pin the panel offers has a header position from TI\'s figure and a role', () => {
  for (const p of pins) { assert.match(p.header, /^J[1-4] pin \d+$/); assert.ok(p.role); assert.ok(p.pin >= 0 && p.pin <= 27); }
});

test('rows become one settings object, and the program\'s limits are named', () => {
  const ok = rowsToCfg([{ kind: 'pwm', pin: 26, hz: 2000, duty: 30 }, { kind: 'scan' }, { kind: 'read', addr: 0x44, reg: 0x7E, n: 2 }]);
  assert.equal(ok.problems.length, 0); assert.equal(ok.cfg.mode, 'pwm'); assert.equal(ok.cfg.pwmHz, 2000); assert.ok(ok.cfg.scan); assert.equal(ok.cfg.reads.length, 1);
  assert.match(rowsToCfg([{ kind: 'pin', pin: 1, mode: 'high' }, { kind: 'toggle', pin: 2, on: 5, off: 5 }]).problems[0], /one pin at a time/);
  const many = Array.from({ length: 5 }, () => ({ kind: 'read', addr: 0x44, reg: 0, n: 1 }));
  assert.match(rowsToCfg(many).problems[0], /4 register reads/);
});
test('PWM and extra reads pack into the added words', () => {
  const rows = [{ kind: 'pwm', pin: 26, hz: 99999, duty: 30 }, { kind: 'scan' }, { kind: 'read', addr: 0x44, reg: 0x7E, n: 2 }, { kind: 'read', addr: 0x40, reg: 0xFC, n: 2 }, { kind: 'read', addr: 0x48, reg: '', n: 9 }];
  const w = packTable(T, { ...rowsToCfg(rows).cfg, seq: 3 });
  assert.deepEqual(w.slice(2, 4), [26, 4]);
  assert.deepEqual(w.slice(6, 9), [0x44, 0x7E, 2]);          // slot 0
  assert.deepEqual(w.slice(9, 12), [20000, 30, 1]);          // hz clamped, duty, scan as well as the read
  assert.deepEqual(w.slice(12, 18), [0x40, 0xFC, 2, 0x48, T.no_reg, 4]);   // slots 1 and 2; n clamped to 4
  assert.deepEqual(w.slice(18, 21), [0, T.no_reg, 0]);
  assert.deepEqual(packTable(T, { seq: 1, pin: 26, mode: 'pwm', pwmHz: 5, pwmDuty: 140, on: 5, off: 5 }).slice(9, 12), [100, 100, 0]);
  assert.equal(packTable(T, { seq: 1, pin: null, mode: 'read', on: 5, off: 5, scan: true, reads: [] })[6], T.i2c_scan);
});
test('the PWM arithmetic matches the firmware and every offered pin has a timer', () => {
  assert.deepEqual(pwmSetup(1000, 25), { hz: 1000, duty: 25, div: 1, period: 32000, cc: 24000, steady: false, actualHz: 1000 });
  const lo = pwmSetup(100, 50); assert.equal(lo.div, 5); assert.equal(lo.period, 64000); assert.ok(lo.period <= 65535);
  assert.equal(pwmSetup(20000, 50).period, 1600);
  assert.ok(pwmSetup(1000, 0).steady && pwmSetup(1000, 100).steady);
  for (const p of pins.filter((x) => x.pwm)) assert.ok(man.table.pwm_pins.includes(p.pin));
  assert.equal(man.table.pwm_pins.length, 6);
  const st = decodeStatus([0x4C545354, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32000000, 32000, 24000, 1, 1, 0x4954, 0, 0, 0, 0]);
  assert.ok(pwmMatches({ pwmHz: 1000, pwmDuty: 25 }, st)); assert.ok(!pwmMatches({ pwmHz: 1000, pwmDuty: 30 }, st));
  assert.equal(Math.round(st.pwm.duty), 25); assert.equal(Math.round(st.pwm.hz), 1000);
  assert.equal(st.extra[0].result, 'acknowledged'); assert.deepEqual(st.extra[0].bytes, [0x54, 0x49, 0, 0]);
});
test('each row shows its own C line and the verdict names every part', () => {
  assert.match(rowC({ kind: 'pwm', pin: 26, hz: 1000, duty: 25 }, pins), /TIMG1 CC0.*\n.*32 MHz \/ 1.*\n.*period = 32000.*\n.*24000, DL_TIMER_CC_0_INDEX.*25% high/);
  assert.match(rowC({ kind: 'read', addr: 0x44, reg: 0x7E, n: 2 }, pins), /reg = 0x7E.*\n.*0x44, DL_I2C_CONTROLLER_DIRECTION_TX/);
  assert.match(rowC({ kind: 'pwm', pin: 26, hz: 1000, duty: 100 }, pins), /steady level/);
  const { cfg } = rowsToCfg([{ kind: 'scan' }, { kind: 'read', addr: 0x44, reg: 0x7E, n: 2 }, { kind: 'read', addr: 0x40, reg: 0xFC, n: 2 }]);
  const st = decodeStatus([0x4C545354, 1, 0, 0, 1, 0x4954, 0, 1 << 8, 0, 0, 0, 32000000, 0, 0, 0, 1, 0x5449, 0, 0, 0, 0]);
  assert.deepEqual(readAnswers(cfg, st).map((a) => a.bytes), [[0x54, 0x49], [0x49, 0x54]]);
  assert.match(verdict(cfg, st, pins), /answered at 0x08.*0x44 answered: 54 49; 0x40 answered: 49 54/);
});

test('the timer is judged through its registers and counter samples', () => {
  const cfg = { pwmHz: 1000, pwmDuty: 25 };
  assert.equal(pwmRegs(26).ctr, 0x40087800); assert.equal(pwmRegs(26).cc, 0x40087810); assert.equal(pwmRegs(27).cc, 0x40087814); assert.equal(pwmRegs(13).base, 0x40084000); assert.equal(pwmRegs(23), null);
  const ramp = Array.from({ length: 400 }, (_, i) => (i * 7919) % 32000);   // a counter that is everywhere in 0..31999
  const good = pwmJudge(cfg, { load: 31999, cc: 24000, ctrctl: 3 }, ramp);
  assert.ok(good.ok, JSON.stringify(good)); assert.ok(Math.abs(good.share - 25) <= 8);
  assert.ok(!pwmJudge(cfg, { load: 31999, cc: 24001, ctrctl: 3 }, ramp).ok);                 // a compare value that is not the one asked
  assert.ok(!pwmJudge(cfg, { load: 31999, cc: 24000, ctrctl: 2 }, ramp).ok);                 // timer not enabled
  assert.ok(!pwmJudge(cfg, { load: 31999, cc: 24000, ctrctl: 3 }, new Array(400).fill(5)).ok);   // a counter that does not move
  assert.ok(!pwmJudge({ pwmHz: 1000, pwmDuty: 60 }, { load: 31999, cc: 12800, ctrctl: 3 }, ramp.map((x) => x % 8000 + 24001)).ok);   // a counter that never leaves the top quarter
});
