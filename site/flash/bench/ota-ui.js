// The CC35X1 over-the-air update panel (data: site/flash/cc35-ota.json, driver: site/flash/cc35-ota.js). The page types the OTA example's menu answers
// over the probe's serial port and reads the replies; it never writes the board's flash. The Wi-Fi password and the server address live only in their
// input boxes: no name, no autocomplete, never stored, never in the log, and the password box is emptied as soon as it is sent.
import { openWebSerial, OtaError } from "../cc35-ota.js";

const el = (tag, props = {}, ...kids) => { const e = document.createElement(tag); Object.assign(e, props); for (const k of kids) e.append(k); return e; };

/** The state line for the menu the board waits in. */
export function stateText(menu, states = {}) {
  return { main: "The board is at its main menu.", download: "The board is joined to Wi-Fi and at its download menu.", trial: "The board runs a new image on trial: keep it or go back.",
    candidate: states.candidate || "A downloaded image is waiting.", network: "The board is waiting for a network number.", url: "The board is waiting for the server address.", staged: "An image is staged for the next restart." }[menu] || (menu ? `The board is in its "${menu}" menu.` : states.silent || "No menu yet.");
}

/** The steps the page offers in order, from the data's flow (ids: connect, scan, join, server, download, install, reboot, keep, revert). */
export function flowIds(data) { return (data.flow || []).map((f) => f.id); }

