// site/flash/cc35-ota.js against a scripted copy of TI's ota_example console (texts taken from the board's own log on
// 2026-10-22, SDK 10.20.00.39): the parsers, the menu detection, the whole update flow, the masked password, and a refusal list.
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { OtaDriver, OtaError, parseScan, parseComponents, parseUpdates, menuOf } from "../../site/flash/cc35-ota.js";

const SCAN = `
 #   SSID                              RSSI   Ch   Security
--------------------------------------------------------------
 1   [fridge]_E30AJU5103012E           -87    1    WPA2
 2                                     -84    7    WPA2
 3   Home Net                          -75    7    WPA2
 4   Home Guest                        -44    10   WPA2

Enter network number (1-4), or 0 to re-scan: `;
const COMPONENTS = `
Components in flash:
 #   Name                      State      Primary  Active   Version
------------------------------------------------------------------------
 0   BL2_Slot_1                UPDATED    Yes      Yes      0.1.18.0
 1   BL2_Slot_2                FAILED     No       No       0.0.0.0
 4   Vendor_Image_Slot_1       UPDATED    Yes      Yes      0.0.1.0
 5   Vendor_Image_Slot_2       CANDIDATE  No       No       0.0.2.0
`;
const UPDATES = `
Available updates from server:
 #   Component            Active (slot)    Server       Status
--------------------------------------------------------------
 [0] Vendor_Image         0.0.1.0 (S1)     0.0.2.0      [NEW]
`;
const MAIN = "\r\n[OTA] Select action:\r\n [1] Connect to WiFi\r\n [2] Show components in flash\r\n [3] Clean component\r\n\r\nChoice: ";
const DOWNLOAD = "\r\n[OTA] Download menu:\r\n [1] Re-query server\r\n [2] Download update\r\n [3] Install (stage candidates)\r\n [4] Show components in flash\r\n [5] Connect to server\r\n [6] Disconnect WiFi\r\n [7] Quit\r\n\r\nChoice: ";
const STAGED = "\r\n[OTA] Updates staged. Select action:\r\n [1] Request reboot\r\n [2] Reject (undo install)\r\n [3] Show components in flash\r\n\r\nChoice: ";
const TRIAL = "\r\n[OTA] TRIAL firmware detected:\r\n  Component 5 (Vendor_Image_Slot_2): 0.0.2.0 [TRIAL]\r\n\r\n[OTA] Trial menu:\r\n [1] Run self-test (WiFi connectivity)\r\n [2] Accept (commit and reboot)\r\n [3] Reject (rollback and reboot)\r\n [4] Show components in flash\r\n [5] Continue to main menu\r\n\r\nChoice: ";
const banner = (v) => `\r\n==============================================\r\n   CC35xx OTA Example Application FRL ${v}\r\n==============================================\r\n\r\n[OTA] PSA FWU initialized\r\n`;
const VLAN = "ERROR rxData_DataPacketDisptcher(): Received VLAN packet - drop it\r\n";

test("parsers read the example's tables", () => {
  const nets = parseScan(SCAN);
  assert.deepEqual(nets.map((n) => [n.n, n.ssid, n.rssi, n.ch, n.security]), [[1, "[fridge]_E30AJU5103012E", -87, 1, "WPA2"], [2, "", -84, 7, "WPA2"], [3, "Home Net", -75, 7, "WPA2"], [4, "Home Guest", -44, 10, "WPA2"]]);
  const c = parseComponents(COMPONENTS);
  assert.equal(c.length, 4); assert.deepEqual(c[3], { n: 5, name: "Vendor_Image_Slot_2", state: "CANDIDATE", primary: false, active: false, version: "0.0.2.0" });
  assert.deepEqual(parseUpdates(UPDATES), [{ index: 0, component: "Vendor_Image", active: "0.0.1.0", slot: 1, server: "0.0.2.0", status: "NEW" }]);
});

test("menuOf finds the last prompt even when status lines follow it", () => {
  assert.equal(menuOf(MAIN), "main");
  assert.equal(menuOf(DOWNLOAD + "STATUS:: IPv4: UP (ip 1.2.3.4)\r\n[OTA] IP acquired\r\n"), "download");
  assert.equal(menuOf(MAIN + "x" + DOWNLOAD), "download");
  assert.equal(menuOf(STAGED), "staged");
  assert.equal(menuOf(TRIAL), "trial");
  assert.equal(menuOf("[OTA] Network is WPA/WPA2 - password required\r\n\r\nEnter WiFi password: "), "password");
  assert.equal(menuOf("Enter server base URL (e.g. https://192.0.2.100:8443): "), "url");
  assert.equal(menuOf("booting"), "");
});

