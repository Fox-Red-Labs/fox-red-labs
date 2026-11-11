// The LaunchPad bench page: connect, see the boards, pick a recipe, flash or send it, and check the result.
// The board work is in ../mspm0-dap.js; the data (board.json, recipes.json, recipes/) is written by
// tools/webflash/build_page.py from the same sources the local tool uses.
import { mountLight } from "../light/light-control.js";
import { Saci, parseHex, crc32, FAMILY, imageWithParams, putBack, readBins } from "../cc-saci.js";
import { WebHidTransport, WebUsbTransport, Dap, Session, elfSegments, sha256, sequenceCheck, FlashError, matchesIdentity, estimateSeconds } from "../mspm0-dap.js?v=20261024-link";
import { bannerFor, summaryFor, checkBackup, readOnlyReason, restoreEstimate, guardUnload, interruptedMessage } from "./recovery.js?v=20261024-unplug";
import { createTrail, diagnosticRecord, diagnosticName } from "./diagnostics.js?v=20261024";
import { listDemos, filterSort, levelName } from "./picker.js";
import { deviceError, asDeviceError, missingApi } from "./device-errors.js?v=20261024-open";
import { boardRow, listSummary, autoPick } from "./boards-list.js?v=20261022-auto";
import { tabState, changeNote, shareLink, reportUrl, aiRows, planRows, paramsClamp, paramsWords, paramsLine, aiCompare, aiCardLine, bestFirst, deeperLinks, estimateText, timeLine, usedPins, pinCells } from "./stage.js?v=20261022-ai2";
import { mountOta } from "./ota-ui.js?v=20261022-ai";
import { terminalText, terminalNote, safeSerial, REPO_WEB } from "./take-home.js?v=20261024-source";
import { packTable, decodeStatus, expectedToggle, verdict as ctlVerdict, rowsToCfg, rowC, pwmSetup, pwmMatches, pwmRegs, pwmJudge, readAnswers, PIN_SLOT, MAX_READS } from "./controls.js";
import { capPush, CON_MAX, LIVE_MAX, rememberCopy } from "./export-safe.js?v=20261025";

// WebHID where the browser has it (desktop Chrome and Edge); WebUSB otherwise (Chrome on Android), or with ?usb=1.
const USE_USB = ("usb" in navigator) && (!("hid" in navigator) || new URLSearchParams(location.search).has("usb"));
const transportFor = (d) => (USE_USB ? new WebUsbTransport(d) : new WebHidTransport(d));

const $ = (id) => document.getElementById(id);
const n = (s) => parseInt(s, 16);
const sentence = (t) => (/[.!?:]$/.test(t.trim()) ? t.trim() : t.trim() + ".");   // a message and its next step read as two sentences
const ui = { now: () => performance.now(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)) };
const COLORS = [[0, "off", "#3a332f"], [1, "red", "#e0362a"], [2, "green", "#2fbf5b"], [4, "blue", "#2f6fe0"],
  [3, "yellow", "#f2c230"], [6, "cyan", "#35c7d6"], [5, "magenta", "#c443c9"], [7, "white", "#f7f3ec"]];
const TIMES = [100, 150, 200, 250, 300, 400, 500, 750, 1000, 1500, 2000];
const svg = (d) => `<svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${d}</svg>`;
const ICONS = {
  pattern: svg('<path d="M9 18h6"/><path d="M10 22h4"/><path d="M15.09 14c.18-.98.65-1.74 1.41-2.5A4.65 4.65 0 0 0 18 8 6 6 0 0 0 6 8c0 1 .23 2.23 1.5 3.5A4.61 4.61 0 0 1 8.91 14"/>'),
  colors: svg('<circle cx="13.5" cy="6.5" r=".5"/><circle cx="17.5" cy="10.5" r=".5"/><circle cx="8.5" cy="7.5" r=".5"/><circle cx="6.5" cy="12.5" r=".5"/><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10c.93 0 1.5-.67 1.5-1.5 0-.4-.15-.74-.4-1-.24-.26-.4-.6-.4-1 0-.83.67-1.5 1.5-1.5H16c3.3 0 6-2.7 6-6 0-5-4.5-9-10-9z"/>'),
  coremark: svg('<path d="m12 14 4-4"/><path d="M3.34 19a10 10 0 1 1 17.32 0"/>'),
  uart: svg('<polyline points="4 17 10 11 4 5"/><line x1="12" y1="19" x2="20" y2="19"/>'),
  oob: svg('<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/>'),
  ble: svg('<path d="m7 7 10 10-5 5V2l5 5L7 17"/>'),
  thermo: svg('<path d="M14 4v10.54a4 4 0 1 1-4 0V4a2 2 0 0 1 4 0Z"/><path d="M12 9v6"/>'),
  button: svg('<rect x="3" y="9" width="18" height="10" rx="2"/><path d="M12 3v4"/><path d="M8 5l1.5 2"/><path d="M16 5l-1.5 2"/>'),
  breathe: svg('<path d="M2 12c2.5-6 5-6 7.5 0s5 6 7.5 0 3.5-4 5-3"/>'),
  controls: svg('<path d="M4 6h10"/><path d="M18 6h2"/><path d="M4 12h2"/><path d="M10 12h10"/><path d="M4 18h6"/><path d="M14 18h6"/><circle cx="16" cy="6" r="2"/><circle cx="8" cy="12" r="2"/><circle cx="12" cy="18" r="2"/>'),
};
const SHORT = { occ: "Count people in a room", loop: "Your first wire", digit: "Draw a digit (AI)", pattern: "Blink a pattern", colors: "Play your colors", thermo: "Live thermometer", uart: "Print a message",
  oob: "Light and temperature", coremark: "Speed test", ble: "Bluetooth light", controls: "Pin, timer, bus", button: "Button to LED", breathe: "Breathing LED" };
// What a pass on each check proves, and what it does not (reader review B10): read from what each check reads.
const PROVES = {
  pattern: ["the program on the chip drives the LED's pin in the pattern asked, timed through the debugger", "that the LED lights: the page reads the pin, so a missing jumper or a dead LED would not show"],
  colors: ["the chip plays your sequence: the page read the three LED pins and compared them with what you asked", "the colors you see, which also depend on the jumpers and the LED"],
  thermo: ["the chip's own sensor is read and converted with TI's formula", "the room's temperature: the chip warms itself, and a chip without a valid factory trim reads a few degrees off"],
  digit: ["the model runs on the chip: it read its ten built-in test digits right, then answers your drawing", "how well it reads handwriting in general"],
  occ: ["the model runs on the chip: it ran its 20 stored windows and matched the reference answers, then answers the window you pick", "how well it counts people in another room: the 20 windows come from the data it was tested on, one room over four days"],
  loop: ["the DAC's voltage reaches the ADC through your wire; the page measures gain and offset", "either one's accuracy against a meter"],
  uart: ["your message leaves the chip over its USB serial port", "anything about the other serial ports"],
  oob: ["TI's demo is on the board and answers this page", "how accurate its light and temperature sensors are"],
  coremark: ["a CoreMark run that checked its own results, with this score", "how fast your own program will run"],
  ble: ["the chip holds the light program: its own CRC check matched", "Bluetooth range or reliability"],
  button: ["the program reads the button pin and drives the LED pin: while you hold the button the page reads the button pin low and the LED pin at its lit level, and the reverse when you let go", "that the LED you see lights: the page reads the pin, so a missing jumper or a dead LED would not show"],
  breathe: ["the program changes its duty smoothly from 0 to 100 percent and back, and the LED pin is high more of the time at high duty than at low duty", "what your eye sees: the page reads the pin and the program's duty, not the light"],
  controls: ["the program applied your settings: a toggle is timed on the pin through the debugger, a bus answer is read back from the chip", "that a driven pin reaches anything beyond the header, or what a part's bytes mean"],
};
// A short path for someone learning: watch, change and compare, measure, wire. Built from the demos the board has.
const LEARN = [["pattern", "Watch it work", "send TI's blink; the page times the LED and shows what it saw."],
  ["colors", "Predict, then change", "pick colors, say what the LED will do, send, and compare Asked with Board did."],
  ["thermo", "Measure something real", "read the chip's temperature, touch the chip and watch it climb."],
  ["loop", "Wire one input", "one jumper from the DAC to the ADC; pull it out and watch the line fall apart."]];
const PIC = {
  occ: '<svg width="56" height="26" viewBox="0 0 56 26" aria-hidden="true"><path d="M2 20 L10 14 L18 17 L26 6 L34 10 L42 4 L54 9" fill="none" stroke="var(--red)" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/></svg><b class="p-guess">2</b>',
  loop: '<svg width="64" height="30" viewBox="0 0 64 30" aria-hidden="true"><rect x="2" y="9" width="12" height="12" rx="2" fill="var(--line)"/><rect x="50" y="9" width="12" height="12" rx="2" fill="var(--line)"/><path class="p-draw" d="M14 15 C 26 2, 38 28, 50 15" fill="none" stroke="var(--red)" stroke-width="3" stroke-linecap="round"/></svg>',
  digit: '<svg width="34" height="34" viewBox="0 0 34 34" aria-hidden="true"><rect x="1" y="1" width="32" height="32" rx="6" fill="#161312"/><path class="p-draw" d="M11 10 C 14 6, 23 7, 22 12 S 12 22, 11 26 L 24 26" fill="none" stroke="#F7F3EC" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/></svg><b class="p-guess">2</b>',
  pattern: '<span class="p-led blink"></span><span class="p-strip"><i style="flex:150"></i><i class="off" style="flex:150"></i><i style="flex:150"></i><i class="off" style="flex:900"></i></span>',
  colors: '<span class="p-led rgb"></span><span class="p-sw" style="background:#FF5A3C"></span><span class="p-sw" style="background:#4CC66A"></span><span class="p-sw" style="background:#4FA3FF"></span>',
  thermo: '<svg width="40" height="30" viewBox="0 0 40 30" aria-hidden="true"><path d="M5 27 A16 16 0 1 1 35 27" fill="none" stroke="var(--line)" stroke-width="5" stroke-linecap="round"/><path class="p-arc" d="M5 27 A16 16 0 1 1 35 27" fill="none" stroke="var(--red)" stroke-width="5" stroke-linecap="round"/></svg><span class="p-wave"><svg viewBox="0 0 200 26" preserveAspectRatio="none" aria-hidden="true"><path d="M0 18 C20 18 25 6 45 6 S70 20 90 20 S115 4 135 8 S160 20 180 14 S195 8 200 10 M200 18 C220 18 225 6 245 6 S270 20 290 20 S315 4 335 8 S360 20 380 14 S395 8 400 10" fill="none" stroke="var(--red)" stroke-width="2"/></svg></span>',
  uart: '<span class="p-term"><span class="p-type"><b>&gt;</b> Hello from my LaunchPad 3</span></span>',
  oob: '<svg width="22" height="30" viewBox="0 0 22 30" aria-hidden="true"><rect x="8" y="2" width="6" height="18" rx="3" fill="none" stroke="currentColor" stroke-width="1.75"/><circle cx="11" cy="23" r="4.5" fill="var(--red)"/></svg><span class="p-sun"></span>',
  coremark: '<svg width="40" height="30" viewBox="0 0 40 30" aria-hidden="true"><path d="M4 26 A16 16 0 0 1 36 26" fill="none" stroke="var(--line)" stroke-width="5" stroke-linecap="round"/><line class="p-needle" x1="20" y1="26" x2="20" y2="10" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"/></svg>',
  ble: '<svg width="30" height="30" viewBox="0 0 24 24" fill="none" stroke="var(--red)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m7 7 10 10-5 5V2l5 5L7 17"/></svg><span class="p-led pulse"></span>',
  button: '<svg width="34" height="30" viewBox="0 0 34 30" aria-hidden="true"><rect x="3" y="15" width="28" height="12" rx="3" fill="var(--line)"/><rect class="p-key" x="9" y="6" width="16" height="9" rx="3" fill="var(--red)"/></svg><span class="p-led blink"></span>',
  breathe: '<span class="p-led pulse"></span><svg width="56" height="24" viewBox="0 0 56 24" aria-hidden="true"><path d="M2 20 Q 14 -2 28 12 T 54 4" fill="none" stroke="var(--red)" stroke-width="2.5" stroke-linecap="round"/></svg>',
  controls: '<svg width="64" height="30" viewBox="0 0 64 30" aria-hidden="true"><rect x="4" y="12" width="26" height="6" rx="3" fill="var(--line)"/><circle cx="20" cy="15" r="6" fill="var(--red)"/><rect x="36" y="12" width="24" height="6" rx="3" fill="var(--line)"/><circle cx="44" cy="15" r="6" fill="var(--fg)"/></svg><span class="p-led blink"></span>',
};
const colorName = (c) => (COLORS.find((x) => x[0] === c) || [c, `color ${c}`])[1];
const colorHex = (c) => (COLORS.find((x) => x[0] === c) || [c, "", "#888"])[2];

const PARAMS = new URLSearchParams(location.search);   // ?board=<id>&recipe=<kind>: a link, or a QR or NFC sticker on the box, opens these demos
let bestData = {};
let catalogue = [], previewId = PARAMS.get("board") || "";
let data, boards = [], predicted = [], cur = null, allRecipes = [], recipes = [], chosen = null, previewChosen = null, device = null, serial = "", steps = [];
// What the page is doing with the board right now, so an unplug can say what state it left the board in (recovery.js
// interruptedMessage): idle, connecting, saving, writing, written, restoring. lost: the board went away during a step.
let phase = "idle", phaseSaved = false, lost = null;
const trail = createTrail();   // the steps of this visit, for "Download a diagnostic record" (diagnostics.js: nothing secret)
let lastResult = null;
let busy = false, writingNow = false;   // writingNow: a write or put-back is under way, so closing the tab asks first
guardUnload(window, () => writingNow);
const asWrite = (fn) => async (...a) => { writingNow = true; try { return await fn(...a); } finally { writingNow = false; } };
                 // one debugger operation at a time (Codex review 2026-10-16)
const guard = (fn) => async (...a) => { if (busy) return; busy = true; try { return await fn(...a); } finally { busy = false; } };
const known = new Map();          // HIDDevice or USBDevice -> {serial, board, what, id}
const copies = { saved: new Map(), last: null };   // board key -> {sha256, file} downloaded this visit, and the last copy (export-safe.js)
const savedSha = copies.saved;

// ---------------------------------------------------------------- data

// What the Session needs for one board and recipe (addresses from boards.json; the LED from the recipe's check).
/** The unsupported card: copy the page link, or open a mail draft with it, so a phone reader can carry the page to a computer. */
function wireUnsupportedShare() {
  const url = location.href.split("#")[0];
  const mail = $("un-mail"); if (mail) mail.href = "mailto:?subject=" + encodeURIComponent("Fox Red Labs Digital Lab") + "&body=" + encodeURIComponent("Open this on a computer in Chrome or Edge, with your board plugged in:\n" + url);
  const btn = $("un-copy"); if (!btn || btn.dataset.wired) return; btn.dataset.wired = "1";
  btn.addEventListener("click", async () => {
    let ok = false;
    try { await navigator.clipboard.writeText(url); ok = true; } catch (e) { try { const t = document.createElement("textarea"); t.value = url; document.body.appendChild(t); t.select(); ok = document.execCommand("copy"); t.remove(); } catch (e2) {} }
    $("un-note").textContent = ok ? "Link copied." : "Copy failed. Press and hold the address bar to copy it.";
  });
}
function sessionBoard(b, r) {
  const c = (r && r.check) || {};
  const led = c.led ? { gpio: n(c.led.gpio), bit: c.led.bit, label: c.led.label, activeHigh: c.led.activeHigh !== false } : { gpio: 0x400A0000, bit: 0, label: "" };
  return { kit: b.kit,
    identity: { ...b.identity, addr: n(b.identity.addr), deviceid: n(b.identity.deviceid), deviceidMask: n(b.identity.deviceidMask) },
    flash: { start: n(b.flash.start), end: n(b.flash.end) },
    led, gpio: { dout: n(b.gpio.dout), doe: n(b.gpio.doe) },
    check: { seconds: c.seconds || 4, tolMs: c.tolMs || 40 },
    ram: b.ram, ramStart: b.ramStart ? n(b.ramStart) : undefined, cmdbyten: b.cmdbyten ? n(b.cmdbyten) : undefined,
    // Cortex-M33 parts (MSPM33, AM13): their flash controller, semaphore, reset registers and flash alias
    fc: b.fc ? { base: n(b.fc.base), sector: b.fc.sector, word: b.fc.word,
      semaphore: { request: n(b.fc.semaphore.request), status: n(b.fc.semaphore.status), clear: n(b.fc.semaphore.clear), held: n(b.fc.semaphore.held) },
      protectAtRest: { a: n(b.fc.protectAtRest.a), b: n(b.fc.protectAtRest.b) } } : undefined,
    alias: b.alias ? n(b.alias) : 0, cpuss: b.cpuss === null ? null : undefined,
    reset: b.reset === "pin" || b.reset === "aircr" ? b.reset : b.reset ? { level: n(b.reset.level), cmd: n(b.reset.cmd) } : undefined };
}

async function imageOf(r) {
  const res = await fetch(r.image);
  if (!res.ok) throw new FlashError("image", `the program file is missing from this copy of the site (${res.status})`, "use a copy that includes the Digital Lab's program files");
  const buf = await res.arrayBuffer();
  if (await sha256(buf) !== r.sha256) throw new FlashError("image", "the program file does not match its checksum", "reload the page");
  return { segments: elfSegments(buf), buf };
}

// ---------------------------------------------------------------- boards

const allowedFilter = () => ({ vendorId: data.usb.vendorId, productId: data.usb.productId, usagePage: data.usb.usagePage });
// ?auto=1&probe=A,B: a regression run on a policy-granted Chrome keeps only the probes the link names where the browser shows a serial (WebUSB); WebHID shows none, so there the run limits getDevices itself.
const AUTO_PROBES = PARAMS.get("auto") === "1" && PARAMS.get("probe") ? PARAMS.get("probe").split(",").filter(Boolean) : null;
async function allowed() {
  const f = allowedFilter();
  const all = USE_USB ? (await navigator.usb.getDevices()).filter((d) => d.vendorId === f.vendorId && d.productId === f.productId)
    : (await navigator.hid.getDevices()).filter((d) => d.vendorId === f.vendorId && d.productId === f.productId
      && (d.collections || []).some((c) => c.usagePage === f.usagePage));
  return AUTO_PROBES ? all.filter((d) => !d.serialNumber || AUTO_PROBES.includes(d.serialNumber)) : all;   // WebHID does not expose a serial: a run that must not touch other probes limits getDevices itself
}

async function chooseDevice() {   // Chrome's chooser; must be the first await of a click
  // ?auto=1 (a regression run on a Chrome whose device policy already grants the XDS110): take the allowed probe the link names
  // (&probe=<serial>) with no chooser. Off unless the link says so; a reader's page never skips the chooser.
  if (PARAMS.get("auto") === "1") return autoPick(await allowed(), AUTO_PROBES ? AUTO_PROBES[0] : "");
  if (!isSecureContext) throw asDeviceError(Object.assign(new Error("insecure"), { name: "SecurityError" }), { api: USE_USB ? "usb" : "hid" });
  if (USE_USB) {
    const d = await navigator.usb.requestDevice({ filters: [{ vendorId: data.usb.vendorId, productId: data.usb.productId }] });
    return d ? [d] : [];
  }
  return navigator.hid.requestDevice({ filters: [allowedFilter()] });
}

async function drawBoards() {
  const list = $("boards"); list.innerHTML = "";
  const devs = await allowed();
  for (const d of devs) {
    const k = known.get(d) || {}, row = boardRow({ ...k, serial: safeSerial(k.serial) || safeSerial(d.serialNumber) }, d === device, statusOf);
    const li = document.createElement("li");
    if (d === device) li.className = "in-use";
    li.innerHTML = `<span class="kit"></span><span class="tag"></span><span class="sn"></span><span class="w"></span>`;
    li.querySelector(".kit").textContent = row.kit;
    li.querySelector(".tag").textContent = row.tag; li.querySelector(".tag").classList.toggle("hidden", !row.tag);
    li.querySelector(".sn").textContent = row.serial ? `XDS110 ${row.serial}` : "XDS110";
    li.querySelector(".w").textContent = row.note;
    if (row.canUse) {
      const b = document.createElement("button"); b.className = "quiet"; b.textContent = "Use this one";
      b.addEventListener("click", () => useBoard(d, k));
      li.appendChild(b);
    } else if (row.canCheck && d !== device) {
      const b = document.createElement("button"); b.className = "quiet"; b.textContent = "Check";
      b.addEventListener("click", guard(async () => { known.set(d, await identify(d, false)); await drawBoards(); }));
      li.appendChild(b);
    }
    list.appendChild(li);
  }
  $("bd-empty").textContent = listSummary(devs.length, !!device);
  $("survey").classList.toggle("hidden", devs.length < 1);
  return devs;
}

// The one word that says what this page has actually done with a board (open aperture):
// Tested here (a run on this kit is recorded), Predicted (the device tables describe it, no run yet), Recognised (named only).
function statusOf(b) {
  if (!b) return "";
  if (b.status === "predicted") return "Predicted, not tested here";
  if (b.programmable || (b.recovery && b.recovery.tested)) return "Tested here";
  return "Recognised";
}

// Which board is behind one XDS110: its serial (DAP_Info) and the first known chip ID that matches. Reads only;
// with wake = true a chip that does not answer is reset once (its program restarts; nothing is written).
async function identify(d, wake) {
  const t = transportFor(d), dap = new Dap(t);
  const out = { serial: safeSerial(d.serialNumber), board: null, what: "" };
  try {
    try { await t.open(); } catch (e) { out.openError = e; throw e; }
    out.serial = safeSerial(await dap.info(0x03).catch(() => "")) || out.serial;
    let up = true;
    try { await dap.connect(); } catch (e) { up = false; }
    if (up) {
      // a CC23xx/CC27xx whose port is already awake: TI's CFG-AP on port 1 names it before any memory read (also while
      // it waits in SACI, when its memory port on AP0 reads as 0)
      if ((await dap.apRead(1, 0xFC).catch(() => null)) === 0x002E0001) {
        const probe = { ...out }; await cfgIdentify(dap, probe, 1);
        if (probe.board) { Object.assign(out, probe); return out; }
      }
      // a TI wireless chip whose access port 0 is not a memory port (CC35xx) has its CFG-AP there
      const ap0 = await dap.apRead(0, 0xFC).catch(() => null);
      if (ap0 === null) await dap.dpWrite(0x0, 0x1E).catch(() => {});
      else if (((ap0 >>> 13) & 0xF) !== 8) { await cfgIdentify(dap, out, 0); return out; }
    } else {    // CC23xx and CC27xx start with the debug port dormant
      dap.dormant = true; dap.select = -1; dap.csw = -1;
      try { await dap.connect(); } catch (e) { out.what = "the probe answered but no chip did: check the board has power (a board on a separate probe needs its own USB cable or the probe's power jumper) and its debug jumpers are on, then press Connect again"; return out; }
      await cfgIdentify(dap, out, 1); return out;
    }
    const addrs = [...new Set(boards.filter((b) => b.identity.addr).map((b) => b.identity.addr))];
    for (let pass = 0; pass < (wake ? 2 : 1) && !out.board; pass++) {
      let anyRead = false;
      for (const a of addrs) {
        const id = await dap.read32(n(a) + 4).catch(() => null);
        if (id === null) continue;
        anyRead = true;
        out.id = id >>> 0;
        const candidates = boards.filter((b) => b.identity.addr === a && matchesIdentity({ ...b.identity, userid: undefined }, id));
        const userid = candidates.some((b) => b.identity.userid !== undefined) ? await dap.read32(n(a) + 8).catch(() => null) : null;
        out.board = candidates.find((b) => matchesIdentity(b.identity, id, userid)) || null;
        if (out.board) {
          // a predicted board must report the flash size TI's SDK gave it before anything is believed (SRAMFLASH, factory region +0x18)
          if (out.board.check && out.board.check.sramflashKb) {
            const sf = await dap.read32(n(a) + 0x18).catch(() => null);
            if (sf !== null) { out.sizeKb = sf & 0xFFF; out.sramKb = (sf >>> 16) & 0x3FF; out.sizeOk = out.sizeKb === out.board.check.sramflashKb; }
          }
          break;
        }
        out.what = `an Arm chip this page does not know (ID 0x${(id >>> 0).toString(16).toUpperCase()} at ${a})`;
      }
      if (!out.board && !anyRead && wake) { await dap.pulseReset().catch(() => {}); }
      else if (!out.board && !anyRead) out.what = "its chip did not answer (it may be asleep: Connect wakes it)";
    }
    if (out.board) out.what = `${out.board.kit} (${statusOf(out.board)}): ${out.board.note || "this page can program it"}`;
  } catch (e) {
    out.what = "could not open it (another page or program may be using it)";
  } finally {
    await dap.disconnect().catch(() => {});
    await t.close();
  }
  return out;
}

