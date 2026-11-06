// Run the page's OTA driver (site/flash/cc35-ota.js) from the Mac against a board, through a serial-to-TCP bridge.
// This is the bench proof of what the Digital Lab does over Web Serial: same module, same menu answers.
//
//   python3 tools/frl/lock.py exec --serial <probe> -- python3 cc35x1/ota/bridge.py /dev/cu.usbmodem<probe>1 7701 &
//   FRL_WIFI_SSID=... FRL_WIFI_PASSWORD=... node cc35x1/ota/drive.mjs --server https://<mac ip>:8443 --end revert|keep
//
// Wi-Fi name and password come from the environment only; the driver masks the password in everything it prints.
import net from "node:net";
import { OtaDriver } from "../../site/flash/cc35-ota.js";

const arg = (k, d) => { const i = process.argv.indexOf("--" + k); return i > 0 ? process.argv[i + 1] : d; };
const server = arg("server"), end = arg("end", "revert"), port = +arg("tcp", 7701);
const ssid = process.env.FRL_WIFI_SSID, pw = process.env.FRL_WIFI_PASSWORD;
if (!server || !ssid) { console.error("need --server https://host:8443 and FRL_WIFI_SSID (and FRL_WIFI_PASSWORD for a secured network)"); process.exit(2); }

const sock = net.connect(port, "127.0.0.1");
await new Promise((r, j) => { sock.once("connect", r); sock.once("error", j); });
const t0 = Date.now();
const d = new OtaDriver({ write: (s) => new Promise((r) => sock.write(s, r)) }, { log: (l, k) => { if (process.env.QUIET !== "1") console.log(`${((Date.now() - t0) / 1000).toFixed(1).padStart(6)} ${k === "tx" ? "TX" : "  "} ${l}`); } });
sock.on("data", (b) => d.feed(b.toString("latin1")));
const step = async (name, f) => { const s = Date.now(); const r = await f(); console.log(`== ${name}: ${((Date.now() - s) / 1000).toFixed(1)} s`); return r; };

try {
  console.log("attached in:", await d.attach({ poke: false, ms: 1500 }).catch(() => d.attach({ poke: true })));
  if (await d.here() === "candidate") { console.log("a downloaded image was waiting; cancelling it for a clean run"); await step("cancel candidate", () => d.candidate("cancel")); }
  const before = await step("components before", () => d.components());
  const nets = await step("scan", () => d.scan());
  const row = nets.filter((n) => n.ssid === ssid).sort((a, b) => b.rssi - a.rssi)[0];
  if (!row) throw new Error("network not in scan");
  const { ip } = await step("join", () => d.join(row.n, pw));
  console.log("board ip:", ip);
  const ups = await step("query server", () => d.server(server));
  console.log("offered:", JSON.stringify(ups));
  const nw = ups.find((u) => u.status === "NEW");
  if (!nw) throw new Error("no NEW image on the server");
  await step("download", () => d.download(nw.index));
  await step("install", () => d.install());
  const rb = await step("reboot into trial", () => d.reboot());
  console.log("trial:", rb.trial, "version:", rb.version);
  const fin = await step(end === "keep" ? "accept" : "reject", () => (end === "keep" ? d.keep() : d.revert()));
  console.log("after:", fin.version, fin.menu);
  const after = await step("components after", () => d.components());
  console.log("slots after:", after.filter((c) => c.name.startsWith("Vendor_Image")).map((c) => `${c.name} ${c.state} ${c.version} active=${c.active}`).join("; "));
  console.log("TIMELINE", JSON.stringify(d.timeline));
} catch (e) { console.error("FAILED:", e.kind || "", e.message, "|", e.next || ""); process.exitCode = 1; }
sock.destroy();
