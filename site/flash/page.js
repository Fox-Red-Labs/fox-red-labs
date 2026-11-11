// The page's own code: buttons, messages and downloads. The board work is in mspm0-dap.js.
import { deviceError, missingApi } from "./bench/device-errors.js";
import { WebHidTransport, Session, elfSegments, sha256, FlashError } from "./mspm0-dap.js";

const $ = (id) => document.getElementById(id);
const n = (s) => parseInt(s, 16);
const ui = { now: () => performance.now(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)) };
let board, manifest, imageBuf;

function step(list, text) { const li = document.createElement("li"); li.textContent = text; list.appendChild(li); list.scrollTop = list.scrollHeight; }
function sayTo(list) { let last = ""; return (t) => { const k = t.replace(/\d+%$/, ""); if (k === last && list.lastChild) list.lastChild.textContent = t; else step(list, t); last = k; }; }

async function load() {
  const raw = await (await fetch("board.json")).json();
  board = { ...raw,
    identity: { addr: n(raw.identity.addr), deviceid: n(raw.identity.deviceid), deviceidMask: n(raw.identity.deviceidMask) },
    flash: { start: n(raw.flash.start), end: n(raw.flash.end) },
    led: { gpio: n(raw.led.gpio), bit: raw.led.bit, label: raw.led.label },
    gpio: { dout: n(raw.gpio.dout), doe: n(raw.gpio.doe) } };
  manifest = await (await fetch("demo/manifest.json")).json();
  imageBuf = await (await fetch("demo/" + manifest.image.file)).arrayBuffer();
  $("about").textContent = `The demo is TI's gpio_toggle_output example from the MSPM0 SDK ${manifest.tools.sdk.version} with one line changed (${manifest.source.edited_line}), built with TI Arm Clang ${manifest.tools.compiler.version}; image sha256 ${manifest.image.sha256.slice(0, 16)}…, ${manifest.image.segments[0].bytes} bytes. The page writes main flash only, never the boot configuration, and erases only the sectors the image needs.`;
}

async function pickDevice() {
  if (!navigator.hid) { const m = missingApi("hid", isSecureContext); throw new FlashError("browser", m.message, m.next); }
  const devs = await navigator.hid.requestDevice({ filters: [{ vendorId: board.usb.vendorId, productId: board.usb.productId, usagePage: board.usb.usagePage }] });
  if (!devs.length) throw new FlashError("no-device", "no device was chosen", "press the button again and pick XDS110 … CMSIS-DAP");
  return devs[0];
}

// The device the person picked first, then any other XDS110 this page was allowed before: the first that is
// an LP-MSPM0L1306 is used. Chrome shows every XDS110 under one name, so on a desk with several boards the
// first pick is often another board; nothing is written to a board that is not the one this page flashes.
async function openBoard(say) {
  const chosen = await pickDevice();
  const allowed = (await navigator.hid.getDevices()).filter((d) => d.vendorId === board.usb.vendorId && d.productId === board.usb.productId
    && (d.collections || []).some((c) => c.usagePage === board.usb.usagePage));
  const tried = [];
  for (const d of [chosen, ...allowed.filter((d) => d !== chosen)]) {
    const s = new Session(new WebHidTransport(d), board, { ...ui, say });
    try {
      const id = await s.start();
      return { s, id };
    } catch (e) {
      await s.end().catch(() => {});
      if (!(e instanceof FlashError) || e.kind !== "wrong-chip") throw e;
      tried.push(e);
    }
  }
  if (tried.length === 1) throw tried[0];
  throw new FlashError("wrong-chip", `none of the ${tried.length} XDS110s this page may use is on an ${board.kit}`,
    "nothing was written. Press the button again and pick another XDS110, or unplug the other boards");
}

function download(name, text) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([text], { type: "application/json" }));
  a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 5000);
}

function show(ok, verdict, detail, next, levels) {
  $("result").classList.remove("hidden");
  $("verdict").textContent = verdict;
  $("verdict").className = "result " + (ok ? "ok" : "bad");
  $("detail").textContent = detail || "";
  $("next").textContent = next || "";
  const bars = $("bars"); bars.innerHTML = "";
  if (levels && levels.length) {
    const total = levels.reduce((s, [, ms]) => s + ms, 0);
    for (const [lvl, ms] of levels) { const s = document.createElement("span"); s.className = lvl ? "on" : "off"; s.style.flex = String(ms / total); bars.appendChild(s); }
  }
}