async function cfgIdentify(dap, out, ap) {   // read-only: DEVICEID from TI's configuration access port
  const id = await dap.apRead(ap, 0x0).catch(() => null);
  if (id === null) { out.what = "a TI chip whose configuration port did not answer"; return; }
  out.id = id >>> 0;
  const partid = await dap.apRead(ap, 0x4).catch(() => null);   // PARTID (targetdb CFGAP.xml); bits 23:16 are the variant TI's GEL tests
  if (partid !== null) { out.partid = partid >>> 0; out.variant = (partid >>> 16) & 0xFF; }
  out.board = boards.find((b) => b.identity.kind === "cfgap" && b.identity.ap === ap
    && b.identity.deviceids.some((d) => ((id & n(b.identity.deviceidMask)) >>> 0) === ((n(d) & n(b.identity.deviceidMask)) >>> 0))) || null;
  out.what = out.board ? `${out.board.kit} (${statusOf(out.board)}): ${out.board.note}` : `a TI chip this page does not know (DEVICEID 0x${(id >>> 0).toString(16).toUpperCase()}): recognised as TI's, named by nothing`;
}

async function survey() {
  $("survey").disabled = true;
  for (const d of await allowed()) {
    if (d === device && known.get(d) && known.get(d).board) continue;   // the connected board is already named, and a live reader may hold it
    try { known.set(d, await identify(d, false)); } catch (e) { known.set(d, { serial: safeSerial(d.serialNumber), board: null, what: "could not be read: " + (e.message || e) }); }
    await drawBoards();
  }
  $("survey").disabled = false;
}

// The resources strip: what the chip has, what the chosen demo takes, what it drives, and what an
// operating system leaves. Numbers come from boards.json (resources: data/boards.csv and the board's memory breakdown)
// and the recipe's own size and check; a board or demo without a record says so.
function drawResources() {
  const b = cur || previewBoard(), box = $("res");
  if (!b) { box.classList.add("hidden"); return; }
  const kb = (x) => (x >= 1048576 ? `${(x / 1048576).toFixed(1)} MB` : x >= 1024 ? `${(x / 1024).toFixed(x % 1024 ? 1 : 0)} kB` : `${x} B`);
  const pct = (a, t) => (t ? `${(100 * a / t).toFixed(a / t < 0.1 ? 1 : 0)} %` : "");
  const rs = b.resources || {}, flash = b.flash ? n(b.flash.end) - n(b.flash.start) : (rs.flash_kb ? rs.flash_kb * 1024 : 0), ram = b.ram || (rs.ram_kb ? rs.ram_kb * 1024 : 0);
  const r = (chosen && chosen.board === b.id ? chosen : null) || (!cur && previewChosen && previewChosen.board === b.id ? previewChosen : null);
  const builds = rs.builds || [];
  const BUILD_OF = { "blink-pattern": "blink", "coremark-O3-32": "coremark", "out-of-box": "out_of_box" };   // demos whose RAM a measured build records
  const same = r ? builds.find((x) => x.id === BUILD_OF[r.recipe]) : null;
  const fUsed = r && r.bytes ? r.bytes : 0, rUsed = same ? same.ram : 0;
  $("res-flash").textContent = flash ? (r ? `${kb(fUsed)} of ${kb(flash)}` : `${kb(flash)}`) : "unknown";
  $("res-flash-s").textContent = flash ? (r ? `${pct(fUsed, flash)} used by this demo` : "pick a demo to see what it takes") : "no record for this board";
  $("res-flash-bar").style.width = flash ? `${Math.min(100, 100 * fUsed / flash)}%` : "0";
  $("res-ram").textContent = ram ? (same ? `${kb(rUsed)} of ${kb(ram)}` : kb(ram)) : "unknown";
  $("res-ram-s").textContent = ram ? (same ? `${pct(rUsed, ram)} used by this demo` : (r ? "this demo's RAM is not measured" : "working memory")) : "no record for this board";
  $("res-ram-bar").style.width = ram && same ? `${Math.min(100, 100 * rUsed / ram)}%` : "0";
  $("res-clock").textContent = rs.clock_mhz ? `${rs.clock_mhz} MHz` : "unknown";
  const io = r ? ((r.check.led && r.check.led.label) || { colors: "the RGB LED", uart: "UART over USB", oob: "UART, LEDs, light sensor, thermistor", thermo: "the chip's temperature sensor", digit: "RAM only, read by the debugger", occ: "RAM only, read by the debugger", coremark: "RAM only, read by the debugger", ble: "the radio and an LED" }[r.check.kind] || "see the demo") : "";
  $("res-io").textContent = (io || "pick a demo").replace(/,\s+/g, "\n");   // one I/O per line $("res-io-s").textContent = r ? "what the page checks" : "";
  // what fits beside this demo: one drop-down of the builds measured on this board, the chosen one's RAM left shown as a readout
  const sel = $("res-os"); sel.innerHTML = "";
  if (!builds.length) { sel.hidden = true; $("res-os-left").textContent = ""; $("res-os-s").textContent = "not measured for this board"; }
  else {
    sel.hidden = false;
    const sorted = [...builds].sort((p, q) => (q.ram_total - q.ram) - (p.ram_total - p.ram));
    for (const x of sorted) { const o = document.createElement("option"); o.value = x.id || x.label; o.textContent = x.label; sel.appendChild(o); }
    const show = () => {
      const x = sorted[sel.selectedIndex] || sorted[0], left = Math.max(x.ram_total - x.ram, 0);
      $("res-os-left").textContent = `${kb(left)} RAM left for your code`;
      $("res-os-left").classList.toggle("tight", left < 2048);
      $("res-os-s").textContent = `flash ${kb(x.flash)} used, RAM ${kb(x.ram)} used of ${kb(x.ram_total)}; sizes measured on this board` + (left < 2048 ? "; under 2 kB left" : "");
    };
    sel.onchange = show; show();
  }
  drawPins(b, r);
  box.classList.remove("hidden");
}
let pinouts = {}, otaData = null, otaFor = "";
async function drawOta(shown) {
  const box = $("ota"), on = shown.id === "cc35x1";
  box.classList.toggle("hidden", !on);
  if (!on || otaFor === shown.id) return;
  if (!otaData) { try { otaData = await (await fetch("../cc35-ota.json", { cache: "no-store" })).json(); } catch (e) { otaData = null; } }
  if (!otaData) { box.textContent = "The update data did not load. Pull the complete repository and restart the local server."; return; }
  otaFor = shown.id; mountOta(box, otaData, shown);
}
const b0flash = () => { const bd = cur || previewBoard(); const f = bd && bd.flash ? n(bd.flash.end) - n(bd.flash.start) : 0; return f ? `${f / 1024} kB flash` : "flash"; };
const b0ram = () => { const bd = cur || previewBoard(); return bd && bd.ram ? `${bd.ram / 1024} kB RAM` : "RAM"; };
function drawPins(b, r) {
  const po = pinouts[b.id], el = $("pinout"); el.innerHTML = "";
  $("res-pins").hidden = !po;
  if (!po) return;
  const used = usedPins(r, b);
  for (const c of pinCells(po, used)) {
    const s = document.createElement("span"); s.className = c.used ? "used" : ""; s.title = c.used ? `${c.name}: used by this demo` : c.name;
    s.innerHTML = "<i></i><b></b>"; s.firstChild.textContent = c.n; s.lastChild.textContent = c.name; s.lastChild.style.fontWeight = c.used ? "700" : "400"; el.appendChild(s);
  }
  $("res-pins").querySelector("summary").textContent = `All ${po.pins.length} pins of the ${po.package} package` + (used.size ? `; this demo uses ${[...used].join(", ")}` : "");
  $("pinout-src").textContent = po.source;
}

function useBoard(d, k) {
  if (thermo.s) thermoStop();   // a live reader on the board being left lets go of it
  device = d; serial = safeSerial(k.serial); cur = k.board; previewId = cur.id;
  found(`${cur.kit}: ${statusOf(cur)}${serial ? ", on XDS110 " + serial : ""}`, cur.status === "predicted" && k.sizeOk === false ? "bad" : "on");
  $("scene-kit").textContent = cur.kit.toUpperCase();
  $("scene-chip").textContent = cur.kit.replace(/^LP-(EM-)?(MSP)?/, "");
  scene("on", [`bench · ${cur.kit}`, `XDS110 ${serial || ""}`, k.id !== undefined ? `chip 0x${k.id.toString(16).toUpperCase()} ✓` : "chip ✓", "", cur.programmable ? "ready" : "recognized"]);
  $("connect").textContent = "Connect another"; $("connect").className = "quiet";
  const kb = (x) => (x >= 1024 * 1024 ? `${x / 1024 / 1024} MB` : `${x / 1024} kB`);
  const facts = [];
  if (cur.flash) facts.push(`flash ${kb(n(cur.flash.end) - n(cur.flash.start))}`);
  if (cur.ram) facts.push(`RAM ${kb(cur.ram)}`);
  if (k.id !== undefined) facts.push(`DEVICEID 0x${k.id.toString(16).toUpperCase().padStart(8, "0")}, read from the chip`);
  if (k.partid !== undefined) facts.push(`PARTID 0x${k.partid.toString(16).toUpperCase().padStart(8, "0")} (variant 0x${k.variant.toString(16).toUpperCase().padStart(2, "0")}), read through TI's configuration port`);
  if (k.sizeKb !== undefined) facts.push(k.sizeOk ? `flash size ${k.sizeKb} kB matches the SDK` : `STOP: the chip reports ${k.sizeKb} kB of flash, the SDK says ${cur.check.sramflashKb} kB for this kit; it may be a sibling part`);
  if (cur.siblings && cur.siblings.length) facts.push(`this ID is shared with ${cur.siblings.join(", ")}`);
  if (cur.status === "predicted") facts.push(`predicted from the device tables (${cur.basis.sdk}); nothing is written to this board`);
  // open aperture: a sibling chip that shares the entry is named as itself, with what that means
  const idKey = k.id !== undefined ? `0x${k.id.toString(16).toUpperCase().padStart(8, "0")}` : "";
  const sibling = idKey && cur.identity && cur.identity.deviceidNames && cur.identity.deviceidNames[idKey];
  if (sibling) { facts.push(sibling); found(`${sibling.split(":")[0]} (${cur.kit} entry)${serial ? " on XDS110 " + serial : ""}`, "on"); }
  $("chip-facts").textContent = facts.join(" · "); $("chip-facts").classList.toggle("hidden", !facts.length);
  $("result-card").classList.remove("hidden");
  drawRecipes(); drawBoards(); drawRecovery(); drawResources(); fileChanged();
}

// The board the promises are about: the one connected, else the one a link named (?board=), else none.
// A recipe the link names, by recipe name, id or check kind (?recipe=blink-pattern, blink-150-150-150-900 or pattern).
const recipeMatches = (r, want) => !!want && [r.recipe, r.id, r.check.kind].includes(want);
// The boards that carry the demo a link asks for, in the page's own board order.
const orList = (xs) => (xs.length < 2 ? xs.join("") : xs.slice(0, -1).join(", ") + " or " + xs[xs.length - 1]);
function boardsFor(want) { return boards.filter((b) => allRecipes.some((r) => r.board === b.id && recipeMatches(r, want))); }
// The board shown before one is connected: the one a link named, else the first that has the demo the link asked for
// (a link to the Bluetooth light opens on a Bluetooth board, not on the L1306), else the first board.
function previewBoard() {
  const entry = catalogue.find((b) => b.id === previewId);
  const named = boards.find((b) => b.id === previewId) || (entry && { id: entry.id, kit: entry.kit, note: entry.note,
    recovery: { kind: "none", changes: "nothing", back: "Use the board instructions.", tested: "" } });
  return named || boardsFor(PARAMS.get("recipe"))[0] || boards[0];
}
function promiseBoard() { return cur || (previewId && previewBoard()) || (PARAMS.get("recipe") && boardsFor(PARAMS.get("recipe"))[0]) || null; }

// The banner and "Put back what was there", from the board's own recovery entry (boards.json).
function drawRecovery() {
  const b = promiseBoard(), entry = b && catalogue.find((x) => x.id === b.id);
  const ban = entry && !entry.recognized ? { tone: "neutral", lead: "Use the board instructions.", text: "This page does not connect to or write this board over USB yet." } : bannerFor(b);
  $("safe").dataset.tone = ban.tone;
  $("safe-lead").textContent = (b && !cur ? `For the ${b.kit}: ` : "") + ban.lead;
  $("safe-text").textContent = ban.text;
  const r = b && b.recovery, v = $("restore-vendor");
  if (!r) return;
  $("restore-page").classList.toggle("hidden", r.kind !== "page");
  v.classList.toggle("hidden", r.kind !== "vendor");
  $("restore-for").textContent = r.kind === "page"
    ? `On the ${b.kit} the page saves ${r.saves}. Choose that file to put it back; it goes back only onto the chip it came from. ${r.tested}`
    : r.kind === "vendor" ? `On the ${b.kit} the page saves ${r.saves}. ${r.back}`
    : `The page writes nothing to the ${b.kit}, so there is nothing to put back.`;
  if (r.kind !== "vendor") { v.innerHTML = ""; return; }
  v.innerHTML = `<p class="small"><b>${r.proven ? "Tested." : "Experimental."}</b> <span class="t"></span></p><table><tr><th>Saved by this page</th><th>Rename to</th><th>What it holds</th></tr></table><ol class="next small"></ol><p class="small">Needs <span class="tool"></span>.</p>`;
  v.querySelector(".t").textContent = r.tested;
  v.querySelector(".tool").textContent = r.tool;
  for (const row of r.files) {
    const tr = document.createElement("tr");
    row.forEach((c, i) => { const td = document.createElement("td"); if (i < 2) { const k = document.createElement("code"); k.textContent = c; td.appendChild(k); } else td.textContent = c; tr.appendChild(td); });
    v.querySelector("table").appendChild(tr);
  }
  for (const st of r.steps) { const li = document.createElement("li"); li.textContent = st; v.querySelector("ol").appendChild(li); }
  if (b.saci) {   // the radio boards: the page can put the saved files back itself (SACI write, then read the chip again and compare)
    const d = document.createElement("div"); d.id = "cc-put";
    d.innerHTML = '<p class="small"><b>Or put them back from this page.</b> Choose the .bin files this page saved from this same board (the -main.bin and -ccfg.bin, and -scfg.bin on a CC27xx). The chip is written, started and read again, and the page compares.</p><div class="row"><input id="cc-files" type="file" accept=".bin" multiple><button id="cc-put-go" class="quiet" type="button" disabled>Put back from this page</button></div><p class="small" id="cc-put-msg" role="status"></p>';
    v.appendChild(d);
    $("cc-files").addEventListener("change", () => { $("cc-put-go").disabled = !$("cc-files").files.length || !cur; });
    $("cc-put-go").addEventListener("click", guard(asWrite(ccPutBack)));
  }
}
// Put the saved .bin files back on a radio board: about 52 s measured for a CC2755P10 (docs/arrivals/cc2755p10.md).
async function ccPutBack() {
  const msg = $("cc-put-msg"), files = [...$("cc-files").files];
  const bins = {}; let t = null, dap = null, sc = null;
  try {
    if (!device || !cur || !cur.saci) throw new FlashError("no-device", "connect the board first", "then choose the files again");
    for (const f of files) { const u = new Uint8Array(await f.arrayBuffer()); if (/-main\.bin$/.test(f.name)) bins.main = u; else if (/-ccfg\.bin$/.test(f.name)) bins.ccfg = u; else if (/-scfg\.bin$/.test(f.name)) bins.scfg = u; }
    if (!bins.main || !bins.ccfg) throw new FlashError("image", "choose the -main.bin and the -ccfg.bin this page saved", "keep the names it gave them");
    estBegin(52, ["rest-est", "rest-est-bar", "rest-est-text"]);
    msg.textContent = "Writing the saved files back. Keep the cable in and this tab open.";
    t = transportFor(device); await t.open(); dap = new Dap(t); dap.dormant = true;
    const fam = FAMILY[cur.saci] || FAMILY.cc27;
    sc = new Saci(dap, () => {}, fam);
    await dap.connect(); await sc.enter();
    const res = await putBack(sc, bins, (q) => { msg.textContent = `Writing ${Math.round(q * 100)} %. Keep the cable in.`; });
    msg.textContent = res.ok ? "Put back and read back: main flash and the configuration areas match the saved files." : `Written, but the chip does not match the saved files${res.reason ? ": " + res.reason : ` (first difference: main ${res.main}, CCFG ${res.ccfg}, SCFG ${res.scfg})`}.`;
    estStop(res.ok);
  } catch (e) { msg.textContent = "Stopped: " + (e.message || e) + (e.next ? `. ${e.next}` : ""); estStop(false); }
  finally { if (sc) await sc.close().catch(() => {}); else if (dap) await dap.disconnect().catch(() => {}); if (t) await t.close().catch(() => {}); }
}

// "Go further" for the demo picked: its own source, build and one change (recipes.json "further", build_page.py).
function drawFurther(r) {
  if (!r) return;
  const f = r.further || {}, dl = $("f-facts");
  dl.innerHTML = "";
  const rows = [["Source", (f.source || []).join("; ")], ["Built with", f.built_with], ["From commit", f.commit], ["License", f.license]];
  for (const [k, v] of rows) if (v) {
    const dt = document.createElement("dt"); dt.textContent = k; const dd = document.createElement("dd");
    if (k === "Source") sourceLinks(dd, f.source); else dd.textContent = v;
    dl.append(dt, dd);
  }
  if (f.notice) { const dt = document.createElement("dt"); dt.textContent = "Notices"; const dd = document.createElement("dd"); const a = document.createElement("a"); a.href = f.notice; a.textContent = "NOTICE.txt for this image"; dd.appendChild(a); dl.append(dt, dd); }
  dl.classList.toggle("hidden", !dl.children.length);
  $("f-change").textContent = f.change ? `One change to try: ${f.change}` : "";
  $("f-change2").textContent = f.change ? `One change to try in the source: ${f.change}` : "";
  homeDemo = r; drawTerminal();
}

// A repository path opens the file in the reader copy; a description (TI's example "as copied into your project folder") stays text.
function sourceLinks(dd, files) {
  (files || []).forEach((s, i) => {
    if (i) dd.append("; ");
    if (/^[\w.-]+(\/[\w.-]+)+$/.test(s)) { const a = document.createElement("a"); a.href = `${REPO_WEB}/blob/main/${s}`; a.textContent = s; a.target = "_blank"; a.rel = "noopener"; dd.append(a); }
    else dd.append(s);
  });
}

// Copy for your terminal: the exact lines for the board and demo shown, for each computer; ui.js shows the one picked.
let homeDemo = null, kitJson = null;
function drawTerminal() {
  const pre = $("th-term-pre"); if (!pre) return;
  const board = cur || previewBoard(), r = homeDemo;
  document.body.dataset.probe = safeSerial(serial);
  const kit = kitJson && board && kitJson.boards[board.kit];
  $("th-install").textContent = kit && kit.install ? kit.install : "";
  if (!r) { pre.dataset.t = "{}"; pre.textContent = ""; return; }
  const text = {}, note = {};
  for (const os of ["mac", "win", "linux"]) {
    text[os] = terminalText({ os, title: r.title, kit: board.kit, further: r.further, serial, tools: kit && kit.tools });
    note[os] = terminalNote(r.further, os);
  }
  pre.dataset.t = JSON.stringify(text); pre.dataset.n = JSON.stringify(note);
  document.dispatchEvent(new CustomEvent("frl-terminal"));
}


// ---------------------------------------------------------------- the stage: tabs, live values, Edge AI, the plan, Share, the console
let liveVals = new Map(), aiState = {};
const stageBoard = () => cur || previewBoard();
function showTab(p) {
  document.querySelectorAll(".c-tabs [role=tab]").forEach((b) => b.setAttribute("aria-selected", String(b.dataset.p === p)));
  document.querySelectorAll(".c-pane").forEach((x) => { x.hidden = x.id !== "p-" + p; });
  if (p === "live") renderLive(); if (p === "ai") renderAi();
}
function setLive(name, what, value) { capPush((window.frlLiveLog = window.frlLiveLog || []), { t: Date.now(), name, what, value }, LIVE_MAX); liveVals.set(name, [what, value]); if (!$("p-live").hidden) renderLive(); }
function renderLive() {
  const tb = $("c-wt"); tb.innerHTML = "";
  if (!liveVals.size) { const tr = document.createElement("tr"); tr.innerHTML = "<td colspan=3></td>"; tr.firstChild.textContent = "Nothing read yet. Run the demo and its values appear here."; tb.appendChild(tr); return; }
  for (const [name, [what, value]] of liveVals) { const tr = document.createElement("tr"); tr.innerHTML = "<td></td><td></td><td class=v></td>"; tr.children[0].textContent = name; tr.children[1].textContent = what; tr.children[2].textContent = value; tb.appendChild(tr); }
}
function renderAi() {
  const b = stageBoard(), r = chosen, tb = $("c-ai-rows"); tb.innerHTML = "";
  const add = (cells, th) => { const tr = document.createElement("tr"); for (const c of cells) { const td = document.createElement(th ? "th" : "td"); td.textContent = c; tr.appendChild(td); } tb.appendChild(tr); };
  if (r && r.ai) {
    add(["", "This run, on the board in hand", "Our measured row for this board"], true);
    add(["Model", `${r.ai.name} (${r.ai.model}), ${r.ai.size}`, ""]);
    for (const row of aiCompare(r.ai, aiState)) add(row);
    return;
  }
  const flashKb = b && b.flash ? (n(b.flash.end) - n(b.flash.start)) / 1024 : 0, ramKb = b && b.ram ? b.ram / 1024 : 0;
  for (const [k, v] of aiRows({ ...aiState, bytes: r && r.bytes, flashKb, ramKb })) add([k, v, ""]);
}
function applyStage(r) {
  const st = tabState(r.check.kind, "serial" in navigator);
  for (const k of Object.keys(st)) { const t = $("t-" + k); t.hidden = !st[k].show; t.disabled = !st[k].on; }
  $("t-talk-why").hidden = st.talk.on; $("t-talk-why").textContent = st.talk.on ? "" : st.talk.why;
  $("c-change-note").textContent = changeNote(r.check.kind, r.params);
  drawDeeper(r);
  liveVals = new Map(); aiState = {}; conClose(); $("c-plan").hidden = true; $("c-menu").hidden = true;
  $("c-report").href = reportUrl(stageBoard().id, r.project, r.title);
  showTab("see");
}
document.querySelectorAll(".c-tabs [role=tab]").forEach((b) => b.addEventListener("click", () => { if (!b.disabled) showTab(b.dataset.p); }));

