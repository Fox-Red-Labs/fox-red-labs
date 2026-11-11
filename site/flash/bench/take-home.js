// "Take it home": the two copy buttons' text, as pure functions. "Copy for your terminal" is the exact lines to run;
// "Copy for your AI" is the sentence an assistant follows (built in ui.js from kit.json). Both are about the board
// and the demo on this page. tools (from kit.json, written by tools/webflash/build_ui.py):
//   { ide: {mac, win, linux: [lines]}, free: {mac, win, linux: [lines]} }   lines are shell lines or "# comments"

export const REPO_WEB = "https://github.com/Fox-Red-Labs/fox-red-labs";
export const REPO_URL = `${REPO_WEB}.git`;
export const CLONE = `git clone --depth 1 --filter=blob:none --sparse ${REPO_URL}\ncd fox-red-labs`;

/** A probe serial number from the USB device is data, not text to trust: letters and digits, 1 to 16, or it is left out everywhere. */
export const safeSerial = (v) => (typeof v === "string" && /^[A-Za-z0-9]{1,16}$/.test(v) ? v : "");

export const PROBE_NOTE = "# --probe is the serial this page read from the probe. If frl says it is not on USB, use the serial `frl detect` lists for this board.";

/** The demo's run lines: the clone prefix a recipe's command already carries is dropped, the probe is added where start.sh takes one. */
export function runLines(further, serial) {
  serial = safeSerial(serial);
  const cmd = (further && further.command) || "";
  const lines = cmd.split("\n").map((l) => l.trim()).filter((l) => l && !/^git clone /.test(l) && l !== "cd fox-red-labs");
  const out = lines.map((l) => (serial && /^\.\/start\.sh /.test(l) && !/--probe /.test(l) ? `${l} --probe ${serial}` : l));
  // The page reads the probe's CMSIS-DAP serial; on some probes (an LP-XDS110) the USB serial that frl lists is a different string.
  return out.some((l) => l.includes(`--probe ${serial}`)) && serial ? [PROBE_NOTE, ...out] : out;
}

/** The whole terminal block for one OS. */
export function terminalText({ os, title, kit, further, serial, tools }) {
  const f = further || {}, t = tools || {}, o = os === "win" || os === "linux" ? os : "mac";
  // macOS zsh ships with interactive comments off, so a pasted "# ..." line is an error there; this line turns them on. bash ignores it.
  const out = o === "mac" ? ["setopt interactive_comments 2>/dev/null", "# Get the project (a few MB)", CLONE] : ["# Get the project (a few MB)", CLONE];
  const ide = f.route === "guided" ? [] : (t.ide && t.ide[o]) || [];
  const free = (t.free && t.free[o]) || [];
  if (f.route === "guided") out.push("# The next command names any TI tool still missing, with its link and size. It installs nothing unasked.");
  else if (ide.length) out.push("# TI tools, once (free myTI sign-in)", ...ide);
  if (free.length) out.push("# Optional, to build and flash without a vendor IDE", ...free);
  if (o === "win" && f.route === "guided") out.push("# Windows: run the next line in Git Bash. It has not been run there yet (docs/frl.md).");
  const run = runLines(f, serial);
  if (f.route === "build" && run.length) {
    // The demo firmware and its build scripts ship in the reader copy (2026-10-24); the clone is sparse, so the folders the build reads come first.
    if (f.dirs && f.dirs.length) out.push("# Add the folders this build reads (the clone holds only the top-level files)", `git sparse-checkout add ${f.dirs.join(" ")}`);
    out.push(`# Build${title ? ` "${title}"` : ""} from source${kit ? ` for the ${kit}` : ""}`, ...run, ...writeLines(f, serial));
  } else if (run.length) out.push(`# Build, flash and check${title ? ` "${title}"` : ""}${kit ? ` on the ${kit}` : ""}`, ...run);
  return out.join("\n");
}

/** After a build: frl writes the image on a board with a route (data/routes.json, further.frl); elsewhere the file to write and the tools that write it. */
export function writeLines(further, serial) {
  const img = (further && further.image) || "";
  const board = img.split("/")[1] || "";
  if (!img) return [];
  if (further.frl) {
    const s = safeSerial(serial);
    return ["# Write it to the board", `python3 tools/frl/frl.py flash --board ${board} --image ${img}${s ? ` --probe ${s}` : ""}`];
  }
  return [`# The build writes ${img}. Write it with UniFlash or CCStudio; frl does not write to this board yet.`];
}

/** One line under the terminal block: what it needs and where it works. */
export function terminalNote(further, os) {
  const f = further || {};
  if (f.route === "guided") return os === "win" ? "Windows is not tried yet." : "It saves the board's flash first, builds, writes and reads the result back.";
  return "This builds the same program from source, with the tool versions under Go further. It needs TI's tools above.";
}
