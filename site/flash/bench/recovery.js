// What the page saves, what a write changes, and whether this page can put the copy back, for one board and recipe;
// and the checks a chosen backup file passes before "Put it back" is offered. Pure functions: the data comes from
// boards.json (each board's "recovery", written by tools/webflash/build_page.py); tests in tools/webflash/recovery.test.mjs.

export const BACKUP_FORMAT = "frl-webflash-backup-1";

// The one-line promise for the banner at the top of the page.
export function bannerFor(board) {
  if (!board || !board.recovery) {
    return { tone: "neutral", lead: "The page saves a copy of the board's flash before it writes anything.",
      text: "Whether it can put the copy back depends on the board." };
  }
  const r = board.recovery;
  if (r.kind === "page" && r.proven === false) return { tone: "caution", lead: "New board support: hardware check pending.", text: "The page saves your main flash before writing. Flashing and putting that copy back have not yet been checked on this board." };
  if (r.kind === "page") return { tone: "safe", lead: "Safe to try on this board.", text: "Before it first writes, the page saves a copy of the board's main flash to your Downloads folder, and this page can put it back. It never writes the boot configuration." };
  if (r.kind === "vendor") return { tone: "caution", lead: "Read “Put back what was there” first on this board.", text: `The page saves a copy before it writes, but cannot put it back: that takes ${r.tool}.` };
  return { tone: "neutral", lead: "Nothing is written to this board.", text: "This page recognizes it and only reads it." };
}

// The three facts shown beside the write button, for the board and the recipe picked.
export function summaryFor(board, recipe) {
  const r = (board && board.recovery) || null;
  if (!r) return null;
  if (r.kind === "none") return { restoreHere: false, lines: [["Saves", "nothing"], ["Changes", r.changes], ["Put back", r.back]] };
  let changes = r.changes;
  if (recipe && recipe.check && recipe.check.kind === "colors" && r.kind === "page") {
    changes = "the first send writes the program into the sectors it needs; later sends rewrite only the one sector that holds your sequence; never the boot configuration";
  }
  const back = r.kind === "page" && r.proven === false ? "available from this page, but restore on this board has not yet been checked on hardware" : r.kind === "page" ? "yes, from this page (“Put back what was there” below)" : `not from this page; with ${r.tool} (steps in “Put back what was there” below). ${r.tested}`;
  return { restoreHere: r.kind === "page", lines: [["Saves", r.saves], ["Changes", changes], ["Put back", back]] };
}

// A chosen file, checked before the button is offered. Returns {ok, message, copy}. The chip itself (trace ID and
// DEVICEID) is checked again after connecting, in Session.restore, before anything is erased.
export async function checkBackup(text, fileName, board, sha256) {
  const name = fileName || "that file";
  if (/\.bin$/i.test(name)) {
    return { ok: false, message: `${name} is a raw flash copy from a CC23 or CC27 board. This page cannot put it back; follow the steps for that board in this section.` };
  }
  let copy;
  try { copy = JSON.parse(text); } catch (e) { return { ok: false, message: `${name} is not a copy this page saved (it is not readable as one). Nothing will be written.` }; }
  if (!copy || copy.format !== BACKUP_FORMAT) return { ok: false, message: `${name} is not a copy this page saved. Choose the .json file the page put in your Downloads folder.` };
  for (const k of ["kit", "traceid", "deviceid", "bytes", "sha256", "saved", "data"]) {
    if (copy[k] === undefined || copy[k] === null || copy[k] === "") return { ok: false, message: `${name} is damaged: it has no ${k}. Nothing will be written.` };
  }
  if (sha256) {
    let bytes;
    try { bytes = Uint8Array.from(atob(copy.data), (c) => c.charCodeAt(0)); } catch (e) { return { ok: false, message: `${name} is damaged: its contents cannot be read. Nothing will be written.` }; }
    if (bytes.length !== copy.bytes || (await sha256(bytes.buffer)) !== copy.sha256) {
      return { ok: false, message: `${name} is damaged: its contents do not match its checksum. Nothing will be written.` };
    }
  }
  if (board) {
    if (!board.recovery || board.recovery.kind !== "page") {
      return { ok: false, message: `This page cannot put a copy back on the ${board.kit}.` };
    }
    if (copy.kit !== board.kit) {
      return { ok: false, message: `That copy came from an ${copy.kit}; the board connected is an ${board.kit}. Nothing will be written. Plug in the board it came from.` };
    }
  }
  const when = String(copy.saved).slice(0, 16).replace("T", " ");
  return { ok: true, copy, message: `A copy of an ${copy.kit} (trace ID ${copy.traceid}), ${copy.bytes >= 1024 ? Math.round(copy.bytes / 1024) + " kB" : copy.bytes + " bytes"}, saved ${when} UTC. It goes back only onto that same chip.` };
}

