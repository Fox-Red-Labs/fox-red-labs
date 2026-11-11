// The stage beside the demo list (concept 5): tabs, the Share menu's links, the Edge AI numbers and the "Before it writes" plan,
// as pure functions. No page, no board.

export const REPO = "https://github.com/Fox-Red-Labs/fox-red-labs";
export const TABS = ["see", "change", "talk", "live", "ai"];

/** Which tabs a demo has. A tab that does not apply is greyed with the reason; Edge AI appears only where the demo is a model. */
export function tabState(kind, serialOk = true) {
  const talk = kind === "uart";
  return {
    see: { show: true, on: true, why: "" },
    change: { show: true, on: true, why: "" },
    talk: { show: true, on: talk && serialOk, why: talk ? (serialOk ? "" : "this browser cannot open a serial port") : "not for this demo" },
    live: { show: true, on: true, why: "" },
    ai: { show: kind === "digit" || kind === "occ", on: kind === "digit" || kind === "occ", why: "" },
  };
}

/** Say what the Change tab edits: values in a program already on the board, or only the source. */
export function changeNote(kind, params) {
  if (params && params.items && params.items.length) return "Send writes your values into the board's last flash sector and restarts the program. No compiler runs, and the program itself is not rewritten.";
  if (kind === "colors" || kind === "controls") return "Send patches the values in the program already on the board. No compiler runs.";
  if (kind === "ble") return "The light is set over Bluetooth from this page. No cable and no write.";
  return "Nothing here is changed from this page. The source change below needs a rebuild.";
}

export const shareLink = (base, boardId, recipeId) => `${base}?board=${encodeURIComponent(boardId)}&recipe=${encodeURIComponent(recipeId)}`;
export const reportUrl = (boardId, slug, title) => `${REPO}/issues/new?` + new URLSearchParams({ template: "built-it.yml", title: `Built it: ${title}`, board: boardId, project: slug || "" }).toString();

/** The Edge AI tab, about the board in hand only. s: {model, bytes, flashKb, ramKb, selftest, nTest, clkMhz, cycles, pred}. */
export function aiRows(s) {
  const rows = [["Model", s.model || "BitNetMCU cnn_64"]];
  if (s.bytes && s.flashKb) rows.push(["Program size", `${(s.bytes / 1024).toFixed(1)} kB of ${s.flashKb} kB flash (${(100 * s.bytes / 1024 / s.flashKb).toFixed(0)} %)`]);
  if (s.ramKb) rows.push(["RAM", `${s.ramKb} kB on this chip; the model's own use is not measured here`]);
  rows.push(["Self-test", s.selftest === undefined ? "not run yet: start the digit reader" : `${s.selftest} of ${s.nTest} of its built-in test digits read right`]);
  if (s.cycles && s.clkMhz) rows.push(["Last answer", `${s.cycles.toLocaleString("en-US")} CPU cycles, ${(s.cycles / (s.clkMhz * 1000)).toFixed(1)} ms at ${s.clkMhz} MHz${s.pred === undefined ? "" : `, read ${s.pred}`}`]);
  else rows.push(["Last answer", "draw a digit once the reader runs"]);
  return rows;
}

/** "Before it writes": the facts the page already holds, as label and text rows. lines: summaryFor(board, recipe).lines. */
export function planRows(kit, serial, recipe, lines) {
  const rows = [["Board", `${kit}${serial ? `, XDS110 ${serial}` : ""}; the page reads the chip again first and stops if it is not this one`]];
  rows.push(["Will write", recipe.bytes ? `${recipe.title}: ${recipe.bytes.toLocaleString("en-US")} bytes. A program already on the board that matches is not written again.` : recipe.title]);
  for (const [k, v] of lines || []) rows.push([k, v]);
  return rows;
}

// ---- the shared parameter block (tools/webflash/params.py, format in README.md)

/** Clamp what the person typed to each item's limits and step; a missing or non-numeric value is the default. */
export function paramsClamp(items, values) {
  return items.map((it, i) => {
    let v = Number(values && values[i]);
    if (!Number.isFinite(v)) v = it.default;
    v = Math.min(it.max, Math.max(it.min, Math.round(v)));
    return v;
  });
}