// Before it writes: the facts the page holds, then Write it
function showPlan() {
  const r = chosen; if (!r || !cur || $("go").disabled || readOnlyReason(cur)) return;
  const rows = planRows(cur.kit, serial, r, []), t = $("c-plan-t"); t.innerHTML = "";
  for (const [k, v] of rows) { const tr = document.createElement("tr"); tr.innerHTML = "<th></th><td></td>"; tr.children[0].textContent = k; tr.children[1].textContent = v; t.appendChild(tr); }
  $("c-plan").hidden = false; $("c-plan").scrollIntoView({ block: "nearest" });
}
$("go").addEventListener("click", showPlan);
$("c-cancel").addEventListener("click", () => { $("c-plan").hidden = true; });
$("c-write").addEventListener("click", () => { $("c-plan").hidden = true; guard(asWrite(go))(); });

// Share
let toastTimer = null;
function toast(t) { const e = $("c-toast"); e.textContent = t; e.classList.add("on"); clearTimeout(toastTimer); toastTimer = setTimeout(() => e.classList.remove("on"), 2200); }
$("c-share-btn").addEventListener("click", (e) => { e.stopPropagation(); const m = $("c-menu"); m.hidden = !m.hidden; $("c-share-btn").setAttribute("aria-expanded", String(!m.hidden)); });
document.addEventListener("click", (e) => { if (!e.target.closest(".c-share")) { $("c-menu").hidden = true; $("c-share-btn").setAttribute("aria-expanded", "false"); } });
function resultPicture() {
  const cv = document.createElement("canvas"); cv.width = 1200; cv.height = 630; const g = cv.getContext("2d");
  g.fillStyle = "#FAFAF7"; g.fillRect(0, 0, 1200, 630); g.fillStyle = "#B42418"; g.fillRect(0, 0, 1200, 14);
  g.fillStyle = "#6B625B"; g.font = "600 28px Barlow, Arial, sans-serif"; g.fillText("Fox Red Labs  ·  Digital Lab", 60, 80);
  g.fillStyle = "#161312"; g.font = "700 56px Barlow, Arial, sans-serif"; g.fillText(chosen.title.slice(0, 44), 60, 170);
  g.font = "500 32px Barlow, Arial, sans-serif"; g.fillText(stageBoard().kit, 60, 224);
  g.fillStyle = $("verdict").className.includes("ok") ? "#2F6B4F" : "#B42418"; g.font = "700 36px Barlow, Arial, sans-serif";
  const words = $("verdict").textContent.split(" "); let line = "", y = 300;
  for (const w of words) { if (g.measureText(line + w).width > 1080) { g.fillText(line, 60, y); line = ""; y += 46; } line += w + " "; }
  g.fillText(line, 60, y);
  g.fillStyle = "#161312"; let x = 60;
  for (const d of $("stats").children) { g.font = "700 48px Barlow, Arial, sans-serif"; g.fillText(d.children[0].textContent, x, 520); g.fillStyle = "#6B625B"; g.font = "500 24px Barlow, Arial, sans-serif"; g.fillText(d.children[1].textContent, x, 556); g.fillStyle = "#161312"; x += 360; }
  return cv;
}
document.querySelectorAll("[data-share]").forEach((b) => b.addEventListener("click", () => {
  $("c-menu").hidden = true;
  if (!chosen) return;
  if (b.dataset.share === "link") { navigator.clipboard.writeText(shareLink(location.origin + location.pathname, stageBoard().id, chosen.id)).then(() => toast("Link copied"), () => toast("Could not copy the link")); return; }
  const st = $("result-card").dataset.state;
  if (st !== "pass" && st !== "fail") { toast("Run the demo first: the picture shows its result"); return; }
  resultPicture().toBlob((blob) => { if (blob) { downloadBytes(`${stageBoard().id}-${chosen.id}.png`, blob, "image/png"); toast("Result picture saved to Downloads"); } });
}));

// Console: the board's serial port, for the demos that print over it
const con = { port: null, reader: null, run: false };
// Console settings (con-opts in index.html): baud rate, line ending on send, a time on each line, hold the scroll.
const conOpt = (id, dflt) => { const el = $(id); return el ? (el.type === "checkbox" ? el.checked : el.value) : dflt; };
let conAtStart = true;
function conLine(text, cls) {
  const now = Date.now();
  capPush((window.frlConLog = window.frlConLog || []), { t: now, dir: cls === "in" ? "sent" : "received", text }, CON_MAX);
  const e = $("c-con"); const sp = document.createElement("span"); if (cls) sp.className = cls;
  while (e.childElementCount >= 5000) e.firstElementChild.remove();   // the screen shows the last 5,000 pieces; Save has the session
  if (conOpt("c-con-time", false)) {
    const stamp = new Date(now).toTimeString().slice(0, 8) + "." + String(now % 1000).padStart(3, "0") + "  ";
    let out = "";
    for (const ch of text) { if (conAtStart && ch !== "\n" && ch !== "\r") { out += stamp; conAtStart = false; } out += ch; if (ch === "\n") conAtStart = true; }
    sp.textContent = out;
  } else { sp.textContent = text; conAtStart = /\n$/.test(text); }
  e.appendChild(sp); if (!conOpt("c-con-hold", false)) e.scrollTop = e.scrollHeight; if (e.textContent.length > 20000) e.firstChild.remove();
}
async function conClose() {
  con.run = false;
  try { if (con.reader) await con.reader.cancel(); } catch (e) { /* closing */ }
  con.reader = null;
  if (con.port) { await ui.sleep(30); await con.port.close().catch(() => {}); con.port = null; }
  $("c-con-open").textContent = "Open the serial port"; $("c-con-in").disabled = true; $("c-con-send").disabled = true;
}
async function conOpen() {
  if (con.port) { await conClose(); return; }
  const port = await navigator.serial.requestPort({ filters: [{ usbVendorId: data.usb.vendorId, usbProductId: data.usb.productId }] });
  const baud = Number(conOpt("c-con-baud", "")) || (chosen && chosen.check.baud) || 115200;
  await port.open({ baudRate: baud });
  conLine(`[open at ${baud} baud]\n`);
  con.port = port; con.run = true; $("c-con-open").textContent = "Close the serial port"; $("c-con-in").disabled = false; $("c-con-send").disabled = false;
  const dec = new TextDecoder();
  (async () => {
    try { while (con.run && port.readable) { con.reader = port.readable.getReader(); try { for (;;) { const r = await con.reader.read(); if (r.done) break; conLine(dec.decode(r.value, { stream: true })); } } finally { con.reader.releaseLock(); } } }
    catch (e) { conLine(`\n[stopped: ${e.message || e}]\n`); }
  })();
}
async function conSend() {
  const v = $("c-con-in").value; if (!con.port || !con.port.writable) return;
  const eol = { lf: "\n", crlf: "\r\n", cr: "\r", none: "" }[conOpt("c-con-eol", "lf")] ?? "\n";
  const w = con.port.writable.getWriter(); try { await w.write(new TextEncoder().encode(v + eol)); } finally { w.releaseLock(); }
  conLine(`> ${v}\n`, "in"); $("c-con-in").value = "";
}
$("c-con-open").addEventListener("click", () => conOpen().catch((e0) => { const e = devErr(e0, "serial"); conLine(`[${e.message || e}${e.next ? " " + e.next : ""}]\n`); if (e.fold) openEnv(e.fold); }));
$("c-con-send").addEventListener("click", () => conSend().catch(() => {}));
$("c-con-in").addEventListener("keydown", (e) => { if (e.key === "Enter") conSend().catch(() => {}); });
$("c-con-clear").addEventListener("click", () => { $("c-con").textContent = ""; window.frlConLog = []; });

// ---------------------------------------------------------------- the demo picker
let demoData = { projects: [] };
function pickerRows() {
  const shown = cur || previewBoard();
  return { shown, rows: listDemos(shown.id, allRecipes, demoData.projects, (r) => r.short || SHORT[r.check.kind] || r.title) };
}
function drawPicker() {
  const { rows } = pickerRows();
  const out = filterSort(rows, { q: $("pk-q").value, level: $("pk-level").value, where: $("pk-where").value }, $("pk-sort").value);
  $("pk-count").textContent = out.length === rows.length ? `${rows.length} ${rows.length === 1 ? "demo" : "demos"}` : `${out.length} of ${rows.length} demos`;
  const ul = $("pk-list"); ul.innerHTML = "";
  if (!out.length) { const li = document.createElement("li"); li.className = "small"; li.textContent = rows.length ? "No demo matches. Clear the search or a filter." : "No demo is recorded for this board yet."; ul.appendChild(li); return; }
  for (const d of out) {
    const li = document.createElement("li"); li.className = "pk-row";
    const tags = [levelName(d.level) && [`lv-${d.level}`, levelName(d.level)], ["", d.where === "page" ? "Runs in this page" : "Runs on your computer"], d.status && ["", d.status], d.time && ["", d.time]].filter(Boolean);
    li.innerHTML = `<b></b><div class="pk-tags"></div><p></p><span class="pk-go"></span>`;
    li.querySelector("b").textContent = d.title;
    for (const [c, t] of tags) { const sp = document.createElement("span"); if (c) sp.className = c; sp.textContent = t; li.querySelector(".pk-tags").appendChild(sp); }
    li.querySelector("p").textContent = d.what;
    const go = li.querySelector(".pk-go");
    if (d.where === "page") {
      const b = document.createElement("button"); b.type = "button"; b.textContent = "Pick this demo";
      b.addEventListener("click", () => {
        const card = [...document.querySelectorAll("#recipes .recipe")].find((x) => x.dataset.id === d.recipeId);
        $("picker").close();
        if (card && !card.disabled) { card.click(); $("pick").scrollIntoView({ block: "nearest" }); }
      });
      go.appendChild(b);
      if (d.href) { const a = document.createElement("a"); a.href = d.href; a.target = "_blank"; a.rel = "noopener"; a.textContent = "Guide"; a.style.marginLeft = "10px"; go.appendChild(a); }
    } else {
      const a = document.createElement("a"); a.href = d.href; a.target = "_blank"; a.rel = "noopener"; a.className = "btn"; a.textContent = "Open the guide";
      go.appendChild(a);
    }
    ul.appendChild(li);
  }
}
function openPicker() {
  const { shown } = pickerRows();
  $("pk-title").textContent = `All demos for the ${shown.kit}`;
  $("pk-sub").textContent = "Demos that run from this page, and projects that ran end to end on this board and run on your computer. Each project has a guide.";
  drawPicker(); $("picker").showModal(); $("pk-q").focus();
}
$("all-demos").addEventListener("click", openPicker);
$("pk-close").addEventListener("click", () => $("picker").close());
for (const id of ["pk-q", "pk-level", "pk-where", "pk-sort"]) $(id).addEventListener(id === "pk-q" ? "input" : "change", drawPicker);
$("picker").addEventListener("click", (e) => { if (e.target === $("picker")) $("picker").close(); });

// The facts beside the write button for the recipe picked.
function drawSummary(r) {
  const dl = $("r-recovery"), sm = cur && summaryFor(cur, r);
  dl.innerHTML = "";
  dl.classList.toggle("hidden", !sm || $("go").classList.contains("hidden"));
  if (!sm) return;
  dl.dataset.here = String(sm.restoreHere);
  for (const [k, v] of sm.lines) { const dt = document.createElement("dt"); dt.textContent = k; const dd = document.createElement("dd"); dd.textContent = v; dl.append(dt, dd); }
}

// A file chosen for "Put it back": checked here, before the button is offered; the chip is checked again on connect.
let fileOk = null;
async function fileChanged() {
  const f = $("file").files[0], out = $("file-check");
  fileOk = null; $("restore").disabled = true;
  if (!f) { out.textContent = ""; return; }
  const res = await checkBackup(await f.text(), f.name, cur, sha256);
  if (f !== $("file").files[0]) return;   // another file was chosen meanwhile
  out.textContent = res.message; out.className = "small " + (res.ok ? "" : "bad");
  if (res.ok) { fileOk = res.copy; $("restore").disabled = false; out.textContent += " " + restoreEstimate(res.copy.bytes).text; }
}

// Ask Chrome for an XDS110 (the first await of a click), then identify it, and the others already allowed.
async function connectBoard() {
  const devs = await chooseDevice();
  if (!devs.length) throw new FlashError("no-device", "no device was chosen", "press the button again and pick an XDS110");
  // Choosing one probe never authorizes selecting another, even if that other probe is known.
  clearConnection();
  const d = devs[0], k = await identify(d, true);
  known.set(d, k);
  trail.add("identified", { kit: k.board ? k.board.kit : null, id: k.id !== undefined ? "0x" + k.id.toString(16).toUpperCase() : null, sizeOk: k.sizeOk, serial: k.serial, what: k.board ? "" : k.what });
  if (!k.board && k.openError) throw k.openError;   // the probe could not be opened at all (busy, or Linux permissions): say that, not "unknown board"
  if (!k.board) { await drawBoards(); throw new FlashError("wrong-chip", k.what || "that XDS110 is not on a board this page knows", "nothing was written. Choose the board's printed name below for its board notes, or Connect again to choose another probe."); }
  useBoard(d, k);
}

// A Session on the connected board, for one recipe.
async function openBoard(r, say) {
  if (!device || !cur) throw new FlashError("no-device", "connect a board first (step 1)", "then press this button again");
  if (r && readOnlyReason(cur)) throw new FlashError("wrong-chip", `this page does not write demos to the ${cur.kit} yet: ${readOnlyReason(cur)}`, "nothing was written");
  if (!cur.programmable) throw new FlashError("wrong-chip", `this page recognizes the ${cur.kit} but cannot program it yet`, "nothing was written");
  const s = new Session(transportFor(device), sessionBoard(cur, r), { ...ui, say });
  // every write path goes through one of these, whichever demo kind called it; on failure the phase stays where it stopped
  for (const [obj, m, ph, after] of [[s, "save", "saving", "connected"], [s, "flashImage", "writing", "written"], [s, "sendColors", "writing", "written"],
    [s, "restore", "restoring", "written"], [s.m, "writeSector", "writing", "written"]]) {
    const f = obj[m].bind(obj);
    obj[m] = async (...a) => { phase = ph; trail.add(ph); const v = await f(...a); if (ph === "saving") { phaseSaved = true; trail.add("saved", { bytes: v && v.bytes, sha256: v && v.sha256 && v.sha256.slice(0, 12) }); } phase = after; return v; };
  }
  phase = "connecting";
  try { const id = await s.start(); trail.add("session", { deviceid: id.deviceid, traceid: id.traceid }); return { s, id }; } catch (e) { await s.end().catch(() => {}); throw e; }
}

function clearConnection(keepResult = false) {
  if (thermo.s) thermoStop();
  device = null; cur = null; chosen = null;
  $("go").disabled = true; if (!keepResult) $("result-card").classList.add("hidden");
  drawRecipes(); drawRecovery(); fileChanged();
  if (keepResult) $("detail").classList.remove("hidden");   // an unplug part way: the result and its recovery steps stay on screen
}

function drawCatalog() {
  const select = $("board-preview");
  if (!select.options.length) for (const b of catalogue) {
    const o = document.createElement("option"); o.value = b.id;
    o.textContent = `${b.kit}: ${b.status}`; select.appendChild(o);
  }
  const shown = cur || previewBoard(), entry = catalogue.find((b) => b.id === shown.id);
  select.value = shown.id; select.disabled = !!device;
  $("board-coverage").textContent = `${catalogue.length} boards listed; ${boards.length - predicted.length} identified over USB by a run here and ${predicted.length} more predicted from the device tables (named and read, never written); ${catalogue.filter((b) => b.browserFlash).length} have browser demos that write firmware. Other boards use their existing instructions.`;
  const link = $("board-instructions");
  if (entry) {
    link.href = entry.page ? `../../${entry.page}` : location.pathname.startsWith("/site/") ? `../../../${entry.readme}` : `https://github.com/Fox-Red-Labs/fox-red-labs/blob/main/${entry.readme}`;
    link.textContent = `${entry.kit}: board notes and instructions`;
  }
}
$("board-preview").addEventListener("change", () => {
  if (device) return;
  previewId = $("board-preview").value;
  drawRecipes(); drawRecovery(); drawResources();
});

// ---------------------------------------------------------------- recipes

function drawRecipes() {
  drawCatalog(); drawResources();
  const g = $("recipes"); g.innerHTML = "";
  const shown = cur || previewBoard();
  recipes = allRecipes.filter((r) => r.board === shown.id);
  drawOta(shown);
  const unknown = !cur && previewId && !catalogue.some((b) => b.id === previewId);
  $("recipes-note").textContent = cur ? `${recipes.length} ${recipes.length === 1 ? "thing" : "things"} to try on the ${cur.kit}`
    : (unknown ? `This page does not know a board called \u201c${PARAMS.get("board")}\u201d. ` : "") + `Previewing the ${shown.kit}. Choosing a preview never selects a device for writing.`
      + ((!PARAMS.get("board") || unknown) && boardsFor(PARAMS.get("recipe")).length ? ` This demo runs on the ${orList(boardsFor(PARAMS.get("recipe")).map((b) => b.kit))}.` : "");
  if (shown.note) { const p = document.createElement("p"); p.className = "small"; p.textContent = shown.note; g.appendChild(p); }
  const needsSerial = (r) => ["uart", "oob"].includes(r.check.kind) && !("serial" in navigator);   // Chrome on Android has no Web Serial
  const ag = $("ai-models"); ag.innerHTML = "";
  const ordered = bestFirst(shown.id, recipes, bestData);
  const card = (r) => {
    const b = document.createElement("button");
    b.className = "recipe"; b.setAttribute("aria-pressed", "false"); b.dataset.kind = r.check.kind; b.dataset.recipe = r.recipe; b.dataset.id = r.id; b.title = r.title;
    if (r.best) b.dataset.best = "1";
    b.innerHTML = `<span class="pic">${PIC[r.check.kind] || ICONS[r.check.kind] || ""}</span><span class="t"></span><span class="s"></span>`;
    b.querySelector(".t").textContent = r.short || SHORT[r.check.kind] || r.title;
    b.querySelector(".s").textContent = needsSerial(r) ? "Needs a computer: it reads the board's serial port, which this browser cannot open." : r.see;
    if (r.ai) { const l = document.createElement("span"); l.className = "ai-l"; l.textContent = aiCardLine(r.ai); b.appendChild(l); }
    if (needsSerial(r)) { b.disabled = true; return b; }
    b.addEventListener("click", () => choose(r, b));
    return b;
  };
  const plain = ordered.filter((r) => !r.ai);
  for (const r of [...plain.filter((r) => !needsSerial(r)), ...plain.filter(needsSerial)]) g.appendChild(card(r));
  // one card per family with a size choice, small to large; the numbers beside it follow the size picked
  const FAMILIES = [["Draw a digit", ["digit-1k", "digit-12k", "digit-cnn_16small", "digit"], { "digit-1k": "1k", "digit-12k": "12k", "digit-cnn_16small": "16-wide CNN", digit: "64-wide CNN" }],
    ["Room occupancy", ["occ-500", "occ-2k", "occ-8k", "occ-30k"], { "occ-500": "0.6k", "occ-2k": "1.7k", "occ-8k": "8.5k", "occ-30k": "32.8k" }]];
  const kbOf = (v) => (v >= 1024 ? `${(v / 1024).toFixed(1)} kB` : `${v} B`);
  for (const [name, ids, labels] of FAMILIES) {
    const rs = ids.map((id) => ordered.find((r) => r.id === id)).filter(Boolean);
    if (!rs.length) continue;
    const card2 = document.createElement("div"); card2.className = "ai-card";
    const h = document.createElement("b"); h.textContent = name; card2.appendChild(h);
    const row = document.createElement("div"); row.className = "ai-sizes"; row.setAttribute("role", "group"); row.setAttribute("aria-label", `${name}: model size`);
    const fit = document.createElement("p"); fit.className = "small ai-fit";
    const say = (r) => {
      const m = r.ai.measured || {}, c = (m.cycles || [])[0], t = m.ms;
      fit.textContent = `${r.ai.name}: ${r.ai.size}. ` + (c ? `${c.ms} ms at ${c.clockMhz} MHz measured. ` : t ? `${t.value} ms at ${t.clockMhz} MHz measured. ` : "Not measured on this board yet. ")
        + `Image ${kbOf(r.ai.image.flash_b)} flash, ${kbOf(r.ai.image.ram_b)} RAM of this chip's ${b0flash()} and ${b0ram()}.`;
    };
    for (const r of rs) {
      const bt = card(r); bt.className = "recipe ai-size"; bt.innerHTML = ""; bt.textContent = labels[r.id] || r.short;
      bt.addEventListener("click", () => say(r)); bt.addEventListener("mouseenter", () => { if (bt.getAttribute("aria-pressed") !== "true") say(r); });
      row.appendChild(bt);
    }
    card2.append(row, fit); say(rs[0]); ag.appendChild(card2);
  }
  $("ai-pick").classList.toggle("hidden", !ag.children.length);
  if (ag.children.length) {   // one card in the grid that leads to the picker, beside the best demos
    const pk = document.createElement("button"); pk.className = "recipe"; pk.type = "button"; pk.dataset.aiPicker = "1";
    pk.innerHTML = `<span class="pic">${PIC.digit}</span><span class="t">Pick an AI model</span><span class="ai-l">${ag.children.length} models to flash and run on this board</span>`;
    pk.addEventListener("click", () => { const f = ag.querySelector(".recipe:not(:disabled)"); if (f) { f.click(); showTab("ai"); } });
    g.insertBefore(pk, g.children[Math.min(2, g.children.length)] || null);
  }
  const both = () => [...g.querySelectorAll(".recipe:not([data-ai-picker])"), ...ag.querySelectorAll(".recipe")];
  // ?recipe= names a demo by its recipe name (blink-pattern), its id (blink-150-150-150-900) or its check kind (pattern)
  const want = PARAMS.get("recipe");
  const asked = want && both().filter((x) => !x.disabled).find((x) => [x.dataset.recipe, x.dataset.id, x.dataset.kind].includes(want));
  const lp = $("learn"), steps = LEARN.filter(([k]) => g.querySelector(`.recipe[data-kind="${k}"]:not(:disabled)`));
  lp.innerHTML = ""; lp.classList.toggle("hidden", steps.length < 2);
  steps.forEach(([k, head, what]) => {
    const li = document.createElement("li"), b = document.createElement("button");
    b.type = "button"; b.className = "linkish"; b.innerHTML = "<b></b>: "; b.querySelector("b").textContent = head; b.append(what);
    b.addEventListener("click", () => { const t = g.querySelector(`.recipe[data-kind="${k}"]`); if (cur) t.click(); else { g.querySelectorAll(".recipe").forEach((x) => x.setAttribute("aria-pressed", String(x === t))); drawFurther(recipes.find((r) => r.id === t.dataset.id)); } t.scrollIntoView({ block: "nearest" }); });
    li.appendChild(b); lp.appendChild(li);
  });
  const topId = (ordered.find((r) => !needsSerial(r)) || {}).id;
  const first = asked || both().find((x) => x.dataset.id === topId && !x.disabled) || g.querySelector(".recipe:not(:disabled)");
  if (first && cur) first.click();
  else {
    $("detail").classList.add("hidden"); $("about").textContent = "";
    const pre = asked || first;
    if (pre) drawFurther(recipes.find((r) => r.id === pre.dataset.id));
    if (asked) { asked.setAttribute("aria-pressed", "true"); $("recipes-note").textContent += ` Selected demo: ${asked.querySelector(".t").textContent}.`; }
    else if (want) $("recipes-note").textContent += ` No demo called \u201c${want}\u201d on this board.` + (boardsFor(want).length ? ` It runs on the ${orList(boardsFor(want).map((b) => b.kit))}.` : "");
    previewChosen = asked ? recipes.find((r) => r.id === asked.dataset.id) || null : null;
    drawResources();
  }
}

