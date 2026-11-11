// LP-EM-CC35X1: update the board over Wi-Fi by driving TI's ota_example menu on the probe's serial port (route B in
// docs/research/web-flash-espressif-and-cc35-2026-10-22.md). The page never writes the chip's flash: the running ota_example
// downloads a signed image from the user's own HTTPS server, the chip's bootloader checks the signature and swaps it in, and
// the user keeps or rejects the new image. This module only types the menu answers and reads the replies.
//
// What it does not do, by design: no activation, fuse, rollback-protection, lifecycle or erase command exists here, and the
// first programming of the board (the layout with two image slots) stays a Toolbox step (boards.json cc35x1 "ota".firstTime).
// The Wi-Fi password is typed by the user, sent once, and kept out of the log, the transcript and every saved text.
//
// The io object is { write(string) -> Promise }; whoever owns the port calls driver.feed(text) with what arrives.
// openWebSerial() builds both for Chrome and Edge; the unit test and the bench proof use a socket and a recording.

export class OtaError extends Error {
  constructor(kind, message, next) { super(message); this.kind = kind; this.next = next || ""; }
}

// ---- text the example prints, as regular expressions (checked against a real console log in tools/webflash/cc35-ota.test.mjs)
export const RE = {
  banner: /CC35xx OTA Example Application(?: FRL (\d+\.\d+\.\d+\.\d+))?/,
  main: /\[1\] Connect to WiFi\s+\[2\] Show components in flash\s+\[3\] Clean component[\s\S]*?Choice: /,
  trial: /Trial menu:[\s\S]*?\[5\] Continue to main menu[\s\S]*?Choice: /,
  trialSelf: /\[1\] Accept \(commit and reboot\)\s+\[2\] Reject \(rollback and reboot\)\s+\[3\] Back to trial menu[\s\S]*?Choice: /,
  candidate: /Downloaded updates ready to install\. Select action:[\s\S]*?Choice: /,
  download: /Download menu:[\s\S]*?\[7\] Quit[\s\S]*?Choice: /,
  staged: /Updates staged\. Select action:[\s\S]*?Choice: /,
  netPrompt: /Enter network number \(\d+-\d+\), or 0 to re-scan: /,
  pwPrompt: /Enter WiFi password: /,
  urlPrompt: /Enter server base URL[^\n]*: /,
  pickPrompt: /Select update to download \(\d+-\d+\):/,
  needsPw: /Network is WPA\/WPA2 - password required|password required/i,
  ip: /IPv4: UP \(ip (\d+\.\d+\.\d+\.\d+)/,
  failure: /\[OTA\][^\n]*\b(?:[Ff]ailed|[Ee]rror)\b[^\n]*/,
};

/** The scan table: " 3   Home Net   -75   7    WPA2". Hidden networks have an empty name and are kept (the example lists them). */
export function parseScan(text) {
  const rows = [];
  for (const m of text.matchAll(/^\s*(\d+)\s{2,}(.*?)\s+(-\d+)\s+(\d+)\s+(\S+)\s*$/gm)) rows.push({ n: +m[1], ssid: m[2].trim(), rssi: +m[3], ch: +m[4], security: m[5] });
  return rows;
}

/** "Components in flash" table. */
export function parseComponents(text) {
  const rows = [];
  for (const m of text.matchAll(/^\s*(\d+)\s+(\w+_Slot_\d)\s+(\w+)\s+(Yes|No)\s+(Yes|No)\s+(\d+\.\d+\.\d+\.\d+)/gm))
    rows.push({ n: +m[1], name: m[2], state: m[3], primary: m[4] === "Yes", active: m[5] === "Yes", version: m[6] });
  return rows;
}

/** "Available updates from server" table. status is NEW or OK. */
export function parseUpdates(text) {
  const rows = [];
  for (const m of text.matchAll(/^\s*\[(\d+)\]\s+(\w+)\s+(\d+\.\d+\.\d+\.\d+) \(S(\d)\)\s+(\d+\.\d+\.\d+\.\d+)\s+\[(\w+)\]/gm))
    rows.push({ index: +m[1], component: m[2], active: m[3], slot: +m[4], server: m[5], status: m[6] });
  return rows;
}

/** Which menu the console is waiting in: the last prompt in the text. The example keeps printing status lines after a prompt
 *  (IP changes, dropped VLAN frames), so a prompt is found anywhere in the text, not only at its end. "" when none is there. */
export function menuOf(tail) {
  const t = tail.replace(/\r/g, "");
  const order = [["password", RE.pwPrompt], ["network", RE.netPrompt], ["url", RE.urlPrompt], ["pick", RE.pickPrompt], ["trial-test", RE.trialSelf],
    ["trial", RE.trial], ["candidate", RE.candidate], ["staged", RE.staged], ["download", RE.download], ["main", RE.main]];
  let best = "", at = -1;
  for (const [name, re] of order) {
    const g = new RegExp(re.source, "g"); let m, last = -1;
    while ((m = g.exec(t))) last = m.index + m[0].length;
    if (last > at) { at = last; best = name; }
  }
  return best;
}

// The Wi-Fi stack prints this for every VLAN-tagged frame on some home networks; it is not an error for the update.
const NOISE = /ERROR rxData_DataPacketDisptcher\(\): Received VLAN packet - drop it\r?\n?\r?/g;

/** The keystroke that shows the component table in each menu. */
export const SHOW_KEY = { main: "2", download: "4", staged: "3", trial: "4" };

export class OtaDriver {
  /** @param io {write}  @param opts {log(line, kind), now(), secrets} */
  constructor(io, opts = {}) {
    this.io = io; this.log = opts.log || (() => {}); this.now = opts.now || (() => Date.now());
    this.buf = ""; this.waiters = []; this.muted = false; this.timeline = []; this.t0 = this.now();
    this.secret = ""; this.maxBuf = 200000; this.menu = "";
  }
  mark(step) { this.timeline.push({ step, ms: this.now() - this.t0 }); }

  /** Everything the port delivers goes through here. The echo of the password is dropped before anything sees it. */
  feed(chunk) {
    let text = chunk;
    if (this.muted) {
      const nl = text.search(/[\r\n]/);
      if (nl < 0) return;                      // still the echo of the password
      this.muted = false; text = text.slice(nl);
    }
    if (this.secret) text = text.split(this.secret).join("****");
    text = text.replace(NOISE, "");
    this.buf += text.replace(/\u0000/g, "");
    if (this.buf.length > this.maxBuf) this.buf = this.buf.slice(-this.maxBuf / 2);
    // the log gets whole lines: a partial line waits for its end
    this.partial = (this.partial || "") + text.replace(/\r/g, "");
    const lines = this.partial.split("\n"); this.partial = lines.pop();
    for (const line of lines) if (line.trim()) this.log(line, "rx");
    clearTimeout(this.flushTimer);
    this.flushTimer = setTimeout(() => { if (this.partial && this.partial.trim()) this.log(this.partial, "rx"); this.partial = ""; }, 250);
    this.waiters = this.waiters.filter((w) => !w.check());
  }

  async send(line, { secret = false } = {}) {
    if (secret) { this.secret = line; this.muted = true; this.log("(password sent)", "tx"); }
    else this.log("> " + line, "tx");
    this.menu = "";
    await this.io.write(line + "\r");              // a carriage return only: a line feed is read as an empty answer to the next prompt
  }

  /** Wait until test(buf) returns a value, from the text received after the last consume. Rejects with a typed error. */
  expect(test, ms, what) {
    return new Promise((resolve, reject) => {
      const w = { check: () => { const r = test(this.buf); if (r === undefined || r === null || r === false) return false; clearTimeout(timer); resolve(r); return true; } };
      const timer = setTimeout(() => { this.waiters = this.waiters.filter((x) => x !== w); reject(new OtaError("timeout", `No answer from the board for ${what} within ${Math.round(ms / 1000)} s.`, "Check the console is the board's application port and the board is powered; press S1 (reset) and try again.")); }, ms);
      this.waiters.push(w);
      if (w.check()) this.waiters = this.waiters.filter((x) => x !== w);
    });
  }
  /** The menu the board waits in: the one last seen, else what the unread text shows, else ask the console. */
  async here() { return this.menu || menuOf(this.buf) || (await this.attach()); }
  /** Wait for a menu or prompt; take the text up to it. */
  async waitMenu(names, ms, what) {
    const list = Array.isArray(names) ? names : [names];
    const hit = await this.expect((b) => { const m = menuOf(b); return list.includes(m) ? { menu: m } : undefined; }, ms, what);
    const text = this.buf; this.buf = ""; this.menu = hit.menu; return { menu: hit.menu, text };
  }
  failureIn(text) { const m = text.match(RE.failure); return m ? m[0].trim() : ""; }

  /** Find out where the console is. Does not type anything unless `poke` is true. */
  async attach({ poke = false, ms = 2500 } = {}) {
    if (poke) await this.io.write("\r");
    try { const r = await this.waitMenu(["main", "download", "trial", "staged", "candidate", "network", "url", "pick", "trial-test"], ms, "a menu"); this.mark("attached:" + r.menu); return r.menu; }
    catch (e) { throw new OtaError("silent", "The console printed no menu.", "Press S1 on the board to restart it (the menu prints at boot), or pick the other XDS110 serial port: the application prints on the first one, at 115200."); }
  }

  /** Main menu -> start Wi-Fi and list the networks. Takes about 8 s (the radio starts and scans). */
  async scan() {
    this.mark("scan-start");
    const menu = await this.here();
    if (menu === "network") { await this.send("0"); }
    else if (menu === "main") await this.send("1");
    else throw new OtaError("state", `Wi-Fi scan starts from the main menu; the board is in "${menu}".`, "Press S1 to restart it.");
    const r = await this.waitMenu("network", 60000, "the Wi-Fi scan");
    this.mark("scan-done");
    const nets = parseScan(r.text);
    if (!nets.length) throw new OtaError("scan", "The scan found no networks.", "Move the board near the access point and scan again.");
    return nets;
  }

  /** Choose a scan row and join. Returns { ip }. The password is sent once and never logged. */
  async join(n, password) {
    this.mark("join-start");
    await this.send(String(n));
    const r = await this.expect((b) => (RE.pwPrompt.test(b) ? { pw: true } : RE.urlPrompt.test(b) ? { pw: false } : undefined), 15000, "the Wi-Fi password prompt");
    if (r.pw) {
      if (!password) throw new OtaError("password", "This network needs a password.", "Enter it and join again.");
      this.buf = ""; await this.send(password, { secret: true });
    }
    const end = await this.expect((b) => (RE.urlPrompt.test(b) ? { ok: true } : /Connection failed|Failed to connect|connect failed|could not connect|Authentication failed/i.test(b) ? { ok: false, why: b.match(/Connection failed|Failed to connect|connect failed|could not connect|Authentication failed/i)[0] } : undefined), 45000, "the Wi-Fi join");
    const text = this.buf; this.buf = ""; this.menu = end.ok ? "url" : "";
    if (!end.ok) throw new OtaError("join", `The board could not join the network (${end.why}).`, "Check the password and that the network is 2.4 or 5 GHz WPA2, then scan again.");
    const ip = (text.match(RE.ip) || [])[1] || "";
    this.secret = "";
    this.mark("join-done");
    return { ip };
  }

  /** Give the board the update server, a base URL like https://192.0.2.71:8443. Returns the updates the server offers. */
  async server(url) {
    if (!/^https:\/\/[A-Za-z0-9.\-]+(:\d{1,5})?\/?$/.test(url)) throw new OtaError("url", "The server address must look like https://192.0.2.71:8443 (the computer running ota_server.py).", "");
    this.mark("query-start");
    const menu = await this.here();
    if (menu === "download") { await this.send("5"); await this.waitMenu("url", 8000, "the server prompt"); }
    else if (menu !== "url") throw new OtaError("state", `The server address is asked after the Wi-Fi join; the board is in "${menu}".`, "Join Wi-Fi first.");
    await this.send(url.replace(/\/$/, ""));
    const r = await this.waitMenu(["download", "url"], 60000, "the server answer");
    const bad = this.failureIn(r.text) || (/HTTPS\][^\n]*(?:fail|error)/i.test(r.text) ? "The HTTPS request failed." : "");
    const updates = parseUpdates(r.text);
    this.mark("query-done");
    if (r.menu === "url" || !updates.length) throw new OtaError("server", bad || "The server offered no image.", "Start ota_server.py with the signed image in its folder, on the same network as the board, and check the address.");
    return updates;
  }

  /** Download one offered image into the spare slot. A slot left FAILED by the Toolbox is erased first, about 20 s more. */
  async download(index) {
    this.mark("download-start");
    if (await this.here() !== "download") throw new OtaError("state", "Not at the download menu.", "Query the server first.");
    await this.send("2");
    await this.waitMenu("pick", 8000, "the image list");
    await this.send(String(index));
    const r = await this.waitMenu("download", 240000, "the download");
    const bad = this.failureIn(r.text);
    this.mark("download-done");
    if (bad) throw new OtaError("download", bad, "Show the components: if the slot says FAILED, the board erases it on the next try.");
    return { text: r.text };
  }

  /** Stage the downloaded image. The reboot must be requested in the same session. */
  async install() {
    this.mark("install-start");
    await this.send("3");
    const r = await this.waitMenu("staged", 60000, "the install");
    this.mark("install-done");
    if (!/candidates are now STAGED/.test(r.text)) throw new OtaError("install", this.failureIn(r.text) || "The install did not report STAGED.", "Download again.");
    return true;
  }

  /** Reboot into the staged image. Returns { version, trial } read from the banner. The new image starts in trial. */
  async reboot() {
    this.mark("reboot-start");
    await this.send("1");
    const r = await this.expect((b) => (RE.banner.test(b) && menuOf(b.slice(b.search(RE.banner))) ? { ok: true } : undefined), 90000, "the new image to start");
    const text = this.buf; this.buf = "";
    this.mark("reboot-done");
    this.menu = menuOf(text.slice(text.search(RE.banner)));
    const ver = (text.match(RE.banner) || [])[1] || "";
    const trial = /TRIAL firmware detected/.test(text);
    return { version: ver, trial, menu: menuOf(text.slice(text.search(RE.banner))), text };
  }

  /** In the trial menu: keep the new image for good (it reboots into it) or go back to the old one. */
  async keep() { return this._trial("2", "Accepting"); }
  async revert() { return this._trial("3", "Rejecting"); }
  async _trial(key, word) {
    if (await this.here() !== "trial") throw new OtaError("state", "Not in the trial menu.", "Reboot into the new image first.");
    this.mark(word + "-start");
    await this.send(key);
    await this.expect((b) => (new RegExp(word).test(b) ? true : undefined), 15000, "the answer");
    this.buf = this.buf.slice(this.buf.search(new RegExp(word)));
    await this.expect((b) => (RE.banner.test(b) ? { ok: true } : undefined), 90000, "the board to restart");
    const r = await this.waitMenu(["main", "trial", "download"], 60000, "the menu after the restart");
    this.mark(word + "-done");
    return { version: (r.text.match(RE.banner) || [])[1] || "", menu: r.menu, text: r.text };
  }

  /** A boot that finds a downloaded but not installed image offers three choices: install and reboot, cancel the download, go on. */
  async candidate(action) {
    if (await this.here() !== "candidate") throw new OtaError("state", "No downloaded image is waiting.", "");
    if (action === "install") {
      this.mark("candidate-install"); await this.send("1");
      await this.expect((b) => (RE.banner.test(b) && menuOf(b.slice(b.search(RE.banner))) ? true : undefined), 90000, "the new image to start");
      const text = this.buf; this.buf = ""; this.menu = menuOf(text.slice(text.search(RE.banner)));
      return { version: (text.match(RE.banner) || [])[1] || "", trial: /TRIAL firmware detected/.test(text), menu: this.menu, text };
    }
    this.mark("candidate-" + action); await this.send(action === "cancel" ? "2" : "3");
    const r = await this.waitMenu(["main"], 90000, "the main menu");
    return { menu: r.menu };
  }

  /** The component table, from whichever menu the board is in. */
  async components() {
    const menu = await this.here();
    const key = SHOW_KEY[menu]; if (!key) throw new OtaError("state", `The component table is not offered in "${menu}".`, "");
    await this.send(key);
    const r = await this.waitMenu([menu], 20000, "the component table");
    return parseComponents(r.text);
  }
}

