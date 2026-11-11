// What leaves the Digital Lab as a file, made safe: CSV cells that cannot run as spreadsheet formulas, logs that keep a
// stated number of entries and say when a save is partial, and the copy behind ".bin" and "Put back" kept on the board
// just saved. Pure functions: bench.js and con-save.js use them, tools/webflash/export-safe.test.mjs tests them.

export const CON_MAX = 50000;      // console chunks kept this visit (oldest dropped first)
export const LIVE_MAX = 200000;    // live readings kept this visit (oldest dropped first)

// One quoted CSV cell. Board text starting with = + - @ (or a tab or return before one) would run as a formula in a
// spreadsheet; a leading apostrophe makes it text. A plain number, negative ones included, stays a number.
export function csvCell(v) {
  let s = String(v);
  if (/^[=+\-@\t\r]/.test(s) && !/^[+-]?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(s)) s = "'" + s;
  return `"${s.replace(/"/g, '""')}"`;
}

// Append, keeping the newest max entries; log.dropped counts the entries let go.
export function capPush(log, entry, max) {
  log.push(entry);
  if (log.length > max) { const n = log.length - max; log.splice(0, n); log.dropped = (log.dropped || 0) + n; }
  return log;
}

// "" for a whole session, else the sentence a save adds.
export function partialNote(log, max, what) {
  return log && log.dropped ? ` The page keeps the last ${max.toLocaleString("en-US")} ${what} of a visit, so the oldest ${log.dropped.toLocaleString("en-US")} are not in it.` : "";
}

// Remember a saved copy of a board's flash. st: {saved: Map key -> {sha256, file}, last}. Returns {save}: false when this
// exact content of this board was already saved this visit (no second download), and either way st.last is this copy.
export function rememberCopy(st, key, copy, file, kit) {
  const had = st.saved.get(key);
  const save = !(had && had.sha256 === copy.sha256);
  if (save) st.saved.set(key, { sha256: copy.sha256, file });
  st.last = { ...copy, key, kit, file: save ? file : had.file };
  return { save };
}

// The raw .bin's file name: the board, the copy's own name, the address to load it at.
export function binName(c) {
  const start = "0x" + Number(c.start).toString(16).toUpperCase().padStart(8, "0");
  const kit = String(c.kit || "board").replace(/[^A-Za-z0-9-]+/g, "-");
  return `${kit}-${c.name}-at-${start}.bin`;
}
