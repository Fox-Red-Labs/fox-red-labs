// The Digital Lab's top strip, example cards, chip view and debugger panel (concept 7, approved 2026-11-09).
// It reads what the engine already puts in the page (bench.js, ui.js, con-save.js, stage.js, recovery.js) and never touches a device:
// no flashing, no erase, no new USB calls. A value the engine cannot read shows "Not connected" or the engine's own state.
// The chip drawing is site/chip/chipview.js, imported as it is. Data: lab-chip.json (tools/webflash/build_lab.py).
import { mountChip } from "../../chip/chipview.js";

const $ = (id) => document.getElementById(id);
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
const PARAMS = new URLSearchParams(location.search);
const sel = $("board-preview");
let loaded = false, LAB = { boards: {}, photos: {} }, CATALOG = [], MAPS = {}, grouped = false;
const roots = {};            // board id -> { el, slug } : one chip view per board, built once
let shown = "";              // the board whose examples are on screen
let tab = "pins";

const connected = () => document.body.classList.contains("connected");
const boardId = () => sel.value || "";
const entry = (id) => CATALOG.find((b) => b.id === id);
const kitOf = (id) => (entry(id) || {}).kit || id;

// ---- the dropdown: every board, the ones this page can run first
function regroup() {
  if (grouped || !loaded || !sel.options.length || !CATALOG.length) return;
  grouped = true;
  const cur = sel.value, chip = (b) => !!LAB.boards[b.id], kinds = [["With a chip view", chip], ["Runs from this page", (b) => !chip(b) && b.browserFlash], ["Read only from this page", (b) => !chip(b) && b.recognized && !b.browserFlash], ["Board notes only", (b) => !chip(b) && !b.recognized]];
  sel.innerHTML = "";
  for (const [label, test] of kinds) {
    const list = CATALOG.filter(test); if (!list.length) continue;
    const g = document.createElement("optgroup"); g.label = label;
    for (const b of list) { const o = document.createElement("option"); o.value = b.id; o.textContent = b.kit; g.appendChild(o); }
    sel.appendChild(g);
  }
  sel.value = cur;
}

// ---- the strip: what is connected now
function drawStrip() {
  const on = connected(), strip = $("scene"), name = $("lab-name"), stat = $("lab-stat"), t = $("lab-stat-t"), th = $("lab-thumb");
  const kit = on ? ($("cb-kit") && $("cb-kit").textContent.trim()) || kitOf(boardId()) : "";
  name.textContent = on ? kit : "No board connected";
  stat.dataset.on = on ? "1" : "0";
  const probe = /^[A-Za-z0-9]{1,16}$/.test(document.body.dataset.probe || "") ? document.body.dataset.probe : "";
  t.textContent = on ? `Connected${probe ? ", XDS110 serial " + probe : ""}` : "Plug a board in and press Connect a board, or pick one below to look around.";
  const id = boardId(), photo = on ? LAB.photos[id] : "";
  th.hidden = !photo; if (photo) { th.src = photo; th.alt = ""; }
  strip.dataset.conn = on ? "1" : "0";
  const lab = document.querySelector(".lab-pick");
  sel.title = sel.disabled ? "Unplug the connected board to look at another one." : "";
  lab.dataset.off = sel.disabled ? "1" : "0";
  const e = entry(id), bl = $("lab-boardlink");
  bl.hidden = !(e && e.page); if (e && e.page) bl.href = "../../" + e.page;
}