export function mountOta(root, data, board) {
  root.innerHTML = "";
  const note = (t) => el("p", { className: "small", textContent: t });
  const head = el("div", { className: "head" }, el("h2", { className: "eyebrow", textContent: data.title }), note(data.summary));
  const logBox = el("pre", { className: "ota-log", ariaLive: "polite" });
  const status = el("p", { className: "result", textContent: "Not connected." });
  const state = { driver: null, close: null, nets: [], updates: [] };
  const log = (line, kind) => { logBox.append(document.createTextNode((kind === "tx" ? "" : "") + line + "\n")); while (logBox.childNodes.length > 300) logBox.firstChild.remove(); logBox.scrollTop = logBox.scrollHeight; };
  const say = (t, bad) => { status.textContent = t; status.className = "result " + (bad ? "bad" : "ok"); };
  const busyBtns = [];
  const act = (btn, fn) => btn.addEventListener("click", async () => {
    if (btn.dataset.busy) return;
    for (const b of busyBtns) b.disabled = true;
    try { await fn(); } catch (e) { say(e instanceof OtaError ? `${e.message} ${e.next || ""}`.trim() : `Stopped: ${e.message || e}`, true); log(`[${e.kind || "error"}] ${e.message || e}`, "err"); }
    finally { for (const b of busyBtns) b.disabled = !state.driver && b.dataset.needs !== "none"; }
  });
  const button = (label, needs = "driver") => { const b = el("button", { type: "button", className: needs === "none" ? "" : "quiet", textContent: label }); b.dataset.needs = needs; b.disabled = needs !== "none"; busyBtns.push(b); return b; };
  const needDriver = () => { if (!state.driver) throw new OtaError("state", "Connect the console first.", ""); return state.driver; };

  // once, and the server
  const ft = data.firstTime || {}, sv = data.server || {};
  const first = el("details", {}, el("summary", { textContent: ft.title || "Once: program the OTA example" }), note(ft.why),
    el("pre", { textContent: `${ft.build || ""}\n${ft.command || ""}` }), note(`From ${ft.from || "the Toolbox folder"}. ${ft.after || ""} ${ft.measured || ""}`));
  const srv = el("details", {}, el("summary", { textContent: "The update server on your computer" }), ...(sv.steps || []).map((s) => el("p", { className: "small", textContent: s })), note(sv.limit));
  const tb = board && board.toolbox ? el("details", {}, el("summary", { textContent: "A board without the OTA example: program it with the Toolbox" }), note(board.toolbox.before),
    el("pre", { textContent: board.toolbox.command }), note(`From ${board.toolbox.from}. ${board.toolbox.after} ${board.toolbox.note}`)) : null;
  const con = board && board.console ? note(`Console: ${board.console.baud} baud on ${board.console.port}. ${board.console.baudNote}`) : null;

  // 1 connect
  const bConnect = button("Connect the console", "none");
  act(bConnect, async () => {
    if (state.driver) { await state.close(); state.driver = null; }
    const s = await openWebSerial({ log, baudRate: (data.serial && data.serial.baud) || 115200 });
    state.driver = s.driver; state.close = s.close;
    const m = await s.driver.attach({ poke: true });
    say(stateText(m.menu || s.driver.menu, data.states) + " " + ((data.serial && data.serial.pick) || ""), false);
  });
  // 2 scan, 3 join
  const netSel = el("select", { ariaLabel: "Wi-Fi network" }), bScan = button("Find Wi-Fi networks");
  act(bScan, async () => {
    const d = needDriver(); say("Starting the radio and scanning (a few seconds).", false);
    state.nets = await d.scan(); netSel.innerHTML = "";
    for (const n of state.nets) netSel.append(el("option", { value: String(n.n), textContent: `${n.ssid || "(hidden)"}  ${n.rssi} dBm  ch ${n.ch}  ${n.security}` }));
    say(`${state.nets.length} networks found. Pick one and type its password.`, false);
  });
  const pw = el("input", { type: "password", autocomplete: "new-password", placeholder: "Wi-Fi password", ariaLabel: "Wi-Fi password" }), bJoin = button("Join");
  pw.setAttribute("data-lpignore", "true");
  act(bJoin, async () => {
    const d = needDriver(); const word = pw.value; pw.value = "";           // emptied the moment it is read: it is never kept
    const r = await d.join(Number(netSel.value), word); say(`Joined the network${r.ip ? `, the board's address is ${r.ip}` : ""}. Give it your server's address.`, false);
  });
  // 4 server
  const url = el("input", { type: "text", autocomplete: "off", placeholder: "https://192.0.2.71:8443", ariaLabel: "Update server address", spellcheck: false }), bServer = button("Check the server");
  const upSel = el("select", { ariaLabel: "Offered image" });
  act(bServer, async () => {
    const d = needDriver(); state.updates = await d.server(url.value.trim()); upSel.innerHTML = "";
    for (const u of state.updates) upSel.append(el("option", { value: String(u.index), textContent: `${u.component} ${u.server} (now ${u.active}) ${u.status}` }));
    say(`The server offers ${state.updates.length} image${state.updates.length === 1 ? "" : "s"}.`, false);
  });
  // 5 download, install, reboot
  const bDown = button("Download"), bInst = button("Install"), bRe = button("Restart into the new image");
  act(bDown, async () => { await needDriver().download(Number(upSel.value)); say("Downloaded and checked. Press Install.", false); });
  act(bInst, async () => { await needDriver().install(); say("The image is staged. Press Restart into the new image.", false); });
  act(bRe, async () => { const r = await needDriver().reboot(); say(`The board restarted${r.version ? ` into ${r.version}` : ""}${r.trial ? " on trial" : ""}. Keep it or go back.`, false); });
  // 6 keep or go back, and the candidate menu
  const bKeep = button("Keep it"), bBack = button("Go back");
  act(bKeep, async () => { const r = await needDriver().keep(); say(`Kept${r.version ? `: the board runs ${r.version}` : ""}.`, false); });
  act(bBack, async () => { const r = await needDriver().revert(); say(`Went back${r.version ? `: the board runs ${r.version}` : ""}.`, false); });
  const cInst = button("Install the waiting image"), cCan = button("Cancel it"), cGo = button("Go on");
  act(cInst, async () => { const r = await needDriver().candidate("install"); say(`Installed${r.version ? `, now ${r.version}` : ""}${r.trial ? " on trial" : ""}.`, false); });
  act(cCan, async () => { await needDriver().candidate("cancel"); say("The waiting image is cancelled.", false); });
  act(cGo, async () => { await needDriver().candidate("continue"); say("Went on to the main menu.", false); });

  const row = (...k) => el("div", { className: "c-row" }, ...k);
  const steps = (data.flow || []).map((f) => `${f.label}: ${f.does}`);
  root.append(head, con || "", first, srv, tb || "", row(bConnect), row(bScan, netSel), row(pw, bJoin), row(url, bServer), row(upSel, bDown, bInst, bRe), row(bKeep, bBack), row(cInst, cCan, cGo), status,
    el("details", {}, el("summary", { textContent: "What each step does" }), el("ol", {}, ...steps.map((s) => el("li", { className: "small", textContent: s })))),
    note(data.measured ? `Measured: ${data.measured.note}` : ""), note(data.never || ""), el("details", { open: true }, el("summary", { textContent: "What the board printed" }), logBox));
  return { destroy: async () => { if (state.close) await state.close(); } };
}
