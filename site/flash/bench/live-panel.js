// The shared live-readings panel (System Lab step 4, 2026-10-17): one line format from the board, a table and traces, and
// predicted against measured on one plot with the signed difference under it inside a stated tolerance band.
// Pure functions and SVG strings, no I/O, no fetch, no model: every number is computed here from the samples given.
// Loads in node (require) and in the browser (window.LivePanel). Used by the Digital Lab (site/flash/bench/, copied by
// tools/webflash/build_page.py) and inlined into the motor console by tools/render_motor_console.py.
//
//   const LP = require("./live_panel.js");
//   LP.parseLine("speed:12.5, ref:10")               -> { speed: 12.5, ref: 10 }
//   const f = new LP.Feed(); f.push(t, values)         -> traces per name, sample rate, dropped samples
//   LP.compare(pred, meas, { abs, rel, time })         -> aligned points, pass fraction, violations
//   LP.overlay(cmp, { title, unit, xunit })            -> SVG: both on one axis, difference and band below, red/green bar
(function (root) {
  "use strict";
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
  const ok = (v) => typeof v === "number" && isFinite(v);
  const f1 = (x) => (Math.round(x * 10) / 10).toString();
  const fmt = (x, d) => (ok(x) ? Number(x.toFixed(d === undefined ? 3 : d)).toString() : "n/a");

  // Arduino's serial-plotter convention: "name:value" pairs separated by comma, space or tab, one line per sample. A bare
  // number gets the name v1, v2, ... in order. Labels with spaces are not supported, as in the original.
  function parseLine(line) {
    const out = {}; let k = 0;
    for (const tok of String(line).trim().split(/[,\s]+/)) {
      if (!tok) continue;
      const m = tok.match(/^([A-Za-z_][\w.]*):(-?[\d.]+(?:e-?\d+)?)$/i);
      if (m) out[m[1]] = Number(m[2]);
      else if (/^-?[\d.]+(?:e-?\d+)?$/i.test(tok)) out["v" + (++k)] = Number(tok);
    }
    return out;
  }

  // Traces per name with a cap, the sample rate, and samples dropped: a gap over three times the median spacing counts as
  // dropped samples (the number the gap would have held), shown and never hidden.
  class Feed {
    constructor(opt) { opt = opt || {}; this.cap = opt.cap || 2000; this.traces = {}; this.n = 0; this.dropped = 0; this.t0 = null; this.last = null; this.dts = []; }
    push(t, values) {
      if (this.t0 === null) this.t0 = t;
      if (this.last !== null) {
        const dt = t - this.last; this.dts.push(dt); if (this.dts.length > 200) this.dts.shift();
        const med = median(this.dts);
        if (med > 0 && dt > 3 * med) this.dropped += Math.round(dt / med) - 1;
      }
      this.last = t; this.n++;
      for (const [k, v] of Object.entries(values)) {
        if (!ok(v)) continue;
        const tr = this.traces[k] || (this.traces[k] = { t: [], v: [], unit: "" });
        tr.t.push(t); tr.v.push(v);
        if (tr.t.length > this.cap) { tr.t.shift(); tr.v.shift(); }
      }
    }
    rate() { const med = median(this.dts); return med > 0 ? 1000 / med : 0; }   // samples per second, t in ms
    latest(k) { const tr = this.traces[k]; return tr && tr.v.length ? tr.v[tr.v.length - 1] : undefined; }
    names() { return Object.keys(this.traces); }
  }
  function median(a) { if (!a.length) return 0; const s = [...a].sort((x, y) => x - y); const m = s.length >> 1; return s.length % 2 ? s[m] : 0.5 * (s[m - 1] + s[m]); }

  // Linear interpolation of a series onto new times; outside the series it holds the end values.
  function interp(t, v, at) {
    const out = new Array(at.length); let j = 0;
    for (let i = 0; i < at.length; i++) {
      const x = at[i];
      while (j < t.length - 2 && t[j + 1] < x) j++;
      if (x <= t[0]) out[i] = v[0];
      else if (x >= t[t.length - 1]) out[i] = v[v.length - 1];
      else { const a = t[j], b = t[j + 1]; out[i] = b === a ? v[j] : v[j] + (v[j + 1] - v[j]) * (x - a) / (b - a); }
    }
    return out;
  }

  // Predicted against measured. The prediction is interpolated (linear) onto the measured times; the band is drawn
  // around the prediction (it is the claim under test): abs + rel * |predicted|, and a point is inside if any predicted
  // value within +-time of it is within that band (the time tolerance absorbs a small shift, as Simulink's inspector does).
  // pred and meas are { t: [], v: [] } with t in the same unit (ms). Returns one row per measured sample.
  function compare(pred, meas, tol) {
    tol = Object.assign({ abs: 0, rel: 0, time: 0 }, tol || {});
    if (!pred || !meas || !pred.t || !meas.t || pred.t.length < 2 || meas.t.length < 1) return { points: [], passFrac: null, worst: null, violations: [], tol, n: 0 };
    const p = interp(pred.t, pred.v, meas.t);
    const pts = [], viol = []; let worst = 0, pass = 0, open = null;
    for (let i = 0; i < meas.t.length; i++) {
      const t = meas.t[i], m = meas.v[i];
      let d = m - p[i], band = tol.abs + tol.rel * Math.abs(p[i]), inside = Math.abs(d) <= band;
      if (!inside && tol.time > 0) {   // any predicted value within the time window counts
        const lo = interp(pred.t, pred.v, [t - tol.time])[0], hi = interp(pred.t, pred.v, [t + tol.time])[0];
        const pmin = Math.min(lo, hi, p[i]), pmax = Math.max(lo, hi, p[i]);
        const b2 = tol.abs + tol.rel * Math.max(Math.abs(pmin), Math.abs(pmax));
        if (m >= pmin - b2 && m <= pmax + b2) { inside = true; d = m < pmin ? m - pmin : m > pmax ? m - pmax : 0; }
      }
      if (inside) pass++;
      if (Math.abs(d) > Math.abs(worst)) worst = d;
      pts.push({ t, p: p[i], m, d, band, ok: inside });
      if (!inside) { if (!open) { open = { from: t, to: t, worst: d }; viol.push(open); } else { open.to = t; if (Math.abs(d) > Math.abs(open.worst)) open.worst = d; } }
      else open = null;
    }
    return { points: pts, passFrac: pass / pts.length, worst, violations: viol, tol, n: pts.length, interpolation: "linear, prediction onto the measured sample times" };
  }

  // A square wave as a prediction: [[level, ms], ...] repeated to cover span ms, in the shape compare() reads.
  function squareWave(steps, span, t0) {
    t0 = t0 || 0; const t = [], v = []; let x = t0;
    const cycle = steps.reduce((s, [, ms]) => s + ms, 0);
    if (!(cycle > 0)) return { t: [t0, t0 + span], v: [0, 0] };
    while (x < t0 + span) for (const [lvl, ms] of steps) { t.push(x); v.push(lvl); t.push(x + ms - 1e-6); v.push(lvl); x += ms; if (x >= t0 + span) break; }
    return { t, v };
  }

  // Two panes, one time axis: the top overlays prediction (ink, dashed) and measurement (red); the bottom shows the signed
  // difference inside the band; a bar along the top is green where inside and red where not; a cursor marks one
  // violation (opt.cursor = index into cmp.violations). Pure SVG, the labs' class names (s0.. tl at pa), scoped style.
  function overlay(cmp, opt) {
    opt = opt || {};
    const W = opt.W || 760, H = opt.H || 300, L = 58, R = W - 14, T = 22, mid = Math.round(H * 0.62), B = H - 34, gap = 10;
    const pts = cmp.points || [];
    if (!pts.length) return `<svg viewBox="0 0 ${W} ${H}" class="lp"><text x="${L}" y="${T + 20}" class="tl">${esc(opt.empty || "no samples yet")}</text></svg>`;
    const xs = pts.map((q) => q.t), xmin = Math.min(...xs), xmax = Math.max(...xs) || xmin + 1;
    const ys = pts.flatMap((q) => [q.p, q.m]), ymin0 = Math.min(...ys), ymax0 = Math.max(...ys), pad = (ymax0 - ymin0 || 1) * 0.12;
    const ymin = ymin0 - pad, ymax = ymax0 + pad;
    const ds = pts.flatMap((q) => [q.d, q.band, -q.band]), dmax = Math.max(1e-9, ...ds.map(Math.abs)) * 1.15;
    const X = (t) => L + (t - xmin) / (xmax - xmin) * (R - L);
    const Y1 = (y) => T + (1 - (y - ymin) / (ymax - ymin)) * (mid - T - gap);
    const Y2 = (d) => mid + (1 - (d + dmax) / (2 * dmax)) * (B - mid);
    const path = (f, k) => pts.map((q, i) => (i ? "L" : "M") + f1(X(q.t)) + " " + f1(f(q[k]))).join(" ");
    let s = `<svg viewBox="0 0 ${W} ${H}" class="lp" role="img" aria-label="${esc(opt.title || "predicted against measured")}">`;
    if (opt.style !== false) s += `<style>.lp{font:12px Barlow,system-ui,sans-serif;max-width:100%;height:auto}.lp .pa{fill:#F7F3EC;stroke:#E2D8C9}.lp .gr{stroke:#EEE8DE}.lp .tl{fill:#6B625B}.lp .at{fill:#161312;font-weight:600}.lp .pp{fill:none;stroke:#161312;stroke-width:1.6;stroke-dasharray:5 4}.lp .mm{fill:none;stroke:#B42418;stroke-width:1.8}.lp .dd{fill:none;stroke:#161312;stroke-width:1.4}.lp .band{fill:#B42418;fill-opacity:.12}.lp .okb{fill:#1D6B35}.lp .bad{fill:#B42418}.lp .cur{stroke:#B42418;stroke-width:1;stroke-dasharray:2 3}</style>`;
    s += `<rect class="pa" x="${L}" y="${T}" width="${R - L}" height="${mid - T - gap}"/><rect class="pa" x="${L}" y="${mid}" width="${R - L}" height="${B - mid}"/>`;
    // the pass bar along the top
    let x0 = X(pts[0].t), st = pts[0].ok;
    for (let i = 1; i <= pts.length; i++) {
      const q = pts[i], x = q ? X(q.t) : R;
      if (!q || q.ok !== st) { s += `<rect class="${st ? "okb" : "bad"}" x="${f1(x0)}" y="${T - 8}" width="${f1(Math.max(1, x - x0))}" height="5"/>`; x0 = x; st = q ? q.ok : st; }
    }
    // the band around the prediction, both panes
    const up = pts.map((q, i) => (i ? "L" : "M") + f1(X(q.t)) + " " + f1(Y1(q.p + q.band))).join(" ");
    const dn = pts.slice().reverse().map((q) => "L" + f1(X(q.t)) + " " + f1(Y1(q.p - q.band))).join(" ");
    s += `<path class="band" d="${up} ${dn} Z"/>`;
    const bu = pts.map((q, i) => (i ? "L" : "M") + f1(X(q.t)) + " " + f1(Y2(q.band))).join(" ");
    const bd = pts.slice().reverse().map((q) => "L" + f1(X(q.t)) + " " + f1(Y2(-q.band))).join(" ");
    s += `<path class="band" d="${bu} ${bd} Z"/>`;
    s += `<line class="gr" x1="${L}" x2="${R}" y1="${f1(Y2(0))}" y2="${f1(Y2(0))}"/>`;
    s += `<path class="pp" d="${path(Y1, "p")}"/><path class="mm" d="${path(Y1, "m")}"/><path class="dd" d="${path(Y2, "d")}"/>`;
    if (ok(opt.cursor) && cmp.violations[opt.cursor]) { const vx = X(cmp.violations[opt.cursor].from); s += `<line class="cur" x1="${f1(vx)}" x2="${f1(vx)}" y1="${T}" y2="${B}"/>`; }
    const u = opt.unit || "", xu = opt.xunit || "ms";
    s += `<text class="tl" x="${L}" y="${T - 10}">${esc(opt.title || "")}</text>`;
    s += `<text class="tl" x="${L - 4}" y="${f1(Y1(ymax0))}" text-anchor="end">${fmt(ymax0, 2)}</text><text class="tl" x="${L - 4}" y="${f1(Y1(ymin0))}" text-anchor="end">${fmt(ymin0, 2)}</text>`;
    s += `<text class="tl" x="${L - 4}" y="${f1(Y2(dmax))}" text-anchor="end">+${fmt(dmax / 1.15, 2)}</text><text class="tl" x="${L - 4}" y="${f1(Y2(-dmax)) - 2}" text-anchor="end">−${fmt(dmax / 1.15, 2)}</text>`;
    s += `<text class="tl" x="${L}" y="${B + 14}">${fmt(xmin, 0)} ${esc(xu)}</text><text class="tl" x="${R}" y="${B + 14}" text-anchor="end">${fmt(xmax, 0)} ${esc(xu)}</text>`;
    s += `<text class="at" x="${L}" y="${B + 28}">predicted (dashed) and measured (red), ${esc(u)}; below, measured minus predicted; band ${bandText(cmp.tol, u)}; ${Math.round(100 * cmp.passFrac)} % inside; worst ${fmt(cmp.worst, 3)} ${esc(u)}; ${esc(cmp.interpolation || "")}</text>`;
    return s + "</svg>";
  }
  function bandText(tol, u) {
    const parts = [];
    if (tol.abs) parts.push(`±${fmt(tol.abs, 3)} ${u}`); if (tol.rel) parts.push(`±${Math.round(tol.rel * 100)} %`); if (tol.time) parts.push(`±${fmt(tol.time, 1)} ms in time`);
    return parts.join(" and ") || "none";
  }

  // The readings table: name, value, unit, samples per second and dropped samples, as HTML rows the caller places.
  function tableRows(feed, units) {
    units = units || {};
    return feed.names().map((k) => `<tr><th>${esc(k)}</th><td class="num">${fmt(feed.latest(k), 3)}</td><td>${esc(units[k] || "")}</td></tr>`).join("")
      + `<tr><th>rate</th><td class="num">${fmt(feed.rate(), 1)}</td><td>samples/s, ${feed.dropped} dropped</td></tr>`;
  }

  // Take it away: the compared points as CSV, and as a workbook when an xlsx writer is loaded.
  function csv(cmp, opt) {
    opt = opt || {};
    const u = opt.unit || "", lines = [`t_${opt.xunit || "ms"},predicted_${u},measured_${u},difference_${u},band_${u},inside`];
    for (const q of cmp.points || []) lines.push([q.t, q.p, q.m, q.d, q.band, q.ok ? 1 : 0].map((x) => (typeof x === "number" ? Number(x.toPrecision(7)) : x)).join(","));
    lines.push("", `# band ${bandText(cmp.tol || {}, u)}; ${cmp.n || 0} samples; ${cmp.passFrac === null ? "n/a" : Math.round(100 * cmp.passFrac) + " %"} inside; worst ${fmt(cmp.worst, 4)} ${u}; ${cmp.interpolation || ""}; ${opt.title || ""}`);
    return lines.join("\n");
  }
  function workbook(cmp, opt, Xlsx) {
    opt = opt || {};
    const u = opt.unit || "", pts = cmp.points || [];
    const rows = [[{ v: opt.title || "predicted against measured", s: "title" }], [],
      [{ v: "band", s: "bold" }, bandText(cmp.tol || {}, u)], [{ v: "samples", s: "bold" }, cmp.n || 0], [{ v: "inside", s: "bold" }, cmp.passFrac === null ? "n/a" : cmp.passFrac], [{ v: "worst", s: "bold" }, cmp.worst || 0], [{ v: "interpolation", s: "bold" }, cmp.interpolation || ""], [],
      [`t, ${opt.xunit || "ms"}`, `predicted, ${u}`, `measured, ${u}`, `difference, ${u}`, `band, ${u}`, "inside"].map((v) => ({ v, s: "head" }))];
    const first = rows.length + 1;
    pts.forEach((q, i) => { const r = first + i; rows.push([q.t, q.p, q.m, { f: `C${r}-B${r}`, c: q.d }, q.band, { f: `IF(ABS(D${r})<=E${r},1,0)`, c: q.ok ? 1 : 0 }]); });
    return Xlsx.build({ title: opt.title || "predicted against measured", sheets: [{ name: "Compare", rows, widths: [12, 14, 14, 14, 12, 8], freeze: first - 1 }] });
  }

  // Known answers: run before trusting the compare on a page (tool-review rule 1).
  function selfTest() {
    const bad = [];
    const p = parseLine("speed:12.5, ref:10 7"); if (p.speed !== 12.5 || p.ref !== 10 || p.v1 !== 7) bad.push("parseLine");
    const f = new Feed(); [0, 10, 20, 30, 80, 90].forEach((t) => f.push(t, { a: t })); if (f.dropped !== 4 || Math.abs(f.rate() - 100) > 1e-9) bad.push(`feed dropped ${f.dropped} rate ${f.rate()}`);
    const pred = { t: [0, 10, 20], v: [0, 1, 1] }, meas = { t: [0, 5, 10, 15, 20], v: [0, 0.5, 1, 1.2, 1] };
    const c = compare(pred, meas, { abs: 0.1 }); if (c.n !== 5 || c.passFrac !== 0.8 || Math.abs(c.worst - 0.2) > 1e-12 || c.violations.length !== 1 || c.violations[0].from !== 15) bad.push("compare abs");
    const c2 = compare({ t: [0, 10, 10, 20], v: [0, 0, 1, 1] }, { t: [0, 5, 9, 11, 20], v: [0, 0, 1, 1, 1] }, { abs: 0.05, time: 2 }); if (c2.passFrac !== 1) bad.push(`compare time ${c2.passFrac}`);
    const sq = squareWave([[1, 100], [0, 100]], 400); if (sq.t.length !== 8 || sq.v[0] !== 1 || sq.v[2] !== 0) bad.push("squareWave");
    return bad;
  }

  const api = { parseLine, Feed, interp, compare, squareWave, overlay, tableRows, csv, workbook, selfTest, median };
  if (typeof module !== "undefined" && module.exports) module.exports = api; else root.LivePanel = api;
})(typeof window !== "undefined" ? window : globalThis);