/** A scripted board: answers keystrokes the way ota_example does. */
class Board {
  constructor(opts = {}) { this.s = "main"; this.line = ""; this.sent = []; this.log = []; this.opts = opts; this.active = "0.0.1.0"; this.slot2 = "FAILED"; }
  attach(d) { this.d = d; }
  emit(t, delay = 0) { setTimeout(() => this.d.feed(t), delay); }
  async write(str) {
    for (const ch of str) {
      if (ch !== "\r") { this.line += ch; if (this.s === "password") this.emit(ch); continue; }
      const l = this.line; this.line = ""; this.sent.push([this.s, l]); this.step(l);
    }
  }
  step(l) {
    const e = (t) => this.emit("\r\n" + t);
    switch (this.s) {
      case "main": if (l === "1") { this.s = "net"; e("[OTA] Scanning...\r\n" + SCAN); } else if (l === "2") e(COMPONENTS + MAIN); break;
      case "net": this.s = "password"; e("[OTA] Network is WPA/WPA2 - password required\r\n\r\nEnter WiFi password: "); break;
      case "password": this.s = "url"; e("\r\n[OTA] Connecting...\r\nSTATUS:: IPv4: UP (ip 192.0.2.173, gw 192.0.2.1, mask 255.255.255.0)\r\n[OTA] IP acquired\r\n\r\nEnter server base URL (e.g. https://192.0.2.100:8443): " + VLAN + VLAN); break;
      case "url": if (this.opts.noServer) { this.s = "url"; e("[HTTPS] TLS handshake failed\r\n[OTA] Server query failed\r\n\r\nEnter server base URL (e.g. https://192.0.2.100:8443): "); } else { this.s = "dl"; e(UPDATES + DOWNLOAD + VLAN); } break;
      case "dl":
        if (l === "2") { this.s = "pick"; e("Available updates:\r\n [0] Vendor_Image (version 0.0.2.0)\r\n\r\nSelect update to download (0-0): "); }
        else if (l === "3") { this.s = "staged"; e("[OTA] Installing candidates...\r\n[OTA] Install OK - candidates are now STAGED\r\n" + STAGED); }
        else if (l === "4") e(COMPONENTS + DOWNLOAD); break;
      case "pick": this.s = "dl"; e("[OTA] Updating Vendor_Image -> component 5\r\n[OTA] Download complete\r\n" + DOWNLOAD); break;
      case "staged": if (l === "1") { this.s = "trial"; e("[OTA] Requesting reboot...\r\n\x00" + banner("0.0.2.0") + TRIAL); } break;
      case "trial":
        if (l === "3") { this.s = "main"; e("[OTA] Rejecting trial firmware...\r\n[OTA] Rebooting to rollback...\r\n" + banner("0.0.1.0") + MAIN); }
        else if (l === "2") { this.s = "main"; e("[OTA] Accepting trial firmware...\r\n[OTA] Rebooting to commit update...\r\n" + banner("0.0.2.0") + MAIN); } break;
    }
  }
}
function rig(opts) { const b = new Board(opts), lines = []; const d = new OtaDriver(b, { log: (l, k) => lines.push([k, l]) }); b.attach(d); b.emit(banner("0.0.1.0") + MAIN); return { b, d, lines }; }
const PW = "S3cret-pass!";

test("the whole update: scan, join, query, download, install, reboot into trial, roll back", async () => {
  const { b, d, lines } = rig();
  assert.equal(await d.attach(), "main");
  const nets = await d.scan(); assert.equal(nets.length, 4);
  const j = await d.join(3, PW); assert.equal(j.ip, "192.0.2.173");
  const ups = await d.server("https://192.0.2.71:8443"); assert.equal(ups[0].status, "NEW");
  await d.download(0); await d.install();
  const r = await d.reboot(); assert.deepEqual([r.version, r.trial, r.menu], ["0.0.2.0", true, "trial"]);
  const back = await d.revert(); assert.deepEqual([back.version, back.menu], ["0.0.1.0", "main"]);
  assert.deepEqual(b.sent.map((x) => x[1]), ["1", "3", PW, "https://192.0.2.71:8443", "2", "0", "3", "1", "3"]);
  assert.ok(d.timeline.some((t) => t.step === "Rejecting-done"));
  // the password never reaches the log, the buffer or the timeline
  assert.ok(!JSON.stringify(lines).includes(PW) && !d.buf.includes(PW) && !JSON.stringify(d.timeline).includes(PW));
  // dropped-frame noise is not shown as a console line the user must read
  assert.ok(!JSON.stringify(lines).includes("VLAN packet"));
  // a carriage return ends every answer, never a line feed
  assert.ok(b.sent.length === 9);
});

test("accepting commits and the board comes back on the new image", async () => {
  const { d } = rig(); await d.attach(); await d.scan(); await d.join(3, PW); await d.server("https://192.0.2.71:8443");
  await d.download(0); await d.install(); await d.reboot();
  const k = await d.keep(); assert.equal(k.version, "0.0.2.0"); assert.equal(k.menu, "main");
});

test("component table works from the menus that offer it", async () => {
  const { d } = rig(); await d.attach();
  const c = await d.components(); assert.equal(c.find((x) => x.n === 5).state, "CANDIDATE");
});

test("typed refusals: a bad server address, a missing password, an unreachable server, a silent console", async () => {
  { const { d } = rig(); await d.attach(); await d.scan(); await assert.rejects(d.join(3, ""), (e) => e instanceof OtaError && e.kind === "password"); }
  { const { d } = rig(); await d.attach(); await d.scan(); await d.join(3, PW); await assert.rejects(d.server("http://x"), (e) => e.kind === "url"); await assert.rejects(d.server("https://a b"), (e) => e.kind === "url"); }
  { const { d } = rig({ noServer: true }); await d.attach(); await d.scan(); await d.join(3, PW); await assert.rejects(d.server("https://192.0.2.71:8443"), (e) => e.kind === "server" && /Start ota_server/.test(e.next)); }
  { const d = new OtaDriver({ write: async () => {} }, {}); await assert.rejects(d.attach({ ms: 50 }), (e) => e.kind === "silent"); }
});

test("the module has no command for activation, fuses, rollback protection, RMA or a full erase", () => {
  const src = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "..", "site/flash/cc35-ota.js"), "utf8");
  for (const w of ["activation", "fuse", "rma", "full_flash_erase", "rollback_protection", "rollbackProtection"]) assert.ok(!new RegExp(w, "i").test(src.replace(/\/\/.*$/gm, "").replace(/\/\*[\s\S]*?\*\//g, "")), w);
});
