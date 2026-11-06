// The motor console on a phone: this Mac holds the board's serial port and serves site/motor-console.html with a
// stand-in navigator.serial that forwards over a WebSocket (the same bridge as console-bench-test.mjs). Phone browsers
// have no Web Serial, so this is the only way a phone can drive the board.
//
//   NODE_PATH=~/.local/share/fox-red-labs/console-bench/node_modules \
//   python3 tools/frl/lock.py exec --serial AL130001 --op phone-console -- \
//     node tools/motorlink/phone_console.mjs --port /dev/cu.usbmodemAL1300011 --host <a private address of this Mac>
//
// Private by design: it listens only on the given address (the Mac's private-network address, never 0.0.0.0) and every page and
// socket needs the random token printed at start. One phone at a time: a new connection takes the port over. If the phone
// goes quiet the page's heartbeat stops and the bench guard stops the motor within 1 s. Ctrl-C ends it and frees the port.
import { createRequire } from "module";
import { createServer } from "http";
import { readFileSync, existsSync, statSync, writeFileSync, mkdirSync } from "fs";
import { homedir } from "os";
import { dirname, join, extname } from "path";
import { fileURLToPath } from "url";
import { randomBytes } from "crypto";
import { startLines, openLog } from "./console_link.mjs";
// the serial port is opened by serial_relay.py (termios and IOSSIOSPEED, as link.py does): node serialport did not apply
// the 9.2 Mbaud rate on this Mac. The relay is a child process; its stdin and stdout carry the raw bytes.
class Relay {
  constructor(path, baud, cb) {
    this.p = spawn("python3", [join(dirname(fileURLToPath(import.meta.url)), "serial_relay.py"), path, String(baud)]);
    let ready = false;
    this.p.stderr.on("data", (d) => { if (!ready && /relay open/.test(d.toString())) { ready = true; cb(null); } });
    this.p.on("exit", (code) => { if (!ready) cb(new Error("relay exited " + code)); });
  }
  on(ev, fn) { if (ev === "data") this.p.stdout.on("data", fn); if (ev === "error") this.p.on("error", fn); }
  removeAllListeners() { this.p.stdout.removeAllListeners("data"); }
  write(b) { this.p.stdin.write(b); }
  close(done) { this.p.once("exit", () => done && done()); this.p.stdin.end(); setTimeout(() => this.p.kill(), 1000); }
}

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const require = createRequire(join(process.env.NODE_PATH, "x.js"));   // for ws
import { spawn } from "child_process";
const { WebSocketServer } = require("ws");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const ttyPath = arg("--port", "/dev/cu.usbmodemAL1300011"), host = arg("--host", null), port = Number(arg("--http", "8765"));
if (!host || host === "0.0.0.0") { console.error("give --host with the Mac's private-network address; 0.0.0.0 is refused"); process.exit(2); }
// the token persists (owner-only file outside the repo) so a phone's saved link and home-screen icon keep working
const TOKFILE = join(homedir(), ".local/state/fox-red-labs/phone-console.token");
let TOKEN = existsSync(TOKFILE) ? readFileSync(TOKFILE, "utf8").trim() : "";
if (!/^[0-9a-f]{24}$/.test(TOKEN)) { TOKEN = randomBytes(12).toString("hex"); mkdirSync(dirname(TOKFILE), { recursive: true }); writeFileSync(TOKFILE, TOKEN, { mode: 0o600 }); }
// --log <file>: also write the log there, created owner-only (600). The token is never logged.
const logSink = arg("--log", null) ? openLog(arg("--log", null)) : null;
const log = (...a) => { console.log(new Date().toISOString().slice(11, 19), ...a); if (logSink) logSink([new Date().toISOString().slice(11, 19), ...a].join(" ")); };

const shim = `<script>window.FRL_BRIDGE = { cam: "/cam" };(() => {
  const URLW = (location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/serial?t=${TOKEN}";
  let ws = null, sctl = null;
  const port = { readable: null, writable: null,
    open: async ({ baudRate }) => {
      ws = new WebSocket(URLW); ws.binaryType = "arraybuffer";
      await new Promise((res, rej) => { ws.onopen = res; ws.onerror = () => rej(new Error("bridge down")); });
      port.readable = new ReadableStream({ start: (c) => { sctl = c; } });
      const mine = ws;
      port.writable = new WritableStream({ write: (chunk) => { if (mine.readyState === 1) mine.send(chunk); } });
      mine.onclose = () => { try { sctl && sctl.error(new Error("The device has been lost.")); } catch (e) {} };
      await new Promise((res, rej) => { mine.onmessage = (e) => {
        if (typeof e.data === "string") { const m = JSON.parse(e.data); if (m.op === "sopened") res(); if (m.op === "serror") rej(new Error(m.message)); return; }
        try { sctl && sctl.enqueue(new Uint8Array(e.data)); } catch (x) {} };
        mine.send(JSON.stringify({ op: "sopen", baud: baudRate })); });
    },
    close: async () => { try { ws.send(JSON.stringify({ op: "sclose" })); ws.close(); } catch (e) {} sctl = null; } };
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port, getPorts: async () => [port] } });
})();</script>`;

const types = { ".html": "text/html; charset=utf-8", ".js": "text/javascript", ".json": "application/json", ".svg": "image/svg+xml",
  ".css": "text/css", ".woff2": "font/woff2", ".png": "image/png", ".jpg": "image/jpeg", ".txt": "text/plain", ".webmanifest": "application/manifest+json" };
