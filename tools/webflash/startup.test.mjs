import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import vm from 'node:vm';
const html = readFileSync(new URL('../../site/flash/bench/index.html', import.meta.url), 'utf8');
const boot = html.match(/<script>\s*(\(async \(\) => \{[\s\S]*?)<\/script>/)[1];
function page() {
  const nodes = new Map();
  const getElementById = id => {
    if (!nodes.has(id)) nodes.set(id, { disabled: true, textContent: '', classList: { add() {} } });
    return nodes.get(id);
  };
  return { nodes, document: { getElementById } };
}
test('direct-file launch explains the server without importing USB code', async () => {
  const p = page(); let imports = 0;
  await new vm.Script(boot, { importModuleDynamically: () => { imports++; throw Error('must not import'); } })
    .runInNewContext({ document: p.document, location: { protocol: 'file:' } });
  assert.equal(imports, 0);
  assert.equal(p.nodes.get('connect').disabled, true);
  assert.match(p.nodes.get('startup-detail').textContent, /Opened directly from a file/);
  assert.match(p.nodes.get('startup-help').textContent, /localhost:8765/);
});
test('Connect is disabled in the initial HTML before any script runs', () => {
  assert.match(html, /<button id="connect" disabled>/);
});
const source = readFileSync(new URL('../../site/flash/bench/bench.js', import.meta.url), 'utf8');
const load = source.slice(source.indexOf('async function loadData('), source.indexOf('export const ready'));
const run = response => vm.runInNewContext(load + '\nloadData("catalog.json", "boards")', { fetch: async () => response });
test('missing catalog reports its filename and HTTP error', async () => {
  await assert.rejects(run({ ok: false, status: 404 }), /catalog.json: HTTP 404/);
});
test('HTML fallback and malformed catalogs report a useful loading error', async () => {
  await assert.rejects(run({ ok: true, json: async () => { throw Error('HTML'); } }), /expected JSON/);
  await assert.rejects(run({ ok: true, json: async () => ({}) }), /missing boards/);
});
test('valid board data loads', async () => {
  const value = { boards: [{ id: 'mspm0g3507' }] };
  assert.equal(await run({ ok: true, json: async () => value }), value);
});