function choose(r, btn) {
  if (r.check.kind !== "oob") oobClose();   // the serial port is free for the other recipes
  thermoStop();
  chosen = r;
  window.frlChosen = r ? { id: r.id, board: r.board, image: r.image, bytes: r.bytes, sha256: r.sha256 } : null;   // own-tools.js
  drawResources();
  document.querySelectorAll(".recipe").forEach((x) => x.setAttribute("aria-pressed", String(x === btn)));
  $("detail").classList.remove("hidden"); $("log").innerHTML = ""; resultState("");
  $("r-title").textContent = r.title; $("r-outcome").textContent = r.outcome; $("r-see").textContent = r.see;
  const pv = PROVES[r.check.kind];
  $("r-proves").textContent = pv ? `A pass proves ${pv[0]}. It does not prove ${pv[1]}.` : "";
  $("boards-note").classList.toggle("hidden", !!cur);
  $("r-before").textContent = r.before ? (r.check.kind === "ble" ? r.before : "Nothing to change if the board is as it came. For reference: " + r.before) : "";
  const w = $("r-wiring");
  w.classList.toggle("hidden", !r.wiring);
  if (r.wiring) {
    w.innerHTML = `<p><b>Wiring</b></p><img alt="" style="max-width:100%;border-radius:8px"><table><tr><th>Wire from</th><th>To</th><th>What it is</th><th>Done</th></tr></table>`;
    w.querySelector("img").src = "../../" + r.wiring.image; w.querySelector("img").alt = r.wiring.alt || "";
    for (const [a, bpin, what] of r.wiring.pins) {
      const tr = document.createElement("tr");
      tr.innerHTML = "<td></td><td></td><td></td><td><input type=checkbox></td>";
      tr.children[0].textContent = a; tr.children[1].textContent = bpin; tr.children[2].textContent = what;
      w.querySelector("table").appendChild(tr);
    }
  }
  $("editor").classList.toggle("hidden", r.check.kind !== "colors");
  $("controls").classList.toggle("hidden", r.check.kind !== "controls");
  if (r.check.kind === "controls") ctlInit(r);
  $("live").classList.toggle("hidden", r.check.kind !== "oob");
  if (r.check.kind === "oob") { $("oob-value").textContent = ""; $("oob-note").textContent = "Press Light sensor or Thermometer. Chrome asks for a serial port the first time" + (serial ? `: pick the one ending in ${serial}1.` : ": pick the XDS110 one whose name ends in 1."); }
  $("go").disabled = !cur || !!readOnlyReason(cur);
  $("go").classList.toggle("hidden", r.check.kind === "ble" && !(r.hex && cur && cur.saci));
  $("ble").classList.toggle("hidden", r.check.kind !== "ble");
  $("thermo").classList.toggle("hidden", r.check.kind !== "thermo");
  $("watch").classList.toggle("hidden", r.check.kind !== "button" && r.check.kind !== "breathe");
  if (r.check.kind === "button" || r.check.kind === "breathe") watchReset(r);
  $("digit").classList.toggle("hidden", r.check.kind !== "digit");
  $("occ").classList.toggle("hidden", r.check.kind !== "occ");
  if (r.check.kind === "occ") occInit(r);
  drawParams(r);
  $("loop").classList.toggle("hidden", r.check.kind !== "loop");
  if (r.check.kind === "loop") { $("lp-wire").textContent = `The wire: ${r.check.from} to ${r.check.to}.`; $("lp-line").setAttribute("points", ""); }
  if (r.check.kind === "digit") { dg.live = false; dgClear(); $("dg-time").textContent = "Start the digit reader, then draw one digit."; dgLock(); }
  if (r.check.kind === "thermo") { thermoDraw(null); $("th-note").textContent = "Press the button, then touch the chip with a finger."; }
  if (r.check.kind === "ble" && !$("ble").dataset.mounted) { mountLight($("ble")); $("ble").dataset.mounted = "1"; }
  if (r.check.kind === "colors") { steps = r.presets[0].steps.map((x) => [...x]); drawPresets(r); drawSteps(); }
  goLabel = { loop: "Start the sweep", digit: "Start the digit reader", occ: "Start the room model", colors: "Send to the board", controls: "Send to the board", coremark: "Run the speed test",
    thermo: "Start the thermometer", oob: "Put TI's demo on the board", ble: "Put the light program on", uart: "Flash it and read the message" }[r.check.kind] || "Flash it to the board";
  $("go").textContent = goLabel;
  $("go").title = goLabel; chipsReset(r.check.kind);
  $("go-note").textContent = readOnlyReason(cur) ? `Read only for now: ${readOnlyReason(cur)}` : !cur ? "Connect a board first." : !cur.recovery ? "" : cur.recovery.kind === "page" ? "Saves a copy first; this page can put it back."
    : cur.recovery.kind === "vendor" ? "Saves a copy first; putting it back needs TI's UniFlash." : "";
  $("r-writes").textContent = r.check.kind === "colors" || r.check.kind === "controls" ? "The first send saves the board's flash to your Downloads; later sends rewrite only your settings (one 1 kB sector)."
    : r.check.kind === "ble" ? "Only if the board holds something else: erases its main flash (TI's security firmware stays), writes the program and both configuration areas. It saves the main flash first as a .bin; this page cannot put that back (TI's UniFlash can)."
    : `It saves the board's flash to your Downloads, then erases only the sectors the ${r.bytes ? r.bytes.toLocaleString() + "-byte " : ""}program needs. It never writes the boot settings.`;
  $("go-note").classList.toggle("hidden", $("go").classList.contains("hidden"));
  drawSummary(r);
  drawFurther(r);
  applyStage(r);
  $("about").textContent = r.check.kind === "ble" ? `${r.source.replace(/\.$/, "")}. ` + (r.hex && cur && cur.saci
      ? "The button writes the program through the chip's own programming mode, only if the board holds something else, and saves the board's flash first. The color and pattern buttons only talk over Bluetooth."
      : "The page only talks to the board over Bluetooth; it writes nothing to it.")
    : `${(r.source || "").replace(/\.$/, "")}. ${r.sha256 ? "Program sha256 " + r.sha256.slice(0, 16) + "…, " : ""}${r.bytes ? r.bytes + " bytes. " : ""}The page writes main flash only, never the boot configuration, and erases only the sectors it needs.`;
}

// Take the compared points away (System Lab step 6): CSV, or a workbook built in the page by the labs' xlsx writer.
let overlayTake = null;
function downloadBytes(name, bytes, type) {
  const a = document.createElement("a"); a.href = URL.createObjectURL(new Blob([bytes], { type })); a.download = name; a.click(); setTimeout(() => URL.revokeObjectURL(a.href), 2000);
}
$("ov-csv").addEventListener("click", () => { if (overlayTake) downloadBytes(overlayTake.name + ".csv", window.LivePanel.csv(overlayTake.cmp, overlayTake.opt), "text/csv"); });
$("ov-xlsx").addEventListener("click", () => { if (overlayTake && window.XLSX_FRL) downloadBytes(overlayTake.name + ".xlsx", window.LivePanel.workbook(overlayTake.cmp, overlayTake.opt, window.XLSX_FRL), "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"); });

// The no-code controls card (review item 2): a short list of settings, each with the line of C it stands for and
// one verdict line, the way the motor console reads. A plus adds one I/O, a minus removes it; a plugged-in BoosterPack
// brings its own rows. The board drawing lights the chosen pins with their setup.
let ctlSeq = 0, ctRows = [], ctId = 0, ctPacks = [], ctProven = { rows: {} }, ctPos = null, ctCur = null, ctLast = "";
const CT_KINDS = [["pin", "Pin: read it, or drive it low or high"], ["toggle", "Timer: toggle a pin on and off"], ["pwm", "PWM: a pin from a timer, frequency and duty"],
  ["scan", "Bus: find every part that answers"], ["read", "Bus: read n bytes at an address and register"]];
const CT_NAME = { pin: "Pin", toggle: "Timer", pwm: "PWM", scan: "Scan", read: "Read" };
const ctHex = (x) => (Number.isFinite(x) ? "0x" + x.toString(16).toUpperCase() : "");
const ctParse = (t) => { const v = /^\s*0x/i.test(t) ? parseInt(t, 16) : parseInt(t, 10); return Number.isFinite(v) ? v : NaN; };
function ctNew(kind) {
  const base = { id: ++ctId, kind };
  if (kind === "pin") return { ...base, pin: 26, mode: "high" };
  if (kind === "toggle") return { ...base, pin: 26, on: 200, off: 300 };
  if (kind === "pwm") return { ...base, pin: 26, hz: 1000, duty: 50 };
  if (kind === "read") return { ...base, addr: 0x44, reg: 0x7E, n: 2 };
  return base;
}
function ctSlotTaken() { return ctRows.some((x) => PIN_SLOT.includes(x.kind)); }
function ctProv(row) {
  if (row.pack) return { text: "Predicted, not tested here", tested: false };
  const t = ctProven.rows[row.kind === "pin" ? "pin-" + (row.mode || "read") : row.kind];
  return t ? { text: t.label || "Tested here", tested: true, title: t.note || "" } : { text: "Predicted, not tested here", tested: false };
}
function ctRowEl(row, r) {
  const el = document.createElement("div"); el.className = "ct-row"; el.dataset.id = row.id;
  const pinSel = (pwmOnly) => `<label class="small">pin <select data-f="pin">${r.pins.filter((p) => !pwmOnly || p.pwm).map((p) => `<option value="${p.pin}"${p.pin === row.pin ? " selected" : ""}>${p.name}: ${p.role}</option>`).join("")}</select></label>`;
  const num = (f, label, min, max, step) => `<label class="small">${label} <input type="number" data-f="${f}" value="${row[f]}" min="${min}" max="${max}" step="${step}"></label>`;
  let h = `<div class="rh"><b>${CT_NAME[row.kind]}</b>`;
  if (row.kind === "pin") h += pinSel(false) + `<label class="small"><select data-f="mode">${["read", "low", "high"].map((m) => `<option value="${m}"${m === row.mode ? " selected" : ""}>${m === "read" ? "read it" : "drive " + m}</option>`).join("")}</select></label>`;
  else if (row.kind === "toggle") h += pinSel(false) + num("on", "on, ms", 5, 10000, 5) + num("off", "off, ms", 5, 10000, 5);
  else if (row.kind === "pwm") h += pinSel(true) + num("hz", "Hz", 100, 20000, 100) + num("duty", "duty %", 0, 100, 5);
  else if (row.kind === "scan") h += `<span class="small">every address from 0x08 to 0x77 (I2C0, PA0 SDA and PA1 SCL, 400 kHz)</span>`;
  else h += `<label class="small">address <input type="text" data-f="addr" value="${ctHex(row.addr)}" size="5"></label><label class="small">register <input type="text" data-f="reg" value="${ctHex(row.reg)}" size="5" placeholder="none"></label>` + num("n", "bytes", 1, 8, 1);
  h += `<span class="ct-prov"></span><button type="button" class="rm" aria-label="Remove this row" title="Remove this row">−</button></div>`;
  if (row.why) h += `<p class="small why"></p>`;
  h += `<pre class="mono"></pre>`;
  el.innerHTML = h;
  if (row.why) el.querySelector(".why").textContent = row.why + (row.expect ? `. Expected: ${row.expect}.` : ".");
  el.querySelectorAll("[data-f]").forEach((inp) => inp.addEventListener("input", () => {
    const f = inp.dataset.f;
    row[f] = f === "mode" ? inp.value : f === "addr" || f === "reg" ? (inp.value.trim() === "" ? "" : ctParse(inp.value)) : Number(inp.value);
    ctUpdate(r);
  }));
  el.querySelector(".rm").addEventListener("click", () => { ctRows = ctRows.filter((x) => x !== row); ctDraw(r); });
  return el;
}
function ctMenu(r) {
  const m = $("ct-menu"); m.innerHTML = "";
  for (const [kind, text] of CT_KINDS) {
    const b = document.createElement("button"); b.type = "button"; b.textContent = "+ " + text;
    const why = PIN_SLOT.includes(kind) && ctSlotTaken() ? "The program drives one pin at a time: remove the pin row first."
      : kind === "scan" && ctRows.some((x) => x.kind === "scan") ? "Already listed." : kind === "read" && ctRows.filter((x) => x.kind === "read").length >= MAX_READS ? `The program holds ${MAX_READS} register reads.` : "";
    b.disabled = !!why; b.title = why;
    b.addEventListener("click", () => { ctRows.push(ctNew(kind)); $("ct-plus").open = false; ctDraw(r); });
    m.appendChild(b);
  }
}
function ctBoard(r) {
  const bd = $("ct-bd"); if (!ctPos) { bd.innerHTML = ""; return; }
  const c = ctPos.crop, W = ctPos.image_size[0], H = ctPos.image_size[1];
  bd.style.backgroundImage = "url(boards/" + ctPos.image + ")"; bd.style.backgroundSize = `${(W / c.w) * 100}% auto`;
  bd.style.backgroundPosition = `${(c.x / (W - c.w)) * 100}% ${(c.y / (H - c.h)) * 100}%`;
  bd.innerHTML = "";
  const mark = (header, text, cls) => {
    const xy = ctPos.positions[header]; if (!xy) return;
    const fx = ((xy[0] - c.x) / c.w) * 100, fy = ((xy[1] - c.y) / c.h) * 100;
    const m = document.createElement("i"); m.className = "ct-mk " + (cls || ""); m.style.left = fx + "%"; m.style.top = fy + "%"; bd.appendChild(m);
    const l = document.createElement("span"); l.className = "ct-lb"; l.textContent = text; l.style.top = fy + "%";
    if (fx > 50) l.style.right = (100 - fx + 3.4) + "%"; else l.style.left = (fx + 3.4) + "%";
    bd.appendChild(l);
  };
  const setup = (x) => x.kind === "pin" ? (x.mode === "read" ? "read" : "driven " + x.mode) : x.kind === "toggle" ? `toggle ${x.on} / ${x.off} ms` : `PWM ${x.hz} Hz, ${x.duty}%`;
  for (const x of ctRows.filter((y) => PIN_SLOT.includes(y.kind))) {
    const p = r.pins.find((q) => q.pin === x.pin); if (p) mark(p.header, `${p.name} ${p.role}: ${setup(x)}`);
  }
  if (ctRows.some((x) => x.kind === "scan" || x.kind === "read")) { mark("J1 pin 10", "SDA (PA0): I2C0 400 kHz", "bus"); mark("J1 pin 9", "SCL (PA1)", "bus"); }
  const names = ctRows.filter((x) => PIN_SLOT.includes(x.kind)).map((x) => (r.pins.find((q) => q.pin === x.pin) || {}).header).filter(Boolean);
  $("ct-bd-cap").textContent = (names.length ? `Lit: ${names.join(", ")}. ` : "") + "Positions measured on the drawing (boards/mspm0l1306.pins.json) from TI's SLAU869E figure 2-9.";
}
function ctUpdate(r) {
  const { cfg, problems } = rowsToCfg(ctRows);
  for (const el of $("ct-rows").children) {
    const row = ctRows.find((x) => String(x.id) === el.dataset.id); if (!row) continue;
    let c = ""; try { c = rowC(row, r.pins); } catch (e) { c = "(fill in every field)"; }
    el.querySelector("pre").textContent = c;
    const pv = ctProv(row), sp = el.querySelector(".ct-prov"); sp.textContent = pv.text; sp.title = pv.title || ""; sp.classList.toggle("tested", pv.tested);
  }
  const reads = ctRows.filter((x) => x.kind === "read").length;
  $("ct-limit").textContent = problems.length ? problems[0] : ctRows.length ? `${reads} of ${MAX_READS} register reads; ${ctSlotTaken() ? "the one pin slot is used" : "the pin slot is free"}. Send writes the whole list to the board at once.` : "Nothing listed: press + Add.";
  $("ct-verdict").textContent = ctLast;
  $("go").disabled = !cur || !ctRows.length || problems.length > 0;
  ctMenu(r); ctBoard(r);
}
function ctDraw(r) {
  const box = $("ct-rows"); box.innerHTML = "";
  for (const row of ctRows) box.appendChild(ctRowEl(row, r));
  ctUpdate(r);
}
function ctPick(r, id) {
  ctRows = ctRows.filter((x) => !x.pack);
  const pack = ctPacks.find((x) => x.id === id);
  $("ct-bp-note").textContent = "";
  if (pack) {
    const skipped = [];
    for (const pr of pack.rows) {
      if (PIN_SLOT.includes(pr.kind) && ctSlotTaken()) { skipped.push(pr.why); continue; }
      if (pr.kind === "scan" && ctRows.some((x) => x.kind === "scan")) continue;
      if (pr.kind === "read" && ctRows.filter((x) => x.kind === "read").length >= MAX_READS) continue;
      ctRows.push({ ...ctNew(pr.kind), ...pr, pack: pack.id, id: ++ctId });
    }
    $("ct-bp-note").textContent = `${pack.note} On the bench: ${pack.bench.replace(/^on the bench,\s*/i, "").split(":")[0]}. ${pack.fit}${skipped.length ? " Left out because the pin slot is in use: " + skipped.join("; ") + "." : ""}`;
  }
  ctDraw(r);
}
async function ctInit(r) {
  if (!ctCur || ctCur !== r.id) {
    ctCur = r.id; ctLast = ""; ctRows = [Object.assign(ctNew("toggle"), { pin: 26, on: 200, off: 300 })];
    try { ctPos = await (await fetch("boards/mspm0l1306.pins.json")).json(); } catch (e) { ctPos = null; }
    try { ctPacks = (await (await fetch("boosterpacks.json")).json()).packs; } catch (e) { ctPacks = []; }
    try { ctProven = await (await fetch("controls-proven.json")).json(); } catch (e) { ctProven = { rows: {} }; }
    $("ct-bp").innerHTML = `<option value="">none</option>` + ctPacks.map((p) => `<option value="${p.id}">${p.name}</option>`).join("");
    $("ct-bp").onchange = () => ctPick(r, $("ct-bp").value);
  }
  ctDraw(r);
}
function ctlRead(r) {
  const { cfg, problems } = rowsToCfg(ctRows);
  if (problems.length) throw new FlashError("other", problems[0], "Remove a row and send again.");
  const p = r.pins.find((x) => x.pin === cfg.pin);
  return { ...cfg, seq: ++ctlSeq, name: p ? `${p.name} (${p.role})` : cfg.pin === null ? "no pin" : `PA${cfg.pin}` };
}
function ctlInit(r) { ctInit(r); }

function drawPresets(r) {
  const p = $("presets"); p.innerHTML = "";
  for (const pr of r.presets) {
    const b = document.createElement("button"); b.className = "quiet"; b.textContent = pr.name;
    b.addEventListener("click", () => { steps = pr.steps.map((x) => [...x]); drawSteps(); });
    p.appendChild(b);
  }
}

function drawSteps() {
  const box = $("steps-ed"); box.innerHTML = "";
  steps.forEach((st, i) => {
    const row = document.createElement("div"); row.className = "step";
    const label = document.createElement("span"); label.className = "mono small"; label.textContent = String(i + 1).padStart(2, " ");
    row.appendChild(label);
    for (const [c, name, hexc] of COLORS) {
      const sw = document.createElement("button"); sw.className = "sw"; sw.title = name; sw.setAttribute("aria-label", name);
      sw.style.background = hexc; sw.setAttribute("aria-pressed", String(st[0] === c));
      sw.addEventListener("click", () => { st[0] = c; drawSteps(); });
      row.appendChild(sw);
    }
    const sel = document.createElement("select");
    for (const ms of TIMES) { const o = document.createElement("option"); o.value = ms; o.textContent = `${ms} ms`; if (ms === st[1]) o.selected = true; sel.appendChild(o); }
    if (!TIMES.includes(st[1])) { const o = document.createElement("option"); o.value = st[1]; o.textContent = `${st[1]} ms`; o.selected = true; sel.appendChild(o); }
    sel.addEventListener("change", () => { st[1] = Number(sel.value); drawSteps(); });
    row.appendChild(sel);
    const del = document.createElement("button"); del.className = "quiet"; del.textContent = "Remove";
    del.disabled = steps.length <= 1;
    del.addEventListener("click", () => { steps.splice(i, 1); drawSteps(); });
    row.appendChild(del);
    box.appendChild(row);
  });
  $("add-step").disabled = steps.length >= chosen.table.max_steps;
  const total = steps.reduce((t, [, ms]) => t + ms, 0);
  $("total").textContent = `One cycle: ${(total / 1000).toFixed(2)} s${total > 6000 ? ". Keep it to 6 s or less, so the check can see two cycles" : ""}`;
  $("go").disabled = total > 6000;
}

// ---------------------------------------------------------------- run

function step(list, text) { const li = document.createElement("li"); li.textContent = text; list.appendChild(li); list.scrollTop = list.scrollHeight; if (list.id === "log") progress(text); }
function sayTo(list) { let last = ""; return (t) => { const k = t.replace(/\d+%$/, ""); if (k === last && list.lastChild) { list.lastChild.textContent = t; if (list.id === "log") progress(t); } else step(list, t); last = k; }; }

function download(name, text) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: typeof text === "string" ? "application/json" : "application/octet-stream" }));
  a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  return name;
}

function found(text, state, fold) {
  $("found").textContent = text; $("found").dataset.state = state || "";
  $("found-diag").classList.toggle("hidden", state !== "bad");
  const h = $("found-help"); h.classList.toggle("hidden", !fold); h.dataset.fold = fold || "";
  if (fold) openEnv(fold);
}
// Open "Before you connect" at the row that explains the failure, and mark that row.
function openEnv(row) {
  const env = $("env"), el = row && document.getElementById("env-" + row);
  env.open = true; env.querySelectorAll("p.hit").forEach((p) => p.classList.remove("hit"));
  if (el) { el.classList.add("hit"); el.scrollIntoView({ block: "nearest" }); }
}
$("found-help").addEventListener("click", () => openEnv($("found-help").dataset.fold));
// Any device or port request: turn the browser's own error into one plain sentence and one next step.
const devErr = (e, api) => asDeviceError(e, { api }, (kind, message, next) => new FlashError(kind, message, next));