// A board the page reads and restores but does not write demos to, with the reason (boards.json "readOnly"); "" when it writes.
export const readOnlyReason = (board) => (board && typeof board.readOnly === "string" ? board.readOnly : "");

// How long "Put it back" takes before it starts, so nobody closes the tab in the middle: the page reads the whole flash to find the
// sectors that differ (about 24 kB a second over the XDS110's HID link since reads went one packet per 14 words on 2026-10-22: 64 kB in 2.7 s, was 5.2 s; the MSPM33's 1 MB was 80 s before), then
// erases and writes only those.
export function restoreEstimate(bytes) {
  const read = Math.ceil(bytes / 24000), mins = read >= 90 ? `about ${Math.round(read / 60 * 2) / 2} minutes` : `about ${Math.max(5, Math.round(read / 5) * 5)} seconds`;
  return { seconds: read, text: `Expect ${mins} (it reads the whole flash first, then writes only what differs). Keep this tab open and the cable in until it says it is done: closing it halfway can leave the chip locked.` };
}

// While the page erases or writes the chip, closing or reloading the tab asks first. active() says whether a write or put-back runs now.
export function guardUnload(win, active) {
  const h = (e) => { if (!active()) return; e.preventDefault(); e.returnValue = "The board is being written. Leaving now can leave it locked."; return e.returnValue; };
  win.addEventListener("beforeunload", h);
  return h;
}

// What to tell the person when the board disappears (unplugged, reset, or the link drops) part way through a step.
// phase: "connecting" | "saving" | "writing" | "written" | "restoring"; saved: a copy was saved before this write.
export function interruptedMessage(phase, { saved = false, kit = "the board" } = {}) {
  const head = `The ${kit === "the board" ? "board" : kit} was unplugged or lost its USB link`;
  if (phase === "writing") return saved
    ? { severity: "bad", message: `${head} while the page was writing it. It may now hold a partly written program.`,
      next: "Plug the USB cable back in and press Connect. Then open “Put back what was there”, choose the .json copy this page saved to your Downloads folder before the write, and press Put it back. Run no demo until it says it is done." }
    : { severity: "bad", message: `${head} while the page was writing it. It may now hold a partly written program.`,
      next: "Plug the USB cable back in, press Connect and run the same demo again: the page rewrites what is missing. No copy was saved this time because the board already held this page's program." };
  if (phase === "restoring") return { severity: "bad", message: `${head} during Put back. Part of the saved copy may be back, part not.`,
    next: "Plug the USB cable back in, press Connect and press Put it back again with the same file. It compares first and writes only what still differs." };
  if (phase === "written") return { severity: "warn", message: `${head} after the new program was written and read back.`,
    next: "The page read the new program back and it matched before the link dropped. Plug the USB cable back in and press Connect to check it, or put the saved copy back." };
  return { severity: "warn", message: `${head} before anything was written.`, next: "Plug the USB cable back in and press Connect. The board is unchanged." };
}
