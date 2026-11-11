// The Bluetooth light controls (Web Bluetooth), shared by the bench page and the light page.
// Firmware and protocol: cc2755p10/ble-light/README.md.
const U = (n) => `8a57000${n}-4724-4979-ab46-406d217495e5`;
export const SERVICE = U(1);
const COLOUR = U(2), PATTERN = U(3), STATUS = U(4);
const R = [255, 0, 0], G = [0, 255, 0], Y = [255, 255, 0], OFF = [0, 0, 0];
const COLOURS = [["Red", R, "#d33"], ["Green", G, "#2a2"], ["Yellow", Y, "#db2"], ["Off", OFF, "transparent"]];
const PATTERNS = [
  ["Blink", [[R, 150], [OFF, 150], [R, 150], [OFF, 900]], "#d33"],
  ["Traffic light", [[G, 2000], [Y, 700], [R, 2000]], "#db2"],
  ["Police", [[R, 120], [OFF, 60], [R, 120], [OFF, 200], [G, 120], [OFF, 60], [G, 120], [OFF, 200]], "#d33"],
  ["Heartbeat", [[R, 100], [OFF, 120], [R, 100], [OFF, 800]], "#d33"],
];
const CSS = `.frl-light .grid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin:6px 0 12px}
.frl-light .grid button{background:var(--bg);color:var(--fg);border:1px solid var(--line);text-align:left;font-weight:600;padding:10px 14px}
.frl-light .grid button[aria-pressed="true"]{border:2px solid var(--red);padding:9px 13px}
.frl-light .sw{display:inline-block;width:14px;height:14px;border-radius:50%;margin-right:8px;vertical-align:-2px;border:1px solid var(--line)}
.frl-light .status{font-family:ui-monospace,Menlo,monospace;font-size:.92rem}
.frl-light .err{color:var(--red)}`;

export function mountLight(root) {
  if (!document.getElementById("frl-light-css")) { const s = document.createElement("style"); s.id = "frl-light-css"; s.textContent = CSS; document.head.appendChild(s); }
  root.classList.add("frl-light");
  root.innerHTML = `<div class="row"><button data-k="connect">Find the light</button> <button data-k="disconnect" class="quiet hidden">Disconnect</button></div>
    <p data-k="found" class="small">Chrome on Android, or Chrome or Edge on a computer. Turn Bluetooth on first.</p>
    <p class="small"><b>Colour</b></p><div class="grid" data-k="colours"></div>
    <p class="small"><b>Pattern</b></p><div class="grid" data-k="patterns"></div>
    <p class="small">The board says: <span class="status" data-k="status">not connected</span></p>`;
  const q = (k) => root.querySelector(`[data-k="${k}"]`);
  let dev = null, chars = {};
  const say = (t, bad) => { q("found").textContent = t; q("found").classList.toggle("err", !!bad); };
  const enable = (on) => root.querySelectorAll(".grid button").forEach((b) => { b.disabled = !on; });
  const press = (btn) => root.querySelectorAll(".grid button").forEach((b) => b.setAttribute("aria-pressed", String(b === btn)));
  const buttons = (where, list, send) => {
    for (const [name, value, swatch] of list) {
      const b = document.createElement("button"); b.setAttribute("aria-pressed", "false");
      b.innerHTML = `<span class="sw"></span>`; b.querySelector(".sw").style.background = swatch; b.append(name);
      b.addEventListener("click", async () => { try { await send(value); press(b); } catch (e) { say("Could not send: " + e.message, true); } });
      q(where).appendChild(b);
    }
  };
  buttons("colours", COLOURS, (rgb) => chars.colour.writeValueWithResponse(new Uint8Array(rgb)));
  buttons("patterns", PATTERNS, (steps) => {
    const b = new Uint8Array(steps.length * 5);
    steps.forEach(([rgb, ms], i) => { b.set(rgb, i * 5); b[i * 5 + 3] = ms & 0xff; b[i * 5 + 4] = ms >> 8; });
    return chars.pattern.writeValueWithResponse(b);
  });
  enable(false);
  q("connect").addEventListener("click", async () => {
    if (!navigator.bluetooth) { say("This browser has no Bluetooth for web pages. Use Chrome on Android or a computer.", true); return; }
    try {
      say("Pick \"FRL Light\" in the list...");
      dev = await navigator.bluetooth.requestDevice({ filters: [{ services: [SERVICE] }] });
      dev.addEventListener("gattserverdisconnected", () => { enable(false); q("disconnect").classList.add("hidden"); say("Disconnected. Tap Find the light to connect again."); q("status").textContent = "not connected"; });
      say("Connecting to " + (dev.name || "the light") + "...");
      const s = await (await dev.gatt.connect()).getPrimaryService(SERVICE);
      chars = { colour: await s.getCharacteristic(COLOUR), pattern: await s.getCharacteristic(PATTERN), status: await s.getCharacteristic(STATUS) };
      const show = (v) => { q("status").textContent = new TextDecoder().decode(v); };
      show(await chars.status.readValue());
      chars.status.addEventListener("characteristicvaluechanged", (e) => show(e.target.value));
      await chars.status.startNotifications();
      enable(true); q("disconnect").classList.remove("hidden");
      say("Connected to " + (dev.name || "the light") + ". Pick a colour or a pattern.");
    } catch (e) {
      say(e.name === "NotFoundError" ? "No light picked. Is the board powered and running the light program?" : "Could not connect: " + e.message, true);
    }
  });
  q("disconnect").addEventListener("click", () => { if (dev && dev.gatt.connected) dev.gatt.disconnect(); });
}