// The drawn bench: idle (no board), on, run, pass, fail; the laptop's screen follows the steps.
function scene(st, lines) {
  $("scene").dataset.state = st;
  if (lines) lines.forEach((t, i) => { if (t !== null) $("scr" + i).textContent = t; });
  $("cable-label").textContent = st === "idle" ? "plug in a USB cable that carries data" : "USB cable";
}
function screen(i, t) { $("scr" + i).textContent = t; }
let ledTimer = null;
function ledPlay(levels, color) {   // the drawn LED repeats what the debugger read, three times
  clearTimeout(ledTimer);
  const led = $("led"), glow = $("led-glow");
  const set = (lvl) => { const on = !!lvl; led.classList.toggle("on", on); glow.classList.toggle("on", on);
    if (color) { const c = on ? colorHex(lvl) : "#FF5A3C"; led.setAttribute("fill", c); glow.setAttribute("fill", c); } };
  if (!levels || !levels.length) { set(1); ledTimer = setTimeout(() => set(0), 2000); return; }
  const seq = [...levels, ...levels, ...levels]; let i = 0;
  const next = () => { if (i >= seq.length) { set(0); led.setAttribute("fill", "#FF5A3C"); glow.setAttribute("fill", "#FF5A3C"); return; }
    set(seq[i][0]); ledTimer = setTimeout(next, Math.max(40, seq[i][1])); i++; };
  next();
}

// The run card: a ring that fills with the steps the session reports, and four marks.
const CHECK_STEP = { button: "Watched the button", breathe: "Watched the fade", loop: "Swept the loop", digit: "Self-test", occ: "Self-test", pattern: "Timed the LED", colors: "Timed the LED", thermo: "Read the sensor", uart: "Read the message",
  oob: "Started", coremark: "Ran CoreMark", ble: "Restarted" };
let goLabel = "Flash it to the board", ringAt = 0;
function chipsReset(kind) {
  const ol = $("chips"); ol.innerHTML = "";
  for (const t of ["Saved", "Wrote", "Read back", CHECK_STEP[kind] || "Checked"]) { const li = document.createElement("li"); li.textContent = t; ol.appendChild(li); }
}
function chipAt(k, cls) { [...$("chips").children].forEach((li, i) => { if (li.classList.contains("skip")) return; li.className = i < k ? "done" : i === k ? (cls || "now") : ""; }); }
function ring(p, phase) {
  ringAt = Math.max(ringAt, Math.min(100, p));
  $("ring").style.strokeDashoffset = String(276 - 2.76 * ringAt);
  if (phase !== undefined) $("go").innerHTML = `<b>${Math.round(ringAt)}%</b><span>${phase}</span>`;
  $("scr-bar").setAttribute("width", String(2 * ringAt));
  $("go").style.setProperty("--p", ringAt + "%");
}
function progress(text) {
  if ($("result-card").dataset.state !== "run") return;
  const m = text.match(/(\d+)%/), n = m ? Number(m[1]) : 0;
  if (/already on the board|nothing to write/i.test(text)) {   // nothing saved, nothing written
    [...$("chips").children].slice(0, 2).forEach((li) => { li.className = "skip"; });
    chipAt(2); ring(60, "checked"); screen(1, "already on the board"); screen(2, "nothing to write"); }
  else if (/Saved|not saved again/i.test(text)) { chipAt(1); ring(20, "saved"); screen(1, "saved a copy → Downloads"); }
  else if (/erasing|Flashing: writing|Writing main flash|Writing;/i.test(text)) { chipAt(1); ring(25 + n * 0.45, "writing"); screen(2, `writing ${n ? n + "%" : "…"}`); }
  else if (/sequence/i.test(text)) { chipAt(2); ring(65, "writing"); screen(2, "wrote your sequence"); }
  else if (/Reading the flash back|programming mode/i.test(text)) { chipAt(2); ring(75, "reading back"); screen(2, "wrote it"); screen(3, "reading back…"); }
  else if (/Timing|Watching|Running CoreMark|serial port for/i.test(text)) { chipAt(3); ring(85 + Math.min(14, n), "checking"); screen(3, "read back: matches"); screen(4, "checking…"); }
  else ring(ringAt, undefined);
}

// The result card: "" waiting, "run" while the button works, "pass" or "fail" once show() lands.
let runT0 = 0;
function resultState(st) {
  $("result-card").dataset.state = st;
  $("result-head").textContent = ({ run: "Checking", pass: "It works", fail: "Not yet" }[st] || "Result");
  $("wait-text").textContent = st === "run" ? "Listening to the board…" : "The board's answer lands here.";
  $("go").classList.toggle("busy", st === "run");
  $("ring").style.stroke = st === "pass" ? "var(--ok)" : st === "fail" ? "var(--bad)" : "var(--red)";
  if (st === "run") {
    runT0 = performance.now(); ringAt = 0; ring(0, "starting");
    chipsReset(chosen ? chosen.check.kind : ""); chipAt(0);
    scene("run", [null, "saving a copy…", "", "", "working…"]); $("led").classList.remove("on"); $("led-glow").classList.remove("on");
  } else if (st === "pass" || st === "fail") {
    ring(100); $("go").textContent = goLabel;
  } else {
    ringAt = 0; $("go").style.setProperty("--p", "0%"); $("ring").style.strokeDashoffset = "276"; $("scr-bar").setAttribute("width", "0"); $("go").textContent = goLabel;
    if (chosen) chipsReset(chosen.check.kind);
    if ($("scene").dataset.state !== "idle") scene("on", [null, null, "", "", cur && cur.programmable ? "ready" : "recognized"]);
  }
  if (st !== "pass" && st !== "fail") $("result").classList.add("hidden");
}

// Save the board's flash to Downloads, unless this visit already saved exactly this content.
function keep(copy, id, log) {
  const key = `${cur.id}-${id.traceid}`, file = `${key}-${copy.saved.slice(0, 19).replace(/[:T]/g, "")}.json`;
  const { save } = rememberCopy(copies, key, { start: copy.start, data: copy.data, bytes: copy.bytes, sha256: copy.sha256, json: JSON.stringify(copy) }, file, cur.kit);
  const c = copies.last;
  window.frlLastCopy = { ...c, name: c.file.replace(/\.json$/, "") };   // con-save.js (.bin) and ui.js (put back) use it: always this board's
  if (!save) { step(log, `The board's flash is the same as the copy already saved this visit (${c.file}): not saved again`); return; }
  download(file, c.json);
  step(log, `Saved the board's flash to Downloads: ${file} (${Math.round(copy.bytes / 1024)} kB, sha256 ${copy.sha256.slice(0, 12)}…)`);
}

function strip(box, levels, color, faded) {
  box.innerHTML = "";
  const total = levels.reduce((s, [, ms]) => s + ms, 0);
  for (const [lvl, ms] of levels) {
    const s = document.createElement("span"); s.style.flex = String(ms / total);
    s.style.background = color ? colorHex(lvl) : (lvl ? "var(--red)" : "var(--line)");
    if (faded && (color || lvl)) s.style.opacity = ".45";
    s.title = `${color ? colorName(lvl) : (lvl ? "on" : "off")} ${ms} ms`;
    box.appendChild(s);
  }
}

// extra: {asked: levels the person asked for, askedText, didText, stats: [[value, label], ...]}
function show(ok, verdict, measured, next, levels, color, extra = {}) {
  resultState(ok ? "pass" : "fail");
  if (ok && flashArmed) { try { if (window.frlPop) window.frlPop.flash(flashArmed); } catch (e) { /* counting is optional */ } flashArmed = null; }
  const at = [...$("chips").children].findIndex((li) => li.className !== "done" && li.className !== "skip");
  chipAt(ok ? 4 : Math.max(0, at), ok ? "" : "bad");
  scene(ok ? "pass" : "fail", [null, null, null, null, ok ? "PASS" : "FAIL"]);
  if (ok) ledPlay(levels, color);
  const has = !!(extra.asked && extra.asked.length);
  $("asked-row").classList.toggle("hidden", !has); $("did-row").classList.toggle("hidden", !has);
  $("asked").innerHTML = "";
  if (has) { strip($("asked"), extra.asked, color, true); $("asked-text").textContent = extra.askedText || ""; $("did-text").textContent = extra.didText || ""; }
  const stats = extra.stats || [];
  stats.push([`${((performance.now() - runT0) / 1000).toFixed(1)} s`, "in all"]);
  $("stats").innerHTML = "";
  for (const [v, l] of stats) setLive(l, "read in this run", v);
  for (const [v, l] of stats.slice(-3)) { const d = document.createElement("div"); d.innerHTML = "<b></b><span></span>"; d.children[0].textContent = v; d.children[1].textContent = l; $("stats").appendChild(d); }
  $("result").classList.remove("hidden");
  $("verdict").textContent = verdict; $("verdict").className = "result " + (ok ? "ok" : "bad");
  $("measured").textContent = measured || ""; $("next").textContent = next || "";
  const bars = $("bars"); bars.innerHTML = ""; bars.style.display = ""; bars.style.height = "";
  if (!extra.keepOverlay) { $("overlay").innerHTML = ""; $("overlay-take").classList.add("hidden"); overlayTake = null; }
  if (levels && levels.length) {
    const total = levels.reduce((s, [, ms]) => s + ms, 0);
    for (const [lvl, ms] of levels) {
      const s = document.createElement("span"); s.style.flex = String(ms / total);
      s.style.background = color ? colorHex(lvl) : (lvl ? "var(--red)" : "var(--line)");
      s.title = `${color ? colorName(lvl) : (lvl ? "on" : "off")} ${ms} ms`;
      bars.appendChild(s);
    }
  }
}

// ---------------------------------------------------------------- TI's out-of-box demo, live
// The demo's own protocol (examples/.../demos/out_of_box/MSP_GUI): each message is a one-entry MessagePack map
// ended by a newline, both ways. Commands: lightEnable, thermEnable, blinkEnable (true or false), blinkTick
// (seconds x 256). Readings: lsADC (light, ADC counts) and thADC (degrees C x 32).
const oob = { port: null, reader: null, mode: null, points: [], last: 0, buf: new Uint8Array(0) };

function mpEncode(key, v) {
  const k = new TextEncoder().encode(key), out = [0x81, 0xA0 | k.length, ...k];
  if (v === true) out.push(0xC3); else if (v === false) out.push(0xC2);
  else if (v < 0x80) out.push(v); else if (v < 0x100) out.push(0xCC, v); else out.push(0xCD, v >> 8, v & 0xFF);
  out.push(0x0A);
  return new Uint8Array(out);
}

function mpDecode(b, i) {   // one value at b[i]: {v, next} or null when more bytes are needed
  const t = b[i];
  if (t === undefined) return null;
  const need = (n) => b.length >= i + n;
  if ((t & 0xF0) === 0x80) {
    let o = i + 1; const m = {};
    for (let k = 0; k < (t & 0x0F); k++) { const kk = mpDecode(b, o); if (!kk) return null; const vv = mpDecode(b, kk.next); if (!vv) return null; m[kk.v] = vv.v; o = vv.next; }
    return { v: m, next: o };
  }
  if ((t & 0xE0) === 0xA0) { const n = t & 0x1F; return need(1 + n) ? { v: new TextDecoder().decode(b.subarray(i + 1, i + 1 + n)), next: i + 1 + n } : null; }
  if (t <= 0x7F) return { v: t, next: i + 1 };
  if (t === 0xC2 || t === 0xC3) return { v: t === 0xC3, next: i + 1 };
  if (t === 0xCC) return need(2) ? { v: b[i + 1], next: i + 2 } : null;
  if (t === 0xCD) return need(3) ? { v: (b[i + 1] << 8) | b[i + 2], next: i + 3 } : null;
  if (t === 0xCE) return need(5) ? { v: ((b[i + 1] << 24) | (b[i + 2] << 16) | (b[i + 3] << 8) | b[i + 4]) >>> 0, next: i + 5 } : null;
  if (t === 0xCA) return need(5) ? { v: new DataView(b.buffer, b.byteOffset + i + 1, 4).getFloat32(0), next: i + 5 } : null;
  return { v: null, next: i + 1 };
}

function oobMessage(m) {
  oob.last = performance.now();
  if ("lsADC" in m) { oob.points.push(m.lsADC); $("oob-value").textContent = `${m.lsADC} light`; $("oob-note").textContent = "Light level in ADC counts. Cover the light sensor (D1, on the bottom edge below jumpers J4 to J6, which must be on) or shine a light on it: the number moves and LED2 follows it."; }
  if ("thADC" in m) { const c = m.thADC / 32; oob.points.push(c); $("oob-value").textContent = `${c.toFixed(1)} °C`; $("oob-note").textContent = "The board's thermistor (RT1, on the bottom edge in the middle, labelled Thermistor; jumpers T.PATH and T.PWR on). Warm it with a finger: LED2 turns red at 2 degrees above where it started, blue at 2 below."; }
  if (oob.points.length > 200) oob.points.shift();
  drawChart();
}

