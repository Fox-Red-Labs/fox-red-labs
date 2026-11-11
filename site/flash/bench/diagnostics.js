// A diagnostic record of one visit to the Digital Lab: what the page tried, in order, and how each step ended, for a
// person to attach to a report. Pure: no page, no board; tests in tools/webflash/diagnostics.test.mjs.
//
// What it never holds: flash contents (the backup's data), Wi-Fi names or passwords, server addresses, typed messages,
// file paths, or a full probe serial (only its last two characters, enough to tell two probes apart in one record).

export const DIAG_FORMAT = "frl-digital-lab-diagnostic-1";
const MAX_EVENTS = 400;
// Keys whose values are dropped wherever they appear, whatever the caller passed in.
const SECRET_KEYS = /^(data|password|pass|psk|wifi|ssid|key|token|secret|server|host|url|address|message|text|path|file|files)$/i;

/** Last two characters of a probe serial, so a record never carries the whole one. */
export function maskSerial(s) {
  const v = String(s || "").replace(/[^A-Za-z0-9]/g, "");
  if (!v) return "";
  return v.length <= 2 ? "**" : "*".repeat(Math.min(6, v.length - 2)) + v.slice(-2);
}

// Scrub one value: secret keys out, serials masked, long strings cut, anything that looks like a URL or a local path removed.
function scrub(v, depth = 0) {
  if (v === null || v === undefined) return v;
  if (typeof v === "number" || typeof v === "boolean") return v;
  if (typeof v === "string") {
    let t = v.replace(/https?:\/\/\S+/g, "<url>").replace(/(?:\/Users|\/home|[A-Z]:\\Users)[^\s"']*/g, "<path>")
      .replace(/(?<![\/.\d])\b(?:\d{1,3}\.){3}\d{1,3}\b(?![.\d])/g, "<ip>");   // not a version after a slash (Chrome/150.0.1.2)
    return t.length > 300 ? t.slice(0, 300) + "…" : t;
  }
  if (depth > 4) return "<nested>";
  if (Array.isArray(v)) return v.slice(0, 40).map((x) => scrub(x, depth + 1));
  if (typeof v === "object") {
    const out = {};
    for (const [k, x] of Object.entries(v)) {
      if (SECRET_KEYS.test(k)) continue;
      out[k] = /^(serial|probe|probeSerial)$/i.test(k) && typeof x === "string" ? maskSerial(x) : scrub(x, depth + 1);
    }
    return out;
  }
  return String(v);
}

/** The trail: add(step, fields) records one event with the time since the page opened. */
export function createTrail(now = () => Date.now()) {
  const t0 = now(), events = [];
  return {
    events,
    add(step, fields = {}) {
      events.push({ ms: Math.round(now() - t0), step: String(step), ...scrub(fields) });
      if (events.length > MAX_EVENTS) events.splice(0, events.length - MAX_EVENTS);
    },
  };
}

/** The record a person downloads. env: browser facts the page can see; state: the board and demo in hand. */
export function diagnosticRecord(trail, env = {}, state = {}) {
  return {
    format: DIAG_FORMAT,
    created: new Date().toISOString(),
    note: "Made by the Digital Lab for a problem report. It holds the steps the page took and their outcome; no flash contents, network names, passwords, addresses or full serial numbers.",
    browser: scrub({ userAgent: env.userAgent, platform: env.platform, secure: env.secure, webhid: env.webhid, webusb: env.webusb, webserial: env.webserial, page: env.page }),
    board: scrub(state.board || null),
    demo: scrub(state.demo || null),
    lastResult: scrub(state.lastResult || null),
    events: trail.events.map((e) => scrub(e)),
  };
}

/** A file name for the record: no serial, no board path. */
export function diagnosticName(when = new Date()) {
  return `digital-lab-diagnostic-${when.toISOString().slice(0, 19).replace(/[-:T]/g, "")}.json`;
}