$("go").addEventListener("click", async () => {
  const steps = $("steps"); steps.innerHTML = ""; $("result").classList.add("hidden");
  $("go").disabled = true;
  const t0 = performance.now();
  let s;
  try {
    if (await sha256(imageBuf) !== manifest.image.sha256) throw new FlashError("image", "the demo image does not match its checksum", "reload the page");
    const image = { segments: elfSegments(imageBuf), pattern: manifest.pattern_ms };
    let id;
    ({ s, id } = await openBoard(sayTo(steps)));
    const copy = await s.save();
    download(`${board.id}-${id.traceid}-${copy.saved.slice(0, 19).replace(/[:T]/g, "")}.json`, JSON.stringify(copy));
    step(steps, "Saved a copy of the board's flash (check your downloads)");
    const flashMs = await s.flashImage(image);
    await s.run();
    const r = await s.check(image.pattern);
    const secs = ((performance.now() - t0) / 1000).toFixed(1);
    const seen = r.levels.slice(0, 8).map(([l, ms]) => `${l ? "on" : "off"} ${ms}`).join(", ");
    if (r.ok && r.outputEnabled) {
      show(true, `It works: ${board.led.label} repeats ${manifest.pattern_ms.join(", ")} ms.`,
        `Timed through the debugger: ${seen} (ms), ${r.samples} reads in ${(r.span / 1000).toFixed(1)} s. Flash written and read back in ${(flashMs / 1000).toFixed(1)} s; ${secs} s in all.`,
        "Look at the board: LED2 flashes red twice, then pauses. Nobody had to look for the check to pass; the page read the LED pin.", r.levels.slice(0, 12));
    } else {
      show(false, "Flashed, but the LED did not follow the pattern.",
        r.outputEnabled ? `It did: ${seen || "no change at all"} (ms).` : `${board.led.label} is not set as an output.`,
        "Check the LED2 jumpers, then flash again. If it fails twice, use the terminal route.", r.levels.slice(0, 12));
    }
  } catch (e) {
    const fe = e instanceof FlashError ? e : new FlashError("other", e.message || String(e), "unplug the board, plug it back in, reload this page and try again");
    const d = deviceError(e, { api: "hid" });
    if (d) { fe.message = d.message; fe.next = d.next; }
    show(false, "Stopped: " + fe.message, "", fe.next);
  } finally {
    if (s) await s.end().catch(() => {});
    $("go").disabled = false;
  }
});

$("file").addEventListener("change", () => { $("restore").disabled = !$("file").files.length; });
$("restore").addEventListener("click", async () => {
  const list = $("rsteps"); list.innerHTML = ""; const out = $("rresult"); out.classList.add("hidden");
  $("restore").disabled = true;
  let s;
  try {
    const copy = JSON.parse(await $("file").files[0].text());
    ({ s } = await openBoard(sayTo(list)));
    const ms = await s.restore(copy);
    await s.run();
    out.textContent = `Put back and read back: the board's flash matches the copy saved ${copy.saved.slice(0, 16).replace("T", " ")} UTC (${(ms / 1000).toFixed(1)} s).`;
    out.className = "result ok";
  } catch (e) {
    const d = deviceError(e, { api: "hid" });
    out.textContent = d ? `${d.message} ${d.next}` : "Stopped: " + (e.message || e) + (e.next ? `. ${e.next}` : "");
    out.className = "result bad";
  } finally {
    if (s) await s.end().catch(() => {});
    $("restore").disabled = false;
  }
});

document.querySelectorAll("[data-copy]").forEach((b) => b.addEventListener("click", async () => {
  await navigator.clipboard.writeText($(b.dataset.copy).textContent);
  const t = b.textContent; b.textContent = "Copied"; setTimeout(() => { b.textContent = t; }, 1500);
}));

if (!("hid" in navigator)) { $("unsupported").classList.remove("hidden"); $("go").disabled = true; }
load().catch((e) => show(false, "The page could not load its demo image.", String(e), "reload the page"));
