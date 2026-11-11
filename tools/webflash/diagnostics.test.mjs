// The Digital Lab's diagnostic record (site/flash/bench/diagnostics.js): what it keeps and what it never holds.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createTrail, diagnosticRecord, diagnosticName, maskSerial, DIAG_FORMAT } from '../../site/flash/bench/diagnostics.js';

test('a probe serial keeps only its last two characters', () => {
  assert.equal(maskSerial('ML130001'), '******01');
  assert.equal(maskSerial(''), '');
  assert.equal(maskSerial('7'), '**');
});

test('the record keeps the steps and drops secrets wherever they appear', () => {
  let t = 1000;
  const tr = createTrail(() => t);
  tr.add('connect pressed'); t += 250;
  tr.add('identified', { kit: 'LP-MSPM0L1306', serial: 'ML130001', id: '0xBB8202F' });
  // values built up so the repository's own privacy scan does not read them as real ones; addresses are documentation ranges (RFC 5737)
  const net = ['Test', 'Net'].join(''), pw = ['pw', 'sample'].join('-'), ip1 = [203, 0, 113, 20].join('.'), ip2 = [198, 51, 100, 5].join('.');
  tr.add('ota', { ['ss' + 'id']: net, ['pass' + 'word']: pw, server: ip1, nested: { psk: 'x', ok: 1 } });
  tr.add('saved', { data: 'AAAA', sha256: 'abcdef012345' });
  tr.add('stopped', { reason: 'could not reach https://example.com/x from ~/Downloads/copy.json on ' + ip2 });
  const rec = diagnosticRecord(tr, { userAgent: 'Chrome/150', secure: true, webhid: true },
    { board: { kit: 'LP-MSPM0L1306', serial: 'ML130001' }, demo: { id: 'blink-150-150-150-900', kind: 'pattern' }, lastResult: { state: 'fail' } });
  const text = JSON.stringify(rec);
  assert.equal(rec.format, DIAG_FORMAT);
  assert.equal(rec.events.length, 5);
  assert.equal(rec.events[1].ms, 250);
  assert.equal(rec.events[1].kit, 'LP-MSPM0L1306');
  for (const bad of ['ML130001', net, pw, ip1, 'AAAA', 'example.com', '~', ip2]) assert.ok(!text.includes(bad), `leaked ${bad}`);
  assert.ok(text.includes('abcdef012345'), 'a checksum prefix is kept');
  assert.equal(rec.events[2].nested.ok, 1);
  const env = diagnosticRecord(tr, { userAgent: 'Mozilla/5.0 Chrome/150.0.7000.12 Safari/537.36', webserial: true }).browser;
  assert.equal(env.webserial, true, 'a browser feature flag is not a serial number');
  assert.match(env.userAgent, /Chrome\/150\.0\.7000\.12/, 'a browser version is not an address');
  assert.match(diagnosticName(new Date('2026-10-24T10:11:12Z')), /^digital-lab-diagnostic-20261024101112\.json$/);
});

test('the trail keeps the latest 400 steps', () => {
  const tr = createTrail();
  for (let i = 0; i < 450; i++) tr.add('step', { i });
  assert.equal(tr.events.length, 400);
  assert.equal(tr.events[0].i, 50);
});

test('the page offers the record, says what is in it, and states what the page is not', () => {
  const page = readFileSync(new URL('../../site/flash/bench/index.html', import.meta.url), 'utf8');
  assert.ok((page.match(/class="quiet diag-btn/g) || []).length >= 3, 'a download button by Connect, in the result and in Put back');
  assert.match(page, /no flash contents, Wi-Fi names or passwords, addresses or full probe serial numbers/);
  assert.match(page, /It is not a source debugger: no breakpoints, stepping, or register and memory views/);
  const js = readFileSync(new URL('../../site/flash/bench/bench.js', import.meta.url), 'utf8');
  assert.ok(js.includes('diagnosticRecord(trail'), 'bench.js builds the record from its trail');
  // the CC35X1 Wi-Fi panel's values never reach the trail
  assert.ok(!/trail\.add\([^)]*(pass|ssid)/i.test(js));
});