function drawChart() {
  const c = $("oob-chart"), g = c.getContext("2d"), p = oob.points;
  g.clearRect(0, 0, c.width, c.height);
  if (p.length < 2) return;
  const lo = Math.min(...p), hi = Math.max(...p), span = Math.max(hi - lo, 1e-6);
  g.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--red").trim() || "#B42418";
  g.lineWidth = 2; g.beginPath();
  p.forEach((v, i) => { const x = (i / 199) * c.width, y = c.height - 8 - ((v - lo) / span) * (c.height - 16); i ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.stroke();
}

async function oobOpen() {
  if (oob.port) return;
  const port = await navigator.serial.requestPort({ filters: [{ usbVendorId: data.usb.vendorId, usbProductId: data.usb.productId }] });
  await port.open({ baudRate: chosen.check.baud });
  oob.port = port; oob.reader = port.readable.getReader();
  (async () => {
    try {
      for (;;) {
        const { value, done } = await oob.reader.read();
        if (done) break;
        const b = new Uint8Array(oob.buf.length + value.length); b.set(oob.buf); b.set(value, oob.buf.length);
        let i = 0;
        for (;;) {
          while (b[i] === 0x0A) i++;
          const r = mpDecode(b, i);
          if (!r) break;
          if (r.v && typeof r.v === "object") oobMessage(r.v);
          i = r.next;
        }
        oob.buf = b.slice(i);
      }
    } catch (e) { /* port closed */ }
  })();
}

async function oobClose() {
  if (!oob.port) return;
  try { await oob.reader.cancel(); } catch (e) { /* closed */ }
  try { oob.reader.releaseLock(); } catch (e) { /* released */ }
  try { await oob.port.close(); } catch (e) { /* closed */ }
  oob.port = null; oob.reader = null; oob.mode = null; oob.buf = new Uint8Array(0);
}

async function oobSend(...msgs) {
  const w = oob.port.writable.getWriter();
  try { for (const m of msgs) { await w.write(m); await ui.sleep(30); } } finally { w.releaseLock(); }
}

async function oobCommand(what) {
  try {
    await oobOpen();
    oob.points = []; drawChart(); $("oob-value").textContent = "";
    const off = oob.mode ? [mpEncode({ light: "lightEnable", thermometer: "thermEnable", blink: "blinkEnable" }[oob.mode], false)] : [];
    if (what === "stop") { await oobSend(...off); oob.mode = null; $("oob-note").textContent = "Stopped: LED1 blinks every half second while the demo waits."; return; }
    if (what === "blink") {
      await oobSend(...off, mpEncode("blinkEnable", true), mpEncode("blinkTick", Number($("oob-rate").value)));
      oob.mode = "blink"; $("oob-note").textContent = "LED2 blinks green at the rate you chose. Change it and press Blink LED again."; return;
    }
    await oobSend(...off, mpEncode(what === "light" ? "lightEnable" : "thermEnable", true));
    oob.mode = what;
    const asked = performance.now();
    await ui.sleep(3000);
    if (oob.last < asked) $("oob-note").textContent = "No answer from the board. TI's demo may not be on it any more: press \"Put TI's demo back on the board\" (it saves a copy first), or check J16 and J17 are on XDS.";
  } catch (e) {
    $("oob-note").textContent = "Stopped: " + (e.message || e);
    const d = deviceError(e, { api: "serial" });
    if (d) { $("oob-note").textContent = `${d.message} ${d.next}`; openEnv(d.fold); }
  }
}

// ---------------------------------------------------------------- the thermometer
// The chip's ADC0 leaves each reading of its temperature sensor in MEMRES0; the page reads it with the factory trim
// (TEMP_SENSE0) through the debugger and converts it the way TI's adc12_internal_temp_sensor_mathacl example does.
const thermo = { s: null, run: false, points: [], lo: null, hi: null };
const TH_LO = 10, TH_HI = 40;

// The factory trim's reference differs between TI's documents, and early L1306 samples carry a wrong one: use the
// reference that puts it in the window, else the typical trim voltage (build_sensor.py has the sources).
function thermoTrim(c, trim) {
  const v = c.trim_refs.map((r) => (trim - 0.5) * r / (2 ** c.bits)).find((x) => x >= c.trim_window_v[0] && x <= c.trim_window_v[1]);
  return v === undefined ? { v: c.typical_vtrim, factory: false } : { v, factory: true };
}

function thermoC(c, raw, vtrim) {
  return ((raw - 0.5) * c.vref / (2 ** c.bits) - vtrim) / (c.mv_per_c / 1000) + c.trim_c;
}

function thermoDraw(t) {
  if (t !== null) setLive("Temperature", "the chip's own sensor, converted with TI's formula", `${t.toFixed(1)} °C`);
  const f = t === null ? 0 : Math.min(1, Math.max(0, (t - TH_LO) / (TH_HI - TH_LO)));
  $("th-arc").style.strokeDasharray = `${(386 * f).toFixed(1)} 516`;
  const a = (135 + 270 * f) * Math.PI / 180;
  $("th-knob").setAttribute("cx", (100 + 82 * Math.cos(a)).toFixed(1)); $("th-knob").setAttribute("cy", (100 + 82 * Math.sin(a)).toFixed(1));
  $("th-num").textContent = t === null ? "--" : t.toFixed(1);
  $("th-f").textContent = t === null ? "" : `${(t * 9 / 5 + 32).toFixed(1)} °F`;
  const cv = $("th-chart"), g = cv.getContext("2d"), p = thermo.points;
  g.clearRect(0, 0, cv.width, cv.height);
  if (p.length < 2) return;
  const lo = Math.min(...p) - 0.5, hi = Math.max(...p) + 0.5;
  g.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--red").trim() || "#B42418";
  g.lineWidth = 3; g.beginPath();
  const span = Math.max(p.length, 40) - 1;   // fills from the left; scrolls once it has 240 points (one minute)
  p.forEach((v, i) => { const x = (i / span) * cv.width, y = cv.height - 10 - ((v - lo) / (hi - lo)) * (cv.height - 20); i ? g.lineTo(x, y) : g.moveTo(x, y); });
  g.stroke();
}

async function thermoRead(s, c, trim) {   // the mean of eight reads of MEMRES0, 12 bits
  let sum = 0;
  for (let i = 0; i < 8; i++) sum += (await s.dap.read32(n(c.memres))) & 0xFFF;
  return thermoC(c, sum / 8, trim);
}

// ---------------------------------------------------------------- button to LED, breathing LED
// Two readouts beside the result: the button pin and the LED pin (button), or the duty the program keeps in RAM and the
// LED pin (breathe). Both are read through the debugger, so the LED itself is not seen: the proof text says so.
function watchReset(r) {
  const btn = r.check.kind === "button";
  $("wt-a-l").textContent = btn ? r.check.button.label : "Duty the program set";
  $("wt-b-l").textContent = r.check.led.label;
  $("wt-a").textContent = "--"; $("wt-b").textContent = "--"; $("wt-a").classList.remove("on"); $("wt-b").classList.remove("on");
  $("wt-bar").parentElement.classList.toggle("hidden", btn); $("wt-bar").style.width = "0";
  $("wt-note").textContent = btn ? `Press Flash it. Once it is written, hold and release ${r.check.button.label.split(" (")[0]}: this panel follows the pins live for ${r.check.seconds} s.`
    : "Press Flash it. The page then watches the fade for three seconds.";
}

// Hold S2: sample the button pin and the LED pin together; pass when the LED followed the button in both states.
async function watchButton(s, r, say) {
  const c = r.check, sw = n(c.button.gpio) + n(cur.gpio.dout) + 0x100, lg = n(c.led.gpio) + n(cur.gpio.dout);
  const inv = c.led.activeHigh === false ? 1 : 0, sbit = c.button.bit, lbit = c.led.bit;
  const t = { held: 0, heldLit: 0, free: 0, freeDark: 0 };
  const t0 = performance.now();
  say("Watching the button and the LED pin: hold the button, then let go");
  let done = false;
  // For tools/webflash/bench-test.mjs --examples only (window.__frlTestPress): with nobody there to press the button, the
  // debugger drives the button pin low for 0.8 s at a time, the way S2 does, and releases it.
  const dout = n(c.button.gpio) + n(cur.gpio.dout), doe = n(c.button.gpio) + n(cur.gpio.doe);
  const drive = async (down) => { await s.dap.write32(dout, ((await s.dap.read32(dout)) & ~(1 << sbit)) >>> 0);
    const e = (await s.dap.read32(doe)) >>> 0; await s.dap.write32(doe, (down ? e | (1 << sbit) : e & ~(1 << sbit)) >>> 0); };
  while (!done && performance.now() - t0 < c.seconds * 1000) {
    if (window.__frlTestPress) await drive(Math.floor((performance.now() - t0) / 800) % 2 === 1);
    const smp = await s.dap.sampleWords([sw, lg], 120, ui.now);
    for (const x of smp) {
      const held = ((x.v[0] >>> sbit) & 1) === c.button.pressedLevel, lit = (((x.v[1] >>> lbit) & 1) ^ inv) === 1;
      if (held) { t.held++; if (lit) t.heldLit++; } else { t.free++; if (!lit) t.freeDark++; }
    }
    const last = smp[smp.length - 1], held = ((last.v[0] >>> sbit) & 1) === c.button.pressedLevel, lit = (((last.v[1] >>> lbit) & 1) ^ inv) === 1;
    $("wt-a").textContent = held ? "held" : "not pressed"; $("wt-a").classList.toggle("on", held);
    $("wt-b").textContent = lit ? "lit" : "dark"; $("wt-b").classList.toggle("on", lit);
    $("wt-note").textContent = `Seen so far: ${t.held} readings with the button held, ${t.free} with it released. ` + (t.held ? "Let go to finish." : "Hold the button now.");
    done = t.held >= 40 && t.free >= 40 && performance.now() - t0 > 1500;
  }
  if (window.__frlTestPress) await drive(false);
  const bad = (t.held - t.heldLit) + (t.free - t.freeDark);
  return { ...t, bad, saw: t.held >= 20 && t.free >= 20, ok: t.held >= 20 && t.free >= 20 && bad <= 0.02 * (t.held + t.free) };
}

// The program leaves its duty in RAM; read it together with the LED pin for a little over one breath.
async function watchBreathe(s, r, say) {
  const c = r.check, ad = n(c.addr), lg = n(c.led.gpio) + n(cur.gpio.dout), inv = c.led.activeHigh === false ? 1 : 0, lbit = c.led.bit;
  const tw = performance.now();
  while (((await s.dap.read32(ad)) >>> 0) !== (n(c.magic) >>> 0)) {
    if (performance.now() - tw > 5000) throw new FlashError("other", "the breathing program did not start on the chip", "unplug the board, plug it back in and try again");
    await ui.sleep(100);
  }
  say(`Watching the fade for ${c.seconds} s`);
  const all = [], t0 = performance.now();
  while (performance.now() - t0 < c.seconds * 1000) {
    const tb = performance.now() - t0, smp = await s.dap.sampleWords([ad + 4, lg], 150, ui.now);
    for (const x of smp) x.g = tb + x.t;   // time since the watch began; x.t restarts with every batch
    all.push(...smp);
    const last = smp[smp.length - 1], d = last.v[0] >>> 0, lit = (((last.v[1] >>> lbit) & 1) ^ inv) === 1;
    $("wt-a").textContent = `${d} %`; $("wt-b").textContent = lit ? "lit" : "dark"; $("wt-b").classList.toggle("on", lit);
    $("wt-bar").style.width = `${Math.min(100, d)}%`;
  }
  const duty = all.map((x) => x.v[0] >>> 0), min = Math.min(...duty), max = Math.max(...duty);
  // The program steps 1 percent every periodMs / 200 ms. The debugger's answers arrive in batches and the host sometimes
  // delays one by 100 to 500 ms, so the time stamps can be late and a step between two readings is judged against the
  // longest pause seen, not the stamps: a jump bigger than that pause explains is a fault, a smaller one is the host.
  const perStep = (c.periodMs || 2000) / 200;
  let jump = 0, pause = 0;
  for (let i = 1; i < duty.length; i++) {
    pause = Math.max(pause, all[i].g - all[i - 1].g);
    if (duty[i] !== duty[i - 1]) jump = Math.max(jump, Math.abs(duty[i] - duty[i - 1]));
  }
  const room = 3 + Math.ceil(1.5 * pause / perStep), bad = jump > room ? 1 : 0;
  const slack = Math.min(25, room - 3);   // a long pause can also skip the very top or bottom of the fade
  const share = (lo, hi) => { const g = all.filter((x) => (x.v[0] >>> 0) >= lo && (x.v[0] >>> 0) <= hi); return g.length ? g.filter((x) => (((x.v[1] >>> lbit) & 1) ^ inv) === 1).length / g.length : null; };
  const high = share(80, 100), low = share(0, 20);
  return { n: all.length, min, max, jump, high, low, pause: Math.round(pause), ok: min <= 5 + slack && max >= 95 - slack && bad === 0 && high !== null && low !== null && high >= 0.6 && low <= 0.4 };
}

async function thermoLive(s, c, trim) {
  thermo.s = s; thermo.run = true; $("th-stop").classList.remove("hidden");
  try {
    while (thermo.run) {
      const raw = await thermoRead(s, c, trim);
      const t = thermo.points.length ? thermo.points[thermo.points.length - 1] * 0.7 + raw * 0.3 : raw;   // light smoothing for the dial
      thermo.points.push(t); if (thermo.points.length > 240) thermo.points.shift();
      thermo.lo = thermo.lo === null ? t : Math.min(thermo.lo, t); thermo.hi = thermo.hi === null ? t : Math.max(thermo.hi, t);
      thermoDraw(t);
      $("th-note").textContent = `Since you started: low ${thermo.lo.toFixed(1)} °C, high ${thermo.hi.toFixed(1)} °C. Touch the chip and watch it climb.`;
      await ui.sleep(250);
    }
  } catch (e) {
    $("th-note").textContent = "Stopped reading: " + (e.message || e) + ". Press the button to start again.";
  } finally {
    thermo.run = false; thermo.s = null; $("th-stop").classList.add("hidden");
    await s.end().catch(() => {});
  }
}

function thermoStop() { thermo.run = false; }

// ---------------------------------------------------------------- the first wire
// The chip sets its DAC to the code the page writes and averages 16 ADC readings of the wired pin (wire.c).
async function lpAsk(s, addr, code) {
  await s.dap.write32(addr + 16, code);
  const seq = ((await s.dap.read32(addr + 8)) + 1) >>> 0;
  await s.dap.write32(addr + 8, seq);
  const t0 = performance.now();
  while (performance.now() - t0 < 2000) { if ((await s.dap.read32(addr + 12)) === seq) return (await s.dap.read32(addr + 20)) / 16; await ui.sleep(5); }
  throw new Error("the chip did not answer within 2 s");
}

async function lpSweep(s, addr) {   // 33 codes, 0 to 4095; a straight-line fit gives gain and offset
  const pts = [];
  for (let k = 0; k <= 32; k++) { const c = Math.min(4095, k * 128); pts.push([c, await lpAsk(s, addr, c)]); }
  $("lp-line").setAttribute("points", pts.map(([c, v]) => `${(40 + c / 4095 * 270).toFixed(1)},${(190 - v / 4095 * 180).toFixed(1)}`).join(" "));
  const mid = pts.filter(([c]) => c >= 256 && c <= 3840), nn = mid.length;   // away from the rails
  const sx = mid.reduce((a, [c]) => a + c, 0), sy = mid.reduce((a, [, v]) => a + v, 0);
  const sxx = mid.reduce((a, [c]) => a + c * c, 0), sxy = mid.reduce((a, [c, v]) => a + c * v, 0);
  const slope = (nn * sxy - sx * sy) / (nn * sxx - sx * sx), icpt = (sy - slope * sx) / nn;
  const worst = Math.max(...mid.map(([c, v]) => Math.abs(v - c)));
  return { pts, slope, icpt, worst, wired: worst < 150 };
}

async function lpLive(s, addr) {
  thermo.s = s; thermo.run = true;
  try {
    while (thermo.run) {
      const r = await lpSweep(s, addr);
      $("lp-note").textContent = r.wired ? `Wire in: the ADC follows the DAC. Gain ${(100 * (r.slope - 1)).toFixed(2)} %, offset ${r.icpt.toFixed(1)} codes, worst point ${r.worst.toFixed(0)} codes off. Pull the wire out and watch it fall apart.`
        : "No wire found: the ADC pin is floating. Unplug the USB cable, place the wire, plug it back in and press Start.";
      await ui.sleep(600);
    }
  } catch (e) {
    $("lp-note").textContent = "Stopped: " + (e.message || e) + ". Press Start to connect again.";
  } finally {
    thermo.run = false; thermo.s = null;
    await s.end().catch(() => {});
  }
}

// ---------------------------------------------------------------- draw a digit
// The pad keeps a 72 x 72 grid of 4-pixel cells; dgPrepare() is the touch demo's prepare() (ports/bitnetmcu/touch/
// prepare.h) in JavaScript: the digit's larger side scaled to 20 of 28 pixels, its center of mass in the middle,
// shrunk to 16 x 16 by area, normalized with MNIST's mean and spread, then scaled to +-127.
const DG_N = 72, DG_CELL = 4, DG_BRUSH = 3;
const dg = { grid: new Uint8Array(DG_N * DG_N), down: false, last: null, pending: null, timer: null, addr: 0, live: false };

// The pad only means something while the chip runs the reader. Before that (no board, reader not started, reader stopped)
// it is covered by a lock that says why and carries the Start button, so a drawing is never made that nothing will read.
function dgLock(text, head) {
  const pad = document.querySelector(".dg-pad"); if (!pad) return;
  pad.classList.toggle("live", dg.live);
  if (dg.live) return;
  $("dg-lock-h").textContent = head || (cur ? "The digit reader is not on the chip yet." : "No board is connected.");
  $("dg-lock-t").textContent = text || (cur ? "Press the red button to put it on the board. The pad wakes up when the chip answers." : "Connect a board first, then start the digit reader.");
  $("dg-start").disabled = !cur; $("dg-start").classList.toggle("hidden", !cur);
}

function dgClear() {
  dg.grid.fill(0); dg.last = null;
  const g = $("dg-canvas").getContext("2d"); g.fillStyle = "#161312"; g.fillRect(0, 0, 288, 288);
  $("dg-in").getContext("2d").clearRect(0, 0, 160, 160); $("dg-ans").textContent = "?";
}

function dgStamp(cx, cy) {
  const g = $("dg-canvas").getContext("2d"); g.fillStyle = "#F7F3EC";
  for (let y = cy - DG_BRUSH; y <= cy + DG_BRUSH; y++) for (let x = cx - DG_BRUSH; x <= cx + DG_BRUSH; x++) {
    if (x < 0 || y < 0 || x >= DG_N || y >= DG_N || (x - cx) ** 2 + (y - cy) ** 2 > DG_BRUSH * DG_BRUSH) continue;
    dg.grid[y * DG_N + x] = 1; g.fillRect(x * DG_CELL, y * DG_CELL, DG_CELL, DG_CELL);
  }
}

function dgPoint(e) {
  const r = $("dg-canvas").getBoundingClientRect();
  const cx = Math.floor((e.clientX - r.left) / r.width * DG_N), cy = Math.floor((e.clientY - r.top) / r.height * DG_N);
  if (dg.last) { const [lx, ly] = dg.last, n = Math.max(Math.abs(cx - lx), Math.abs(cy - ly), 1);
    for (let i = 1; i <= n; i++) dgStamp(Math.round(lx + (cx - lx) * i / n), Math.round(ly + (cy - ly) * i / n)); }
  else dgStamp(cx, cy);
  dg.last = [cx, cy];
}

export function dgPrepare(grid) {
  let minx = DG_N, miny = DG_N, maxx = -1, maxy = -1, sx = 0, sy = 0, cnt = 0;
  for (let y = 0; y < DG_N; y++) for (let x = 0; x < DG_N; x++) if (grid[y * DG_N + x]) {
    minx = Math.min(minx, x); maxx = Math.max(maxx, x); miny = Math.min(miny, y); maxy = Math.max(maxy, y); sx += x + 0.5; sy += y + 0.5; cnt++;
  }
  if (!cnt) return null;
  const side = Math.max(maxx - minx, maxy - miny) + 1, s = 20 / side, comx = sx / cnt, comy = sy / cnt;
  const v = new Float32Array(256); let vmax = 0;
  for (let j = 0; j < 16; j++) for (let i = 0; i < 16; i++) {
    let hit = 0;
    for (let b = 0; b < 4; b++) for (let a = 0; a < 4; a++) {
      const fx = (i + (a + 0.5) / 4) * 1.75, fy = (j + (b + 0.5) / 4) * 1.75;
      const cx = Math.trunc((fx - 14) / s + comx), cy = Math.trunc((fy - 14) / s + comy);
      if (cx >= 0 && cy >= 0 && cx < DG_N && cy < DG_N && grid[cy * DG_N + cx]) hit++;
    }
    const p = (Math.fround(hit / 16) - 0.1307) / 0.3081;
    v[j * 16 + i] = p; vmax = Math.max(vmax, Math.abs(p));
  }
  const out = new Int8Array(256);
  for (let k = 0; k < 256; k++) { const q = v[k] * 127 / vmax; out[k] = Math.trunc(q < 0 ? q - 0.5 : q + 0.5); }
  return out;
}

function dgShowInput(inp) {
  const g = $("dg-in").getContext("2d"), c = 10;
  for (let k = 0; k < 256; k++) { const l = Math.round((inp[k] + 128) / 255 * 255); g.fillStyle = `rgb(${l},${l},${l})`; g.fillRect((k % 16) * c, Math.floor(k / 16) * c, c, c); }
}

class DgTimeout extends Error {}
async function dgAsk(s, addr, inp) {   // write the drawing, bump seq_in, wait for seq_out (offsets from digit.c)
  const words = []; for (let k = 0; k < 256; k += 4) words.push(((inp[k] & 0xFF) | ((inp[k + 1] & 0xFF) << 8) | ((inp[k + 2] & 0xFF) << 16) | ((inp[k + 3] & 0xFF) << 24)) >>> 0);
  await s.dap.writeBlock(addr + 32, words);
  const seq = ((await s.dap.read32(addr + 16)) + 1) >>> 0;
  await s.dap.write32(addr + 16, seq);
  const t0 = performance.now();
  while (performance.now() - t0 < 3000) {
    if ((await s.dap.read32(addr + 20)) === seq) return { pred: await s.dap.read32(addr + 24), cycles: await s.dap.read32(addr + 28) };
    await ui.sleep(20);
  }
  throw new DgTimeout("the chip did not answer within 3 s");
}

async function dgLive(s, addr, clk) {
  thermo.s = s; thermo.run = true; dg.live = true; dg.pending = null; dgLock();
  let misses = 0, why = "";
  try {
    while (thermo.run) {
      if (dg.pending) {
        const inp = dg.pending; dg.pending = null;
        try {
          const a = await dgAsk(s, addr, inp);
          misses = 0;
          $("dg-ans").textContent = String(a.pred);
          aiState = { ...aiState, cycles: a.cycles, pred: a.pred, clkMhz: clk / 1e6 }; renderAi();
          setLive("Digit read", "the model's answer, read from the mailbox", String(a.pred)); setLive("Cycles", "CPU cycles for that answer", a.cycles.toLocaleString("en-US"));
          $("dg-time").textContent = `The chip read ${a.pred} in ${(a.cycles / clk * 1000).toFixed(1)} ms (${a.cycles.toLocaleString()} cycles at ${clk / 1e6} MHz). Clear and draw another.`;
          ledPlay(null);
        } catch (e) {
          if (!(e instanceof DgTimeout) || ++misses >= 3) throw e;   // a slow answer is not the end of the session; three in a row is
          $("dg-time").textContent = "The chip did not answer that one. Draw it again.";
        }
      }
      await ui.sleep(40);
    }
  } catch (e) {
    why = "Stopped: " + (e.message || e) + ".";
    $("dg-time").textContent = why + " Press the red button to start it again.";
  } finally {
    thermo.run = false; thermo.s = null; dg.live = false;
    dgLock(why ? why + " Press the red button to start the reader again." : "The reader is stopped. Press the red button to start it again.", "The digit reader stopped.");
    await s.end().catch(() => {});
  }
}

function dgLift() {
  if (!dg.down) return;
  dg.down = false; dg.last = null;
  clearTimeout(dg.timer);
  dg.timer = setTimeout(() => {   // a short pause, so a two-stroke digit (4, 7) is sent once
    const inp = dgPrepare(dg.grid); if (!inp) return;
    dgShowInput(inp);
    if (thermo.s) { dg.pending = inp; $("dg-time").textContent = "Reading…"; } else $("dg-time").textContent = "Press Start first: the chip reads it.";
  }, 450);
}

// Horizontal bars: the best measured CoreMark per board, this board's new score marked.
function drawCompare(rows, me, score) {
  const box = $("bars"); box.innerHTML = ""; box.style.display = "block"; box.style.height = "auto";
  const max = Math.max(...rows.map((x) => x.score), score);
  for (const x of rows) {
    const line = document.createElement("div");
    line.style.cssText = "display:grid;grid-template-columns:11em 1fr 4.5em;gap:8px;align-items:center;font-size:.82rem;margin:2px 0";
    const name = document.createElement("span"); name.textContent = x.name; name.title = `${x.vendor} ${x.core}, ${x.mhz} MHz`;
    const bar = document.createElement("span");
    const v = x.board === me ? score : x.score;
    bar.style.cssText = `height:12px;border-radius:3px;width:${Math.max(1, 100 * v / max)}%;background:${x.board === me ? "var(--red)" : "var(--line)"}`;
    const num = document.createElement("span"); num.className = "mono"; num.textContent = String(v);
    if (x.board === me) name.style.fontWeight = "700";
    line.append(name, bar, num); box.appendChild(line);
  }
}

// The same rule as tools/frl/verify.py message_check: the message arrived at least twice with its counter moving.
function messageCheck(text, message) {
  const lines = text.replace(/\0/g, "").replace(/\r/g, "\n").split("\n").map((l) => l.trim()).filter(Boolean);
  const hits = lines.filter((l) => l.startsWith(`> ${message} `));
  const counts = new Set(hits.map((h) => h.split(" ").pop()));
  return { ok: hits.length >= 2 && counts.size >= 2, hits: hits.length, lines: lines.slice(-4) };
}

// The probe's serial path hands over bytes in bursts (a line can sit in it for seconds), so the window is long and a caller that already has what it needs (enough) stops early.
async function readSerial(port, baud, seconds, enough) {
  await port.open({ baudRate: baud });
  const reader = port.readable.getReader(), dec = new TextDecoder();
  let text = "";
  const end = performance.now() + seconds * 1000;
  try {
    while (performance.now() < end) {
      const left = end - performance.now();
      const r = await Promise.race([reader.read(), ui.sleep(left).then(() => ({ timeout: true }))]);
      if (r.timeout || r.done) break;
      text += dec.decode(r.value, { stream: true });
      if (enough && enough(text)) break;
    }
  } finally {
    await reader.cancel().catch(() => {}); reader.releaseLock(); await port.close().catch(() => {});
  }
  return text;
}

// CC27xx: write a program through TI's SACI mailbox (../cc-saci.js). The chip checks the image itself first (CRC32);
// if it already holds it, nothing is written. Otherwise the page saves the board's flash, then erases and writes.
let flashArmed = null;   // the build slug of a program write in progress; the first passing result counts one flash (popularity.js)
async function goCc(r) {
  flashArmed = r.project || null;
  const log = $("log"); log.innerHTML = ""; resultState("run"); $("go").disabled = true;
  const t0 = performance.now(), secs = () => ((performance.now() - t0) / 1000).toFixed(1);
  let t = null, dap = null, sc = null, writing = false, savedBins = false;
  try {
    if (!device) throw new FlashError("no-device", "connect the board first (step 1)", "then press this button again");
    const img = parseHex(await (await fetch(r.hex)).text());
    const crc = crc32(img.main), nSect = img.main.length / 2048;
    step(log, `The light program: ${Math.round(img.main.length / 1024)} kB of main flash and the board's two configuration areas`);
    t = transportFor(device); await t.open(); dap = new Dap(t); dap.dormant = true;
    const fam = FAMILY[cur.saci] || FAMILY.cc27;
    sc = new Saci(dap, (x) => { if (!/^NOP/.test(x)) step(log, x); }, fam);
    await dap.connect();
    const id = await dap.apRead(1, 0x0);
    if (!cur.identity.deviceids.some((d) => ((id & n(cur.identity.deviceidMask)) >>> 0) === ((n(d) & n(cur.identity.deviceidMask)) >>> 0)))
      throw new FlashError("wrong-chip", "this is not the board that was connected", "press Connect a board again");
    step(log, "Putting the chip in its programming mode"); await sc.enter();
    const same = (await sc.verifyMain(img.main.length, crc)) === 0;
    let wrote = false;
    if (same) step(log, "The chip's own check matches this program: nothing to write");
    else {
      step(log, "The board holds something else: saving a copy of its flash first");
      const st0 = await sc.bootAndWait();
      if (!st0.ahbOpen && st0.boot === 0x20 && fam === FAMILY.cc23) {
        // With the debugger quiet, the CC23 ROM only stays in SACI (BOOTSTA 0x20) when it finds no program to start.
        step(log, "The chip has no program that starts (it waits in its programming mode): nothing to save. Writing");
      } else {
        if (!st0.ahbOpen) throw new FlashError("other", "the board's flash cannot be read to save a copy", "use the terminal route for this board");
        const bytesOf = (words) => { const b = new Uint8Array(words.length * 4); words.forEach((w, i) => { b[i * 4] = w & 0xff; b[i * 4 + 1] = (w >>> 8) & 0xff; b[i * 4 + 2] = (w >>> 16) & 0xff; b[i * 4 + 3] = w >>> 24; }); return b; };
        const stamp = `${cur.id}-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "")}`;
        const b = bytesOf(await dap.readBlock(0, fam.mainEnd / 4));
        const name = download(`${stamp}-main.bin`, b);
        download(`${stamp}-ccfg.bin`, bytesOf(await dap.readBlock(0x4E020000, 2048 / 4)));   // the boot configuration too, for a full put-back with TI's tools
        if (fam === FAMILY.cc27) download(`${stamp}-scfg.bin`, bytesOf(await dap.readBlock(0x4E040000, 1024 / 4)));
        savedBins = true;
        step(log, `Saved to Downloads: ${name} (${Math.round(b.length / 1024)} kB) and its configuration. Writing${fam.retain ? "; TI's security firmware stays" : ""}`);
      }
      await sc.enter();
      const say = sayTo(log);
      writing = true;
      await sc.program(img, (p) => say(`Writing main flash ${Math.round(p * 100)}%`));
      writing = false; wrote = true;
    }
    const st = await sc.bootAndWait();
    if (r.check.kind === "status" || r.check.kind === "uart") {   // the radio demos: a status block in RAM, or a message on the serial port
      const what = r.title.replace(/:.*$/, "");
      let verdict = null;
      if (r.check.kind === "status" && st.ahbOpen) {
        const addr = n(r.check.addr), rd = async () => (await dap.readBlock(addr, 4)).map((x) => x >>> 0);
        step(log, `Reading the demo's status block for ${r.check.seconds} s`);
        const a = await rd(); await ui.sleep(r.check.seconds * 1000); const c = await rd();
        verdict = { ok: a[0] === (n(r.check.magic) >>> 0) && c[2] - a[2] >= r.check.minRise, rise: c[2] - a[2] };
      }
      const detail = (wrote ? "Written and checked by the chip's own CRC32. " : "Already on the board. ") + `${secs()} s in all.`;
      if (verdict) show(verdict.ok, verdict.ok ? `It works: ${what} runs and its counter rose by ${verdict.rise}.` : "Written, but the demo's counter did not move.", detail,
        verdict.ok ? "Change its settings in the Change tab: the page rewrites the image with your values (about 3 s)." : "Unplug the board, plug it back in and press the button again.", null, false, { stats: [[`${verdict.rise}`, "counter rise"]] });
      else show(st.ahbOpen, st.ahbOpen ? `${what} is on the board.` : "Written, but the chip's debug port did not open.", detail,
        st.ahbOpen ? "Open the Console tab to read the board's message over its serial port." : "Unplug the board, plug it back in and press the button again.");
      return;
    }
    show(st.ahbOpen, wrote ? "The light program is on the board." : "The light program is already on the board.",
      `The chip checked ${nSect} sectors itself (CRC32 0x${crc.toString(16).toUpperCase()}) and started again with its debug port ${st.ahbOpen ? "open" : "closed"}; ${secs()} s in all.`,
      st.ahbOpen ? "Now press Find the light." : "Unplug the board, plug it back in and press the button again.");
  } catch (e) {
    const fe = e instanceof FlashError ? e : new FlashError("other", e.message || String(e), "unplug the board, plug it back in, reload this page and try again");
    { const d = deviceError(e, { api: "hid" }); if (d) { fe.message = d.message; fe.next = d.next; fe.fold = d.fold; openEnv(d.fold); } }
    show(false, "Stopped: " + fe.message, writing
      ? "It stopped while writing, so the board may hold a partly written program. " + (savedBins
        ? "The .bin copies saved before the write are in your Downloads folder; this page cannot put them back. \u201cPut back what was there\u201d lists the steps with TI's UniFlash, or press the button again to write the light program whole."
        : "Nothing was saved first (the board had no program that starts). Press the button again to write the light program whole.")
      : "", fe.next);
  } finally {
    if (sc) await sc.close().catch(() => {});      // reset and run, then let go of the debugger
    else if (dap) await dap.disconnect().catch(() => {});
    if (t) await t.close().catch(() => {});
    $("go").disabled = false;
    if ($("result-card").dataset.state === "run") resultState("");
  }
}

// The elapsed timer: the expected time is said before the write starts, then a bar and a line update every second.
let estTimer = null, estIds = ["est", "est-bar", "est-text"];
function estBegin(est, ids = ["est", "est-bar", "est-text"]) {
  estIds = ids;
  const t0 = performance.now(), box = $(ids[0]);
  box.hidden = false; $(ids[2]).textContent = `This takes ${estimateText(est)}. Keep the cable in and this tab open.`; $(ids[1]).style.width = "2%";
  clearInterval(estTimer);
  estTimer = setInterval(() => { const l = timeLine(est, (performance.now() - t0) / 1000); $(ids[1]).style.width = l.pct + "%"; $(ids[2]).textContent = l.text; }, 1000);
}
function estStart(r) {
  let est = 0;
  try { const sb = sessionBoard(cur, r), flash = sb.flash.end - sb.flash.start; est = estimateSeconds(sb, { bytes: r.bytes || 0, total: r.bytes || 0, read: savedSha.has(cur.traceid) ? 0 : flash }); } catch (e) { est = 0; }
  estBegin(est);
}
function estStop(ok) { clearInterval(estTimer); estTimer = null; const [box, bar, txt] = estIds; if (!$(box).hidden) { $(bar).style.width = "100%"; $(txt).textContent = ok === false ? "Stopped." : "Done."; setTimeout(() => { if (!estTimer) $(box).hidden = true; }, 4000); } }

async function go() {
  await conClose(); $("c-plan").hidden = true;
  if (thermo.s) { thermoStop(); while (thermo.s) await ui.sleep(50); }   // let the live reader release the debugger first
  if (chosen && (chosen.hex || chosen.check.kind === "ble")) return goCc(chosen);
  const r = chosen, log = $("log"); log.innerHTML = ""; resultState("run"); flashArmed = r.project || null;
  phase = "idle"; phaseSaved = false; lost = null; trail.add("run", { demo: r.id, kind: r.check.kind, image: r.sha256 && r.sha256.slice(0, 12) });
  $("go").disabled = true; if (!["ble"].includes(r.check.kind)) estStart(r);
  const t0 = performance.now();
  let s, port = null, writing = false, saved = false;
  try {
    if (r.check.kind === "uart" && !device) {
      throw new FlashError("no-device", "connect the board first (step 1)", "then press this button again; Chrome asks for its serial port");
    }
    if (r.check.kind === "uart") {
      // the serial chooser must open first, while the click still counts as the person's action
      step(log, `Chrome asks for a serial port: pick the XDS110 one${serial ? ` ending in ${serial}1` : " whose name ends in 1"}`);
      port = await navigator.serial.requestPort({ filters: [{ usbVendorId: data.usb.vendorId, usbProductId: data.usb.productId }] });
    }
    const img = await imageOf(r);
    let id;
    ({ s, id } = await openBoard(r, sayTo(log)));
    if (r.check.kind === "loop") {
      const has = await s.m.holds(img.segments);
      if (has) step(log, "The loop program is already on the board: nothing to write");
      else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      await s.run();
      const addr = n(r.check.addr), magic = n(r.check.magic) >>> 0, tw = performance.now();
      while ((await s.dap.read32(addr)) >>> 0 !== magic) { if (performance.now() - tw > 5000) throw new FlashError("other", "the loop program did not start on the chip", "unplug the board, plug it back in and try again"); await ui.sleep(100); }
      step(log, "Sweeping the DAC from 0 to 4095 and reading the ADC");
      const res = await lpSweep(s, addr);
      show(res.wired, res.wired ? "It works: the ADC reads back what the DAC sent." : "No wire found: the ADC pin is floating.",
        `33 DAC codes, 16 ADC readings each, both against VDDA. Fit between codes 256 and 3840: slope ${res.slope.toFixed(4)}, offset ${res.icpt.toFixed(1)} codes; worst point ${res.worst.toFixed(0)} codes from the line it should follow.`,
        res.wired ? "Pull the wire out and watch the line on the chart fall apart; push it back in." : `Unplug the USB cable, connect ${r.check.from} to ${r.check.to}, plug it back in and press Start.`,
        null, false, res.wired ? { stats: [[`${(100 * (res.slope - 1)).toFixed(2)} %`, "gain error"], [`${res.icpt.toFixed(1)}`, "offset, codes"]] } : {});
      lpLive(s, addr); s = null;
      return;
    }
    if (r.check.kind === "button" || r.check.kind === "breathe") {
      const has = await s.m.holds(img.segments);
      if (has) step(log, "This program is already on the board: nothing to write");
      else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      await s.run();
      watchReset(r);
      const say = sayTo(log);
      if (r.check.kind === "button") {
        const res = await watchButton(s, r, say), name = r.check.button.label.split(" (")[0];
        show(res.ok, res.ok ? `It works: ${r.check.led.label.split(" (")[0]} followed ${name}.` : res.saw ? `The LED did not follow ${name} every time.` : `No ${name} press was seen.`,
          `Read through the debugger: ${res.held} readings with the button held (the LED pin was lit in ${res.heldLit}), ${res.free} with it released (the LED pin was dark in ${res.freeDark}). ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
          res.ok ? "Change when the LED lights in button.c (Go further), or press Flash it again to watch it once more." : res.saw ? "Check the LED's jumpers, then flash it again." : `Press Flash it again, then hold ${name} for a second and let go. If the readout never says held, check the board's S2 and its jumpers.`,
          null, false, { stats: [[`${res.held}`, "readings held"], [`${res.free}`, "readings released"]] });
      } else {
        const res = await watchBreathe(s, r, say);
        const pct = (v) => (v === null ? "no readings" : `${Math.round(v * 100)} %`);
        show(res.ok, res.ok ? "It works: the duty rises from 0 to 100 percent and falls back, and the pin follows it." : "The fade did not check out.",
          `${res.n} readings through the debugger. Duty ranged ${res.min} to ${res.max} percent, largest step ${res.jump} (longest pause between readings ${res.pause} ms). The LED pin was lit ${pct(res.high)} of the readings at 80 to 100 percent duty and ${pct(res.low)} at 0 to 20 percent. ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
          res.ok ? "Change how fast it breathes in breathe.c (Go further) and rebuild." : "Check the LED's jumpers, then flash it again.",
          null, false, { stats: [[`${res.min}-${res.max} %`, "duty seen"], [`${res.n}`, "readings"]] });
      }
      return;
    }
    if (r.check.kind === "digit") {
      const has = await s.m.holds(img.segments);
      if (has) step(log, "The digit reader is already on the board: nothing to write");
      else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      await s.run();
      const addr = n(r.check.addr), magic = n(r.check.magic) >>> 0;
      step(log, "The chip reads its ten test digits first");
      let st = null; const tw = performance.now();
      while (performance.now() - tw < 10000) { if ((await s.dap.read32(addr)) >>> 0 === magic) { st = true; break; } await ui.sleep(200); }
      if (!st) throw new FlashError("other", "the digit reader did not start on the chip", "unplug the board, plug it back in and try again");
      const clk = await s.dap.read32(addr + 4), right = await s.dap.read32(addr + 8), nt = await s.dap.read32(addr + 12);
      aiState = { selftest: right, right, nTest: nt, clkMhz: clk / 1e6 }; renderAi();
      show(right === nt, right === nt ? `Ready: the chip read ${right} of ${nt} test digits right.` : `The chip read only ${right} of ${nt} test digits right.`,
        `BitNetMCU ${r.check.model || "cnn_64"} on the chip at ${clk / 1e6} MHz; ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
        "Now draw a digit on the pad below.", null, false, { stats: [[`${right}/${nt}`, "self-test"], [`${clk / 1e6} MHz`, "clock"]] });
      if (right === nt) { dgLive(s, addr, clk); s = null; $("dg-time").textContent = "Draw one digit, 0 to 9."; }
      else dgLock(`The chip read only ${right} of ${nt} of its own test digits right, so the pad stays locked.`, "The self-test did not pass.");
      return;
    }
    if (r.check.kind === "occ") {
      const has = await s.m.holds(img.segments);
      if (has) step(log, "The model is already on the board: nothing to write");
      else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      await s.run();
      const addr = n(r.check.addr), magic = n(r.check.magic) >>> 0;
      step(log, "The chip runs its 20 stored windows first");
      let up = false; const tw = performance.now();
      while (performance.now() - tw < 15000) { if ((await s.dap.read32(addr)) >>> 0 === magic) { up = true; break; } await ui.sleep(200); }
      if (!up) throw new FlashError("other", "the room model did not start on the chip", "unplug the board, plug it back in and try again");
      const w = await s.dap.readBlock(addr, 7), clk = w[1];
      aiState = { right: w[4], sameRef: w[3], nTest: w[2], cycMin: w[5], cycles: w[5], clkMhz: clk / 1e6 }; renderAi();
      const okAll = w[3] === w[2] && w[2] === 20;
      show(okAll, okAll ? `Ready: all 20 stored windows match the reference answers; ${w[4]} of 20 counts equal the true head count.` : `Only ${w[3]} of 20 windows match the reference answers.`,
        `${r.ai ? r.ai.name + " (" + r.ai.model + ")" : "Room model"} on the chip at ${clk / 1e6} MHz; the fastest window took ${w[5].toLocaleString()} cycles and the slowest ${w[6].toLocaleString()}. ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
        "Now pick a window above the result.", null, false, { stats: [[`${w[3]}/20`, "match reference"], [`${w[4]}/20`, "head count right"], [`${(w[5] / clk * 1000).toFixed(1)} ms`, "fastest window"]] });
      if (okAll) { occLive(s, addr, clk, r); s = null; }
      return;
    }
    if (r.check.kind === "thermo") {
      const has = await s.m.holds(img.segments);
      if (has) step(log, "The thermometer is already on the board: nothing to write");
      else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      await s.run();
      const trim = (await s.dap.read32(n(r.check.trim))) & 0xFFF, cal = thermoTrim(r.check, trim);
      thermo.points = []; thermo.lo = thermo.hi = null;
      const t = await thermoRead(s, r.check, cal.v);
      const ok = t > -20 && t < 90;
      show(ok, ok ? `It works: the chip reads ${t.toFixed(1)} °C.` : `The reading looks wrong: ${t.toFixed(1)} °C.`,
        (cal.factory ? `Calibrated with this chip's factory trim (${trim}).` : `Not calibrated: this chip's factory trim (${trim}) is outside the expected range, a known issue on early LaunchPad samples, so the page uses TI's typical value; expect a few degrees of offset.`)
          + ` ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
        ok ? "Touch the chip with a finger: the dial climbs within a few seconds." : "Unplug the board, plug it back in and try again.", null, false,
        { stats: [[`${t.toFixed(1)} °C`, "now"], [`${(t * 9 / 5 + 32).toFixed(1)} °F`, "now"]] });
      if (ok) { thermoLive(s, r.check, cal.v); s = null; }
      return;
    }
    if (r.check.kind === "controls") {
      const cfg = ctlRead(r);
      const has = await s.m.holds(img.segments);
      if (!has) { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
      step(log, "Writing your settings to the board's flash (one sector)");
      writing = true;
      await s.m.writeSector(n(r.table.addr), packTable(r.table, cfg));
      writing = false;
      await s.run();
      const sa = n(r.status.addr), tw = performance.now();
      step(log, "Waiting for the program to apply them");
      while (((await s.dap.read32(sa + 4)) >>> 0) !== (cfg.seq >>> 0)) { if (performance.now() - tw > 8000) throw new FlashError("other", "the controls program did not apply the settings", "unplug the board, plug it back in and send again"); await ui.sleep(50); }
      let res = null, samples = null, pw = null;
      const lvOf = (v) => (v[0] >>> cfg.pin) & 1;
      if (cfg.pin !== null && cfg.mode === "toggle") {
        const secs = Math.min(10, Math.max(r.check.seconds, (2.2 * (cfg.on + cfg.off) + 300) / 1000));
        step(log, `Timing ${cfg.name} through the debugger for ${secs.toFixed(1)} s`);
        samples = await s.dap.sampleWords([n(r.check.gpio) + n(cur.gpio.dout)], secs * 1000, ui.now);
        res = sequenceCheck(samples, lvOf, expectedToggle(cfg), r.check.tolMs);
      } else if (cfg.pin !== null && cfg.mode === "pwm") {
        // A pad driven by a timer is not seen by the GPIO input register (read back as 0 here, with TI's own PWM example too),
        // so a timer-driven pad is judged through the timer: its load and compare registers, the enable bit, and 1.5 s of counter
        // samples (the pad is high while the counter is above the compare value). A steady 0 or 100 percent is GPIO and is read from the pad.
        const setup = pwmSetup(cfg.pwmHz, cfg.pwmDuty), stOnce = decodeStatus(await s.dap.readBlock(sa, r.status.words.length));
        if (setup.steady) {
          step(log, `Sampling ${cfg.name} through the debugger for 1.5 s`);
          samples = await s.dap.sampleWords([n(r.check.gpio) + n(cur.gpio.dout) + 0x100], 1500, ui.now);
          const hi = samples.filter((x) => lvOf(x.v)).length;
          pw = { steady: true, n: samples.length, hi, setup, regsOk: pwmMatches(cfg, stOnce), ok: pwmMatches(cfg, stOnce) && (setup.duty === 100 ? hi === samples.length : hi === 0) };
        } else {
          const rg = pwmRegs(cfg.pin);
          step(log, `Reading the timer behind ${cfg.name} through the debugger for 1.5 s`);
          samples = await s.dap.sampleWords([rg.ctr], 1500, ui.now);
          const regs = { load: (await s.dap.read32(rg.load)) >>> 0, cc: (await s.dap.read32(rg.cc)) >>> 0, ctrctl: (await s.dap.read32(rg.ctrctl)) >>> 0 };
          const j = pwmJudge(cfg, regs, samples.map((x) => x.v[0] >>> 0));
          pw = { ...j, regs, regsOk: j.regsOk && pwmMatches(cfg, stOnce), hi: Math.round((j.share * j.n) / 100) };
          pw.ok = j.ok && pwmMatches(cfg, stOnce);
          samples = null;
        }
      } else if (cfg.pin !== null && (cfg.mode === "high" || cfg.mode === "low")) {
        // a driven pin is read back from the pad (GPIO DIN, input enabled by the program)
        step(log, `Reading ${cfg.name} back through the debugger`);
        samples = await s.dap.sampleWords([n(r.check.gpio) + n(cur.gpio.dout) + 0x100], 400, ui.now);
        const hi = samples.filter((x) => lvOf(x.v)).length, want = cfg.mode === "high" ? samples.length : 0;
        pw = { steady: true, pin: true, n: samples.length, hi, mode: cfg.mode, ok: hi === want };
        samples = null;
      } else await ui.sleep(150);
      const st = decodeStatus(await s.dap.readBlock(sa, r.status.words.length));
      const secs = ((performance.now() - t0) / 1000).toFixed(1);
      const answers = readAnswers(cfg, st), packRows = ctRows.filter((x) => x.kind === "read"), anyPack = ctRows.some((x) => x.pack);
      const readsOk = answers.every((a, i) => a.result === "acknowledged" || (packRows[i] && packRows[i].pack));
      const ok = st.ok && (res ? res.ok : true) && (pw ? pw.ok : true) && readsOk;
      // predicted against measured on one plot (the shared live panel): the square wave asked, from the first rising edge
      // the debugger saw, against the pin's output register sampled through the debugger
      if (res && samples && window.LivePanel) {
        const LP = window.LivePanel, lv = lvOf;
        const first = samples.findIndex((x, i) => i > 0 && lv(x.v) === 1 && lv(samples[i - 1].v) === 0);
        if (first > 0) {
          const meas = { t: samples.slice(first).map((x) => x.t - samples[first].t), v: samples.slice(first).map((x) => lv(x.v)) };
          const pred = LP.squareWave(expectedToggle(cfg), meas.t[meas.t.length - 1] + 1, 0);
          const cmp = LP.compare(pred, meas, { abs: 0.01, time: r.check.tolMs });
          const ovOpt = { title: `${cfg.name}: asked ${cfg.on} on, ${cfg.off} off (ms), against the pin's output register`, unit: "level", xunit: "ms after the first rising edge", H: 260 };
          $("overlay").innerHTML = LP.overlay(cmp, ovOpt);
          overlayTake = { cmp, opt: ovOpt, name: `${cur.id}-${cfg.name.split(" ")[0]}-${cfg.on}-${cfg.off}` };
          $("overlay-take").classList.remove("hidden");
        }
      }
      const seen = res ? res.levels.slice(0, 8).map(([l, ms]) => `${l ? "high" : "low"} ${ms}`).join(", ") : "";
      const vtext = ctlVerdict(cfg, st, r.pins);
      ctLast = (ok ? "Applied: " : "Sent, but not as asked: ") + vtext + (anyPack ? " The BoosterPack rows stay predicted until that board is on this one." : "");
      $("ct-verdict").textContent = ctLast;
      const pwText = pw ? (pw.pin ? `The pad was read ${pw.hi} of ${pw.n} samples high through the debugger (asked ${cfg.mode}). ` : pw.steady ? `PWM ${pw.setup.duty}% is a steady level: the pad read ${pw.hi} of ${pw.n} samples high through the debugger. ` : `PWM timer read through the debugger: load ${pw.regs.load} (period ${pw.setup.period}), compare ${pw.regs.cc}, enabled ${pw.regs.ctrctl & 1 ? "yes" : "no"}; ${pw.distinct} different counter values in ${pw.n} samples, ${pw.share.toFixed(0)}% of them above the compare value (asked ${pw.setup.duty}% high). The pad's own waveform is not observed here. `) : "";
      show(ok, ok ? "Applied: " + vtext : "Sent, but not as asked: " + vtext,
        `${res ? `Read through the debugger: ${seen} (ms). ` : ""}${pwText}Status block at ${r.status.addr}, settings ${cfg.seq}; ${secs} s in all.`,
        cfg.mode === "toggle" ? "Change the times or the pin and press Send again: only your settings are rewritten." : cfg.scan || cfg.reads.length ? "Put another part on the bus and send again." : "Change a row and press Send again.",
        res ? res.levels.slice(0, 12) : null, false,
        { keepOverlay: true, stats: [[cfg.pin !== null ? cfg.name.split(" ")[0] : "none", "pin"], [cfg.pin === null ? "idle" : cfg.mode === "toggle" ? `${cfg.on} / ${cfg.off} ms` : cfg.mode === "pwm" ? `${pwmSetup(cfg.pwmHz, cfg.pwmDuty).actualHz.toFixed(0)} Hz ${cfg.pwmDuty}%` : cfg.mode, "mode"], [cfg.scan ? `${st.scan.length} found` : cfg.reads.length ? `${answers.filter((a) => a.result === "acknowledged").length} of ${cfg.reads.length} read` : "idle", "bus"]] });
      return;
    }
    if (r.check.kind === "colors") {
      const has = await s.m.holds(img.segments);
      if (!has) {
        keep(await s.save(), id, log); saved = true;
      }
      writing = true;
      const sent = await s.sendColors(img, r.table, steps);
      await s.run();
      const res = await s.checkColors(r.table, steps);
      const secs = ((performance.now() - t0) / 1000).toFixed(1);
      const seen = res.levels.slice(0, 8).map(([c, ms]) => `${colorName(c)} ${ms}`).join(", ");
      if (res.ok) {
        show(true, "LED2 is playing your sequence.", `Read through the debugger: ${seen} (ms). ${sent.flashed ? "Program written, then your" : "Your"} sequence written in ${(sent.ms / 1000).toFixed(1)} s; ${secs} s in all.`,
          "Change a color or a time and press Send again: only your sequence is rewritten, so it plays about a second later.", res.levels.slice(0, 16), true,
          { asked: steps, askedText: `${steps.length} steps`, didText: "read from the LED pins", stats: [[`${steps.length}`, "steps"], [`${(sent.ms / 1000).toFixed(1)} s`, "to send"]] });
      } else {
        show(false, "Sent, but LED2 did not play the sequence as asked.", `It did: ${seen || "no change at all"} (ms).`,
          "Check LED2's jumpers, then send it again.", res.levels.slice(0, 16), true);
      }
      return;
    }
    keep(await s.save(), id, log); saved = true;
    writing = true;
    const flashMs = await s.flashImage(img);
    await s.run();
    const secs = () => ((performance.now() - t0) / 1000).toFixed(1);
    if (r.check.kind === "oob") {
      await s.end(); s = null;      // the debugger idle, so the serial port carries the demo's messages
      show(true, "TI's out-of-box demo is on the board.", `Written and read back in ${(flashMs / 1000).toFixed(1)} s; ${secs()} s in all.`,
        "Now press Light sensor or Thermometer above.");
      return;
    }
    if (r.check.kind === "coremark") {
      const say = sayTo(log);
      const res = await s.readCoreMark(r.check, (t) => say(`Running CoreMark on the chip: ${t} s`));
      if (res.score && res.valid) {
        show(true, `CoreMark ${res.score} on the ${cur.kit} at ${r.check.clock_mhz} MHz (${(res.score / r.check.clock_mhz).toFixed(2)} per MHz).`,
          `${r.source.split(";")[0]}, ${r.check.iterations} iterations timed in CPU clock ticks, read from the chip's memory after ${res.seconds} s. CoreMark checked its own results ("Correct operation validated"). Our earlier measurement on this board: ${r.check.expect_score}. ${secs()} s in all.`,
          "Each bar is the best score we measured on that board, at its own clock speed and with its own compiler: a rough guide, not a contest.");
        $("stats").innerHTML = `<div><b>${res.score}</b><span>CoreMark</span></div><div><b>${(res.score / r.check.clock_mhz).toFixed(2)}</b><span>per MHz</span></div><div><b>${secs()} s</b><span>in all</span></div>`;
        drawCompare(r.compare, cur.id, res.score);
      } else {
        show(false, "CoreMark did not report a score in time.", res.text ? "It wrote: " + res.text.slice(-160) : "Nothing was written to its log.",
          "Flash it again; if it fails twice, use the terminal route.");
      }
      return;
    }
    if (r.check.kind === "pattern") {
      const res = await s.check(r.check.pattern);
      const on = res.levels.findIndex(([l]) => l);   // start the strip at the first "on", like the pattern asked for
      if (on > 0) res.levels = res.levels.slice(on);
      const seen = res.levels.slice(0, 8).map(([l, ms]) => `${l ? "on" : "off"} ${ms}`).join(", ");
      if (res.ok && res.outputEnabled) show(true, `It works: the pin that drives ${r.check.led.label} repeats ${r.check.pattern.join(", ")} ms.`,
        `Read through the debugger: ${seen} (ms). ${r.bytes.toLocaleString()} bytes written and read back in ${(flashMs / 1000).toFixed(1)} s; ${secs()} s in all.`,
        "Measured on the pin: the page read the LED pin's output register while the program ran. The LED itself lights only with its jumper on.", res.levels.slice(0, 12), false,
        { asked: r.check.pattern.map((ms, i) => [i % 2 ? 0 : 1, ms]), askedText: r.check.pattern.join(" · ") + " ms", didText: res.levels.slice(0, 4).map(([, ms]) => ms).join(" · ") + " ms",
          stats: [[`${Math.floor(res.levels.length / r.check.pattern.length)}`, "cycles seen"], [`${(flashMs / 1000).toFixed(1)} s`, `to write ${r.bytes.toLocaleString()} bytes`]] });
      else show(false, "Flashed, but the LED did not follow the pattern.", `It did: ${seen || "no change at all"} (ms).`, "Check LED2's jumpers, then flash again.", res.levels.slice(0, 12), false);
    } else {
      // Let go of the debugger first: while the page drives it, the XDS110 passed the board's serial output on
      // late or not at all (seen 2026-10-14), so the message is read with the debugger idle.
      await s.end(); s = null;
      step(log, `Reading the board's serial port for ${r.check.seconds} s`);
      const text = await readSerial(port, r.check.baud, r.check.seconds, (t) => messageCheck(t, r.check.message).ok);
      const res = messageCheck(text, r.check.message);
      if (res.ok) show(true, `It works: the board prints "> ${r.check.message} N", counting.`, `Read from the board's serial port: ${res.lines.join(" | ")}. ${secs()} s in all.`,
        "To print your own words, change the message with the terminal route or an assistant (Go further).");
      else show(false, "Flashed, but your message did not arrive.", res.lines.length ? `The port showed: ${res.lines.join(" | ")}` : "Nothing arrived.",
        "Check J16 and J17 are on XDS (right), and that you picked the port whose name ends in 1.");
    }
  } catch (e) {
    const fe = e instanceof FlashError ? e : new FlashError("other", e.message || String(e), "unplug the board, plug it back in, reload this page and try again");
    { const d = deviceError(e, { api: r.check.kind === "uart" && !port ? "serial" : "hid" }); if (d) { fe.message = d.message; fe.next = d.next; fe.fold = d.fold; openEnv(d.fold); } }
    if (fe.kind === "wrong-chip" || fe.kind === "connect") device = null;
    trail.add("stopped", { at: lost ? lost.phase : phase, kind: fe.kind, reason: fe.message, lost: !!lost });
    if (lost) {
      const im = interruptedMessage(lost.phase, { saved: lost.saved, kit: lost.kit });
      show(false, "Stopped: " + im.message, "", im.next);
    } else show(false, "Stopped: " + fe.message, !writing ? ""
      : saved ? "It stopped while writing, so the board may hold a partly written program. The copy saved before the write is in your Downloads folder: \"Put back what was there\" restores it."
      : "It stopped while writing your sequence. The board already held this page's color player, so no copy was saved this time: press Send again.", fe.next);
  } finally {
    if (s) await s.end().catch(() => {});
    phase = "idle"; lastResult = { demo: r.id, state: $("result-card").dataset.state, verdict: $("verdict").textContent.slice(0, 200) }; trail.add("result", lastResult);
    $("go").disabled = false; estStop($("result-card").dataset.state !== "fail");
    if ($("result-card").dataset.state === "run") resultState("");
  }
}

async function restore() {
  if (thermo.s) { thermoStop(); while (thermo.s) await ui.sleep(50); }   // let the live reader release the debugger first
  const list = $("rlog"); list.innerHTML = ""; const out = $("rresult"); out.classList.add("hidden");
  $("restore").disabled = true;
  let s; phase = "idle"; lost = null; trail.add("put-back pressed");
  try {
    const f = $("file").files[0];
    const chk = f ? await checkBackup(await f.text(), f.name, cur, sha256) : { ok: false, message: "choose the .json copy first" };
    if (!chk.ok) throw new FlashError("image", chk.message.replace(/\.$/, ""), "");
    const copy = chk.copy;
    step(list, restoreEstimate(copy.bytes).text);
    { let est = 0; try { est = estimateSeconds(sessionBoard(cur, null), { bytes: Math.round(copy.bytes / 4), total: copy.bytes, read: copy.bytes }); } catch (e) { est = 0; } estBegin(est, ["rest-est", "rest-est-bar", "rest-est-text"]); }
    ({ s } = await openBoard(null, sayTo(list)));
    const ms = await s.restore(copy);
    await s.run().catch(() => {});   // the copy is already back and read back; a slow restart is not a failure
    out.textContent = `Put back and read back: the board's flash matches the copy saved ${copy.saved.slice(0, 16).replace("T", " ")} UTC (${(ms / 1000).toFixed(1)} s).`;
    out.className = "result ok";
  } catch (e) {
    const at = lost ? lost.phase : phase;
    if (lost || (e.kind === "connect" && at === "restoring")) { const im = interruptedMessage("restoring", { kit: cur ? cur.kit : "the board" }); out.textContent = `Stopped: ${im.message} ${im.next}`; }
    else out.textContent = "Stopped: " + (e.message || e) + (e.next ? `. ${e.next[0].toUpperCase()}${e.next.slice(1)}` : "");
    out.className = "result bad"; trail.add("put-back stopped", { at, kind: e.kind || e.name, reason: e.message || String(e) });
  } finally {
    estStop(out.className.includes("ok"));
    if (out.className.includes("ok")) trail.add("put back", { verified: true });
    phase = "idle";
    if (s) await s.end().catch(() => {});
    $("restore").disabled = !fileOk;
  }
}

// ---------------------------------------------------------------- Go deeper (one short block under the stage)
function drawDeeper(r) {
  const box = $("deeper-links"); box.innerHTML = "";
  const kit = stageBoard().kit, kb = kitJson && kitJson.boards && kitJson.boards[kit];
  for (const [label, href, why] of deeperLinks(kit, kb && kb.folder)) {
    const a = document.createElement("a"); a.href = href; a.target = "_blank"; a.rel = "noopener"; a.textContent = label; a.title = why; box.appendChild(a);
  }
}

// ---------------------------------------------------------------- Change: the shared parameter block (README.md, tools/webflash/params.py)
function drawParams(r) {
  const p = r.params, box = $("c-params");
  box.classList.toggle("hidden", !p);
  if (!p) return;
  const rows = $("cp-rows"); rows.innerHTML = "";
  p.items.forEach((it, i) => {
    const l = document.createElement("label"), t = document.createElement("span"), inp = document.createElement("input");
    t.textContent = `${it.label} (${it.unit})`;
    if (it.note) { const sm = document.createElement("small"); sm.textContent = `${it.note}; ${it.min} to ${it.max}`; t.appendChild(sm); }
    inp.type = "number"; inp.min = it.min; inp.max = it.max; inp.step = it.step; inp.value = it.default; inp.dataset.i = i;
    l.append(t, inp); rows.appendChild(l);
  });
  $("cp-note").textContent = `Written to ${p.addr}, the last flash sector. The program reads it at start; a block for another program is ignored.`;
}
$("cp-reset").addEventListener("click", () => { if (chosen && chosen.params) chosen.params.items.forEach((it, i) => { $("cp-rows").children[i].querySelector("input").value = it.default; }); });
// The radio boards: a change is a whole write of the image with the settings sector filled (imageWithParams, then program), about 3 s for a small demo.
async function sendParamsCc(r, vals) {
  const log = $("log"); log.innerHTML = ""; resultState("run"); $("go").disabled = true; $("cp-send").disabled = true;
  const t0 = performance.now(); let t = null, sc = null, dap = null, writing = false;
  try {
    if (!device) throw new FlashError("no-device", "connect the board first (step 1)", "then press Send again");
    const map = Object.fromEntries(r.params.items.map((it, i) => [it.id, vals[i]]));
    const img = imageWithParams(parseHex(await (await fetch(r.hex)).text()), r.params, map);
    t = transportFor(device); await t.open(); dap = new Dap(t); dap.dormant = true;
    const fam = FAMILY[cur.saci] || FAMILY.cc27;
    sc = new Saci(dap, (x) => { if (!/^NOP/.test(x)) step(log, x); }, fam);
    await dap.connect();
    step(log, "Putting the chip in its programming mode"); await sc.enter();
    const say = sayTo(log); writing = true;
    await sc.program(img, (q) => say(`Writing main flash ${Math.round(q * 100)}%`));
    writing = false;
    const st = await sc.bootAndWait();
    const lines = r.params.items.map((it, i) => paramsLine(it, vals[i])).join("; ");
    show(st.ahbOpen, st.ahbOpen ? "Sent: the demo restarted with your settings." : "Written, but the chip's debug port did not open.", `${lines}. The image was written again with your values and checked by the chip's own CRC32; ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`,
      "Change a value and press Send again.", null, false, { stats: [[`${vals.length}`, "values sent"]] });
  } catch (e) {
    const fe = e instanceof FlashError ? e : new FlashError("other", e.message || String(e), "unplug the board, plug it back in and try again");
    show(false, "Stopped: " + fe.message, writing ? "It stopped while writing: the board may hold a partly written program. Press Send again to write it whole." : "", fe.next);
  } finally {
    if (sc) await sc.close().catch(() => {}); else if (dap) await dap.disconnect().catch(() => {});
    if (t) await t.close().catch(() => {});
    $("cp-send").disabled = false; $("go").disabled = !cur || !!readOnlyReason(cur);
  }
}
async function sendParams() {
  const r = chosen, p = r && r.params; if (!p) return;
  if (thermo.s) { thermoStop(); while (thermo.s) await ui.sleep(50); }
  const vals = paramsClamp(p.items, [...$("cp-rows").querySelectorAll("input")].map((x) => x.value));
  [...$("cp-rows").querySelectorAll("input")].forEach((x, i) => { x.value = vals[i]; });
  if (r.hex) return sendParamsCc(r, vals);
  const log = $("log"); log.innerHTML = ""; resultState("run"); $("go").disabled = true; $("cp-send").disabled = true;
  const t0 = performance.now(); let s, saved = false, writing = false;
  try {
    const img = await imageOf(r); let id;
    ({ s, id } = await openBoard(r, sayTo(log)));
    const has = await s.m.holds(img.segments);
    if (has) step(log, "The program is already on the board: only your settings are written");
    else { keep(await s.save(), id, log); saved = true; writing = true; await s.flashImage(img); writing = false; }
    step(log, "Writing your settings into the last flash sector");
    writing = true;
    await s.m.writeSector(n(p.addr), paramsWords(p, vals));
    writing = false;
    await s.run();
    const lines = p.items.map((it, i) => paramsLine(it, vals[i])).join("; ");
    let ok = true, head = "Sent: the program restarted with your settings.", detail = `${lines}. Written and read back from flash; ${((performance.now() - t0) / 1000).toFixed(1)} s in all.`;
    if (r.check.kind === "pattern") {
      const res = await s.check(vals);
      ok = res.ok && res.outputEnabled;
      const seen = res.levels.slice(0, 8).map(([l, ms]) => `${l ? "on" : "off"} ${ms}`).join(", ");
      head = ok ? `It works: the LED repeats ${vals.join(", ")} ms.` : "Sent, but the LED did not follow the new pattern.";
      detail += ` Read through the debugger: ${seen} (ms).`;
    }
    show(ok, head, detail, r.check.kind === "digit" || r.check.kind === "occ" ? "Press the red button to start the reader again; the program now uses your settings." : "Change a value and press Send again: only the settings sector is rewritten.",
      null, false, { stats: [[`${vals.length}`, "values sent"]] });
  } catch (e) {
    const fe = e instanceof FlashError ? e : new FlashError("other", e.message || String(e), "unplug the board, plug it back in and try again");
    show(false, "Stopped: " + fe.message, writing ? "It stopped while writing the settings sector; the program itself is untouched. Press Send again." : "", fe.next);
  } finally { if (s) await s.end().catch(() => {}); $("cp-send").disabled = false; $("go").disabled = !cur || !!readOnlyReason(cur); }
}
$("cp-send").addEventListener("click", guard(asWrite(sendParams)));

// ---------------------------------------------------------------- the room-occupancy model (occ.c): pick one of its 20 stored windows
const occ = { s: null, live: false, wins: null, pending: null, sel: -1, addr: 0 };
async function occWindows() {
  if (!occ.wins) { try { occ.wins = await (await fetch("occ-windows.json", { cache: "no-store" })).json(); } catch (e) { occ.wins = null; } }
  return occ.wins;
}
function occDraw(i) {
  const cv = $("oc-chart"), g = cv.getContext("2d"), w = occ.wins; g.clearRect(0, 0, cv.width, cv.height);
  if (!w || i < 0) return;
  const cols = ["#B42418", "#6B625B", "#2F6B4F", "#161312"], names = w.channels;
  for (let c = 0; c < 4; c++) {
    const ys = w.x[i].slice(c * 32, c * 32 + 32), lo = Math.min(...ys), hi = Math.max(...ys), span = hi - lo || 1;
    g.strokeStyle = cols[c]; g.lineWidth = 2; g.beginPath();
    ys.forEach((v, k) => { const x = 10 + k * (cv.width - 20) / 31, y = cv.height - 10 - (v - lo) / span * (cv.height - 24); if (k) g.lineTo(x, y); else g.moveTo(x, y); });
    g.stroke(); g.fillStyle = cols[c]; g.font = "12px Barlow, sans-serif"; g.fillText(`${names[c]}: ${lo.toFixed(2)} to ${hi.toFixed(2)}`, 10 + c * 155, 12);
  }
}
async function occInit(r) {
  occ.live = false; occ.sel = -1; occ.pending = null;
  const box = $("oc-wins"); box.innerHTML = "";
  await occWindows();
  const lab = (r.check.labels || []);
  for (let i = 0; i < 20; i++) {
    const b = document.createElement("button"); b.type = "button"; b.textContent = String(i + 1); b.disabled = true; b.title = `window ${i + 1}: ${lab[i]} in the room`; b.setAttribute("aria-pressed", "false");
    b.addEventListener("click", () => { if (!occ.live) return; occ.sel = i; occ.pending = i; box.querySelectorAll("button").forEach((x, k) => x.setAttribute("aria-pressed", String(k === i))); occDraw(i); $("oc-true").textContent = String(lab[i]); $("oc-ans").textContent = "…"; $("oc-time").textContent = "Asking the chip…"; });
    box.appendChild(b);
  }
  $("oc-ans").textContent = "?"; $("oc-true").textContent = "?"; $("oc-time").textContent = "Pick a window."; occDraw(-1);
  $("oc-lock").hidden = false; $("oc-lock").textContent = cur ? "Press the red button to put the model on the board. The windows wake up when the chip has read its own 20." : "Connect a board first, then start the model.";
}
async function occAsk(s, addr, v) {
  await s.dap.write32(addr + 36, v);
  const seq = ((await s.dap.read32(addr + 28)) + 1) >>> 0;
  await s.dap.write32(addr + 28, seq);
  const t0 = performance.now();
  while (performance.now() - t0 < 3000) {
    if ((await s.dap.read32(addr + 32)) === seq) { const w = await s.dap.readBlock(addr + 40, 2); return { pred: w[0], cycles: w[1] }; }
    await ui.sleep(20);
  }
  throw new DgTimeout("the chip did not answer within 3 s");
}
async function occLive(s, addr, clk, r) {
  thermo.s = s; thermo.run = true; occ.live = true; occ.pending = null;
  $("oc-lock").hidden = true; $("oc-wins").querySelectorAll("button").forEach((b) => { b.disabled = false; });
  let why = "";
  try {
    while (thermo.run) {
      if (occ.pending !== null) {
        const v = occ.pending; occ.pending = null;
        const a = await occAsk(s, addr, v);
        $("oc-ans").textContent = String(a.pred);
        aiState = { ...aiState, cycles: a.cycles, pred: a.pred, clkMhz: clk / 1e6 }; renderAi();
        const t = (r.check.labels || [])[v];
        setLive("Head count", "the model's answer", String(a.pred)); setLive("Cycles", "CPU cycles for that answer", a.cycles.toLocaleString("en-US"));
        $("oc-time").textContent = `Window ${v + 1}: the chip says ${a.pred}, the room had ${t}${a.pred === t ? "" : " (a miss)"}. ${(a.cycles / clk * 1000).toFixed(1)} ms (${a.cycles.toLocaleString()} cycles at ${clk / 1e6} MHz).`;
      }
      await ui.sleep(40);
    }
  } catch (e) { why = "Stopped: " + (e.message || e) + "."; $("oc-time").textContent = why + " Press the red button to start it again."; }
  finally {
    thermo.run = false; thermo.s = null; occ.live = false;
    $("oc-wins").querySelectorAll("button").forEach((b) => { b.disabled = true; });
    $("oc-lock").hidden = false; $("oc-lock").textContent = why || "The model is stopped. Press the red button to start it again.";
    await s.end().catch(() => {});
  }
}

// ---------------------------------------------------------------- start

$("connect").addEventListener("click", guard(async () => {
  try { found("Looking at the board…", "busy"); screen(4, "reading the chip…"); trail.add("connect pressed"); await connectBoard(); }
  catch (e0) { const e = devErr(e0, USE_USB ? "usb" : "hid"); trail.add("connect stopped", { kind: e.kind || e0.name, reason: e.message || String(e) }); found((e.kind === "chooser" ? "" : "Stopped: ") + sentence(e.message || String(e)) + (e.next ? ` ${sentence(e.next[0].toUpperCase() + e.next.slice(1))}` : ""), "bad", e.fold); if (!device) screen(4, "no board yet"); }
}));
$("survey").addEventListener("click", guard(survey));
// "Download a diagnostic record": the steps of this visit for a problem report (diagnostics.js keeps out flash contents,
// Wi-Fi details, addresses, typed text and full serials).
document.querySelectorAll(".diag-btn").forEach((b) => b.addEventListener("click", () => {
  const rec = diagnosticRecord(trail, { userAgent: navigator.userAgent, platform: navigator.platform, secure: isSecureContext,
    webhid: "hid" in navigator, webusb: "usb" in navigator, webserial: "serial" in navigator, page: location.pathname.split("/").slice(-3).join("/") },
  { board: cur ? { id: cur.id, kit: cur.kit, status: statusOf(cur), serial } : null, demo: chosen ? { id: chosen.id, kind: chosen.check.kind } : null, lastResult });
  download(diagnosticName(), JSON.stringify(rec, null, 2));
  trail.add("diagnostic downloaded");
}));
// Naming the boards the page may already open (reads only). Guarded, so a Connect press during it is ignored rather than racing it for the probe.
// A message that tells the person how to recover (an unplug part way, a refused probe) is put back afterwards, not cleared.
const autoSurvey = guard(async () => {
  const keepText = $("found").dataset.state === "bad" ? $("found").textContent : "";
  found("Reading the boards this page may already open…", "busy");
  const was = $("connect").disabled; $("connect").disabled = true;   // a press during the reads would be dropped by guard(): show it is not ready
  try { await survey(); } finally { $("connect").disabled = was; if (!device) found(keepText, keepText ? "bad" : ""); }
});
$("th-stop").addEventListener("click", thermoStop);
$("dg-canvas").addEventListener("pointerdown", (e) => { if (!dg.live) return; dg.down = true; clearTimeout(dg.timer); $("dg-canvas").setPointerCapture(e.pointerId); dgPoint(e); });
$("dg-canvas").addEventListener("pointermove", (e) => { if (dg.down) dgPoint(e); });
for (const ev of ["pointerup", "pointercancel"]) $("dg-canvas").addEventListener(ev, dgLift);
$("dg-clear").addEventListener("click", dgClear);
$("dg-start").addEventListener("click", () => { if (!$("go").disabled) $("go").click(); });
// For tools/webflash/bench-test.mjs: a whole 72 x 72 grid as if drawn, then lifted (it cannot draw with a mouse that fast).
window.addEventListener("frl-test-digit", (e) => {
  dgClear(); dg.grid.set(e.detail);
  const g = $("dg-canvas").getContext("2d"); g.fillStyle = "#F7F3EC";
  e.detail.forEach((v, i) => { if (v) g.fillRect((i % DG_N) * DG_CELL, Math.floor(i / DG_N) * DG_CELL, DG_CELL, DG_CELL); });
  dg.down = true; dgLift();
});
for (const api of [navigator.hid, navigator.usb]) if (api && api.addEventListener) api.addEventListener("disconnect", (e) => {
  if (e.device !== device) { trail.add("unplugged", { which: "another probe" }); drawBoards(); return; }
  const kit = cur && cur.kit, mid = phase !== "idle";
  if (mid) lost = { phase, saved: phaseSaved, kit: kit || "the board" };
  trail.add("unplugged", { which: "the board in use", during: phase, kit });
  known.delete(e.device); clearConnection(mid);
  const im = interruptedMessage(mid ? phase : "connecting", { saved: phaseSaved, kit: kit || "the board" });
  found(mid ? `${im.message} ${im.next}` : `Unplugged: ${kit || "the board"}. Plug it back in and press Connect.`, "bad");
  scene("idle", ["bench", "board unplugged", "", "", "plug the cable in"]); drawBoards();
});
for (const api of [navigator.hid, navigator.usb]) if (api && api.addEventListener) api.addEventListener("connect", () => { trail.add("plugged in"); drawBoards().then((d) => { if (d.length && !device && !busy) autoSurvey().catch(() => {}); }).catch(() => {}); });
document.querySelectorAll("[data-oob]").forEach((b) => b.addEventListener("click", () => oobCommand(b.dataset.oob)));
$("add-step").addEventListener("click", () => { steps.push([steps.length ? steps[steps.length - 1][0] : 1, 300]); drawSteps(); });
$("file").addEventListener("change", fileChanged);
$("restore").addEventListener("click", guard(asWrite(restore)));
document.querySelectorAll("[data-copy]").forEach((b) => b.addEventListener("click", async () => {
  await navigator.clipboard.writeText($(b.dataset.copy).textContent);
  const t = b.textContent; b.textContent = "Copied"; setTimeout(() => { b.textContent = t; }, 1500);
}));

async function loadData(path, field) {
  const response = await fetch(path, { cache: "no-store" });
  if (!response.ok) throw new Error(`${path}: HTTP ${response.status}. Pull the complete repository and restart the local server.`);
  let value;
  try { value = await response.json(); }
  catch { throw new Error(`${path}: expected JSON data. Check the server address and pull the complete repository.`); }
  if (!Array.isArray(value[field]) || !value[field].length) throw new Error(`${path}: missing ${field} data. Pull the complete repository.`);
  return value;
}

export const ready = (async () => {
  data = await loadData("boards.json", "boards");
  boards = data.boards;
  try { predicted = (await loadData("boards-predicted.json", "boards")).boards.filter((p) => !boards.some((b) => b.id === p.id)); } catch (e) { predicted = []; }
  boards = [...boards, ...predicted];   // tested entries first: identify() takes the first match
  catalogue = (await loadData("catalog.json", "boards")).boards;
  for (const b of predicted) if (!catalogue.some((c) => c.id === b.id)) catalogue.push({ id: b.id, kit: b.kit, recognized: true, browserFlash: false, demos: 0, readme: "docs/inventory.md", page: null,
    status: "predicted; named and read here, never written", note: b.note, predicted: true });
  allRecipes = (await loadData("recipes.json", "recipes")).recipes;
  try { demoData = await (await fetch("demos.json", { cache: "no-store" })).json(); } catch (e) { demoData = { projects: [] }; }
  try { pinouts = (await (await fetch("pinouts.json", { cache: "no-store" })).json()).boards; } catch (e) { pinouts = {}; }
  try { bestData = await (await fetch("best.json", { cache: "no-store" })).json(); } catch (e) { bestData = {}; }
  try { kitJson = await (await fetch("kit.json", { cache: "no-store" })).json(); } catch (e) { kitJson = null; }
  const order = { pattern: 0, button: 0.4, breathe: 0.6, colors: 1, controls: 1.5, thermo: 2, digit: 3, occ: 3.1, loop: 4, uart: 4, oob: 5, coremark: 9 };
  allRecipes.sort((a, b) => order[a.check.kind] - order[b.check.kind]);
  if (!("serial" in navigator)) allRecipes = allRecipes.filter((r) => r.check.kind !== "uart" && r.check.kind !== "oob");
  drawRecipes(); drawRecovery();
  $("result-card").classList.add("hidden");
  if (!device) found("", "");
  if (!("hid" in navigator) && !("usb" in navigator)) { const m = missingApi("hid", isSecureContext); $("un-what").textContent = m.message; $("un-next").textContent = m.next; $("unsupported").classList.remove("hidden"); wireUnsupportedShare(); found("No board can be connected from this browser (see above).", "bad", m.fold); $("connect").disabled = true; $("go").disabled = true; return; }
  $("connect").disabled = false;
  // A blocked permission lookup must not prevent the explicit Connect chooser.
  try { const devs = await drawBoards(); if (devs.length && !device) autoSurvey().catch(() => {}); }   // name every board the page may already open: reads only, nothing is written
  catch (e) { found("Saved device permissions could not be read. Press Connect to choose a board. " + (e.message || e), "bad"); }
})();