const authed = (req) => {
  const u = new URL(req.url, "http://x");
  return u.searchParams.get("t") === TOKEN || (req.headers.cookie || "").split(/;\s*/).includes(`frlpc=${TOKEN}`);
};
// the bench camera inside the remote, so the phone never leaves the page (iOS freezes a background tab, the heartbeat
// stops and the guard stops the motor). ffmpeg reads the camera's RTSP stream and serves MJPEG; one viewer at a time.
// Private like the rest: private-network address and token only.
// the camera's stream address is private: --cam, or the owner-only file ~/.local/state/fox-red-labs/lab-cam-url; never in the repo
const CAMFILE = join(homedir(), ".local/state/fox-red-labs/lab-cam-url");
const CAM = arg("--cam", existsSync(CAMFILE) ? readFileSync(CAMFILE, "utf8").trim() : "");
let camProc = null;
function serveCam(req, res) {
  if (!CAM) { res.writeHead(404); res.end("no camera configured"); return; }
  if (camProc) { try { camProc.kill("SIGKILL"); } catch (e) {} }
  const ff = spawn("ffmpeg", ["-hide_banner", "-loglevel", "error", "-rtsp_transport", "tcp", "-i", CAM,
    "-vf", "scale=640:-2,fps=8", "-q:v", "7", "-f", "mpjpeg", "-"]);
  camProc = ff;
  res.writeHead(200, { "Content-Type": "multipart/x-mixed-replace;boundary=ffmpeg", "Cache-Control": "no-store" });
  ff.stdout.pipe(res);
  const end = () => { try { ff.kill("SIGKILL"); } catch (e) {} if (camProc === ff) camProc = null; };
  req.on("close", end); ff.on("exit", () => { try { res.end(); } catch (e) {} });
  log("camera view opened");
}
const http = createServer((req, res) => {
  if (!authed(req)) { res.writeHead(403); res.end("forbidden"); return; }
  const u = new URL(req.url, "http://x");
  if (u.pathname === "/cam") { serveCam(req, res); return; }
  let rel = decodeURIComponent(u.pathname);
  if (rel === "/") rel = "/flash/remote/motor.html";   // the phone remote; the desktop console stays at /motor-console.html
  const p = join(ROOT, "site", rel);
  if (!p.startsWith(join(ROOT, "site")) || !existsSync(p) || statSync(p).isDirectory()) { res.writeHead(404); res.end(); return; }
  const head = { "Content-Type": types[extname(p)] || "application/octet-stream", "Cache-Control": "no-store",
    "Set-Cookie": `frlpc=${TOKEN}; Path=/; HttpOnly; SameSite=Strict` };
  let body = readFileSync(p);
  if (p.endsWith(".html") && body.includes("navigator.serial")) body = Buffer.from(body.toString().replace(/<head[^>]*>/i, (m) => m + shim));
  res.writeHead(200, head); res.end(body);
});
const wss = new WebSocketServer({ noServer: true });
http.on("upgrade", (req, sock, headBuf) => {
  if (!req.url.startsWith("/serial") || !authed(req)) { sock.destroy(); return; }
  wss.handleUpgrade(req, sock, headBuf, (ws) => wss.emit("connection", ws, req));
});

let tty = null, cur = null;
const closeTty = () => new Promise((r) => { if (!tty) return r(); const t = tty; tty = null; t.removeAllListeners("data"); t.close(() => r()); });
wss.on("connection", (ws, req) => {
  log("phone connected from", req.socket.remoteAddress);
  ws.on("message", async (m, isBin) => {
    if (isBin) { if (tty && cur === ws) tty.write(m); return; }
    const msg = JSON.parse(m.toString());
    if (msg.op === "sopen") {
      if (cur && cur !== ws) { log("a new connection takes the port over"); try { cur.terminate(); } catch (e) {} }
      await closeTty(); cur = ws;
      tty = new Relay(ttyPath, msg.baud, (err) => {
        if (err) { log("serial open failed:", err.message); ws.send(JSON.stringify({ op: "serror", message: err.message })); return; }
        log("serial open at", msg.baud, "baud"); ws.send(JSON.stringify({ op: "sopened" }));
      });
      tty.on("data", (d) => { if (cur === ws && ws.readyState === 1) ws.send(d); });
      tty.on("error", (e) => log("serial error:", e.message));
    }
    if (msg.op === "sclose" && cur === ws) { await closeTty(); cur = null; log("serial closed by the page"); }
  });
  ws.on("close", async () => { if (cur === ws) { await closeTty(); cur = null; log("phone disconnected; port released"); } });
});

http.listen(port, host, () => {
  // the full link only to a terminal; a redirected stdout (a log file) gets it masked, as does the --log file
  for (const l of startLines(host, port, TOKEN, process.stdout.isTTY)) {
    if (process.stdout.isTTY) { console.log(new Date().toISOString().slice(11, 19), l); logSink && logSink(new Date().toISOString().slice(11, 19) + " " + startLines(host, port, TOKEN, false)[0]); }
    else log(l);
  }
});
const stop = async () => { await closeTty(); process.exit(0); };
process.on("SIGINT", stop); process.on("SIGTERM", stop);