/** The words the page writes at the block's address: magic, format, count, image id, then the values. */
export function paramsWords(p, values) {
  return [parseInt(p.magic, 16) >>> 0, p.format, values.length, p.image_id, ...paramsClamp(p.items, values)];
}

/** The C line each value stands for, so the Change tab can show what was set (same idea as the controls card). */
export const paramsLine = (it, v) => `${it.label}: ${v} ${it.unit}`;

// ---- Pick an AI model: what the chip did beside what we measured

const fmt = (v) => Number(v).toLocaleString("en-US");

/** run: {right, nTest, cycles, clkMhz, bytes, ramB}. ai: the recipe's `ai`. Rows of [what, this run, our measured row]. */
export function aiCompare(ai, run) {
  const m = (ai && ai.measured) || {}, rows = [];
  const none = "no row for this board yet";
  if (ai && ai.family === "digit") {
    const c = (m.cycles || []).find((x) => run && x.clockMhz === run.clkMhz) || (m.cycles || [])[0];
    rows.push(["Cycles per answer", run && run.cycles ? fmt(run.cycles) : "run it first", c ? `${fmt(c.value)} at ${c.clockMhz} MHz (${c.row})` : none]);
    rows.push(["Time per answer", run && run.cycles && run.clkMhz ? `${(run.cycles / run.clkMhz / 1000).toFixed(1)} ms at ${run.clkMhz} MHz` : "", c ? `${c.ms} ms at ${c.clockMhz} MHz` : none]);
  } else if (ai) {
    const t = m.ms;
    rows.push(["Cycles per answer", run && run.cycles ? `${fmt(run.cycles)} (fastest of the 20 windows ${fmt(run.cycMin || 0)})` : "run it first", t ? `${(t.value * t.clockMhz * 1000).toLocaleString("en-US", { maximumFractionDigits: 0 })} cycles, from ${t.value} ms at ${t.clockMhz} MHz (${t.row})` : none]);
    rows.push(["Time per answer", run && run.cycles && run.clkMhz ? `${(run.cycles / run.clkMhz / 1000).toFixed(1)} ms at ${run.clkMhz} MHz` : "", t ? `${t.value} ms at ${t.clockMhz} MHz` : none]);
  }
  const img = ai && ai.image;
  const sz = m.size;
  rows.push(["Flash", img ? `${fmt(img.flash_b)} B for the whole image this page wrote` : "", sz && sz.flash ? `${fmt(sz.flash)} B (${sz.row}; ${ai.family === "digit" ? "whole image" : "the model alone"})` : none]);
  rows.push(["RAM", img ? `${fmt(img.ram_b)} B used by the image, stack included` : "", sz && sz.ram ? `${fmt(sz.ram)} B (${sz.row})` : none]);
  if (run && run.nTest) rows.unshift([ai.family === "digit" ? "Its ten stored digits" : "Its 20 stored windows", ai.family === "digit" ? `${run.right} of ${run.nTest} right` : `${run.right} of ${run.nTest} counts equal the true head count; ${run.sameRef} of ${run.nTest} match the reference answers exactly`, ai.accuracy]);
  return rows;
}

/** One line under a model card: the weights and what we measured on this board, only what has a row. */
export function aiCardLine(ai) {
  const m = (ai && ai.measured) || {};
  const bits = [ai.size];
  const c = (m.cycles || [])[0];
  if (c) bits.push(`${c.ms} ms at ${c.clockMhz} MHz, measured`);
  else if (m.ms) bits.push(`${m.ms.value} ms at ${m.ms.clockMhz} MHz, measured`);
  else bits.push("not measured on this board yet");
  return bits.join("; ");
}

// ---- best first

/** Recipes of one board in the order of best[boardId] (ids), the rest after in their given order; best ones get `best: true`. */
export function bestFirst(boardId, recipes, best) {
  const want = (best && best[boardId]) || [], rank = (r) => { const i = want.indexOf(r.id); return i < 0 ? 1e6 : i; };
  return recipes.map((r, i) => [r, i]).sort((a, b) => rank(a[0]) - rank(b[0]) || a[1] - b[1]).map(([r]) => ({ ...r, best: want.includes(r.id) }));
}

