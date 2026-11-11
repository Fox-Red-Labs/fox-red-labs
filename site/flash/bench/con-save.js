// Get the serial console's text and the live values out: copy all, or save the whole session (not only what is still on screen) as
// .txt with a time on each line, or .csv (time, direction, text). bench.js keeps the session in window.frlConLog;
// Clear empties it. Nothing here touches the board or the port. CSV cells, log limits and the .bin name come from
// export-safe.js (tested in tools/webflash/export-safe.test.mjs).
import { csvCell, partialNote, CON_MAX, LIVE_MAX, binName } from "./export-safe.js?v=20261025";
(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  if (!$("c-con-copy") && !$("c-live-csv")) return;
  const note = (s) => { $("c-con-saved").textContent = s; };
  const log = () => window.frlConLog || [];
  const iso = (t) => new Date(t).toISOString().replace("T", " ").replace("Z", "");
  // Received text arrives in chunks: split into lines, each stamped with the time its chunk arrived.
  function lines() {
    const out = [];
    let open = null;
    for (const e of log()) {
      const parts = String(e.text).replace(/\r\n?/g, "\n").split("\n");
      parts.forEach((p, i) => {
        if (open && open.dir === e.dir && i === 0) open.text += p;
        else { open = { t: e.t, dir: e.dir, text: p }; out.push(open); }
        if (i < parts.length - 1) open = null;
      });
    }
    for (const l of out) if (l.dir === "sent") l.text = l.text.replace(/^> /, "");   // the console shows what you typed as "> ..."
    return out.filter((l) => l.text !== "");
  }
  function kit() { return (($("cb-kit") || {}).textContent || "board").trim().replace(/[^A-Za-z0-9-]+/g, "-") || "board"; }
  function save(text, ext, type) {
    const a = document.createElement("a");
    const stamp = new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-");
    a.download = `${kit()}-serial-${stamp}.${ext}`;
    a.href = URL.createObjectURL(new Blob([text], { type }));
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    return a.download;
  }
  const empty = () => { if (!log().length) { note("Nothing received yet."); return true; } return false; };
  $("c-con-copy").addEventListener("click", async () => {
    if (empty()) return;
    const text = lines().map((l) => (l.dir === "sent" ? "> " : "") + l.text).join("\n") + "\n";
    try { await navigator.clipboard.writeText(text); note(`Copied ${lines().length} lines.`); }
    catch (e) { note("The browser did not allow copying. Use Save as .txt instead."); }
  });
  $("c-con-txt").addEventListener("click", () => {
    if (empty()) return;
    const text = lines().map((l) => `${iso(l.t)}  ${l.dir === "sent" ? "> " : ""}${l.text}`).join("\n") + "\n";
    note(`Saved ${save(text, "txt", "text/plain")} to your Downloads folder.${partialNote(log(), CON_MAX, "pieces of text")}`);
  });
  $("c-con-csv").addEventListener("click", () => {
    if (empty()) return;
    const text = "time_utc,direction,text\n" + lines().map((l) => [iso(l.t), l.dir, csvCell(l.text)].join(",")).join("\n") + "\n";
    note(`Saved ${save(text, "csv", "text/csv")} to your Downloads folder.${partialNote(log(), CON_MAX, "pieces of text")}`);
  });
  // Remember the console settings in this browser (a convenience: the page works the same without storage).
  for (const id of ["c-con-baud", "c-con-eol", "c-con-time", "c-con-hold"]) {
    const el = $(id); if (!el) continue;
    const key = "frl-con-" + id, box = el.type === "checkbox";
    try { const v = localStorage.getItem(key); if (v !== null) { if (box) el.checked = v === "1"; else el.value = v; } } catch (e) { /* no storage */ }
    el.addEventListener("change", () => {
      try { localStorage.setItem(key, box ? (el.checked ? "1" : "0") : el.value); } catch (e) { /* no storage */ }
      if (id === "c-con-baud" && /Close the serial port/.test(($("c-con-open") || {}).textContent || "")) note("Close and open the port to use the new rate.");
    });
  }
  // The copy saved before the last write, as a raw .bin that other tools load at its start address.
  const binRow = $("bin-row"), binBtn = $("bin-save");
  if (binRow && binBtn) {
    setInterval(() => { const c = window.frlLastCopy; if (binRow.hidden === !!c) binRow.hidden = !c; }, 1000);
    binBtn.addEventListener("click", () => {
      const c = window.frlLastCopy; if (!c) return;
      const raw = atob(c.data), bytes = new Uint8Array(raw.length);
      for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
      const start = "0x" + Number(c.start).toString(16).toUpperCase().padStart(8, "0");
      const a2 = document.createElement("a");
      a2.download = binName(c);
      a2.href = URL.createObjectURL(new Blob([bytes], { type: "application/octet-stream" }));
      document.body.appendChild(a2); a2.click(); a2.remove();
      setTimeout(() => URL.revokeObjectURL(a2.href), 1000);
      $("bin-note").textContent = `Saved ${a2.download}: the ${c.kit || "board"}'s flash, ${bytes.length} bytes from address ${start}, sha256 ${c.sha256.slice(0, 12)}... Load it at ${start}, on this board only.`;
    });
  }
  // Live values: every reading this visit (bench.js setLive keeps them in window.frlLiveLog), one row each.
  const lv = $("c-live-csv");
  if (lv) lv.addEventListener("click", () => {
    const rows = window.frlLiveLog || [];
    if (!rows.length) { $("c-live-saved").textContent = "Nothing read yet."; return; }
    const text = "time_utc,name,what,value\n" + rows.map((r) => [iso(r.t), csvCell(r.name), csvCell(r.what), csvCell(r.value)].join(",")).join("\n") + "\n";
    const a = document.createElement("a");
    a.download = `${kit()}-values-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.csv`;
    a.href = URL.createObjectURL(new Blob([text], { type: "text/csv" }));
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    $("c-live-saved").textContent = `Saved ${rows.length} readings as ${a.download} to your Downloads folder.${partialNote(rows, LIVE_MAX, "readings")}`;
  });
})();