// ---- examples and the chip view for the picked board
function cardsHtml(b) {
  return b.cards.map((c) => `<button type="button" class="cv-card" data-slug="${esc(c.slug)}" aria-pressed="false" aria-label="${esc(c.title)}"><span class="pic"><img src="${esc(c.img)}" alt="" width="400" height="300" loading="lazy"></span><span class="tx"><b>${esc(c.title)}</b><span class="ln">${esc(c.line)}</span><small><i${c.ran ? ' class="ran"' : ""}></i>${esc(c.status)}</small></span></button>`).join("");
}
function makeRoot(id) {
  const b = LAB.boards[id], el = $("lab-chip-tpl").content.firstElementChild.cloneNode(true);
  $("lab-chip-mount").replaceChildren(el);                         // attach first: the chip sizes itself to its box
  el.querySelector("#builds").innerHTML = cardsHtml(b);
  el.querySelector("#cv-legend").innerHTML = b.legend;
  const r = { el, slug: PARAMS.get("pick") && b.jobs.builds[PARAMS.get("pick")] ? PARAMS.get("pick") : Object.keys(b.jobs.builds)[0] };
  mountChip(el, b.jobs);
  el.querySelectorAll(".cv-card").forEach((c) => c.addEventListener("click", () => { r.slug = c.dataset.slug; drawDebug(); }));
  roots[id] = r; return r;
}
function drawExamples() {
  const id = boardId(), sec = $("lab-examples"), b = LAB.boards[id];
  if (!id) { sec.hidden = true; shown = ""; return; }
  if (!b) {   // a board with no chip view yet: say so, and the demos below still work
    sec.hidden = false; shown = ""; $("lab-chip-mount").replaceChildren();
    const hd = sec.querySelector("h2"); if (hd) hd.textContent = `Examples for the ${kitOf(id)}`;
    $("lab-ex-sub").textContent = `${kitOf(id)}. No chip view for this board yet.` + ((entry(id) || {}).page ? " Its pins and parts are on the board page." : ""); return;
  }
  sec.hidden = false;
  const on = connected();
  const hd = sec.querySelector("h2"); if (hd) hd.textContent = `Examples for the ${kitOf(id)}`;
  const note = b.jobs.chip.clock_note ? b.jobs.chip.clock_note[0].toUpperCase() + b.jobs.chip.clock_note.slice(1) + ". " : "";
  $("lab-ex-sub").textContent = `${kitOf(id)}. ${note}` + (on ? "Pick one to see it inside the chip. " : "") + " Status: Works ran on this board; Should work ran on a sister board; Could work has not been run yet; Not for this chip does not fit." + (on ? "" : " You are looking at this board without one connected. Running one needs the board plugged in; nothing is written until you do.");
  if (shown !== id) {
    const had = roots[id], r = had || makeRoot(id);
    if (had) { $("lab-chip-mount").replaceChildren(r.el); dispatchEvent(new Event("resize")); }
    shown = id;
  }
}