// ---- Go deeper

export const DEEPER_REPO = "https://github.com/Fox-Red-Labs/fox-red-labs";

/** One short block: the repo, the board's folder, its AGENTS.md, more demos and files. folder: the board folder in the repo ("" if none). */
export function deeperLinks(kit, folder) {
  const out = [["The code", DEEPER_REPO, "the Fox Red Labs repository (the reader copy)"]];
  if (folder) {
    out.push([`${kit} folder`, `${DEEPER_REPO}/tree/main/${folder}`, "scripts, results and notes for this board"]);
    out.push(["AGENTS.md", `${DEEPER_REPO}/blob/main/${folder}/AGENTS.md`, "what an assistant needs to build and run this board's demos"]);
  }
  out.push(["More demos", `${DEEPER_REPO}/tree/main/docs/builds`, "step-by-step builds"], ["Files", `${DEEPER_REPO}/tree/main/prebuilt`, "the images this page writes, with their notices"]);
  return out;
}

// ---- how long a write takes: said before it starts, then an elapsed timer that never looks frozen (flashing and putting back only; not on demo cards)

/** "about 25 seconds" / "about 2 minutes": from estimateSeconds() in mspm0-dap.js (per-family measured timing). */
export function estimateText(sec) {
  if (!(sec > 0)) return "a few seconds";
  if (sec < 8) return "a few seconds";
  if (sec < 90) return `about ${Math.round(sec / 5) * 5} seconds`;
  const m = Math.round(sec / 30) / 2;
  return `about ${m} minute${m === 1 ? "" : "s"}`;
}

/** One line and a bar fraction for the elapsed timer. The bar stops at 95 percent until the write ends; a board slower than the estimate is said so plainly. */
export function timeLine(est, elapsed) {
  const e = Math.max(0, Math.floor(elapsed)), over = est > 0 && elapsed > est * 1.5 + 3;
  const pct = est > 0 ? Math.min(95, Math.round(100 * elapsed / (est * 1.15))) : 0;
  const still = ["Still writing", "Still working, nothing is stuck", "Still going; keep the cable in"][Math.floor(elapsed / 4) % 3];
  const text = `${e} s so far of ${estimateText(est)}. ${over ? "This one takes a little longer than usual. " : ""}${still}.`;
  return { pct, text, over };
}

// ---- the pins of the chip, with the ones a demo uses marked

/** Pin names (PA17, PB3, ...) a demo's own record names: its LED and button, wiring table, colour pins, controls pins and, for a serial demo, the board's console jumpers. Nothing is guessed. */
export function usedPins(r, board) {
  const found = new Set();
  if (!r) return found;
  const grab = (v) => { for (const m of JSON.stringify(v || "").matchAll(/\bP[A-C]\d{1,2}\b/g)) found.add(m[0]); };
  grab(r.check && { led: r.check.led, button: r.check.button, from: r.check.from, to: r.check.to, pins: r.check.pins });
  grab(r.wiring); grab(r.table && r.table.pins); grab(r.pins);
  if (r.check && (r.check.kind === "uart" || r.check.kind === "oob") && board && board.console) grab(board.console.jumpers);
  return found;
}

/** Cells for the pin panel: {n, name, used}. pinout: pinouts.json boards[id]. */
export function pinCells(pinout, used) {
  return (pinout ? pinout.pins : []).map((p) => ({ n: p.n, name: p.name, used: used.has(p.name) }));
}

/** Counts one Digital Lab flash only when a program was really written and the run then passed.
 *  arm(slug) at the start of a run; wrote() when the write happens; verdict(ok) on every result, which always disarms. */
export function flashLedger(count) {
  let slug = null, written = false;
  return {
    arm(s) { slug = s || null; written = false; },
    wrote() { written = true; },
    verdict(ok) { const s = slug, w = written; slug = null; written = false; if (s && ok && w) { try { count(s); } catch (e) { /* counting is optional */ } } },
  };
}
