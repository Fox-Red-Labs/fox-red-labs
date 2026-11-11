import { test } from 'node:test';
import assert from 'node:assert/strict';
import { deviceError, asDeviceError, missingApi } from '../../site/flash/bench/device-errors.js';
const dom = (name, message = '') => Object.assign(new Error(message), { name });
const html = new URL('../../site/flash/bench/index.html', import.meta.url);
import { readFileSync } from 'node:fs';
const page = readFileSync(html, 'utf8');
const rowExists = f => assert.ok(page.includes(`id="env-${f}"`), `Before you connect has no row env-${f}`);
const cases = [
  ['NotFoundError', 'Failed to execute \'requestDevice\' on \'HID\': No device selected.', 'hid', 'chooser', /No board was picked/],
  ['NotFoundError', 'No port selected by the user.', 'serial', 'chooser', /No serial port was picked/],
  ['SecurityError', 'Access denied', 'hid', 'permission', /blocked WebHID/],
  ['NotAllowedError', 'blocked by policy', 'usb', 'permission', /blocked WebUSB/],
  ['NotSupportedError', '', 'serial', 'browser', /does not support Web Serial/],
  ['NetworkError', 'Failed to open the device.', 'hid', 'busy', /Another program/],
  ['NetworkError', 'Failed to open serial port.', 'serial', 'busy', /serial port is busy/],
  ['NetworkError', 'Access denied (insufficient permissions)', 'usb', 'denied', /did not let the browser open/],
  ['InvalidStateError', 'The device is already open', 'hid', 'busy', /Another program/],
  ['NotFoundError', 'The device was lost', 'hid', 'gone', /unplugged/],
  // Chrome's HIDDevice.open() when another program holds the probe (or Linux hidraw permissions): not a policy block
  ['NotAllowedError', 'Failed to open the device.', 'hid', 'busy', /could not open the board/],
];
for (const [name, msg, api, kind, re] of cases) test(`${name} (${msg || 'no text'}) on ${api}`, () => {
  const d = deviceError(dom(name, msg), { api });
  assert.equal(d.kind, kind); assert.match(d.message, re);
  assert.ok(d.next.length > 10 && !d.message.includes('—') && !d.next.includes('—'));
  rowExists(d.fold);
});
test('an empty chooser result (WebHID, WebUSB) is the cancel case', () => {
  const d = deviceError(Object.assign(new Error('no device was chosen'), { kind: 'no-device', next: 'x' }), { api: 'hid' });
  assert.equal(d.kind, 'chooser'); rowExists(d.fold);
});
test('probe firmware too old maps to its own row', () => {
  const d = deviceError(Object.assign(new Error('the debugger did not understand x'), { kind: 'connect', next: 'update' }));
  assert.equal(d.kind, 'firmware'); rowExists(d.fold);
});
test('page errors and unknown errors pass through', () => {
  assert.equal(deviceError(Object.assign(new Error('wrong chip'), { kind: 'wrong-chip', next: 'x' })), null);
  const e = new TypeError('x'); assert.equal(asDeviceError(e, { api: 'hid' }), e);
});
test('missing API and insecure origin', () => {
  assert.equal(missingApi('hid', true).fold, 'browser'); assert.match(missingApi('hid', true).next, /Chrome or Edge/);
  assert.equal(missingApi('hid', false).fold, 'policy');
  rowExists('browser'); rowExists('policy'); rowExists('nolist'); rowExists('busy'); rowExists('linux'); rowExists('firmware');
});

test("an iPhone is told it is the device, not the address", () => {
  const ios = "Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Mobile/15E148 Safari/604.1";
  const m = missingApi("hid", false, ios);
  assert.match(m.message, /iPhone/);
  assert.doesNotMatch(m.message + m.next, /secure address|localhost/);
  const ff = missingApi("hid", true, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:131.0) Gecko/20100101 Firefox/131.0");
  assert.match(ff.next, /Chrome or Edge/);
  const chrome = missingApi("hid", false, "Mozilla/5.0 (Macintosh) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0 Safari/537.36");
  assert.match(chrome.message, /secure address/);
});