// ---- the debugger: pins, registers, serial, live
const cur = () => { const id = boardId(), b = LAB.boards[id], r = roots[id]; return b && r && shown === id ? { id, b, slug: r.slug, build: b.jobs.builds[r.slug] } : null; };
function drawPins() {
  const el = $("p-pins"), c = cur();
  if (c) {
    const list = c.b.pins[c.slug] || [], sigOf = (p) => (c.build.pins[p.pin] || {}).signal || "", withSig = list.some((p) => sigOf(p));
    const rows = list.map((p) => { const bl = c.b.jobs.blocks[p.block]; return `<tr><td>${esc(p.pin)}</td><td>${esc(p.where)}</td>${withSig ? `<td>${esc(sigOf(p))}</td>` : ""}<td>${esc(bl ? bl.name : p.block)}</td></tr>`; }).join("");
    el.innerHTML = `<p class="dbg-h">Pins used by ${esc(c.build.card_title || c.build.title)}</p><table class="dbg-t"><thead><tr><th>Pin</th><th>Where</th>${withSig ? "<th>Signal</th>" : ""}<th>Block</th></tr></thead><tbody>${rows}</tbody></table>`;
    return;
  }
  const po = $("pinout"), has = po && po.children.length;
  el.innerHTML = has ? `<p class="dbg-h">All pins of the chip</p>` : `<p class="empty">No pin table for this board yet.</p>`;
  if (has) { const d = document.createElement("div"); d.className = "dbg-clone pinout"; d.innerHTML = po.innerHTML; el.appendChild(d); }
}
function drawRegs() {
  const el = $("p-regs"), c = cur(), map = c && MAPS[c.id];
  if (!c || !map) { el.innerHTML = `<p class="empty">No register map for this board yet.</p>`; return; }
  const used = [];
  c.build.jobs.forEach((j) => j.chains.forEach((ch) => ch.forEach((x) => { const k = x === "i2c" ? "I2C1" : x.toUpperCase(); if (map.peripherals[k] && !used.includes(k)) used.push(k); })));
  const on = connected();
  const rows = used.map((k) => `<tr><td>${esc(k)}</td><td>0x${map.peripherals[k].base.toString(16).toUpperCase().padStart(8, "0")}</td><td class="dbg-v">${on ? "Not read on this page" : "Not connected"}</td></tr>`).join("");
  el.innerHTML = `<p class="dbg-h">Blocks this example uses</p><table class="dbg-t"><thead><tr><th>Block</th><th>Base address</th><th>Value</th></tr></thead><tbody>${rows}</tbody></table>`
    + `<p class="small">Addresses are from the chip's header file. <a href="../chip/">Inside the chip</a> is the live view: it reads these blocks on a running board without stopping it.</p>`;
}
function drawSerialState() {
  let n = $("dbg-serial-state");
  if (!n) { n = document.createElement("p"); n.id = "dbg-serial-state"; n.className = "dbg-h"; $("p-talk").prepend(n); }
  n.textContent = connected() ? "Connected. Open the serial port to read what the board prints." : "No serial port open";
}
function drawLive() {
  const tb = $("c-wt"), log = window.frlLiveLog || [];
  if (!tb) return;
  if (!log.length && !tb.children.length) {
    const tr = document.createElement("tr"), td = document.createElement("td"); td.colSpan = 3;
    td.textContent = connected() ? "Nothing read yet. Run a demo and its values appear here." : "Not connected"; tr.appendChild(td); tb.appendChild(tr); return;
  }
  if (log.length) {                                          // the engine rewrites this table itself while the tab is open
    const last = new Map(); log.forEach((e) => last.set(e.name, e)); tb.innerHTML = "";
    for (const e of last.values()) { const tr = document.createElement("tr"); for (const v of [e.name, e.what, e.value]) { const td = document.createElement("td"); td.textContent = v; tr.appendChild(td); } tb.appendChild(tr); }
  }
}
function drawTabs() {
  document.querySelectorAll("#dbg-tabs button").forEach((b) => b.setAttribute("aria-selected", String(b.dataset.dt === tab)));
  const ids = { pins: "p-pins", regs: "p-regs", con: "p-talk", live: "p-live" };
  for (const [k, id] of Object.entries(ids)) $(id).hidden = k !== tab;
}
function drawDebug() {
  drawPins(); drawRegs(); drawSerialState(); drawLive(); drawTabs();
  $("dbg-note").textContent = connected() ? "" : "Pins and the chip view need no board. Serial and live values come from a connected board. This page shows where each register is; Inside the chip reads their values.";
}
document.querySelectorAll("#dbg-tabs button").forEach((b) => b.addEventListener("click", () => { tab = b.dataset.dt; drawDebug(); }));

// ---- keep the three in step with the engine
let t = 0;
function sync() { regroup(); drawStrip(); drawExamples(); drawDebug(); }
const later = () => { clearTimeout(t); t = setTimeout(sync, 60); };
sel.addEventListener("change", later);
new MutationObserver(later).observe($("recipes"), { childList: true });
new MutationObserver(later).observe(sel, { childList: true, attributes: true, attributeFilter: ["disabled"] });
new MutationObserver(later).observe(document.body, { attributes: true, attributeFilter: ["class", "data-probe"] });
new MutationObserver(later).observe($("found"), { attributes: true, attributeFilter: ["data-state"] });

(async () => {
  try { LAB = await (await fetch("lab-chip.json")).json(); } catch (e) { /* no chip view; the rest of the page works */ }
  try { CATALOG = (await (await fetch("catalog.json")).json()).boards; } catch (e) { CATALOG = []; }
  for (const id of Object.keys(LAB.boards)) { const m = LAB.boards[id].map; if (m) { try { MAPS[id] = await (await fetch(m)).json(); } catch (e) { /* addresses stay out */ } } }
  // first look: a board with a chip view, unless the link or a connected board already chose one
  const start = LAB.boards.mspm0g3507 ? "mspm0g3507" : Object.keys(LAB.boards)[0];
  const ask = () => { if (!PARAMS.get("board") && !connected() && !sel.disabled && start && sel.value !== start && [...sel.options].some((o) => o.value === start)) { sel.value = start; sel.dispatchEvent(new Event("change")); } };
  const wait = setInterval(() => { if (sel.options.length) { clearInterval(wait); regroup(); ask(); sync(); } }, 100);
  loaded = true; sync();
  document.body.dataset.labReady = "1";
})();