/** Chrome and Edge: ask for the probe's serial port, open it at 115200, and feed a driver. Resolves to { driver, close }. */
export async function openWebSerial({ filters = [{ usbVendorId: 0x0451 }], baudRate = 115200, log } = {}) {
  if (!("serial" in navigator)) throw new OtaError("browser", "This browser has no Web Serial.", "Use Chrome or Edge on a computer.");
  let port;
  try { port = await navigator.serial.requestPort({ filters }); }
  catch (e) {
    if (e && e.name === "NotFoundError") throw new OtaError("chooser", "No serial port was picked.", "Press Connect again and pick the XDS110 port named Application/User UART (the first of its two ports). No port listed: check the USB cable carries data and the board's power LED is on.");
    if (e && e.name === "SecurityError") throw new OtaError("permission", "The browser blocked serial access on this page.", "Open the page over https or from localhost, and check that a work or school policy does not turn off Web Serial (chrome://policy).");
    throw new OtaError("chooser", `The port chooser failed: ${e && e.message || e}`, "Reload the page and press Connect again.");
  }
  try { await port.open({ baudRate }); }
  catch (e) {
    if (e && e.name === "InvalidStateError") throw new OtaError("busy", "That port is already open in this page.", "Reload the page and connect once.");
    throw new OtaError("busy", "The port could not be opened; another program is probably using it.", "Close any terminal, CCStudio, UniFlash or other browser tab that has the board's serial port open, then press Connect again. Linux: add your user to the dialout group and log in again.");
  }
  const writer = port.writable.getWriter(), enc = new TextEncoder(), dec = new TextDecoder();
  const driver = new OtaDriver({ write: (s) => writer.write(enc.encode(s)) }, { log });
  let run = true, reader = null;
  (async () => {
    try { while (run && port.readable) { reader = port.readable.getReader(); try { for (;;) { const r = await reader.read(); if (r.done) break; driver.feed(dec.decode(r.value, { stream: true })); } } finally { reader.releaseLock(); } } }
    catch (e) { driver.log(`[console stopped: ${e.message || e}]`, "err"); }
  })();
  return { driver, async close() { run = false; try { if (reader) await reader.cancel(); } catch (e) { /* closing */ } try { writer.releaseLock(); } catch (e) { /* closing */ } await port.close().catch(() => {}); } };
}

/** The lines the page shows beside the buttons, from the board's "ota" data (boards.json). Plain strings, no markup. */
export function serverHelp(ota, ip) {
  const o = ota || {};
  return [
    ...(o.serverSteps || []),
    ip ? `The board joined the network and got ${ip}. Your computer's address is the one on that same network.` : "",
  ].filter(Boolean);
}
